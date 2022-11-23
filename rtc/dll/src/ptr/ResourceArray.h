//<HEADER> 
/*
Data & Model Server (DMS) is a server written in C++ for DSS applications. 
Version: see srv/dms/rtc/dll/src/RtcVersion.h for version info.

Copyright (C) 1998-2004  YUSE GSO Object Vision BV. 

Documentation on using the Data & Model Server software can be found at:
http://www.ObjectVision.nl/DMS/

See additional guidelines and notes in srv/dms/Readme-srv.txt 

This library is free software; you can use, redistribute, and/or
modify it under the terms of the GNU General Public License version 2 
(the License) as published by the Free Software Foundation,
provided that this entire header notice and readme-srv.txt is preserved.

See LICENSE.TXT for terms of distribution or look at our web site:
http://www.objectvision.nl/DMS/License.txt
or alternatively at: http://www.gnu.org/copyleft/gpl.html

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
General Public License for more details. However, specific warranties might be
granted by an additional written contract for support, assistance and/or development
*/
//</HEADER>
#pragma once

#if !defined(__RTC_PTR_RESOURCEARRAY_H)
#define __RTC_PTR_RESOURCEARRAY_H

#include "ptr/PtrBase.h"

struct ResourceArrayBase
{
	ResourceArrayBase()
		:	m_N(0)
	{}

	virtual void Destroy() = 0;

	SizeT Size() const { return m_N; }

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
			get_ptr()->Destroy(); 
	}

	void    init   (pointer ptr)       { dms_assert(is_null()); m_Ptr = ptr; }
	pointer release()                  { pointer tmp_ptr = m_Ptr; m_Ptr = nullptr; return tmp_ptr; }
	void    reset  (pointer ptr = nullptr)  { dms_assert(ptr != get_ptr() || !ptr); ResourceArrayHandle tmp(ptr); tmp.swap(*this); }
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

	void Destroy() override
	{
		SizeT n = m_N;
		this->~ResourceArray();
		deallocate(reinterpret_cast<char*>(this), ByteSize(n));
	}

	static ResourceArray* Create(SizeT n)
	{
		char* resultMemPtr = allocate(ByteSize(n));
		ResourceArray* result = new (resultMemPtr) ResourceArray;
		ResourceArrayHandle resHandle(result);
		result->Construct(n);
		resHandle.release();
		return result;
	}

private:
	static SizeT ByteSize(SizeT n)
	{
		return n*sizeof(R)+sizeof(ResourceArray);
	}

	void Construct(SizeT n)
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
