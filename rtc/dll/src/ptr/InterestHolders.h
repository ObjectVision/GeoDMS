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

//  -----------------------------------------------------------------------
//  Name        : dms\tic\src\InterestHolders.h
//  -----------------------------------------------------------------------

#if !defined(__RTC_PTR_INTERESTHOLDER_H)
#define __RTC_PTR_INTERESTHOLDER_H

#include "RtcBase.h"
#include "act/any.h"
#include "utl/swap.h"
struct Actor;


//----------------------------------------------------------------------
// class  : OptionalInterest
//----------------------------------------------------------------------

template <typename T> void OptionalInterestInc(T* ptr)          { if (ptr) ptr->IncInterestCount(); }
template <typename T> garbage_t OptionalInterestDec(T* ptr) noexcept { if (!ptr) return {};  return ptr->DecInterestCount(); }

//----------------------------------------------------------------------
// class  : InterestPtr
//----------------------------------------------------------------------

template <typename CPtr>
struct InterestPtr
{
	using IPtr = typename pointer_traits<CPtr>::ptr_type;
	using IRef = typename pointer_traits<CPtr>::ref_type;
	using IVal = typename pointer_traits<CPtr>::value_type;

	InterestPtr(std::nullptr_t = nullptr) noexcept 
		:	m_Item() // may be raw pointer that must be initialized
	{}

	InterestPtr(InterestPtr&& src) noexcept
		:	m_Item(std::move(src.m_Item)) 
	{
		src.m_Item = CPtr();
	}

	InterestPtr(const InterestPtr& src) noexcept
		: m_Item(src.m_Item)
	{
		OptionalInterestInc<IVal>(get_ptr());
	}

	template <typename T>
	InterestPtr(T* ptr)
		: m_Item(ptr)
	{
		OptionalInterestInc<IVal>(get_ptr());
	}
	template <typename T>
	InterestPtr(SharedPtr<T>&& item)
		: m_Item(std::move(item))
	{
		OptionalInterestInc<IVal>(get_ptr());
	}
	template <typename T>
	InterestPtr(const SharedPtr<T>& item)
		: m_Item(item)
	{
		OptionalInterestInc<IVal>(get_ptr());
	}

	template <typename SrcPtr>
	InterestPtr(InterestPtr<SrcPtr>&& rhs) noexcept
		: m_Item(std::move(rhs.m_Item))
	{
		rhs.m_Item = SrcPtr();
	}
	template <typename SrcPtr>
	InterestPtr(const InterestPtr<SrcPtr>& rhs) noexcept
		: m_Item(rhs.m_Item)
	{
		OptionalInterestInc<IVal>(get_ptr());
	}

	InterestPtr& operator =(InterestPtr&& rhs) noexcept
	{
		omni::swap(m_Item, rhs.m_Item);
		return *this;
	}

	void operator =(const InterestPtr& rhs) noexcept
	{
		InterestPtr rhsCopy(rhs);
		omni::swap(m_Item, rhsCopy.m_Item);        // this line shouldn't throw
		 // fullfill the promise to Decrement old m_Item by destructing temporary rhs
	}

	template <typename SrcPtr>
	void operator =(SrcPtr&& rhs)
	{
		InterestPtr rhsCopy(std::forward<SrcPtr>(rhs));
		omni::swap(m_Item, rhsCopy.m_Item);        // this line shouldn't throw
		 // fullfill the promise to Decrement old m_Item by destructing temporary rhs
	}
	void release() noexcept { m_Item = CPtr(); } // remove responsibility for decrement

	~InterestPtr() noexcept
	{
		OptionalInterestDec<IVal>(get_ptr());
	}

	explicit operator bool() const { return m_Item; }
	bool operator !() const { return !m_Item;  }
	template<
		typename U = CPtr,
		typename = typename std::enable_if< std::is_class<U>::value >::type
	>
	operator IPtr() const { return get_ptr(); }
	operator CPtr() const { return m_Item; }

	IPtr operator ->() const { dms_assert(m_Item != nullptr); return  get_ptr(); }
	IRef operator * () const { dms_assert(m_Item != nullptr); return *get_ptr(); }

	bool operator <  (const InterestPtr& right) const { return get_ptr() <  right.get_ptr(); }
	bool operator == (const InterestPtr& right) const { return get_ptr() == right.get_ptr(); }
	bool operator == (      IPtr      rightPtr) const { return get_ptr() == rightPtr; }

	bool has_ptr() const { return get_ptr() != nullptr; }
	bool is_null() const { return get_ptr() == nullptr; }

	IPtr get_ptr() const { return pointer_traits<CPtr>::get_ptr(m_Item); }
	void swap(InterestPtr& rhs) { omni::swap(m_Item, rhs.m_Item); }

private:
	CPtr m_Item;
	template<typename SrcPtr> friend struct InterestPtr;
};

//----------------------------------------------------------------------
// class  : Specific InterestPtrs
//----------------------------------------------------------------------

using SharedActorInterestPtr = InterestPtr<SharedPtr<const Actor> >;


#endif // __RTC_PTR_INTERESTHOLDER_H
