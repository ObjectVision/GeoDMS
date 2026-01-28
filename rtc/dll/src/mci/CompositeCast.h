// Copyright (C) 1998-2026 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

#ifndef __RTC_MCI_COMPOSITECAST_H
#define __RTC_MCI_COMPOSITECAST_H

#include "dbg/Check.h"
#include "dbg/DebugCast.h"
#include "ptr/PtrBase.h"

template <typename T> struct SharedPtr;

//**********   pointer destillation **********

template <typename P> 
auto AsPtr(P const& ptr) -> typename raw_ptr<P>::type
{
	return pointer_traits<P>::get_ptr(ptr);
}

//********** DMS specific casts **********

class AbstrDataItem;
template <typename V> class Unit;
template <typename V> struct TileFunctor;
template <typename V> using DataArray = TileFunctor<V>; // TODO G8: SUBSTITUTE AWAY

template<typename V, typename P> const Unit<V>*
const_unit_cast(P const& ptr)
{
	return debug_valcast<const Unit<V>*>( AsPtr(ptr) );
}

template<typename V, typename P> const Unit<V>*
const_unit_dynacast(P const& ptr)
{
	return dynamic_cast<const Unit<V>*>( AsPtr(ptr) );
}

template<typename V, typename P> const Unit<V>*
const_unit_checkedcast(P const& ptr)
{
	return checked_cast<const Unit<V>*>( AsPtr(ptr) );
}

template<typename V, typename P> Unit<V>*
mutable_unit_cast(P const& ptr)
{
	return debug_valcast<Unit<V>*>(AsPtr(ptr));
}

template<typename V, typename T> 
auto mutable_unit_cast(OwningPtr<T>&& ptr) -> OwningPtr<Unit<V>>
{
	auto result = OwningPtr<Unit<V>>(debug_valcast<Unit<V>*>(ptr.get()));
	ptr.release();
	return result;
}

template<typename V, typename T>
auto mutable_unit_cast(const OwningPtr<T>& ptr) -> OwningPtr<void>;


template<typename V, typename P> Unit<V>*
mutable_unit_dynacast(P const& ptr)
{
	return dynamic_cast<Unit<V>*>(AsPtr(ptr));
}

template<typename V, typename P> Unit<V>*
mutable_unit_checkedcast(P const& ptr)
{
	return checked_cast<Unit<V>*>( AsPtr(ptr) );
}

template<typename V, typename P> 
auto const_array_cast(const AbstrDataObject* ptr) -> const TileFunctor<V>*
{
	return debug_valcast<const TileFunctor<V>*>(ptr);
}

template<typename V, typename P> 
auto const_array_cast(P const& ptr) -> const TileFunctor<V>*
{
	return const_array_cast<V>(
		debug_valcast<const AbstrDataItem*>(AsPtr(ptr))->GetCurrRefObj().get()
	);
}

template<typename V, typename P> const TileFunctor<V>*
const_opt_array_cast(P const& ptr)
{
	return ptr != nullptr ? const_array_cast<V, P>(ptr) : nullptr;
}

template<typename V, typename P>
auto const_array_dynacast(P const& ptr) -> const TileFunctor<V>*
{
	return dynamic_cast<const TileFunctor<V>*>(
		debug_valcast<const AbstrDataItem*>(AsPtr(ptr))->GetCurrRefObj().get()
	);
}

template<typename V, typename P>
auto const_array_checked_cast(P const& ptr) -> const TileFunctor<V>*
{
	return checked_cast<const TileFunctor<V>*>(
		checked_valcast<const AbstrDataItem*>(AsPtr(ptr))->GetCurrRefObj().get()
	);
}

template<typename V, typename P> 
auto const_opt_array_checkedcast(P const& ptr) -> const TileFunctor<V>*
{
	return (ptr == nullptr)
		?	nullptr
		:	const_array_checked_cast<V, P>(ptr);
}

template<typename V, typename P>
auto mutable_array_cast(P const& ptr) -> TileFunctor<V>*
{
	return debug_valcast<TileFunctor<V>*>(
		debug_valcast<AbstrDataItem*>(AsPtr(ptr))->GetDataObj().get()
	);
}

template<typename V, typename P>
auto mutable_array_dynacast(P const& ptr) -> TileFunctor<V>*
{
	return dynamic_cast<TileFunctor<V>*>(
		debug_valcast<AbstrDataItem*>(AsPtr(ptr))->GetDataObj().get()
	);
}

template<typename V, typename P> 
auto mutable_array_checkedcast(P const& ptr) -> TileFunctor<V>*
{
	return checked_cast<TileFunctor<V>*>(
		checked_valcast<AbstrDataItem*>(AsPtr(ptr))->GetDataObj().get()
	);
}


// ======================================================================
//	template <typename T> struct composite_cast provides the right access to type specific objects.
// ======================================================================
// use partial template class specialisation to choose the right strategy
//	The resulting class pretents to be a simple pointer
/* REMOVE
template <typename T> struct composite_cast;

template <typename V> 
struct composite_cast<DataArray<V>*> : ptr_base<DataArray<V>, copyable >
{
	using base_type = ptr_base<DataArray<V>, copyable >;
	template <typename P>
		composite_cast(P ptr)
		:	base_type(mutable_array_cast<V>(ptr))
	{}
};

template <typename V> 
struct composite_cast<Unit<V>*> : ptr_base<Unit<V>, copyable >
{
	using base_type = ptr_base<Unit<V>, copyable >;
	template <typename P>
		composite_cast(P ptr)
		:	base_type(mutable_unit_cast<V>(ptr))
	{}
};

template <typename V> 
struct composite_cast<const DataArray<V>*> : ptr_base<const DataArray<V>, copyable >
{
	using base_type = ptr_base<const DataArray<V>, copyable >;
	template <typename CP>
		composite_cast(CP ptr)
		:	base_type(const_array_cast<V>(ptr))
	{}
};

template <typename V> 
struct composite_cast<const Unit<V>*> : ptr_base<const Unit<V>, copyable >
{
	using base_type = ptr_base<const Unit<V>, copyable >;
	template <typename CP>
		composite_cast(CP ptr)
		:	base_type(const_unit_cast<V>(ptr))
	{}
};
*/

#endif // __RTC_MCI_COMPOSITECAST_H
