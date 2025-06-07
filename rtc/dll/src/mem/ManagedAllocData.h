// Copyright (C) 1998-2025 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

#if !defined(__RTC_MEM_MANAGEDALLOCDATA_H)
#define __RTC_MEM_MANAGEDALLOCDATA_H

#include "RtcBase.h"
#include "mem/AllocData.h"
#include "mem/FixedAlloc.h"
#include "set/rangefuncs.h"

//----------------------------------------------------------------------
// managed_alloc_data
//----------------------------------------------------------------------

template <typename V>
struct managed_alloc_data : alloc_data<V>
{
	using typename alloc_data<V>::size_type;
	using alloc_data<V>::first;
	using alloc_data<V>::second;
	using alloc_data<V>::m_Capacity;
	using const_iterator = typename sequence_traits<V>::const_pointer;

	managed_alloc_data()
	{}

	managed_alloc_data(size_type sz, bool mustClear MG_DEBUG_ALLOCATOR_SRC_ARG)
	{
		if (!sz)
			return;

		first = CreateMyAllocator<V>()->allocate(sz MG_DEBUG_ALLOCATOR_SRC_PARAM);
		second = first + sz;
		raw_awake_or_init(first, second, mustClear);
		m_Capacity = sz;
	}

	managed_alloc_data(size_type sz, size_type capacity, bool mustClear MG_DEBUG_ALLOCATOR_SRC_ARG)
	{
		assert(sz <= capacity);
		if (!capacity)
			return;

		first = CreateMyAllocator<V>()->allocate(capacity MG_DEBUG_ALLOCATOR_SRC_PARAM);
		second = first + sz;
		raw_awake_or_init(first, second, mustClear);
		m_Capacity = capacity;
	}

	managed_alloc_data(const_iterator first_, const_iterator last_, size_type capacity MG_DEBUG_ALLOCATOR_SRC_ARG)
	{
		assert(size_type(last_ - first_) <= capacity);
		if (!capacity)
			return;

		first = CreateMyAllocator<V>()->allocate(capacity MG_DEBUG_ALLOCATOR_SRC_PARAM);

		second = raw_copy(first_, last_, first);
		m_Capacity = capacity;
	}


	~managed_alloc_data() { clear(); }

	void destroy_elements()
	{
		destroy_range(first, second);
		second = first; // reset the range
	}

	void clear()
	{
		if (m_Capacity)
		{
			destroy_elements();

			CreateMyAllocator<V>()->deallocate(first, m_Capacity);
			m_Capacity = 0;
		}
	}
	size_type max_size() const
	{
		return CreateMyAllocator<V>()->max_size();
	}
};

//----------------------------------------------------------------------
// my_vector
//----------------------------------------------------------------------


template <typename V>
struct my_vector : managed_alloc_data<V>
{
	using size_type = SizeT;
	using const_iterator = const V*;
	using iterator = V*;
	using value_type = V;
	using reference = V&;
	using const_reference = const V&;
	using pointer = V*;
	using const_pointer = const V*;

	my_vector() = default;

	my_vector(size_type sz MG_DEBUG_ALLOCATOR_SRC_ARG)
		: managed_alloc_data<V>(sz, false MG_DEBUG_ALLOCATOR_SRC_PARAM)
	{
		fast_zero(this->first, this->second);
	}

	my_vector(size_type sz, const V& value MG_DEBUG_ALLOCATOR_SRC_ARG)
		: managed_alloc_data<V>(sz, false MG_DEBUG_ALLOCATOR_SRC_PARAM)
	{
		fast_fill(this->first, this->second, value);
	}
	my_vector(const_iterator first_, const_iterator last_ MG_DEBUG_ALLOCATOR_SRC_ARG)
		: managed_alloc_data<V>(last_ - first_, false MG_DEBUG_ALLOCATOR_SRC_PARAM)
	{
		this->second = fast_copy (first_, last_, this->first);

	}

	my_vector(const my_vector<V>& rhs MG_DEBUG_ALLOCATOR_SRC(CharPtr srcStr = "my_vector::CopyCTOR"))
		: my_vector<V>(rhs.first, rhs.second MG_DEBUG_ALLOCATOR_SRC_PARAM)
	{}

	my_vector(my_vector<V>&& rhs)
	{
		this->swap(rhs);
	}

	void operator =(const my_vector<V>& rhs)
	{
		if (this == &rhs)
			return; // self-assignment, nothing to do
		if (this->m_Capacity >= rhs.size())
		{
			// enough capacity, just copy the elements
			this->destroy_elements();

			assert(this->m_Capacity >= rhs.size());
			assert(this->first != nullptr); // ensure the pointers are valid
			assert(this->second == this->first); 

			this->second = fast_copy(rhs.first, rhs.second, this->first);
			return;
		}
		my_vector<V> rhsCopy(rhs MG_DEBUG_ALLOCATOR_SRC("my_vector::CopyAssignment"));
		this->swap(rhsCopy);
	}

	~my_vector() = default;

	void reserve(size_type sz MG_DEBUG_ALLOCATOR_SRC_ARG)
	{
		if (sz <= this->m_Capacity)
			return; // no need to reserve more space	
		managed_alloc_data<V> newAlloc(0, sz, false MG_DEBUG_ALLOCATOR_SRC_PARAM);
		newAlloc.second = raw_move(this->first, this->second, newAlloc.first);
		this->second = this->first; // raw_move already destroyed the moved elements
		this->swap(newAlloc);
	}
	void grow(size_type sz, bool mustClear MG_DEBUG_ALLOCATOR_SRC_ARG)
	{
		auto newSize = this->size() + sz;
		if (newSize > this->m_Capacity)
		{
			MakeMax<SizeT>(newSize, 2 * this->m_Capacity);
			reserve(newSize MG_DEBUG_ALLOCATOR_SRC_PARAM);
		}
		auto newLast = this->second + sz;
		if (mustClear)
			fast_zero(this->second, newLast);
		this->second = newLast;
	}

	void resize(size_type sz MG_DEBUG_ALLOCATOR_SRC_ARG)
	{
		resizeSO(sz, true MG_DEBUG_ALLOCATOR_SRC_PARAM); // resize without clearing
	}

	void resizeSO_impl(size_type sz, bool mustKeep, bool mustClear MG_DEBUG_ALLOCATOR_SRC_ARG)
	{
		if (sz < this->size())
		{
			destroy_range(this->first + sz, this->second); // destroy the elements that are no longer needed
			this->second = this->first + sz; // adjust the end pointer
		}
		else
		{
			if (sz > this->m_Capacity)
			{
				if (!mustKeep)
					this->destroy_elements(); // destroy existing elements if not keeping them
				reserve(sz MG_DEBUG_ALLOCATOR_SRC_PARAM); // ensure capacity, but avoid extra reservation
			}

			auto newLast = this->first + sz;
			raw_awake_or_init(this->second, newLast, mustClear); // awake or initialize the new elements
			this->second = newLast; // adjust the end pointer
		}
	}
	void resizeSO(size_type sz, bool mustClear MG_DEBUG_ALLOCATOR_SRC_ARG)
	{
		resizeSO_impl(sz, true, mustClear MG_DEBUG_ALLOCATOR_SRC_PARAM); // resize with keeping existing elements
	}
		
	void reallocSO(size_type sz, bool mustClear MG_DEBUG_ALLOCATOR_SRC_ARG)
	{
		resizeSO_impl(sz, false, mustClear MG_DEBUG_ALLOCATOR_SRC_PARAM); // resize without keeping existing elements
	}
	void insert(const_iterator pos, const V& value MG_DEBUG_ALLOCATOR_SRC_ARG)
	{
		assert(pos >= this->first && pos <= this->second); // ensure the range is valid
		grow(1, false MG_DEBUG_ALLOCATOR_SRC_PARAM);
		auto afterPos = raw_move_backward_unchecked(pos, this->cend(), this->end() + 1); // move elements to the right
		new (--afterPos) V(value); // placement new to construct the object in place
	}

	template <typename InIter>
	void append(InIter first, InIter last MG_DEBUG_ALLOCATOR_SRC_ARG)
	{
		SizeT oldSize = this->size();
		SizeT n = last - first;

		assert(oldSize + n >= oldSize); // No overflow
		grow(n, false MG_DEBUG_ALLOCATOR_SRC_PARAM); // ensure enough space
		assert(this->size() == oldSize + n);
		fast_copy(first, last, this->begin() + oldSize);
	}

	template <std::ranges::range Range>
		requires std::convertible_to<std::ranges::range_value_t<Range>, V>
	void append_range(Range&& range MG_DEBUG_ALLOCATOR_SRC_ARG)
	{
		append(std::begin(range), std::end(range) MG_DEBUG_ALLOCATOR_SRC_PARAM);
	}

	void push_back(const V& value MG_DEBUG_ALLOCATOR_SRC_ARG)
	{
		grow(1, false MG_DEBUG_ALLOCATOR_SRC_PARAM);
		new (&this->back()) V(value); // placement new to construct the object in place
	}
	template <typename... Args>
	void emplace_back(MG_DEBUG_ALLOCATOR_FIRST_ARG Args&& ...args)
	{
		grow(1, false MG_DEBUG_ALLOCATOR_SRC_PARAM);
		new (&this->back()) V(std::forward<Args>(args)...); // placement new to construct the object in place
	}
	void erase(const_iterator first, const_iterator last)
	{
		assert(this->first <= first && first <= last && last <= this->second); // ensure the range is valid
		destroy_range(first, last); // destroy the elements in the range before they could be run over
		auto newEnd = raw_move(last, this->cend(), this->first + (first - this->first)); // move elements to fill the gap
		this->second = newEnd; // adjust the end pointer
	}
	void pop_back()
	{
		assert(!this->empty());
		auto last = this->second;
		--last; // get the last element's position
		last->~V(); // explicitly call the destructor for the last element
		this->second = last; // adjust the end pointer
	}
};

#endif //!defined(__RTC_MEM_MANAGEDALLOCDATA_H)
