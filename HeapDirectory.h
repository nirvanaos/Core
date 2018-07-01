#ifndef NIRVANA_CORE_HEAPDIRECTORY_H_
#define NIRVANA_CORE_HEAPDIRECTORY_H_

#include "core.h"
#include <Memory.h>
#include <stddef.h>

#ifndef MIN
#define MIN(a,b)  (((a) < (b)) ? (a) : (b))
#endif

/*
������������ �������� ������������� ������ � �������� ������������ �������� ������.
���� ���������������, ��� ������������ ������ ��������, ������� ������� 2,
������������� �� �������, ������� ������� �����. ��� ��������� ����� �������� S
������ ��������� ���� �������� m << n >= S, ��� m - ����������� ������ �����.
���� ����������� ������ ����� ������, ���������� � ����� ��������� ������������
����������� �� ��������� ������� ������, ������� ������� ����� ������ ������� 2.
��������� ��� ���������� ��������� ���������� ������� ������ ������ ������� 2,
��� �����������, ��� ����, �� ������� ������� ��������, ����� �������� �� �������
��������, ����, ������� ������� ������� ����, ����� �������� �� �� ������� � �. �.
����� �������, �������������� ����������� ������������ ���������� ������ ��� �����
����������� � ������������ ������������������.

���������� � ��������� ������ �������� � ������� �����. ������� ����� ������������
����� "��������", � ������� ������� ����� ������� ������ (�������) �������������
���� ���. ���� ��� ����������, ���� ���� ��������. ��� ��� ������������ ������ �����
������ ������� ����, ������� ����� ��������, ��� �������� �� ��������� ���������.
�� ����� �������� ���������� ��������� ������, UShort, ���������� ���������� �
���������� ��������� ������ �� ��������� �������.
*/

namespace Nirvana {

using namespace CORBA;

//#define HEAP_PARTS (HEAP_HEADER_SIZE / HEAP_DIRECTORY_SIZE)

// ������ ���� (2 ���� ��������� �� ������ HEAP_UNIT_MIN)

#define HEAP_PART_SIZE (HEAP_DIRECTORY_SIZE * 4 * HEAP_UNIT_MIN)

// ����� ������ ����

//#define HEAP_SIZE (HEAP_PART_SIZE * HEAP_PARTS)

// Heap directory. Used for memory allocation on different levels of memory management.
// Heap directory allocates and deallocates abstract "units" in range (0 <= n < UNIT_COUNT).
// Each unit requires 2 bits of HeapDirectory size.
class HeapDirectory
{
public:
	static const UWord UNIT_COUNT = HEAP_DIRECTORY_SIZE * 4;
	static const UWord MAX_BLOCK_SIZE = 1 << (HEAP_LEVELS - 1);

	static void initialize (HeapDirectory* zero_filled_buf);
	static HeapDirectory* create (Memory_ptr memory);

	bool empty () const;

	/// <summary>Allocate block.</summary>
	/// <returns>Block offset in units if succeded, otherwise -1.</returns>
	Word allocate (UWord size, Memory_ptr memory = Memory_ptr::nil ());

	bool allocate (UWord begin, UWord end, Memory_ptr memory = Memory_ptr::nil ());

	// Checks that all units in range are allocated.
	bool check_allocated (UWord begin, UWord end, Memory_ptr memory = Memory_ptr::nil ()) const;
	
	void release (UWord begin, UWord end, Memory_ptr memory = Memory_ptr::nil (), bool right_to_left = false);

private:

	static UWord level_align (UWord offset, UWord size);

	// Number of units per block, for level.
	static UWord block_size (UWord level)
	{
		return sm_block_index1 [HEAP_LEVELS - 1 - level].m_block_size;
		/*
		� ����� ���, ����� ����������, ��� �������
		return HEAP_UNIT_MAX >> level;
		*/
	}

	static UWord block_number (UWord unit, UWord level)
	{
		return unit >> (HEAP_LEVELS - 1 - level);
	}

	static UWord unit_number (UWord block, UWord level)
	{
		return block << (HEAP_LEVELS - 1 - level);
	}

	static UWord bitmap_offset (UWord level)
	{
		return (TOP_BITMAP_WORDS << level) - TOP_BITMAP_WORDS;
	}

	static UWord bitmap_offset_next (UWord bitmap_offset)
	{
		return (bitmap_offset << 1) + TOP_BITMAP_WORDS;
	}

	static UWord bitmap_offset_prev (UWord bitmap_offset)
	{
		return (bitmap_offset - TOP_BITMAP_WORDS) >> 1;
	}

	UShort& free_block_count (UWord level, UWord block_number)
	{
		return m_free_block_index [sm_block_index1 [HEAP_LEVELS - 1 - level].m_block_index_offset
#if (HEAP_DIRECTORY_SIZE > 0x4000)
			// Add index for splitted levels
			+ (block_number >> (sizeof (UShort) * 8))
#endif
		];
	}

	enum
	{
		// Number of top level blocks.
		TOP_LEVEL_BLOCKS = UNIT_COUNT >> (HEAP_LEVELS - 1),

		// ������ �������� ������ ������� ����� � �������� ������
		TOP_BITMAP_WORDS = TOP_LEVEL_BLOCKS / (sizeof (UWord) * 8)
	};

	enum
	{
		// Size of bitmap (in words) in one heap partition
		BITMAP_SIZE = (~((~0) << HEAP_LEVELS)) * TOP_BITMAP_WORDS
	};

	enum
	{
		// Space available before bitmap for free block index.
		FREE_BLOCK_INDEX_MAX = (HEAP_DIRECTORY_SIZE - BITMAP_SIZE * sizeof (UWord)) / sizeof (UShort)
	};

	// ������, ���������� ���������� ��������� ������ �� ������ ������.
	// ���� ����� ���������� ������ �� ������ > 64K, �� ����������� �� �����,
	// ������ �� ������� ������������� ���� ������� �������.
	// ����� �������, ����� ������������, � ������ ������, ����� 64K ��� ��� 2K ����.
	// ���� ����� � ��������� ������������, ������� ������ ������������.
	// � ���� ������, ������� ������� �������� ��������� ���������� ��������� ������
	// �� ���� �������.
	// ������ ���������� - ������ ������ ���� �������.

	enum
	{

#if (HEAP_DIRECTORY_SIZE == 0x10000)
		FREE_BLOCK_INDEX_SIZE = 4 + 2 + MIN ((HEAP_LEVELS - 2), FREE_BLOCK_INDEX_MAX)
#elif (HEAP_DIRECTORY_SIZE == 0x8000)
		FREE_BLOCK_INDEX_SIZE = 2 + MIN ((HEAP_LEVELS - 1), FREE_BLOCK_INDEX_MAX)
#elif (HEAP_DIRECTORY_SIZE == 0x4000)
		FREE_BLOCK_INDEX_SIZE = MIN (HEAP_LEVELS, FREE_BLOCK_INDEX_MAX)
#else
#error HEAP_DIRECTORY_SIZE is invalid.
#endif
	};

	// Free block count index.
	UShort m_free_block_index [FREE_BLOCK_INDEX_SIZE];

	// ������� ����� ��������� ������.
	UWord m_bitmap [BITMAP_SIZE];

	// Alignment
	enum
	{
		ALIGNMENT = HEAP_DIRECTORY_SIZE - FREE_BLOCK_INDEX_SIZE * sizeof (UShort) - BITMAP_SIZE * sizeof (UWord)
	};

#if (ALIGNMENT != 0)
	Octet m_alignment [ALIGNMENT];
#endif
	//Octet m_alignment [HEAP_DIRECTORY_SIZE - FREE_BLOCK_INDEX_SIZE * sizeof (UShort) - BITMAP_SIZE * sizeof (UWord)];

	// ����������� ������� ��� ��������� ������

	// BlockIndex1 �� ������� ����� ���������� �������� ������ ������ � m_free_block_cnt

	struct BlockIndex1
	{
		bool operator < (const BlockIndex1& rhs) const
		{
			return m_block_size < rhs.m_block_size;
		}

		UWord m_block_size;
		UWord m_block_index_offset; // offset in m_free_block_index
	};

	static const BlockIndex1 sm_block_index1 [HEAP_LEVELS];

	// BlockIndex2 �� ��������� �������� �� ����� ������� m_free_block_cnt ����������
	// ������� � ��������� ������� ������� �����.

	struct BlockIndex2
	{
		UWord m_level;
		UWord m_bitmap_offset;
	};

	static const BlockIndex2 sm_block_index2 [FREE_BLOCK_INDEX_SIZE];
};

}

#endif
