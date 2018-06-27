#include "HeapDirectory.h"
#include <algorithm>

namespace Nirvana {

using namespace std;

// BlockIndex1 �� ������� ����� ���������� �������� ������ ������ � m_free_block_index

const HeapDirectory::BlockIndex1 HeapDirectory::sm_block_index1 [HEAP_LEVELS] =
{

#if (HEAP_DIRECTORY_SIZE == 0x10000)
// FREE_BLOCK_INDEX_SIZE == 15

{HEAP_UNIT_MIN,        0},  // �������� �� 4 �����
{HEAP_UNIT_MIN * 2,    4},  // �������� �� 2 �����
{HEAP_UNIT_MIN * 4,    6},
{HEAP_UNIT_MIN * 8,    7},
{HEAP_UNIT_MIN * 16,   8},
{HEAP_UNIT_MIN * 32,   9},
{HEAP_UNIT_MIN * 64,   10},
{HEAP_UNIT_MIN * 128,  11},
{HEAP_UNIT_MIN * 256,  12},
{HEAP_UNIT_MIN * 512,  13},
{HEAP_UNIT_MIN * 1024, 14}

#elif (HEAP_DIRECTORY_SIZE == 0x8000)
	// FREE_BLOCK_INDEX_SIZE == 8

{ HEAP_UNIT_MIN,        0 },  // �������� �� 2 �����
{HEAP_UNIT_MIN * 2,    2},
{HEAP_UNIT_MIN * 4,    3},
{HEAP_UNIT_MIN * 8,    4},
{HEAP_UNIT_MIN * 16,   5},
{HEAP_UNIT_MIN * 32,   6},
{HEAP_UNIT_MIN * 64,   7},  // 5 ������� ������� ����������
{HEAP_UNIT_MIN * 128,  7},
{HEAP_UNIT_MIN * 256,  7},
{HEAP_UNIT_MIN * 512,  7},
{HEAP_UNIT_MIN * 1024, 7}

#elif (HEAP_DIRECTORY_SIZE == 0x4000)
	// FREE_BLOCK_INDEX_SIZE == 4

{ HEAP_UNIT_MIN,        0 },
{HEAP_UNIT_MIN * 2,    1},
{HEAP_UNIT_MIN * 4,    2},
{HEAP_UNIT_MIN * 8,    3},  // 8 ������� ������� ����������
{HEAP_UNIT_MIN * 16,   3},
{HEAP_UNIT_MIN * 32,   3},
{HEAP_UNIT_MIN * 64,   3},
{HEAP_UNIT_MIN * 128,  3},
{HEAP_UNIT_MIN * 256,  3},
{HEAP_UNIT_MIN * 512,  3},
{HEAP_UNIT_MIN * 1024, 3}

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

{ 4,  TOP_BITMAP_WORDS * 15 },
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
{ 7,  TOP_BITMAP_WORDS * 127 },
{8,  TOP_BITMAP_WORDS * 255},
{9,  TOP_BITMAP_WORDS * 511},
{10, TOP_BITMAP_WORDS * 1023}

#endif

};

HeapDirectory::HeapDirectory ()
{
	// �������������� ������ ���������� ��������� ������
	system_memory ()->commit (m_free_block_index, sizeof (m_free_block_index));
	m_free_block_index [FREE_BLOCK_INDEX_SIZE - 1] = MAX_UNITS_PER_PART;

	// �������������� ������ ������� ������� ����� ���������
	system_memory ()->commit (m_bitmap, MAX_UNITS_PER_PART / 8);
	uninitialized_fill (m_bitmap, m_bitmap + MAX_UNITS_PER_PART / (sizeof (UWord) * 8), ~0);
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

	return (MAX_UNITS_PER_PART == m_free_block_index [HEAP_LEVELS - 1]);

#endif
}

Pointer HeapDirectory::reserve (Pointer heap, UWord cb)
{
	assert (cb);
	assert (cb <= HEAP_UNIT_MAX);

	// Align block size
	cb = (cb + HEAP_UNIT_MIN - 1) & ~(HEAP_UNIT_MIN - 1);

	// Quantize block size
	const BlockIndex1* pi1 = lower_bound (sm_block_index1, sm_block_index1 + HEAP_LEVELS, *reinterpret_cast <const BlockIndex1*> (&cb));
	UWord block_index_offset = pi1->m_block_index_offset;

	// Search in free block index
	UShort* free_blocks_ptr = m_free_block_index + block_index_offset;
	Word cnt = FREE_BLOCK_INDEX_SIZE - block_index_offset;
	while ((cnt--) && !*free_blocks_ptr)
		++free_blocks_ptr;
	if (cnt < 0)
		return 0; // no such blocks

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

	{
		// ���� ��������� �����. �������� ������� �� �����, ��� ��� ��������� ������� �����������,
		// ��� ��������� ����� ����. 

		bitmap_ptr = m_bitmap + bi2.m_bitmap_offset;

		for (;;) {// ����� ��������� ���������������� ��������.

			try {

				while (!*bitmap_ptr)
					++bitmap_ptr;
				break;

			} catch (...) {
				// ���������� ���������������� ��������
#if (FIXED_PROTECTION_UNIT)
				bitmap_ptr += FIXED_PROTECTION_UNIT / sizeof (UWord);
#else
				bitmap_ptr += system_memory ()->query (bitmap_ptr, Memory::PROTECTION_UNIT) / sizeof (UWord);
#endif
			}
		}

		// ���� ��� �������� ���������, ��������� ������� � ������� ����� ����������� ����� ������.
		// ��������� �������� ������� �� ������, ���� ��������� ������ ���� ���� ���������.

#if (BUILD_CHECK_LEVEL >= 1)

		UWord end;
		if (++cnt < FREE_BLOCK_INDEX_SIZE)
			end = sm_block_index2 [cnt].m_bitmap_offset;
		else
			end = BITMAP_SIZE;
		if ((UWord)(bitmap_ptr - m_bitmap) >= end)
			throw INTERNAL ();

#endif

	}

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
	UWord allocated_size = unit_size (bi2.m_level);
	UWord block_offset = block_number * allocated_size;
	*bitmap_ptr &= ~mask; // ���������� ���
	--*free_blocks_ptr;   // ��������� ������� ������������� ���

												// ������� ���� �������� allocated_size. ����� ���� �������� cb.
												// ����������� ���������� �����.

	Pointer block = (Octet*)heap + block_offset;

	try {

		release (block_offset + cb, block_offset + allocated_size);

	} catch (...) {

		// Release cb bytes, not allocated_size bytes!
		release (heap, block, cb);
		throw;
	}

	return block;
}

Pointer HeapDirectory::allocate (Pointer heap, Pointer p, UWord cb, UWord flags)
{
	assert (heap);
	assert (cb);

	if (p) {

		assert (p > heap);

		// �������� ����� � ����
		UWord begin = (Octet*)p - (Octet*)heap;
		if ((!(flags & Memory::EXACTLY)) && (begin % HEAP_UNIT_MIN))
			return 0;

		UWord end = (begin + cb + HEAP_UNIT_MIN - 1) & ~(HEAP_UNIT_MIN - 1);
		assert (end <= HEAP_PART_SIZE);

		begin &= ~(HEAP_UNIT_MIN - 1);

		// �������� ����, �������� ��� �� ����� �������� 2^n, �������� ������� ������ �� �������.
		UWord allocated_begin = begin & ~(HEAP_UNIT_MIN - 1);  // ������ �������� ������������.
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
				mask = 1 << (block_number % (sizeof (UWord) * 8));
				try {
					if (*bitmap_ptr & mask)
						break;  // was found
				} catch (...) {
					// Uncommitted page, assume zero
				}

				if (!level) {

					// No block found. Release allocated.
					release (allocated_begin, allocated_end);

					return 0;
				}

				// Level up
				--level;
				block_number = block_number >> 1;
				level_bitmap_begin = bitmap_offset_prev (level_bitmap_begin);
			}

			// Clear free bit
			*bitmap_ptr &= ~mask;

			// Decrement free blocks counter
			--free_unit_count (level, block_number);

			UWord block_offset = unit_offset (block_number, level);
			if (allocated_begin < block_offset)
				allocated_begin = block_offset;
			allocated_end = block_offset + unit_size (level);
		}

		try { // Release extra space at begin and end
					// ������ ������������� ������� - ������, �����, � ������ ����
					// (������������� ������������� ������� �����) � ������������ ������������ ����������
					// �����, �������������� �������� ���������.
			release (allocated_begin, begin, 0, true);
			release (end, allocated_end);
		} catch (...) {
			release (heap, p, cb);
			throw;
		}

	} else
		p = reserve (heap, cb);

	if (!(flags & Memory::RESERVED)) {

		system_memory ()->commit (p, cb);

		if (flags & Memory::ZERO_INIT) {

			// New committed pages are already zeroed
			// We must zero only partial protection units at begin and end

#if (FIXED_PROTECTION_UNIT)
			static const UWord fpu = FIXED_PROTECTION_UNIT - 1;
#else
			const UWord fpu = system_memory ()->query (p, PROTECTION_UNIT) - 1;

			// Assume that protection unit are the same on one heap
			assert (system_memory ()->query ((Octet*)p + cb - 1, PROTECTION_UNIT) == fpu + 1);
#endif
			Octet* committed_begin = (Octet*)(((UWord)p + fpu) & ~fpu);
			Octet* committed_end = (Octet*)(((UWord)p + cb + fpu) & ~fpu);
			if (committed_begin <= committed_end) {
				zero ((Octet*)p, committed_begin);
				zero (committed_end, (Octet*)p + cb);
			} else
				zero ((Octet*)p, (Octet*)p + cb);
		}
	}

	return p;
}

void HeapDirectory::release (Pointer heap, Pointer p, UWord cb)
{
	assert (cb);

	// �������� ����� � ����
	UWord begin = (Octet*)p - (Octet*)heap;
	UWord end = (begin + cb + HEAP_UNIT_MIN - 1) & ~(HEAP_UNIT_MIN - 1);
	assert (end <= HEAP_PART_SIZE);
	begin &= ~(HEAP_UNIT_MIN - 1);

	release (begin, end, heap);
}

void HeapDirectory::release (UWord begin, UWord end, Pointer heap, bool rtl)
{
	// �������� ����� � ����
	assert (end <= HEAP_PART_SIZE);
	assert (!(begin % HEAP_UNIT_MIN));
	assert (!(end % HEAP_UNIT_MIN));

	// ������������� ���� ������ ���� ������ �� ����� �������� 2^n, �������� �������
	// ������ �� �������.
	while (begin < end) {

		UWord level;
		UWord block;
		if (rtl) {
			level = level_align (end, end - begin);
			block = end - unit_size (level);
		} else {
			level = level_align (begin, end - begin);
			block = begin;
		}

		// ����������� ���� �� ��������� block �� ������ level

		// ������� ������ ������� ����� ������ � ����� �����
		UWord level_bitmap_begin = bitmap_offset (level);
		UWord block_number = unit_number (block, level);

		UWord* bitmap_ptr = m_bitmap + level_bitmap_begin + block_number / (sizeof (UWord) * 8);
		UWord mask = 1 << (block_number % (sizeof (UWord) * 8));

		if (heap) { // heap = 0 for internal usage, without companion check

			while (level > 0) {

				// ���������, ���� �� � ����� ��������� ���������
				UWord companion_mask = mask;
				if (block_number & 1)
					companion_mask >>= 1;
				else
					companion_mask <<= 1;

				// ������ ������� ����� ����� �� ���� �������������, ������������� ���������� ������
				try {

					// ���������� ����� ����� � ������� �����
					UWord* bitmap_ptr = m_bitmap + level_bitmap_begin + block_number / (sizeof (UWord) * 8);

					if (*bitmap_ptr & companion_mask) {

						// ���� ��������� ���������, ���������� ��� � ������������� ������

						// ������� ��� ����������
						*bitmap_ptr &= ~companion_mask;
						--free_unit_count (level, block_number);

						// ����������� �� ������� ����
						--level;
						block_number >>= 1;
						mask = 1 << (block_number % sizeof (UWord));
						level_bitmap_begin = bitmap_offset_prev (level_bitmap_begin);
						bitmap_ptr = m_bitmap + level_bitmap_begin + block_number / (sizeof (UWord) * 8);
					} else
						break;

				} catch (...) {
					// ��� ���������� �������� � ����������������� �������.
					// ��� ���������. ������ ���������� ���.
					break;
				}
			}
		}

		// ��������� ������ ������� �����
		system_memory ()->commit (bitmap_ptr, sizeof (UWord));

		// ������������� ��� ���������� �����

		*bitmap_ptr |= mask;

		// ����������� ������� ��������� ������
		++free_unit_count (level, block_number);

		UWord block_size = unit_size (level);

		if (heap) { // ������������ ����
			block = unit_offset (block_number, level);
			system_memory ()->decommit (block + (Octet*)heap, block_size);
		}

		// ���� ����������
		if (rtl)
			end = block;
		else
			begin = block + block_size;
	}
}

UWord HeapDirectory::level_align (UWord offset, UWord size)
{
	offset &= ~(HEAP_UNIT_MIN - 1);

	// ���� ������������ ������ ����� <= size, �� ������� �������� offset
	UWord mask = HEAP_UNIT_MAX - 1;
	UWord level = 0;
	while ((mask & offset) || (mask >= size)) {
		mask >>= 1;
		++level;
	}
	return level;
}

bool HeapDirectory::check_allocated (UWord begin, UWord end)
{
	// Calculate begin and end block numbers

	begin /= HEAP_UNIT_MIN;
	end = (end + HEAP_UNIT_MIN - 1) / HEAP_UNIT_MIN;

	// Check for all bits on all levels are 0
	UWord level_bitmap_begin = bitmap_offset (HEAP_LEVELS - 1);
	for (Word level = HEAP_LEVELS - 1; level >= 0; --level) {

		assert (begin < end);

		UWord* begin_ptr = m_bitmap + level_bitmap_begin + begin / (sizeof (UWord) * 8);
		UWord* end_ptr = m_bitmap + level_bitmap_begin + end / (sizeof (UWord) * 8);
		UWord begin_mask = (~0) << (begin % (sizeof (UWord) * 8));
		UWord end_mask = ~((~0) << (end % (sizeof (UWord) * 8)));

		try {
			if (begin_ptr >= end_ptr) {

				if (*begin_ptr & begin_mask & end_mask)
					return false;

			} else {

				if (*begin_ptr & begin_mask)
					return false;

				if (*end_ptr & end_mask)
					return false;

				for (;;) {
					try {
						while (++begin_ptr < end_ptr)
							if (*begin_ptr)
								return false;
						break;
					} catch (...) {
						// Skip uncommitted page
#if (FIXED_PROTECTION_UNIT)
						begin_ptr += FIXED_PROTECTION_UNIT / sizeof (UWord);
#else
						begin_ptr += system_memory ()->query (begin_ptr, Memory::PROTECTION_UNIT) / sizeof (UWord) - 1;
#endif
					}
				}
			}
		} catch (...) {
			// Uncommitted page
		}

		// Go to up level
		level_bitmap_begin = bitmap_offset_prev (level_bitmap_begin);
		begin /= 2;
		end = (end + 1) / 2;
	}

	return true;
}

}