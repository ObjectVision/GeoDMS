// Copyright (C) 1998-2026 Object Vision B.V. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif


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
		assert(basePtr);
		assert(!result);
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
	MG_CHECK( dynamic_cast<DerivedPtr>(basePtr) == basePtr );  // detect logic error
    return static_cast<DerivedPtr>(basePtr);
}

template <typename DerivedPtr, typename Base>
inline auto debug_cast(OwningPtr<Base> basePtr) -> OwningPtr<typename pointer_traits<DerivedPtr>::value_type>
{
	// polymorphic_downcast okays casting NULL to NULL; as dynamic_cast for pointers does
	MG_CHECK(dynamic_cast<DerivedPtr>(basePtr.get()) == basePtr.get());  // detect logic error
	return static_cast<DerivedPtr>(basePtr.release());
}

template <typename DerivedPtr, typename Base>
inline auto debug_cast(std::unique_ptr<Base> basePtr) -> std::unique_ptr<typename pointer_traits<DerivedPtr>::value_type>
{
	// polymorphic_downcast okays casting NULL to NULL; as dynamic_cast for pointers does
	MG_CHECK(dynamic_cast<DerivedPtr>(basePtr.get()) == basePtr.get());  // detect logic error
	return std::unique_ptr<typename pointer_traits<DerivedPtr>::value_type>( static_cast<DerivedPtr>(basePtr.release()) );
}

template <typename Derived, typename Base>
inline std::shared_ptr<Derived> debug_pointer_cast(std::shared_ptr<Base> basePtr)
{
	// polymorphic_downcast okays casting NULL to NULL; as dynamic_cast for pointers does
	MG_CHECK(std::dynamic_pointer_cast<Derived>(basePtr) == basePtr);  // detect logic error
	return std::static_pointer_cast<Derived>(basePtr);
}

template <typename DerivedPtr, typename Base>
inline DerivedPtr debug_valcast(Base* basePtr) 
{
	//	it takes a ptr and has "val" in the name to indicate it doesn't accept NULL pointers
	//
	// prefer this cast over debug_cast if you directly access the derived class object
	//	while assuming the basePtr != NULL; which is checked here

	MG_CHECK(basePtr != nullptr); // we don't accept refs to nullptr
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
