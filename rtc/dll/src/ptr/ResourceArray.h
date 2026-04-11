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
	SizeT m_N = 0;
};

struct destroyResourceArray
{
	void operator()(ResourceArrayBase* ptr)
	{
		assert(ptr);
		ptr->destroy();
	}
};

using ResourceArrayHandle = std::unique_ptr<ResourceArrayBase, destroyResourceArray>;

#pragma warning(push)
#pragma warning(disable: 26495)

template <typename R>
struct ResourceArray : ResourceArrayBase
{
#if defined(_MSC_VER)
	R m_Data[];
#else
	// GCC: flexible array members with non-trivial types not supported
	// Use placement storage; begin()/end() provide typed access
	alignas(R) char m_DataStorage[];
#endif

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
		result->construct(n);
		resHandle.release();
		return result;
	}
	R* begin() { 
#if defined(_MSC_VER)
		return m_Data;
#else
		return reinterpret_cast<R*>(m_DataStorage);
#endif
	}
	R* end()   { return begin() + m_N; }
	const R* begin() const {
#if defined(_MSC_VER)
		return m_Data;
#else
		return reinterpret_cast<const R*>(m_DataStorage);
#endif
	}
	const R* end()   const { return begin() + m_N; }

private:
	static SizeT ByteSize(SizeT n)
	{
		return n*sizeof(R)+sizeof(ResourceArray);
	}

	void construct(SizeT n)
	{
		for (SizeT i = 0; i < n; ++i)
			new (begin() + i) R;
		m_N = n;
	}

	~ResourceArray()
	{
		SizeT i = m_N; 
		while (i > 0)
			begin()[--i].~R();
		m_N = 0;
	}
};

#pragma warning(pop)

#endif // __RTC_PTR_RESOURCEARRAY_H
