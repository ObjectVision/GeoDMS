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

#if !defined(__RTC_DBG_DEBUGCAST_H)
#define __RTC_DBG_DEBUGCAST_H

//----------------------------------------------------------------------
// used modules and forward class references
//----------------------------------------------------------------------

#include <typeinfo>

#include "dbg/Check.h"

//----------------------------------------------------------------------
// Typesafe cast
//----------------------------------------------------------------------

template <typename T> inline T typesafe_cast (T x) { return x; }
template <typename T> inline T& volatile_cast (volatile T& x) { return const_cast<T&>(x); }


//----------------------------------------------------------------------
// checked_cast; throws an exception when dynamic cast doesn't apply
//----------------------------------------------------------------------

template <typename DerivedPtr, typename Base> 
inline DerivedPtr checked_cast(Base* basePtr) 
{
//	typename raw_ptr<DerivedPtr>::type result = dynamic_cast<typename raw_ptr<DerivedPtr>::type>(basePtr);
	DerivedPtr result = dynamic_cast<DerivedPtr>(basePtr);
	if (result != basePtr )  // detect logic error and compile time check suspected cross-casts or inpossible casts
	{
		dms_assert(basePtr);
		dms_assert(!result);
		throwErrorF("checked_cast", "cannot be casted to type %s", typeid(DerivedPtr).name());
	}
    return result;
}

template <typename DerivedPtr, typename Base> 
inline DerivedPtr checked_valcast(Base* basePtr) 
{
	//	it takes a ptr and has "val" in the name to indicate it doesn't accept NULL pointers
	//
	// prefer this cast over checked_cast if you directly access the derived class object
	//	while assuming the basePtr != NULL; which is checked here

	if (basePtr == nullptr) // we don't accept refs to NULL
		throwErrorD("checked_valcast", "Unexpected nullptr");
	return checked_cast<DerivedPtr>(basePtr);
}

template <typename DerivedRef, typename Base> 
inline DerivedRef checked_refcast(Base& baseRef) 
{
	// polymorphic_cast okays casting NULL to NULL; checked_refcast doesn't; 
	//	it takes a ref and has "ref" in the name to avoid technicalities.

	Base* basePtr = &baseRef; // trap some '&' overloading.
	return *(checked_valcast<std::remove_reference_t<DerivedRef>*>(basePtr));
}

//----------------------------------------------------------------------
// Debug cast 
//	use only for known downcasts
//----------------------------------------------------------------------


template <typename DerivedPtr, typename Base> 
inline DerivedPtr debug_cast(Base* basePtr) 
{
	// polymorphic_downcast okays casting NULL to NULL; as dynamic_cast for pointers does
//	dms_assert( dynamic_cast<typename raw_ptr<DerivedPtr>::type>(basePtr) == basePtr );  // detect logic error
//	return static_cast<typename raw_ptr<DerivedPtr>::type>(basePtr);
	dms_assert( dynamic_cast<DerivedPtr>(basePtr) == basePtr );  // detect logic error
    return static_cast<DerivedPtr>(basePtr);
}

template <typename Derived, typename Base>
inline std::shared_ptr<Derived> debug_pointer_cast(std::shared_ptr<Base> basePtr)
{
	// polymorphic_downcast okays casting NULL to NULL; as dynamic_cast for pointers does
//	dms_assert( dynamic_cast<typename raw_ptr<DerivedPtr>::type>(basePtr) == basePtr );  // detect logic error
//	return static_cast<typename raw_ptr<DerivedPtr>::type>(basePtr);
	dms_assert(std::dynamic_pointer_cast<Derived>(basePtr) == basePtr);  // detect logic error
	return std::static_pointer_cast<Derived>(basePtr);
}

template <typename DerivedPtr, typename Base>
inline DerivedPtr debug_valcast(Base* basePtr) 
{
	//	it takes a ptr and has "val" in the name to indicate it doesn't accept NULL pointers
	//
	// prefer this cast over debug_cast if you directly access the derived class object
	//	while assuming the basePtr != NULL; which is checked here

	dms_assert(basePtr != nullptr); // we don't accept refs to nullptr
	return debug_cast<DerivedPtr>(basePtr);
}

template <typename DerivedRef, typename Base> 
inline DerivedRef debug_refcast(Base& baseRef) 
{
	// polymorphic_downcast okays casting NULL to NULL; debug_refcast doesn't; 
	//	it takes a ref and has "ref" in the name to avoid technicalities.

	Base* basePtr = &baseRef; // trap some '&' overloading.
	return *(debug_valcast<std::remove_reference_t<DerivedRef>*>(basePtr));
}


#endif // __RTC_DBG_DEBUGCAST_H
