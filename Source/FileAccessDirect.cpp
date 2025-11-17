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
#include "FileAccessDirect.h"
#include <Port/Memory.h>

namespace Nirvana {
namespace Core {

const SteadyTime FileAccessDirect::DEFAULT_WRITE_TIMEOUT;
const SteadyTime FileAccessDirect::DEFAULT_DISCARD_TIMEOUT;
const TimeBase::TimeT FileAccessDirect::HOUSEKEEPING_PERIOD;

FileAccessDirect::~FileAccessDirect ()
{
	try {
		housekeeping_timer_->cancel ();

		for (Cache::iterator it = cache_.begin (); it != cache_.end (); ++it) {
			if (it->second.request)
				complete_request (*it);
			Port::Memory::release (it->second.buffer, block_size_);
		}
		if (size_request_)
			size_request_->wait ();
	} catch (...) {
		// TODO: Log
		assert (false);
	}
}

FileAccessDirect::CacheRange FileAccessDirect::request_read (BlockIdx begin_block, BlockIdx end_block)
{
	assert (end_block > begin_block);

	CacheRange blocks (cache_);
	try {
		for (Cache::iterator cached_block = cache_.lower_bound (begin_block);;) {
			BlockIdx not_cached_end;
			if (cached_block == cache_.end ())
				not_cached_end = end_block;
			else
				not_cached_end = std::min (end_block, cached_block->first);
			if (begin_block < not_cached_end) {
				// Issue new read request
				size_t cb = (Size)(not_cached_end - begin_block) * block_size_;
				void* buffer = Port::Memory::allocate (nullptr, cb, 0);
				unsigned new_blocks = 0;
				try {
					uint8_t* block_buf = (uint8_t*)buffer;
					Cache::iterator it_last = cache_.emplace_hint (cached_block, begin_block, block_buf);
					Cache::iterator it_first = it_last;
					Pos offset = begin_block * block_size_;
					for (;;) {
						blocks.append (it_last);
						++new_blocks;
						if (not_cached_end == ++begin_block)
							break;
						block_buf += block_size_;
						it_last = cache_.emplace_hint (cached_block, begin_block, block_buf);
					}
					Ref <IO_Request> request = Base::read (offset, buffer, (Size)cb);
					++it_last;
					for (Cache::iterator it = it_first; it != it_last; ++it) {
						it->second.request = request;
						it->second.request_op = OP_READ;
						it->second.first_request_entry = it_first;
					}
				} catch (...) {
					while (new_blocks--) {
						cache_.erase (--blocks.end);
					}
					Port::Memory::release (buffer, cb);
					throw;
				}
				if (begin_block == end_block)
					break;
			}

			if (cached_block != cache_.end ()) {
				++(cached_block->second.lock_cnt);
				blocks.append (cached_block);
				++cached_block;
			}

			if (end_block == ++begin_block)
				break;
		}

	} catch (...) {
		while (blocks.begin != blocks.end) {
			unlock (*blocks.begin);
			++blocks.begin;
		}
		throw;
	}

	return blocks;
}

void FileAccessDirect::set_dirty (Cache::reference entry, size_t offset, size_t size) noexcept
{
	assert (size > 0 && size <= block_size_);
	if (!entry.second.dirty ())
		++dirty_blocks_;
	size_t block = offset / base_block_size_;
	if (entry.second.dirty_begin > block)
		entry.second.dirty_begin = (uint8_t)block;
	block = (offset + size + base_block_size_ - 1) / base_block_size_;
	if (entry.second.dirty_end < block)
		entry.second.dirty_end = (uint8_t)block;
}

void FileAccessDirect::write_dirty_blocks (SteadyTime timeout)
{
	for (Cache::iterator block = cache_.begin (); dirty_blocks_ && block != cache_.end (); ) {
		if (block->second.dirty ()) {
			SteadyTime time = Chrono::steady_clock ();
			if (time - block->second.last_write_time >= timeout) {
				Cache::iterator first_block = block;
				BlockIdx idx = first_block->first;
				BlockIdx max_idx = idx + std::numeric_limits <Size>::max () / block_size_;
				uint8_t* buf = (uint8_t*)first_block->second.buffer;
				size_t dirty_begin = block->second.dirty_begin;
				size_t dirty_end;
				do {
					dirty_end = block->second.dirty_end;
					++idx;
					++block;
					buf += block_size_;
				} while (block != cache_.end ()
					&& block->second.dirty ()
					&& idx < max_idx && block->first == idx
					&& block->second.buffer == buf
					&& time - block->second.last_write_time >= timeout);

				dirty_begin *= base_block_size_;
				dirty_end *= base_block_size_;
				Pos pos = (Pos)first_block->first * (Pos)block_size_ + (Pos)dirty_begin;
				Size size = (Size)((Pos)(idx - 1) * (Pos)block_size_ + (Pos)dirty_end - pos);
				Ref <IO_Request> request = Base::write (pos, (uint8_t*)first_block->second.buffer + dirty_begin, size);

				for (Cache::iterator it = first_block; it != block; ++it) {
					it->second.request = request;
					it->second.request_op = OP_WRITE;
					it->second.first_request_entry = first_block;
					clear_dirty (*it);
				} while (++first_block != block);

				continue;
			}
		}
		++block;
	}
}

void FileAccessDirect::complete_request (Cache::reference entry, int op)
{
	while (entry.second.request && (entry.second.request->signalled ()
		|| (!op || entry.second.request_op == op))) {

		lock (entry);
		{
			Ref <IO_Request> request = entry.second.request;
			request->wait ();
			if (entry.second.request == request) { // Not yet processed
				IO_Result result = request->result ();
				Cache::iterator block = entry.second.first_request_entry;
				for (;;) {
					block->second.request = nullptr;
					if (result.size >= block_size_) {
						block->second.error = 0;
						result.size -= block_size_;
					} else if ((block->second.error = result.error) && entry.second.request_op == OP_WRITE) {
						set_dirty (*block, result.size, block_size_ - result.size);
					}
					if ((cache_.end () == ++block) || block->second.request != request)
						break;
				}
			}
		}
		unlock (entry);
	}
	if (entry.second.error)
		throw_INTERNAL (make_minor_errno (entry.second.error));
}

void FileAccessDirect::complete_size_request () noexcept
{
	assert (size_request_);
	Ref <IO_Request> rq = size_request_;
	rq->wait ();
	if (size_request_ == rq) {
		if (!rq->result ().error)
			file_size_ = requested_size_;
		size_request_ = nullptr;
	}
}

bool FileAccessDirect::release_cache (Cache::iterator& it, SteadyTime time)
{
	if (it->second.request && it->second.request->signalled ())
		complete_request (*it);
	if (!it->second.lock_cnt && !it->second.dirty ()
		&& (
			(Pos)it->first * (Pos)block_size_ >= file_size_
			|| (!it->second.error && Port::Memory::is_private (it->second.buffer, block_size_)
				&& it->second.last_read_time <= time)))
	{
		Port::Memory::release (it->second.buffer, block_size_);
		it = cache_.erase (it);
		return true;
	} else
		return false;
}

void FileAccessDirect::clear_cache (BlockIdx excl_begin, BlockIdx excl_end)
{
	SteadyTime time = Chrono::steady_clock ();
	if (time < discard_timeout_)
		return;
	time -= discard_timeout_;

	for (Cache::iterator it = cache_.begin (); it != cache_.end ();) {
		if (it->first < excl_begin) {
			if (!release_cache (it, time))
				++it;
		} else
			break;
	}
	
	for (Cache::iterator it = cache_.end (); it != cache_.begin ();) {
		--it;
		if (it->first >= excl_end)
			release_cache (it, time);
		else
			break;
	}
}

void FileAccessDirect::set_size (Pos new_size)
{
	while (size_request_)
		complete_size_request ();

	requested_size_ = new_size;
	Ref <IO_Request> rq = Base::set_size (new_size);
	size_request_ = rq;
	complete_size_request ();
	int err = rq->result ().error;
	if (err)
		throw_INTERNAL (make_minor_errno (err));
}

}
}
