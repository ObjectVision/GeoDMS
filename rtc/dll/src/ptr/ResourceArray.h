// Copyright (C) 1998-2024 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

#if !defined(__RTC_PTR_RESOURCEARRAY_H)
#define __RTC_PTR_RESOURCEARRAY_H

#include "ptr/PtrBase.h"

struct ResourceArrayBase
{
	ResourceArrayBase()
		:	m_N(0)
	{}

	virtual void destroy() = 0;

	SizeT size() const { return m_N; }

protected:
	static char* allocate(SizeT sz)
	{
		std::allocator<char> alloc;
		return alloc.allocate(sz);
	}
	static void  deallocate(char* ptr, SizeT sz)
	{
		std::allocator<char> alloc;
		alloc.deallocate(ptr, sz);
	}
	SizeT m_N;
};

struct ResourceArrayHandle : ptr_base<ResourceArrayBase, movable>
{
	ResourceArrayHandle(pointer ptr = nullptr) noexcept
		:	ptr_base(ptr)
	{}
	ResourceArrayHandle(ResourceArrayHandle&& org) noexcept
		:	ptr_base(org.release()) 
	{}

	~ResourceArrayHandle () 
	{ 
		if (has_ptr())
			get_ptr()->destroy(); 
	}

	void    init   (pointer ptr)       { assert(is_null()); m_Ptr = ptr; }
	pointer release()                  { pointer tmp_ptr = m_Ptr; m_Ptr = nullptr; return tmp_ptr; }
	void    reset  (pointer ptr = nullptr)  { assert(ptr != get_ptr() || !ptr); ResourceArrayHandle tmp(ptr); tmp.swap(*this); }
	void    swap   (ResourceArrayHandle& oth) { std::swap(m_Ptr, oth.m_Ptr); }

	void operator = (ResourceArrayHandle&& rhs) noexcept 
	{ 
		reset(rhs.release()); 
	}

	friend void swap(ResourceArrayHandle& a, ResourceArrayHandle& b) noexcept { a.swap(b); }
};

template <typename R>
struct ResourceArray : ResourceArrayBase
{
	R     m_Data[0] = {};

	void destroy() override
	{
		SizeT n = m_N;
		this->~ResourceArray();
		deallocate(reinterpret_cast<char*>(this), ByteSize(n));
	}

	static ResourceArray* create(SizeT n)
	{
		char* resultMemPtr = allocate(ByteSize(n));
		ResourceArray* result = new (resultMemPtr) ResourceArray;
		ResourceArrayHandle resHandle(result);
		result->Construct(n);
		resHandle.release();
		return result;
	}
	R* begin() { return m_Data; }
	R* end()   { return m_Data + m_N; }
	const R* begin() const { return m_Data; }
	const R* end()   const { return m_Data + m_N; }

private:
	static SizeT ByteSize(SizeT n)
	{
		return n*sizeof(R)+sizeof(ResourceArray);
	}

	void construct(SizeT n)
	{
		while (m_N<n)
			new (m_Data + m_N++) R;
	}

	~ResourceArray()
	{
		while (m_N)
			m_Data[--m_N].~R();
	}
};


#endif // __RTC_PTR_RESOURCEARRAY_H
