// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#pragma once

//	Template OwningPtr holds onto a pointer obtained via new and deletes that
//	object when it itself is destroyed (such as when leaving block scope). It
//	can be used to make calls to new() exception safe and as data-members. Copy
//	& assingment are non-const; the pointer is cleared after transfer
//	vector<owning_ptr< T > > is therefore not allowed.

#if !defined(__RTC_PTR_OWNINGPTRSIZEDARRAY_H)
#define __RTC_PTR_OWNINGPTRSIZEDARRAY_H

#include "dbg/debug.h"
#include "ptr/OwningPtrArray.h"
#include "set/rangefuncs.h"

struct DontInitialize_tag {};
struct ValueConstruct_tag {};
struct Internal_tag {};

constexpr DontInitialize_tag dont_initialize;
constexpr ValueConstruct_tag value_construct;

template <class T>
struct OwningPtrSizedArray : private ref_base<T, movable>
{
	using base_type = ref_base<T, movable>;
	using typename base_type::pointer;
	using typename base_type::const_pointer;
	using typename base_type::reference;
	using typename base_type::const_reference;
	using iterator       = pointer;
	using const_iterator = const_pointer ;

	OwningPtrSizedArray()
	{}

	OwningPtrSizedArray(SizeT sz, Internal_tag  MG_DEBUG_ALLOCATOR_SRC_ARG)
		: ref_base<T, movable>(array_traits<T>::CreateUninitialized(sz MG_DEBUG_ALLOCATOR_SRC_PARAM))
		, m_Size(sz)
	{
	}

	OwningPtrSizedArray(SizeT sz, DontInitialize_tag  MG_DEBUG_ALLOCATOR_SRC_ARG)
		: OwningPtrSizedArray(sz, Internal_tag() MG_DEBUG_ALLOCATOR_SRC_PARAM)
	{
		if constexpr (!is_bitvalue_v<T>)
			std::uninitialized_default_construct(begin(), end());
	}

	OwningPtrSizedArray(SizeT sz, ValueConstruct_tag MG_DEBUG_ALLOCATOR_SRC_ARG)
		: OwningPtrSizedArray(sz, Internal_tag() MG_DEBUG_ALLOCATOR_SRC_PARAM)
	{
		std::uninitialized_value_construct(begin(), end());
	}

	OwningPtrSizedArray(SizeT sz, Undefined MG_DEBUG_ALLOCATOR_SRC_ARG)
		: OwningPtrSizedArray(sz, dont_initialize MG_DEBUG_ALLOCATOR_SRC_PARAM)
	{
		fast_undefine(begin(), begin() + sz);
	}

	OwningPtrSizedArray(OwningPtrSizedArray&& oth) noexcept
	{
		swap(oth);
	}
	~OwningPtrSizedArray()
	{
		reset();
	}
	void reset() noexcept
	{
		if (this->m_Ptr)
			array_traits<T>::Destroy(this->m_Ptr, m_Size);
		else
			assert(!m_Size);
		this->m_Ptr = pointer();
		m_Size = 0;
	}
	void reset(pointer ptr, SizeT newSize) noexcept
	{
		dms_assert(this->m_Ptr != ptr || !ptr);
		if (this->m_Ptr)
			array_traits<T>::Destroy(this->m_Ptr, m_Size);
		else
			assert(!m_Size);
		this->m_Ptr = ptr;
		m_Size = newSize;
	}

	SizeT size() const { return m_Size; }
	bool empty() const { return !m_Size; }
	operator bool() const { return m_Size; }

	void clear() { OwningPtrSizedArray empty; swap(empty); }

	void swap (OwningPtrSizedArray<T>& oth) noexcept { base_type::swap(oth); std::swap(m_Size, oth.m_Size); }

	void operator = (OwningPtrSizedArray<T>&& rhs) noexcept { clear(); swap(rhs); }
	reference       operator [](SizeT i)       { dms_assert(i < size()); return begin()[i]; }
	const_reference operator [](SizeT i) const { dms_assert(i < size()); return begin()[i]; }

	reference       back()       { dms_assert(size()); return begin()[ m_Size -1 ]; }
	const_reference back() const { dms_assert(size()); return begin()[ m_Size -1 ]; }

	iterator       begin()       { return this->get(); }
	const_iterator begin() const { return this->get(); }
	iterator       end  ()       { return begin() + m_Size; }
	const_iterator end  () const { return begin() + m_Size; }

	void resizeSO(SizeT sz, bool mustClear MG_DEBUG_ALLOCATOR_SRC_ARG)
	{
		grow(sz, mustClear MG_DEBUG_ALLOCATOR_SRC_PARAM);
		assert(m_Size >= sz); // Prostcondition of grow.
		if (sz < m_Size)
		{
			array_traits<T>::Destroy(this->m_Ptr + sz, m_Size - sz); // possibly nop, but possibly not
			m_Size = sz; // drop trailing if shrinking was requested, without destroying them (yet).
		}
	}

	void reallocSO(SizeT sz, bool mustClear MG_DEBUG_ALLOCATOR_SRC_ARG) 
	{
		auto oldSz = m_Size;
		if (sz > m_Size)
		{
			OwningPtrSizedArray local(sz, Internal_tag() MG_DEBUG_ALLOCATOR_SRC_PARAM);
			swap(local);
			assert(mustClear || is_simple<T>::value);
			if (mustClear)
				std::uninitialized_value_construct  (begin(), end());
			else
				std::uninitialized_default_construct(begin(), end());
		}
		else if (sz < m_Size)
		{
			array_traits<T>::Destroy(this->m_Ptr + sz, m_Size - sz); // possibly nop, but possibly not
			m_Size = sz; // drop trailing if shrinking was requested, without destroying them (yet).
		}
	}

private:
	void grow(SizeT sz, bool mustClear MG_DEBUG_ALLOCATOR_SRC_ARG)
	{
		if (sz > m_Size)
		{
			OwningPtrSizedArray local(sz, Internal_tag() MG_DEBUG_ALLOCATOR_SRC_PARAM);
			auto localIter = fast_copy(begin(), end(), local.begin());
			if (mustClear)
				std::uninitialized_value_construct(localIter, local.end());
			else
				std::uninitialized_default_construct(localIter, local.end());
			swap(local);
		}
	}

private:
	OwningPtrSizedArray(const OwningPtrSizedArray& src); // don't call this
	SizeT m_Size = 0;
};


template <typename T>
inline bool IsDefined(const OwningPtrSizedArray<T>& v) { return v.IsDefined(); }

template <typename T>
auto GetSeq(OwningPtrSizedArray<T>& so) -> IterRange<typename OwningPtrSizedArray<T>::pointer>
{
	return { so.begin(), so.end() };
}

template <typename T>
auto GetConstSeq(const OwningPtrSizedArray<T>& so) -> IterRange<typename OwningPtrSizedArray<T>::const_pointer>
{
	return { so.begin(), so.end() };
}


#endif // __RTC_PTR_OWNINGPTRSIZEDARRAY_H
