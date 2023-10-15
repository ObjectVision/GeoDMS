#pragma once

#if !defined(__RTC_PTR_SHAREDARRAY_H)
#define __RTC_PTR_SHAREDARRAY_H

#include "cpc/CompChar.h"
#include "mem/MyAllocator.h"
#include "ptr/SharedBase.h"
#include "geo/SequenceTraits.h"
#include "geo/SizeCalculator.h"
#include "set/RangeFuncs.h"

using shared_array_size_t = std::size_t; // different for Win32 and x64

template <typename T>
struct SharedArray : SharedBase
{
	typedef typename sequence_traits<T>::const_pointer   const_iterator;
	typedef typename sequence_traits<T>::pointer         iterator;
	typedef typename sequence_traits<T>::const_reference const_reference;
	typedef typename sequence_traits<T>::reference       reference;
	typedef typename sequence_traits<T>::block_type      block_type;

	using size_t = shared_array_size_t;

	size_t size () const { return m_Size; }
	bool   empty() const { return m_Size == 0; }

	      T&                     operator[](size_t i)       { dms_assert(i<size()); return m_Data[i]; }
	typename param_type<T>::type operator[](size_t i) const { dms_assert(i<size()); return m_Data[i]; }

	      iterator begin()       { return iter_creator<T>()(m_Data, 0); }
	const_iterator begin() const { return iter_creator<T>()(m_Data, 0); }
	      iterator end  ()       { return iter_creator<T>()(m_Data, size()); }
	const_iterator end  () const { return iter_creator<T>()(m_Data, size()); }
	      reference back()       { dms_assert(size()>0); return end()[-1]; }
	const_reference back() const { dms_assert(size()>0); return end()[-1]; }
	      reference sback()       { dms_assert(size()>1); return end()[-2]; }
	const_reference sback() const { dms_assert(size()>1); return end()[-2]; }

	void erase(iterator pos, size_t c=1)
	{
		dms_assert(pos >= begin());
		dms_assert((pos-begin())+c <= size());

		pos = fast_copy(pos+c, end(), pos);
		ShrinkTo (pos - begin() );
	}
	void erase(size_t pos, size_t c=1) { erase(begin()+pos, c); }
	void erase(iterator first, iterator last)
	{
		dms_assert(begin() <= first);
		dms_assert(first <= last);
		dms_assert(last <= end());
		last = fast_copy(last, end(), first);
		ShrinkTo(last - begin());
	}
	void pop_back()
	{
		erase(end()-1);
	}

	static SharedArray<T>* Create(size_t size, bool mustClear)
	{
		SharedArray<T>* result = CreateUninitialized(size);

		iterator f = result->begin();
		raw_awake_or_init( f, f+size, mustClear);

		return result;
	}

	static SharedArray<T>* Create(size_t size, T value)
	{
		SharedArray<T>* result = CreateUninitialized(size);

		iterator f = result->begin();
		raw_fill(f, f + size, value);
		return result; 
	}

	static SharedArray<T>* Create(const T* first, const T* last)
	{
		SharedArray<T>* result = CreateUninitialized(last-first);

		raw_copy(first, last, result->begin());
		return result; 
	}

	constexpr static size_t NrAllocations(size_t nrElems)
	{
		return
			(size_calculator<T>().nr_bytes(nrElems)
				+ (sizeof(SharedArray<T>) + sizeof(allocator_i::value_type) - 1)
				)
			/ sizeof(allocator_i::value_type);
	}

	static SharedArray<T>* CreateUninitialized(SizeT size)
	{
		allocator_i alloc;
		size_t allocSize = NrAllocations(size);
		SharedArray<T>* result = new (alloc.allocate(allocSize)) SharedArray<T>(size, allocSize);
		return result;
	}
	void Release() const
	{
		if (!DecRef())
		{
			size_t allocSize = m_AllocSize;
			const_cast<SharedArray*>(this)->SharedArray::~SharedArray();
			allocator_i().deallocate(
				reinterpret_cast<typename allocator_i::value_type*>( const_cast<SharedArray*>(this) )
			,	allocSize
			);
		}
	}

private: // copying this is not allowed
	void ShrinkTo(size_t newSize)
	{
		dms_assert(newSize <= size());

		iterator
			f = begin()+newSize,
			l = end();

		destroy_range(f, l);

		SetSize(newSize);
	}

private: // copying this is not allowed
	friend SharedCharArray* SharedCharArray_CreateEmptyImpl();

	using allocator_i = std::allocator<size_t>;
	
	void SetSize (size_t i) { m_Size = i; }

	SharedArray() = delete;
	SharedArray(size_t size, size_t allocSize)
		:	m_Size(size)
		,	m_AllocSize(allocSize) 
	{}
	SharedArray(const SharedArray&) = delete;
	~SharedArray()                          { ShrinkTo(0); }

	size_t     m_Size;
	size_t     m_AllocSize;
	block_type m_Data[];
};

#endif // __RTC_PTR_SHAREDARRAY_H
