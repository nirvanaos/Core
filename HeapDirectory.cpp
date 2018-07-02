#include "HeapDirectory.h"
#include <algorithm>
#include <Memory.h>
#include <nlz.h>

namespace Nirvana {

using namespace std;

// BlockIndex1 �� ������� ����� ���������� �������� ������ ������ � m_free_block_index

const HeapDirectory::BlockIndex1 HeapDirectory::sm_block_index1 [HEAP_LEVELS] =
{

#if (HEAP_DIRECTORY_SIZE == 0x10000)
// FREE_BLOCK_INDEX_SIZE == 15

{1,    0},  // �������� �� 4 �����
{2,    4},  // �������� �� 2 �����
{4,    6},
{8,    7},
{16,   8},
{32,   9},
{64,   10},
{128,  11},
{256,  12},
{512,  13},
{1024, 14}

#elif (HEAP_DIRECTORY_SIZE == 0x8000)
// FREE_BLOCK_INDEX_SIZE == 8

{1,    0},  // �������� �� 2 �����
{2,    2},
{4,    3},
{8,    4},
{16,   5},
{32,   6},
{64,   7},  // 5 ������� ������� ����������
{128,  7},
{256,  7},
{512,  7},
{1024, 7}

#elif (HEAP_DIRECTORY_SIZE == 0x4000)
// FREE_BLOCK_INDEX_SIZE == 4

{1,    0},
{2,    1},
{4,    2},
{8,    3},  // 8 ������� ������� ����������
{16,   3},
{32,   3},
{64,   3},
{128,  3},
{256,  3},
{512,  3},
{1024, 3}

#else
#error HEAP_DIRECTORY_SIZE is invalid.
#endif

};

// BlockIndex2 �� ��������� �������� �� ����� ������� m_free_block_cnt ����������
// ������� � ��������� ������� ������� �����.

const HeapDirectory::BlockIndex2 HeapDirectory::sm_block_index2 [FREE_BLOCK_INDEX_SIZE] =
{
#if (HEAP_DIRECTORY_SIZE == 0x10000)
// FREE_BLOCK_INDEX_SIZE == 15

{0,  0},
{1,  TOP_BITMAP_WORDS},
{2,  TOP_BITMAP_WORDS * 3},
{3,  TOP_BITMAP_WORDS * 7},
{4,  TOP_BITMAP_WORDS * 15},
{5,  TOP_BITMAP_WORDS * 31},
{6,  TOP_BITMAP_WORDS * 63},
{7,  TOP_BITMAP_WORDS * 127},
{8,  TOP_BITMAP_WORDS * 255},
{9,  TOP_BITMAP_WORDS * (511 + 256)},
{9,  TOP_BITMAP_WORDS * 511},
{10, TOP_BITMAP_WORDS * (1023 + 512 + 256)},
{10, TOP_BITMAP_WORDS * (1023 + 512)},
{10, TOP_BITMAP_WORDS * (1023 + 256)},
{10, TOP_BITMAP_WORDS * 1023}

#elif (HEAP_DIRECTORY_SIZE == 0x8000)
// FREE_BLOCK_INDEX_SIZE == 8

{4,  TOP_BITMAP_WORDS * 15},
{5,  TOP_BITMAP_WORDS * 31},
{6,  TOP_BITMAP_WORDS * 63},
{7,  TOP_BITMAP_WORDS * 127},
{8,  TOP_BITMAP_WORDS * 255},
{9,  TOP_BITMAP_WORDS * 511},
{10, TOP_BITMAP_WORDS * (1023 + 512)},
{10, TOP_BITMAP_WORDS * 1023}

#elif (HEAP_DIRECTORY_SIZE == 0x4000)
// FREE_BLOCK_INDEX_SIZE == 4

// ����� �������� ������������. ����� �� ����� ������?
{7,  TOP_BITMAP_WORDS * 127},
{8,  TOP_BITMAP_WORDS * 255},
{9,  TOP_BITMAP_WORDS * 511},
{10, TOP_BITMAP_WORDS * 1023}

#endif

};

HeapDirectory* HeapDirectory::create (Memory_ptr memory)
{
	// Reserve memory.
	HeapDirectory* p = reinterpret_cast <HeapDirectory*>(memory->allocate (0, sizeof (HeapDirectory), Memory::RESERVED | Memory::ZERO_INIT));

	// Commit initial part.
	memory->commit (p, reinterpret_cast <Octet*> (p->m_bitmap + TOP_BITMAP_WORDS) - reinterpret_cast <Octet*> (p));

	// Initialize
	initialize (p);

	return p;
}

void HeapDirectory::initialize (HeapDirectory* zero_filled_buf)
{
	assert (sizeof (HeapDirectory) <= HEAP_DIRECTORY_SIZE);
	// Initialize free blocs count on top level.
	zero_filled_buf->m_free_block_index [FREE_BLOCK_INDEX_SIZE - 1] = TOP_LEVEL_BLOCKS;

	// Initialize top level of bitmap by ones.
	fill_n (zero_filled_buf->m_bitmap, TOP_BITMAP_WORDS, ~0);
}

bool HeapDirectory::empty () const
{
#if (HEAP_DIRECTORY_SIZE < 0x10000)

	// ������� ������ ����������
	const UWord* end = m_bitmap + TOP_BITMAP_WORDS;

	for (const UWord* p = m_bitmap; p < end; ++p)
		if (~*p)
			return false;

	return true;

#else

	return (TOP_LEVEL_BLOCKS == m_free_block_index [FREE_BLOCK_INDEX_SIZE - 1]);

#endif
}

Word HeapDirectory::allocate (UWord size, Memory_ptr memory)
{
	assert (size);
	assert (size <= MAX_BLOCK_SIZE);

	// Quantize block size
	const BlockIndex1* pi1 = sm_block_index1 + 32 - nlz ((ULong)(size - 1));
	assert (pi1->m_block_size >= size);
	UWord block_index_offset = pi1->m_block_index_offset;

	// Search in free block index
	UShort* free_blocks_ptr = m_free_block_index + block_index_offset;
	Word cnt = FREE_BLOCK_INDEX_SIZE - block_index_offset;
	while ((cnt--) && !*free_blocks_ptr)
		++free_blocks_ptr;
	if (cnt < 0)
		return -1; // no such blocks

	// ����������, ��� ������
	// Search in bitmap
	BlockIndex2 bi2 = sm_block_index2 [cnt];

	UWord* bitmap_ptr;

#if (HEAP_DIRECTORY_SIZE < 0x10000)

	// ������� ������ ����������

	if (!cnt) { // ������� ������

							// �� ������� ������� ����� �������� ������� � �������� ������ ������
		UWord level = sm_block_index1 + HEAP_LEVELS - 1 - pi1;
		if (bi2.m_level > level) {
			bi2.m_level = level;

			bi2.m_bitmap_offset = bitmap_offset (level);
			bitmap_ptr = m_bitmap + bi2.m_bitmap_offset;
		}

		UWord* end = m_bitmap + bitmap_offset_next (bi2.m_bitmap_offset);
		UWord* begin = bitmap_ptr = m_bitmap + bi2.m_bitmap_offset;

		// ����� � ������� �����. 
		// ������� ������ ������ ��������� � �������������� �������.
		while (!*bitmap_ptr) {

			if (++bitmap_ptr >= end) {

				if (!bi2.m_level)
					return 0; // ���� �� ������

										// ����������� �� ������� ����
				--bi2.m_level;
				end = begin;
				begin = bitmap_ptr = m_bitmap + (bi2.m_bitmap_offset = bitmap_offset_prev (bi2.m_bitmap_offset));
			}

		}

	} else  // �� ��������� ������� ����, ��� ������

#endif

	// ���� ��������� �����. �������� ������� �� �����, ��� ��� ��������� ������� �����������,
	// ��� ��������� ����� ����. 

	bitmap_ptr = m_bitmap + bi2.m_bitmap_offset;

	if (memory) {// ����� ��������� ���������������� ��������.
		UWord page_size = memory->query (this, Memory::COMMIT_UNIT);
		assert (page_size);

		UWord* page_end = round_down (bitmap_ptr, page_size) + page_size / sizeof (UWord);

		for (;;) {
			while (!memory->is_readable (bitmap_ptr, sizeof (UWord))) {
				bitmap_ptr = page_end;
				page_end += page_size;
			}

			while (!*bitmap_ptr) {
				if (++bitmap_ptr == page_end)
					break;
			}
		}
	} else {
		while (!*bitmap_ptr)
			++bitmap_ptr;
	}

	// ���� ��� �������� ���������, ��������� ������� � ������� ����� ����������� ����� ������.
	// ��������� �������� ������� �� ������, ���� ��������� ������ ���� ���� ���������.
	assert (bitmap_ptr < m_bitmap + BITMAP_SIZE);
#ifndef NDEBUG
	{
		Word end = bi2.m_bitmap_offset + min (TOP_LEVEL_BLOCKS << bi2.m_level, 0x10000) / sizeof (UWord);
		assert ((bitmap_ptr - m_bitmap) < end);
	}
#endif

	// �� �������� � ������� ����� � ������� ����� ���������� ����� (�����) �����.
	UWord level_bitmap_begin = bitmap_offset (bi2.m_level);

	// ����� �����:
	assert ((UWord)(bitmap_ptr - m_bitmap) >= level_bitmap_begin);
	UWord block_number = (bitmap_ptr - m_bitmap - level_bitmap_begin) * sizeof (UWord) * 8;

	// ���� ���, ��������������� �����.
	UWord mask = 1;
	UWord bits = *bitmap_ptr;
	while (!(bits & mask)) {
		mask <<= 1;
		++block_number;
	}

	// ���������� �������� ����� � ���� � ��� ������.
	UWord allocated_size = block_size (bi2.m_level);
	UWord block_offset = block_number * allocated_size;
	*bitmap_ptr &= ~mask; // ���������� ��� ���������� �����
	--*free_blocks_ptr;   // ��������� ������� ������������� ���

	// ������� ���� �������� allocated_size. ����� ���� �������� size.
	// ����������� ���������� �����.

	try {

		release (block_offset + size, block_offset + allocated_size);

	} catch (...) {
		// Release cb bytes, not allocated_size bytes!
		release (block_offset, block_offset + size);
		throw;
	}

	return block_offset;
}

bool HeapDirectory::allocate (UWord begin, UWord end, Memory_ptr memory)
{
	assert (begin < end);
	assert (end <= UNIT_COUNT);

	// �������� ����, �������� ��� �� ����� �������� 2^n, �������� ������� ������ �� �������.
	UWord allocated_begin = begin;  // ������ �������� ������������.
	UWord allocated_end = allocated_begin;    // ����� �������� ������������.
	while (allocated_end < end) {

		// ���� ����������� �������, �� ������� ��������� �������� � ������.
		UWord level = level_align (allocated_end, end - allocated_end);

		// ������� ������ ������� ����� ������ � ����� �����.
		UWord level_bitmap_begin = bitmap_offset (level);
		UWord block_number = unit_number (allocated_end, level);

		UWord* bitmap_ptr;
		UWord mask;
		for (;;) {

			bitmap_ptr = m_bitmap + level_bitmap_begin + block_number / (sizeof (UWord) * 8);
			mask = (UWord)1 << (block_number % (sizeof (UWord) * 8));
			// Bitmap page can be not committed.
			if (
				(!memory || memory->is_readable (bitmap_ptr, sizeof (UWord)))
				&&
				(*bitmap_ptr & mask)
			)
				break;  // was found

			if (!level) {

				// No block found. Release allocated.
				release (allocated_begin, allocated_end, Memory_ptr::nil ());

				return false;
			}

			// Level up
			--level;
			block_number = block_number >> 1;
			level_bitmap_begin = bitmap_offset_prev (level_bitmap_begin);
		}

		// Clear free bit
		*bitmap_ptr &= ~mask;

		// Decrement free blocks counter
		--free_block_count (level, block_number);

		UWord block_offset = unit_number (block_number, level);
		if (allocated_begin < block_offset)
			allocated_begin = block_offset;
		allocated_end = block_offset + block_size (level);
	}

	try { // Release extra space at begin and end
		// ������ ������������� ������� - ������, �����, � ������ ����
		// (������������� ������������� ������� �����) � ������������ ������������ ����������
		// �����, �������������� �������� ���������.
		release (allocated_begin, begin, memory, true);
		release (end, allocated_end, memory);
	} catch (...) {
		release (begin, end, Memory_ptr::nil ());
		throw;
	}

	return true;
}

void HeapDirectory::release (UWord begin, UWord end, Memory_ptr memory, bool rtl)
{
	assert (begin <= end);
	assert (end <= UNIT_COUNT);

	// ������������� ���� ������ ���� ������ �� ����� �������� 2^n, �������� �������
	// ������ �� �������.
	while (begin < end) {

		// �������� ����� � ����
		UWord level;
		UWord block_begin;
		if (rtl) {
			level = level_align (end, end - begin);
			block_begin = end - block_size (level);
		} else {
			level = level_align (begin, end - begin);
			block_begin = begin;
		}

		// ����������� ���� �� ��������� block_begin �� ������ level

		// ������� ������ ������� ����� ������ � ����� �����
		UWord level_bitmap_begin = bitmap_offset (level);
		UWord bl_number = block_number (block_begin, level);

		UWord* bitmap_ptr = m_bitmap + level_bitmap_begin + bl_number / (sizeof (UWord) * 8);
		UWord mask = (UWord)1 << (bl_number % (sizeof (UWord) * 8));

		while (level > 0) {

			// ���������, ���� �� � ����� ��������� ���������
			UWord companion_mask = mask;
			if (bl_number & 1)
				companion_mask >>= 1;
			else
				companion_mask <<= 1;

			// ���������� ����� ����� � ������� �����
			UWord* bitmap_ptr = m_bitmap + level_bitmap_begin + bl_number / (sizeof (UWord) * 8);

			// ������ ������� ����� ����� �� ���� �������������.

			if (
				(!memory || memory->is_readable (bitmap_ptr, sizeof (UWord)))
			&&
				(*bitmap_ptr & companion_mask)
			) {

				// ���� ��������� ���������, ���������� ��� � ������������� ������

				// ������� ��� ����������
				*bitmap_ptr &= ~companion_mask;
				--free_block_count (level, bl_number);

				// ����������� �� ������� ����
				--level;
				bl_number >>= 1;
				mask = (UWord)1 << (bl_number % sizeof (UWord));
				level_bitmap_begin = bitmap_offset_prev (level_bitmap_begin);
				bitmap_ptr = m_bitmap + level_bitmap_begin + bl_number / (sizeof (UWord) * 8);
			} else
				break;
		}

		// ��������� ������ ������� �����
		if (memory)
			memory->commit (bitmap_ptr, sizeof (UWord));

		// ������������� ��� ���������� �����

		*bitmap_ptr |= mask;

		// ����������� ������� ��������� ������
		++free_block_count (level, bl_number);

		// ���� ����������
		if (rtl)
			end = block_begin;
		else
			begin = block_begin + block_size (level);
	}
}

UWord HeapDirectory::level_align (UWord offset, UWord size)
{
	// ���� ������������ ������ ����� <= size, �� ������� �������� offset
	UWord mask = MAX_BLOCK_SIZE - 1;
	UWord level = 0;
	while ((mask & offset) || (mask >= size)) {
		mask >>= 1;
		++level;
	}
	return level;
}

bool HeapDirectory::check_allocated (UWord begin, UWord end, Memory_ptr memory) const
{
	UWord page_size;
	if (memory) {
		page_size = memory->query (this, Memory::COMMIT_UNIT);
		assert (page_size);
	}

	// Check for all bits on all levels are 0
	UWord level_bitmap_begin = bitmap_offset (HEAP_LEVELS - 1);
	for (Word level = HEAP_LEVELS - 1; level >= 0; --level) {

		assert (begin < end);

		const UWord* begin_ptr = m_bitmap + level_bitmap_begin + begin / (sizeof (UWord) * 8);
		const UWord* end_ptr = m_bitmap + level_bitmap_begin + end / (sizeof (UWord) * 8);
		UWord begin_mask = (~0) << (begin % (sizeof (UWord) * 8));
		UWord end_mask = ~((~0) << (end % (sizeof (UWord) * 8)));

		if (begin_ptr >= end_ptr) {

			if (
				(!memory || memory->is_readable (begin_ptr, sizeof (UWord)))
			&&
				(*begin_ptr & begin_mask & end_mask)
			)
				return false;

		} else if (memory) {

			const UWord* page_end = round_down (begin_ptr, page_size) + page_size / sizeof (UWord);

			if (memory->is_readable (begin_ptr, sizeof (UWord))) {
				if (*begin_ptr & begin_mask)
					return false;
				++begin_ptr;
			} else
				begin_ptr = page_end;

			while (begin_ptr < end_ptr) {

				for (const UWord* end = min (end_ptr, page_end); begin_ptr < end; ++begin_ptr)
					if (*begin_ptr)
						return false;

				while (begin_ptr < end_ptr && !memory->is_readable (begin_ptr, sizeof (UWord))) {
					begin_ptr = page_end;
					page_end += page_size;
				}
			}

			if (
				((end_ptr < page_end) || memory->is_readable (end_ptr, sizeof (UWord)))
			&&
				(*end_ptr & end_mask)
			)
				return false;

		} else {

			if (*begin_ptr & begin_mask)
				return false;

			while (++begin_ptr < end_ptr)
				if (*begin_ptr)
					return false;

			if (*end_ptr & end_mask)
				return false;
		}

		// Go to up level
		level_bitmap_begin = bitmap_offset_prev (level_bitmap_begin);
		begin /= 2;
		end = (end + 1) / 2;
	}

	return true;
}

}