// Copyright (C) 1998-2026 Object Vision b.v.
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

#if !defined(__RTC_MEM_BLOCKVECTOR_H)
#define __RTC_MEM_BLOCKVECTOR_H

#include "RtcBase.h"
#include "mem/MyAllocator.h"
#include "mem/MyContainers.h"
#include "set/rangefuncs.h"
#include "vt/MinMax.h"

#include <compare>
#include <iterator>
#include <type_traits>

//----------------------------------------------------------------------
// block_vector: a sequence in blocks that the allocation stock serves
//
// A my_vector of n elements is one allocation of n elements. Above 1 MB that bypasses the free stacks of the allocation
// stock and goes to the CRT, and a merge of two of them allocates its result while both inputs still exist, so that the
// last merge of a tree of merges holds the result twice. A block_vector holds its elements in blocks of block_size
// elements from the stock, and a block_vector_reader frees each block once it has read it: a merge that reads two
// block_vectors into a third holds what it has not read yet and what it has written, about the result once.
//
// A block has as many elements as a tile of a domain that SetCount sized (2^16), or fewer where those would exceed the
// largest object that the stock serves from a free stack. A freed block is then of the size of a tile of the same
// element type, which can take its place in the stock, and a whole block can become such a tile itself (TakeBlockIfWhole).
//
// Only the first block can be smaller than block_size, and only while it is the only one: it grows as a my_vector does
// until it has block_size elements, so that a small block_vector takes no more than it needs. Element i therefore lies
// in block i / block_size at offset i % block_size, and elements never move once a second block exists.
//----------------------------------------------------------------------

// the largest object that the allocation stock serves from a free stack: 2^16 elements of 16 bytes, 1 MB (SpecialSize in
// FixedAlloc.cpp)
constexpr SizeT largest_stock_object_size = (SizeT(1) << log2_default_segment_size) * sizeof(Float64) * 2;

template <typename T> struct block_vector_reader;

template <typename T>
struct block_vector
{
	using value_type = T;
	using size_type = SizeT;
	using difference_type = std::ptrdiff_t;
	using reference = T&;
	using const_reference = const T&;

	// as many elements as a tile has, within the largest object of the stock
	static constexpr SizeT block_size = Min<SizeT>(SizeT(1) << log2_default_segment_size, largest_stock_object_size / sizeof(T));
	static_assert(block_size >= 1);

	template <bool IsConst>
	struct iterator_t
	{
		using iterator_category = std::random_access_iterator_tag;
		using value_type = T;
		using difference_type = std::ptrdiff_t;
		using pointer = std::conditional_t<IsConst, const T*, T*>;
		using reference = std::conditional_t<IsConst, const T&, T&>;

		iterator_t() = default;
		iterator_t(T* const* blocks, SizeT index) : m_Blocks(blocks), m_Index(index) {}
		operator iterator_t<true>() const requires (!IsConst) { return iterator_t<true>(m_Blocks, m_Index); }

		reference operator *() const { return m_Blocks[m_Index / block_size][m_Index % block_size]; }
		pointer   operator ->() const { return &**this; }
		reference operator [](difference_type n) const { return *(*this + n); }

		iterator_t& operator ++() { ++m_Index; return *this; }
		iterator_t& operator --() { --m_Index; return *this; }
		iterator_t  operator ++(int) { auto result = *this; ++m_Index; return result; }
		iterator_t  operator --(int) { auto result = *this; --m_Index; return result; }
		iterator_t& operator +=(difference_type n) { m_Index += n; return *this; }
		iterator_t& operator -=(difference_type n) { m_Index -= n; return *this; }
		iterator_t  operator +(difference_type n) const { return iterator_t(m_Blocks, m_Index + n); }
		iterator_t  operator -(difference_type n) const { return iterator_t(m_Blocks, m_Index - n); }
		friend iterator_t operator +(difference_type n, const iterator_t& i) { return i + n; }
		difference_type operator -(const iterator_t& rhs) const { return difference_type(m_Index) - difference_type(rhs.m_Index); }

		bool operator ==(const iterator_t& rhs) const { return m_Index == rhs.m_Index; }
		auto operator <=>(const iterator_t& rhs) const { return m_Index <=> rhs.m_Index; }

		SizeT index() const { return m_Index; }

	private:
		T* const* m_Blocks = nullptr;
		SizeT     m_Index = 0;
	};
	using iterator = iterator_t<false>;
	using const_iterator = iterator_t<true>;

	block_vector() = default;

	template <std::input_iterator InIter>
	block_vector(InIter first, InIter last MG_DEBUG_ALLOCATOR_SRC_ARG_D)
	{
		if constexpr (std::forward_iterator<InIter>)
			reserve(std::distance(first, last) MG_DEBUG_ALLOCATOR_SRC_PARAM);
		for (; first != last; ++first)
			emplace_back(MG_DEBUG_ALLOCATOR_FIRST_PARAM *first);
	}
	block_vector(const block_vector& rhs)
		: block_vector(rhs.begin(), rhs.end() MG_DEBUG_ALLOCATOR_SRC("block_vector::CopyCTOR"))
	{}
	block_vector(block_vector&& rhs) noexcept { swap(rhs); }

	block_vector& operator =(const block_vector& rhs)
	{
		if (this != &rhs)
		{
			block_vector copy(rhs);
			swap(copy);
		}
		return *this;
	}
	block_vector& operator =(block_vector&& rhs) noexcept
	{
		clear(); // before taking rhs over, so that the blocks of both are not held at once
		swap(rhs);
		return *this;
	}
	~block_vector() { clear(); }

	void swap(block_vector& rhs) noexcept
	{
		m_Blocks.swap(rhs.m_Blocks);
		std::swap(m_Size, rhs.m_Size);
		std::swap(m_FirstCapacity, rhs.m_FirstCapacity);
		std::swap(m_WritePos, rhs.m_WritePos);
		std::swap(m_WriteEnd, rhs.m_WriteEnd);
	}

	size_type size() const { return m_Size; }
	bool empty() const { return !m_Size; }

	T&       operator [](size_type i)       { assert(i < m_Size); return m_Blocks[i / block_size][i % block_size]; }
	const T& operator [](size_type i) const { assert(i < m_Size); return m_Blocks[i / block_size][i % block_size]; }
	T&       front()       { return (*this)[0]; }
	const T& front() const { return (*this)[0]; }
	T&       back()        { return (*this)[m_Size - 1]; }
	const T& back()  const { return (*this)[m_Size - 1]; }

	iterator       begin()        { return iterator(m_Blocks.data(), 0); }
	iterator       end()          { return iterator(m_Blocks.data(), m_Size); }
	const_iterator begin()  const { return const_iterator(m_Blocks.data(), 0); }
	const_iterator end()    const { return const_iterator(m_Blocks.data(), m_Size); }
	const_iterator cbegin() const { return begin(); }
	const_iterator cend()   const { return end(); }

	// Sizes the first block for n elements, up to a whole block. Further blocks come one at a time as elements arrive,
	// so that a reserve never takes more than one block ahead.
	void reserve(size_type n MG_DEBUG_ALLOCATOR_SRC_ARG_D)
	{
		MakeMin(n, block_size);
		if (m_Blocks.size() <= 1 && n > m_FirstCapacity)
			ResizeFirstBlock(n MG_DEBUG_ALLOCATOR_SRC_PARAM);
	}

	void push_back(const T& value MG_DEBUG_ALLOCATOR_SRC_ARG_D) { emplace_back(MG_DEBUG_ALLOCATOR_FIRST_PARAM value); }
	void push_back(T&& value MG_DEBUG_ALLOCATOR_SRC_ARG_D) { emplace_back(MG_DEBUG_ALLOCATOR_FIRST_PARAM std::move(value)); }

	// args can refer to an element of this block_vector, which a growing first block moves; see emplace_back_growing
	template <typename... Args>
	T& emplace_back(MG_DEBUG_ALLOCATOR_FIRST_ARG Args&& ...args)
	{
		if (m_WritePos == m_WriteEnd)
			return emplace_back_growing(MG_DEBUG_ALLOCATOR_FIRST_PARAM std::forward<Args>(args)...);
		T* p = new (m_WritePos) T(std::forward<Args>(args)...);
		++m_WritePos; // after the construction, so that a constructor that throws leaves the block_vector as it was
		++m_Size;
		return *p;
	}

	// moves [first, last) to the end, as many elements at a time as the last block takes; the elements moved from are
	// left to their owner to destroy
	void append_moved(T* first, T* last MG_DEBUG_ALLOCATOR_SRC_ARG_D)
	{
		reserve(m_Size + (last - first) MG_DEBUG_ALLOCATOR_SRC_PARAM);
		while (first != last)
		{
			if (m_WritePos == m_WriteEnd)
				Grow(MG_DEBUG_ALLOCATOR_TXT_PARAM);
			SizeT n = Min<SizeT>(last - first, m_WriteEnd - m_WritePos);
			m_WritePos = std::uninitialized_move(first, first + n, m_WritePos);
			first += n;
			m_Size += n;
		}
	}

	void erase(const_iterator first, const_iterator last)
	{
		SizeT f = first.index(), l = last.index();
		assert(f <= l && l <= m_Size);
		std::move(begin() + l, end(), begin() + f);
		truncate(m_Size - (l - f));
	}

	// destroys the elements from n on and frees the blocks that are left without elements
	void truncate(size_type n)
	{
		assert(n <= m_Size);
		for (SizeT i = n; i < m_Size; )
		{
			SizeT b = i / block_size, offset = i % block_size;
			SizeT blockEnd = Min<SizeT>(offset + (m_Size - i), block_size);
			destroy_range(m_Blocks[b] + offset, m_Blocks[b] + blockEnd);
			i += blockEnd - offset;
		}
		m_Size = n;
		SizeT nrBlocks = (n + block_size - 1) / block_size;
		while (m_Blocks.size() > nrBlocks)
		{
			ReleaseBlock(m_Blocks.size() - 1);
			m_Blocks.pop_back();
		}
		if (m_Blocks.empty())
		{
			m_FirstCapacity = 0;
			m_WritePos = m_WriteEnd = nullptr;
		}
		else
		{
			SizeT b = m_Blocks.size() - 1;
			m_WritePos = m_Blocks[b] + (n - b * block_size);
			m_WriteEnd = m_Blocks[b] + (b ? block_size : m_FirstCapacity);
		}
	}

	void clear()
	{
		for (SizeT b = 0, nrBlocks = m_Blocks.size(); b != nrBlocks; ++b)
			ReleaseBlock(b);
		m_Blocks = {};
		m_Size = 0;
		m_FirstCapacity = 0;
		m_WritePos = m_WriteEnd = nullptr;
	}

private:
	template <typename... Args>
	T& emplace_back_growing(MG_DEBUG_ALLOCATOR_FIRST_ARG Args&& ...args)
	{
		T value(std::forward<Args>(args)...); // before the first block grows, which moves the element that args can refer to
		Grow(MG_DEBUG_ALLOCATOR_TXT_PARAM);
		T* p = new (m_WritePos) T(std::move(value));
		++m_WritePos;
		++m_Size;
		return *p;
	}

	// makes room for the next element when the last block is full: a larger first block while it is the only one and
	// smaller than block_size, a new block otherwise
	void Grow(MG_DEBUG_ALLOCATOR_TXT_ARG)
	{
		assert(m_WritePos == m_WriteEnd);
		if (m_FirstCapacity < block_size)
		{
			assert(m_Blocks.size() <= 1);
			ResizeFirstBlock(Min<SizeT>(Max<SizeT>(2 * m_FirstCapacity, 1), block_size) MG_DEBUG_ALLOCATOR_SRC_PARAM);
			return;
		}
		T* block = CreateMyAllocator<T>()->allocate(block_size MG_DEBUG_ALLOCATOR_SRC_PARAM);
		try { m_Blocks.push_back(block); }
		catch (...) { CreateMyAllocator<T>()->deallocate(block, block_size); throw; }
		m_WritePos = block;
		m_WriteEnd = block + block_size;
	}

	// gives the only block, if any, room for n elements, or what the stock gives for that many, within block_size
	void ResizeFirstBlock(SizeT n MG_DEBUG_ALLOCATOR_SRC_ARG)
	{
		assert(m_Blocks.size() <= 1);
		assert(m_Size <= n && n <= block_size);
		SizeT newCapacity = Min<SizeT>(my_allocator<T>::capacity_for(n), block_size);
		T* newBlock = CreateMyAllocator<T>()->allocate(newCapacity MG_DEBUG_ALLOCATOR_SRC_PARAM);
		if (m_Blocks.empty())
		{
			try { m_Blocks.push_back(newBlock); }
			catch (...) { CreateMyAllocator<T>()->deallocate(newBlock, newCapacity); throw; }
		}
		else
		{
			raw_move(m_Blocks[0], m_Blocks[0] + m_Size, newBlock);
			CreateMyAllocator<T>()->deallocate(m_Blocks[0], m_FirstCapacity);
			m_Blocks[0] = newBlock;
		}
		m_FirstCapacity = newCapacity;
		m_WritePos = newBlock + m_Size;
		m_WriteEnd = newBlock + newCapacity;
	}

	// destroys the elements of block b and gives it back to the stock; a block_vector_reader does this for each block it
	// has read, so that a released block is a null pointer
	void ReleaseBlock(SizeT b)
	{
		T* block = m_Blocks[b];
		if (!block)
			return;
		SizeT first = b * block_size;
		SizeT nrElems = (first < m_Size) ? Min<SizeT>(m_Size - first, block_size) : 0;
		destroy_range(block, block + nrElems);
		CreateMyAllocator<T>()->deallocate(block, b ? block_size : m_FirstCapacity);
		m_Blocks[b] = nullptr;
	}

	my_vec_t<T*> m_Blocks;
	SizeT        m_Size = 0;
	SizeT        m_FirstCapacity = 0;   // block_size when there is more than one block
	T*           m_WritePos = nullptr;  // where the next element goes, in the last block
	T*           m_WriteEnd = nullptr;  // the end of the last block

	friend struct block_vector_reader<T>;
};

// Reads the elements of a block_vector that it takes over, from front to back, and frees each block as soon as it has
// read the last element of it. An element that it hands out may be moved from; it is destroyed with its block.
template <typename T>
struct block_vector_reader
{
	explicit block_vector_reader(block_vector<T>&& src)
		: m_Src(std::move(src))
	{
		SetBlock(0);
	}

	bool AtEnd() const { return m_Curr == m_BlockEnd; }
	T& operator *() const { assert(!AtEnd()); return *m_Curr; }
	T* operator ->() const { assert(!AtEnd()); return m_Curr; }

	void operator ++()
	{
		assert(!AtEnd());
		if (++m_Curr == m_BlockEnd)
			SkipBlock();
	}

	// the elements of the current block that are not read yet, for a caller that takes them at once
	T* Curr()     const { return m_Curr; }
	T* BlockEnd() const { return m_BlockEnd; }

	// frees the current block, with the elements that are not read yet, and goes on with the next one
	void SkipBlock()
	{
		m_Src.ReleaseBlock(m_Block);
		SetBlock(m_Block + 1);
	}

	// moves the elements that are not read yet to the end of dst, a block at a time, and frees each block once moved
	void MoveRestTo(block_vector<T>& dst)
	{
		for (; !AtEnd(); SkipBlock())
			dst.append_moved(m_Curr, m_BlockEnd);
	}

	// marks the next n elements of the current block as read, n being at most what is left of it
	void Consume(SizeT n)
	{
		assert(n <= SizeT(m_BlockEnd - m_Curr));
		m_Curr += n;
		if (n && m_Curr == m_BlockEnd)
			SkipBlock();
	}

	// The current block, if none of it is read yet and it holds n elements and was allocated for n, which the caller
	// then owns: it frees it as a tile does, with array_traits<T>::Destroy(block, n). The reader goes on with the next
	// block. Returns nullptr, taking nothing, otherwise.
	T* TakeBlockIfWhole(SizeT n)
	{
		if (AtEnd() || m_Curr != m_Src.m_Blocks[m_Block] || SizeT(m_BlockEnd - m_Curr) != n)
			return nullptr;
		if ((m_Block ? block_vector<T>::block_size : m_Src.m_FirstCapacity) != n)
			return nullptr;
		T* block = m_Curr;
		m_Src.m_Blocks[m_Block] = nullptr;
		SetBlock(m_Block + 1);
		return block;
	}

private:
	void SetBlock(SizeT b)
	{
		m_Block = b;
		SizeT first = b * block_vector<T>::block_size;
		if (first < m_Src.m_Size)
		{
			m_Curr = m_Src.m_Blocks[b];
			m_BlockEnd = m_Curr + Min<SizeT>(m_Src.m_Size - first, block_vector<T>::block_size);
		}
		else
			m_Curr = m_BlockEnd = nullptr;
	}

	block_vector<T> m_Src;
	SizeT           m_Block = 0;
	T*              m_Curr = nullptr;
	T*              m_BlockEnd = nullptr;
};

#endif //!defined(__RTC_MEM_BLOCKVECTOR_H)
