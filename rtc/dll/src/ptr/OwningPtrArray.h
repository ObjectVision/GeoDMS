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

//	Template OwningPtr holds onto a pointer obtained via new and deletes that
//	object when it itself is destroyed (such as when leaving block scope). It
//	can be used to make calls to new() exception safe and as data-members. Copy
//	& assingment are non-const; the pointer is cleared after transfer
//	vector<owning_ptr< T > > is therefore not allowed.

#if !defined(__RTC_PTR_OWNINGPTRARRAY_H)
#define __RTC_PTR_OWNINGPTRARRAY_H

#include "dbg/Check.h"
#include "mem/MyAllocator.h"
#include "ptr/PtrBase.h"

#include <boost/checked_delete.hpp>

template <typename T>
struct array_traits
{
	using pointer = typename sequence_traits<T>::pointer;
	
	static pointer CreateUninitialized(SizeT nrElems MG_DEBUG_ALLOCATOR_SRC_ARG)
	{ 
		auto result = CreateMyAllocator<T>()->allocate(nrElems MG_DEBUG_ALLOCATOR_SRC_PARAM);
		return result;
	}
	static pointer CreateDefaultConstructed(SizeT nrElems MG_DEBUG_ALLOCATOR_SRC_ARG)
	{
		auto result = CreateUninitialized(nrElems MG_DEBUG_ALLOCATOR_SRC_PARAM);
		std::uninitialized_default_construct(result, result + nrElems);
		return result;
	}
	static void Destroy(pointer p, SizeT nrElems)
	{
		std::destroy_n(p, nrElems);
		CreateMyAllocator<T>()->deallocate(p, nrElems);
	}
	static SizeT ByteAlign(SizeT nrElems) { return nrElems;}
	static SizeT ByteSize (SizeT nrElems) { return nrElems * sizeof(T);}
	static SizeT NrElemsIn(SizeT nrBytes) { return (nrBytes+(sizeof(T)-1)) / sizeof(T); }
	static void* DataBegin(pointer p) { return p; }
};

template <bit_size_t N>
struct array_traits<bit_value<N> >
{
	typedef typename sequence_traits<bit_value<N> >::pointer    pointer;
	typedef typename sequence_traits<bit_value<N> >::block_type block_type;
	enum {
		nr_elems_per_byte = 8/N, 
		last_elem_in_byte = nr_elems_per_byte - 1 
	};
	static pointer CreateUninitialized(SizeT nrElems MG_DEBUG_ALLOCATOR_SRC_ARG)
	{ 
		return pointer(array_traits<block_type>::CreateUninitialized(pointer::calc_nr_blocks(nrElems) MG_DEBUG_ALLOCATOR_SRC_PARAM), SizeT(0));
	}
	static pointer CreateDefaultConstructed(SizeT nrElems MG_DEBUG_ALLOCATOR_SRC_ARG)
	{
		return pointer(array_traits<block_type>::CreateDefaultConstructed(pointer::calc_nr_blocks(nrElems) MG_DEBUG_ALLOCATOR_SRC_PARAM), SizeT(0));
	}
	static void Destroy(pointer p, SizeT nrElems)  { array_traits<block_type>::Destroy(p.data_begin(), pointer::calc_nr_blocks(nrElems)); }
	static SizeT ByteAlign(SizeT nrElems) { return (nrElems+last_elem_in_byte) & ~last_elem_in_byte; }
	static SizeT ByteSize (SizeT nrElems) { MGD_PRECONDITION(ByteAlign(nrElems) == nrElems); return nrElems / nr_elems_per_byte; }
	static SizeT NrElemsIn(SizeT nrBytes) { return nrBytes * nr_elems_per_byte; }
	static void* DataBegin(pointer p) { return p.data_begin(); }
};

// constness: value semantics, so a const ptr cannot be reassigned and can only grant const access to the pointee
/* REMOVE
template <class T>
struct OwningPtrArray : ref_base<T, movable>
{
	using base_type = ref_base<T, movable>;
	using typename base_type::pointer;
	using typename base_type::const_pointer;
	using typename base_type::reference;
	using typename base_type::const_reference;

	typedef pointer iterator;
	typedef const_pointer const_iterator;

	OwningPtrArray(pointer ptrArray = nullptr  ) noexcept
		: base_type(ptrArray)
	{}
	OwningPtrArray(SizeT nrElems)
		: base_type(array_traits<T>::Create(nrElems))
	{}
	OwningPtrArray(SizeT nrElems, Undefined) 
		: base_type(array_traits<T>::Create(nrElems))
	{ 
		fast_undefine(begin(), begin() + nrElems); 
	}
	OwningPtrArray(OwningPtrArray<T>&& oth) noexcept 
		: base_type(oth.release())
	{}

	~OwningPtrArray() noexcept
	{
		reset(); 
	}

	pointer release () { pointer tmp_ptr = this->m_Ptr; this->m_Ptr= nullptr; return tmp_ptr; }
	void reset (pointer ptr = pointer()) noexcept 
	{ 
		dms_assert(this->m_Ptr != ptr || !ptr);
		array_traits<T>::Destroy(this->m_Ptr);
		this->m_Ptr = ptr;
	}

	void swap (OwningPtrArray<T>& oth) noexcept
	{
		std::swap(this->m_Ptr, oth.m_Ptr);
	}

	void operator = (OwningPtrArray<T>&& rhs) noexcept
	{ 
		reset(rhs.release());
	}

	const_reference operator [](UInt32 i) const noexcept
	{ 
		dms_assert(this->m_Ptr);
		return this->m_Ptr[i];
	}

	const_pointer begin() const noexcept
	{
		return this->get_ptr();
	}

	reference operator [](UInt32 i) { dms_assert(this->m_Ptr); return this->m_Ptr[i]; }
	pointer begin()   { return this->get_ptr(); }

	friend void swap(OwningPtrArray<T>& a, OwningPtrArray<T>& b) noexcept 
	{
		a.swap(b); 
	}

private: // illegal copy ctors
	OwningPtrArray(const OwningPtrArray<T>& oth) = delete;
};

template <class T>       T* begin_ptr(      OwningPtrArray<T>& opa) { return opa.begin(); }
template <class T> const T* begin_ptr(const OwningPtrArray<T>& opa) { return opa.begin(); }
*/

#endif // __RTC_PTR_OWNINGPTRARRAY_H
