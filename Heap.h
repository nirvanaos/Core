// Nirvana project
// Syncronization domain heap implementation

#ifndef _HEAP_H_
#define _HEAP_H_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "core.h"

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

#define HEAP_PARTS (HEAP_HEADER_SIZE / HEAP_DIRECTORY_SIZE)

// ������ ���� (2 ���� ��������� �� ������ HEAP_UNIT_MIN)

#define HEAP_PART_SIZE (HEAP_DIRECTORY_SIZE * 4 * HEAP_UNIT_MIN)

// ����� ������ ����

#define HEAP_SIZE (HEAP_PART_SIZE * HEAP_PARTS)

class HeapDirectory
{ // ����������� ���� ����. ��������� ����� ��������.
public:

  void* operator new (size_t cb, void* p)
  {
    return p;
  }
  
  void operator delete (void*, void*)
  {
  }

  HeapDirectory ();

  bool empty () const;

  Pointer allocate (Pointer heap, Pointer p, UWord cb, UWord flags);
  void release (Pointer heap, Pointer p, UWord cb);
  bool check_allocated (UWord begin, UWord end);
    
private:

  Pointer reserve (Pointer heap, UWord cb);

  void release (UWord begin, UWord end, Pointer heap = 0, bool right_to_left = false);

  UWord level_align (UWord offset, UWord size);
  
  UWord unit_size (UWord level)
  {
    return sm_block_index1 [HEAP_LEVELS - 1 - level].m_block_size;
/*
    � ����� ���, ����� ����������, ��� �������
    return HEAP_UNIT_MAX >> level;
*/
  }

  UWord unit_number (UWord offset, UWord level)
  {
    return (offset / HEAP_UNIT_MIN) >> (HEAP_LEVELS - 1 - level);
  }

  UWord unit_offset (UWord number, UWord level)
  {
    return (number * HEAP_UNIT_MIN) << (HEAP_LEVELS - 1 - level);
  }

  UWord bitmap_offset (UWord level)
  {
    return (TOP_BITMAP_WORDS << level) - TOP_BITMAP_WORDS;
  }

  UWord bitmap_offset_next (UWord bitmap_offset)
  {
    return (bitmap_offset << 1) + TOP_BITMAP_WORDS;
  }

  UWord bitmap_offset_prev (UWord bitmap_offset)
  {
    return (bitmap_offset - TOP_BITMAP_WORDS) >> 1;
  }

  UShort& free_unit_count (UWord level, UWord unit_number)
  {
    return m_free_block_index [sm_block_index1 [HEAP_LEVELS - 1 - level].m_block_index_offset
#if (HEAP_DIRECTORY_SIZE > 0x4000)
      // Add index for splitted levels
      + (unit_number >> (sizeof (UShort) * 8))
#endif
      ];
  }

  enum {
    // Number of HEAP_UNIT_MAX per one heap partition
    MAX_UNITS_PER_PART = (HEAP_DIRECTORY_SIZE * 4) >> (HEAP_LEVELS - 1),

    // ������ �������� ������ ������� ����� � �������� ������
    TOP_BITMAP_WORDS = MAX_UNITS_PER_PART / (sizeof (UWord) * 8)
  };

  enum {
    // Size of bitmap (in words) in one heap partition
    BITMAP_SIZE = (~((~0) << HEAP_LEVELS)) * TOP_BITMAP_WORDS
  };

  enum {
    // Space available at top of bitmap for free block index
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

  enum {

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

  UShort m_free_block_index [FREE_BLOCK_INDEX_SIZE];
    
  // ������� ����� ��������� ������
  UWord m_bitmap [BITMAP_SIZE];

  // Alignment
  enum {
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

class Heap
{
public:

  void create ()
  {
    create (system_memory ()->allocate (0, HEAP_HEADER_SIZE + HEAP_SIZE, Memory::READ_WRITE | Memory::RESERVED));
  }
  
  void create (Pointer p)
  {
    m_header = p;
    new (p) HeapDirectory ();
#if (HEAP_PARTS > 1)
    (UWord&)m_header |= 1;
#endif  
  }
  
  void destroy ()
  {
    system_memory ()->release (header (), HEAP_HEADER_SIZE + HEAP_SIZE);
  }
  
#if (HEAP_PARTS > 1)

  UWord partitions () const
  {
    return (~((~0) << HEAP_PARTS)) && (UWord)m_header;
  }

  Pointer allocate_in_new_partition (UWord cb, UWord flags)
  {
    assert (cb <= HEAP_UNIT_MAX);
    UWord parts = partitions ();
    HeapDirectory* dir = header ();
    Octet* s = space ();
    UWord mask = 1;
    while (parts & mask) {
      mask <<= 1;
      ++dir;
      s += HEAP_PART_SIZE;
    }
    new (dir) HeapDirectory ();
    (UWord&)m_header |= mask;
    return dir->allocate (s, 0, cb, flags);
  }

#endif

  bool empty () const
  {
#if (HEAP_PARTS > 1)
    HeapDirectory* dir = header ();
    for (UWord parts = partitions (); parts; parts >>= 1) {
      if (parts & 1)
        if (!dir->empty ())
          return false;
    }
    return true;
#else
    return header ()->empty ();
#endif
  }

  Pointer allocate (Pointer p, UWord cb, UWord flags);

  void  release (Pointer p, UWord cb)
  {
    // check_allocate called at level above
    assert (p);
    assert (cb);
    assert (p >= space ());
    
#if (HEAP_PARTS > 1)
    
    Octet* s = (Octet*)space ();
    UWord part = ((Octet*)p - s) / HEAP_PART_SIZE;
    s += part * HEAP_PART_SIZE;
    HeapDirectory* dir = header () + part;
    part = HEAP_PART_SIZE - (((Octet*)p - s) & (HEAP_PART_SIZE - 1));
    do {
      if (part > cb)
        part = cb;
      dir->release (s, p, part);
      cb -= part;
      p = s += HEAP_PART_SIZE;
      part = HEAP_PART_SIZE;
    } while (cb);
    
#else
    
    header ()->release (space (), p, cb);
    
#endif
  }
  
  void check_allocated (Pointer begin, Pointer end);

  bool operator < (Heap rhs) const
  {
    return m_header < rhs.m_header;
  }

  bool operator < (Pointer p) const
  {
    return m_header < p;
  }

  HeapDirectory* header () const
  {
#if (HEAP_PARTS > 1)
    return (HeapDirectory*)(((~0) << HEAP_PARTS) & (UWord)m_header);
#else
    return (HeapDirectory*)m_header;
#endif
  }
  
  Octet* space () const
  {
    return (Octet*)header () + HEAP_HEADER_SIZE;
  }
  
  Octet* space_end () const
  {
    return (Octet*)header () + HEAP_HEADER_SIZE + HEAP_SIZE;
  }
  
private:

  void* m_header;
};

inline bool operator < (Pointer p, Heap h)
{
  return p < h.header ();
}

#endif
