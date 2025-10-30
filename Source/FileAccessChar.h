/// \file
/*
* Nirvana Core.
*
* This is a part of the Nirvana project.
*
* Author: Igor Popov
*
* Copyright (c) 2021 Igor Popov.
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU Lesser General Public License as published by
* the Free Software Foundation; either version 3 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU Lesser General Public
* License along with this library.  If not, see <http://www.gnu.org/licenses/>.
*
* Send comments and/or bug reports to:
*  popov.nirvana@gmail.com
*/
#ifndef NIRVANA_CORE_FILEACCESSCHAR_H_
#define NIRVANA_CORE_FILEACCESSCHAR_H_
#pragma once

#include "IO_Request.h"
#include "UserAllocator.h"
#include "UserObject.h"
#include "EventSync.h"
#include <Nirvana/File.h>
#include <Nirvana/posix_defs.h>
#include <Nirvana/SimpleList.h>
#include <CORBA/CosEventChannelAdmin.h>
#include <deque>

namespace Nirvana {
namespace Core {

class FileAccessCharProxy;
class FileChar;

/// Base class for character device access
class FileAccessChar : public UserObject
{
public:
	virtual bool isatty () const = 0;

	void write (const IDL::String& data)
	{
		while (write_request_) {
			Ref <IO_Request> rq = write_request_;
			rq->wait ();
			if (rq == write_request_) {
				write_request_ = nullptr;
				break;
			}
		}
		Ref <IO_Request> rq = write_start (data);
		write_request_ = rq;
		rq->wait ();
		if (rq == write_request_)
			write_request_ = nullptr;
		auto err = rq->result ().error;
		if (err)
			throw_INTERNAL (make_minor_errno (err));
	}

	FileChar* file () const noexcept
	{
		return file_;
	}

	void check_flags (unsigned flags) const
	{
		unsigned acc_mode = flags_ & O_ACCMODE;
		if ((acc_mode == O_WRONLY || acc_mode == O_RDONLY) && acc_mode != (flags & O_ACCMODE))
			throw_NO_PERMISSION (make_minor_errno (EACCES)); // File is unaccessible for this mode
	}

	unsigned flags () const noexcept
	{
		return flags_;
	}

	void add_push_consumer (FileAccessCharProxy& proxy);
	void remove_push_consumer (FileAccessCharProxy& proxy) noexcept;

	void add_pull_consumer ()
	{
		// Does not allow more than one pull consumer.
		if (pull_consumer_cnt_)
			throw CosEventChannelAdmin::AlreadyConnected ();
		bool first = push_proxies_.empty () && !pull_consumer_cnt_;
		++pull_consumer_cnt_;
		if (first)
			read_start ();
	}

	void remove_pull_consumer () noexcept
	{
		assert (pull_consumer_cnt_);
		if (!--pull_consumer_cnt_) {
			pull_queue_.clear ();
			pull_event_.reset ();
			if (push_proxies_.empty ())
				read_cancel ();
		}
	}

	int_fast16_t clear_read_error () noexcept
	{
		if (read_error_) {
			int_fast16_t err = read_error_;
			read_error_ = 0;
			if (!push_proxies_.empty () || pull_consumer_cnt_)
				read_start ();
			return err;
		} else
			return 0;
	}

	CORBA::Any pull ()
	{
		while (pull_queue_.empty ()) {
			pull_event_.wait ();
		}
		return pull_queue_pop ();
	}

	CORBA::Any try_pull (bool& has_event) noexcept
	{
		CORBA::Any ret;
		if (pull_queue_.empty ())
			has_event = false;
		else {
			ret = pull_queue_pop ();
			has_event = true;
		}
		return ret;
	}

	CORBA::Any pull_queue_pop () noexcept;

protected:
	template <class> friend class CORBA::servant_reference;

	void _add_ref () noexcept
	{
		ref_cnt_.increment ();
	}

	void _remove_ref () noexcept;

	FileAccessChar (FileChar* file, unsigned flags = O_RDWR, DeadlineTime callback_deadline = 1 * TimeBase::MILLISECOND,
		unsigned initial_buffer_size = 256, unsigned max_buffer_size = 4096);

	virtual ~FileAccessChar ();

	/// Start reading one character.
	/// Must be overridden in the implementation class.
	/// This method must return immediately.
	/// 
	/// When character read, on_read() method will be called.
	/// If error occurred, on_error() method will be called.
	/// 
	/// To stop reading, use read_cancel().
	///
	virtual void read_start () noexcept = 0;

	/// Stops the input wait.
	/// This method must guaratee that after return no on_read() or on_error() callback will be called.
	virtual void read_cancel () noexcept = 0;

	/// Start write buffer to output.
	/// 
	/// \param data Characters to write.
	/// \returns IO_Request reference.
	virtual Ref <IO_Request> write_start (const IDL::String& data) = 0;

	/// Called from the interrupt level when character is readed.
	///  
	/// \param c Readed character.
	void on_read (char c) noexcept;

	/// Called from the interrupt level on reading error.
	///
	/// \param err Error number.
	void on_read_error (int err) noexcept;

	/// Push custom event.
	/// 
	/// Derived device drivers can push own event types.
	/// 
	void push_custom_event (CORBA::Any&& evt)
	{
		push_event (evt);
		if (pull_consumer_cnt_)
			pull_queue_.push (std::move (evt));
	}

private:
	void async_callback () noexcept;
	void read_callback () noexcept;

	static bool is_additional_octet (int c) noexcept
	{
		return (0xC0 & c) == 0x80;
	}

	static unsigned utf8_char_size (int first_octet) noexcept
	{
		if ((first_octet & 0x80) == 0)
			return 1;
		else if ((first_octet & 0xE0) == 0xC0)
			return 2;
		else if ((first_octet & 0xF0) == 0xE0)
			return 3;
		else if ((first_octet & 0xF8) == 0xF0)
			return 4;
		else
			return 0;
	}

	bool push_char (char c, IDL::String& s);
	void push_char (char c, bool& repl, IDL::String& s);
	void push_event (const CORBA::Any& evt) const noexcept;

private:
	class ReadCallback : public Runnable
	{
	public:
		ReadCallback (FileAccessChar& obj) :
			object_ (&obj)
		{}

	private:
		virtual void run () noexcept override;

	private:
		Ref <FileAccessChar> object_;
	};

	class Event
	{
	public:
		enum Type
		{
			EVT_READ,
			EVT_ERROR,
			EVT_OTHER
		};

		Event (IDL::String&& data) noexcept :
			type_ (EVT_READ),
			u_ (std::move (data))
		{}

		Event (AccessChar::Error err) noexcept :
			type_ (EVT_ERROR),
			u_ (err)
		{}

		Event (CORBA::Any&& evt) noexcept :
			type_ (EVT_OTHER),
			u_ (std::move (evt))
		{}

		Event (Event&& src) noexcept :
			type_ (src.type_),
			u_ (src.type_, std::move (src.u_))
		{}

		~Event ()
		{
			u_.destruct (type_);
		}

		Event& operator = (Event&& src) noexcept
		{
			u_.destruct (type_);
			new (&u_) U (type_ = src.type_, std::move (src.u_));
			return *this;
		}

		Type type () const noexcept
		{
			return type_;
		}

		IDL::String& data () noexcept
		{
			assert (EVT_READ == type_);
			return u_.data;
		}

		int error () const noexcept
		{
			assert (EVT_ERROR == type_);
			return u_.error.error ();
		}

		CORBA::Any get_any () noexcept
		{
			CORBA::Any any;
			switch (type_) {
			case EVT_READ:
				any <<= std::move (u_.data);
				break;

			case EVT_ERROR:
				any <<= u_.error;
				break;

			default:
				any = std::move (u_.other);
				break;
			}

			return any;
		}

	private:
		union U
		{
			U (IDL::String&& s) noexcept
			{
				CORBA::Internal::construct (data, std::move (s));
			}

			U (AccessChar::Error err) noexcept :
				error (err)
			{}

			U (CORBA::Any&& evt) noexcept :
				other (std::move (evt))
			{}

			U (Type type, U&& src) noexcept;

			~U ()
			{}

			void destruct (Type type) noexcept;

			IDL::String data;
			AccessChar::Error error;
			CORBA::Any other;
		};
		
		Type type_;
		U u_;
	};

	class EventQueue : private std::deque <Event>
	{
		typedef std::deque <Event> Base;

	public:
		void push (CORBA::Any&& evt)
		{
			Base::emplace_back (std::move (evt));
		}

		bool empty () const noexcept
		{
			return Base::empty ();
		}

		CORBA::Any pop () noexcept
		{
			assert (!empty ());
			CORBA::Any any = front ().get_any ();
			Base::pop_front ();
			return any;
		}

		Event& back () noexcept
		{
			assert (!empty ());
			return Base::back ();
		}

		void clear () noexcept
		{
			return Base::clear ();
		}
	};

private:
	Ref <FileChar> file_;
	std::vector <char, UserAllocator <char> > ring_buffer_;
	unsigned max_buffer_size_;
	unsigned read_pos_;
	unsigned write_pos_;
	char utf8_char_ [4];
	unsigned utf8_char_size_;
	unsigned utf8_octet_cnt_;
	Ref <IO_Request> write_request_;
	Ref <SyncContext> sync_context_;
	RefCounter ref_cnt_;
	AtomicCounter <false> read_available_;
	AtomicCounter <false> write_available_;
	unsigned flags_;
	int read_error_;
	std::atomic_flag callback_;
	DeadlineTime callback_deadline_;
	SimpleList <FileAccessCharProxy> push_proxies_;

	// If pull_consumer_cnt_ != 0 we have to maintain event queue.
	unsigned pull_consumer_cnt_;
	EventQueue pull_queue_;
	EventSync pull_event_;
};

}
}

#endif
