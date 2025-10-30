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
#include "FileAccessChar.h"
#include "FileAccessCharProxy.h"
#include "NameService/FileChar.h"
#include "Chrono.h"

namespace Nirvana {
namespace Core {

FileAccessChar::FileAccessChar (FileChar* file, unsigned flags, DeadlineTime callback_deadline,
	unsigned initial_buffer_size, unsigned max_buffer_size) :
	file_ (file),
	ring_buffer_ (initial_buffer_size),
	max_buffer_size_ (max_buffer_size),
	read_pos_ (0),
	write_pos_ (0),
	utf8_char_size_ (0),
	utf8_octet_cnt_ (0),
	sync_context_ (&SyncContext::current ()),
	read_available_ (0),
	write_available_ (initial_buffer_size),
	flags_ (flags),
	read_error_ (0),
	callback_ ATOMIC_FLAG_INIT,
	callback_deadline_ (callback_deadline)
{}

FileAccessChar::~FileAccessChar ()
{}

void FileAccessChar::_remove_ref () noexcept
{
	if (0 == ref_cnt_.decrement ()) {
		if (write_request_) {
			write_request_->cancel ();
			write_request_->wait ();
		}
		if (file_)
			file_->on_access_destroy ();
		delete this;
	}
}

void FileAccessChar::on_read (char c) noexcept
{
	ring_buffer_ [write_pos_] = c;
	write_pos_ = (write_pos_ + 1) % ring_buffer_.size ();
	read_available_.increment ();
	async_callback ();
	if (write_available_.decrement_seq ())
		read_start ();
}

void FileAccessChar::on_read_error (int err) noexcept
{
	read_error_ = err;
	async_callback ();
}

void FileAccessChar::async_callback () noexcept
{
	if (!callback_.test_and_set ()) {
		try {
			ExecDomain::async_call <ReadCallback> (Chrono::make_deadline (callback_deadline_), *sync_context_,
				nullptr, std::ref (*this));
		} catch (...) {
			callback_.clear ();
		}
	}
}

inline
bool FileAccessChar::push_char (char c, IDL::String& s)
{
	if (utf8_octet_cnt_ < utf8_char_size_) {
		if (is_additional_octet (c))
			utf8_char_ [utf8_octet_cnt_++] = c;
		else {
			utf8_octet_cnt_ = 0;
			utf8_char_size_ = 0;
			return false;
		}
	}

	if (utf8_char_size_ != 0 && utf8_octet_cnt_ == utf8_char_size_) {
		s.append (utf8_char_, utf8_char_size_);
		utf8_octet_cnt_ = 0;
		utf8_char_size_ = 0;
	}

	unsigned char_size = utf8_char_size (c);
	if (!char_size)
		return false;
	else if (char_size > 1) {
		utf8_octet_cnt_ = 1;
		utf8_char_size_ = char_size;
		utf8_char_ [0] = c;
	} else
		s += c;

	return true;
}

inline
void FileAccessChar::push_char (char c, bool& repl, IDL::String& s)
{
	// Invalid UTF-8 character replacement
	static const char replacement [] = { '\xEF', '\xBF', '\xBD' };

	if (push_char (c, s))
		repl = false;
	else if (!repl) {
		s.append (replacement, sizeof (replacement));
		repl = true;
	}
}

inline
void FileAccessChar::read_callback () noexcept
{
	auto cc = read_available_.load ();
	callback_.clear ();
	IDL::String data;
	AccessChar::Error error{ 0 };
	if (cc) {
		bool overflow = cc == ring_buffer_.size ();
		try {
			data.reserve (cc + utf8_octet_cnt_);
			bool repl = false;
			while (cc--) {
				push_char (ring_buffer_ [read_pos_], repl, data);
				read_pos_ = (read_pos_ + 1) % ring_buffer_.size ();
				write_available_.increment ();
				read_available_.decrement ();
			}
		} catch (...) {
			data.clear ();
			error.error (ENOMEM);
		}

		if (overflow && ring_buffer_.size () < max_buffer_size_) {
			try {
				ring_buffer_.resize (ring_buffer_.size () * 2);
				read_pos_ = 0;
				write_pos_ = 0;
			} catch (...) {
				// Do not consider this as an read error
				// TODO: Log
			}
			read_start ();
		}

	} else {
		assert (read_error_);
		if (!read_error_)
			return;
		error.error (read_error_);
		read_error_ = 0;
	}

	CORBA::Any evt;
	if (!error.error ())
		evt <<= std::move (data);
	else
		evt <<= error;

	push_event (evt);
	
	if (pull_consumer_cnt_) {
		try {
			if (pull_queue_.empty ()) {
				pull_queue_.push (std::move (evt));
				pull_event_.signal ();
			} else {
				Event& last = pull_queue_.back ();
				if (last.type () == Event::EVT_READ && !error.error ()) {
					const IDL::String* p;
					evt >>= p;
					last.data () += *p;
				} else if (last.type () != Event::EVT_ERROR || last.error () != error.error ())
					pull_queue_.push (std::move (evt));
			}
		} catch (...) {
			// TODO: Log
		}
	}
}

void FileAccessChar::push_event (const CORBA::Any& evt) const noexcept
{
	for (auto it = push_proxies_.begin (); it != push_proxies_.end (); ++it) {
		it->push (evt);
	}
}

CORBA::Any FileAccessChar::pull_queue_pop () noexcept
{
	assert (!pull_queue_.empty ());
	CORBA::Any ret = pull_queue_.pop ();
	if (pull_queue_.empty ())
		pull_event_.reset ();
	return ret;
}

void FileAccessChar::ReadCallback::run () noexcept
{
	object_->read_callback ();
}

FileAccessChar::Event::U::U (Type type, U&& src) noexcept
{
	switch (type) {
	case EVT_READ:
		CORBA::Internal::construct (data, std::move (src.data));
		break;
	case EVT_ERROR:
		error = src.error;
		break;
	default:
		CORBA::Internal::construct (other, std::move (src.other));
	}
}

void FileAccessChar::Event::U::destruct (Type type) noexcept
{
	switch (type) {
	case EVT_READ:
		CORBA::Internal::destruct (data);
		break;
	case EVT_ERROR:
		break;
	default:
		CORBA::Internal::destruct (other);
	}
}

}
}
