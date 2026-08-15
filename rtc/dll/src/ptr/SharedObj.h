// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

//  -----------------------------------------------------------------------
//  Name        : SharedObj.h
//  Description : SharedObj is a possible base class for objects that are
//                referred to by SharedPtr.
//                It offers RefCount() and AddRef(), 
//                but not Release(), which should be implemented
//                by descending class since SharedObj has no
//                virtual calls and therefore no virtual dtor to 
//                allow a descending class to be non-polymorphic
//	Note:         When your class is polymorphic (has a virtual dtor),
//                derive from PersistentSharedObj
//  -----------------------------------------------------------------------

#if defined(_MSC_VER)
#pragma once
#endif

#if !defined(__RTC_PTR_SHAREDOBJ_H)
#define __RTC_PTR_SHAREDOBJ_H

#include "mci/Object.h"
#include "ptr/SharedBase.h"

template <typename VBase>
struct SharedObjWrap : VBase, SharedBase
{
	using base_type = VBase;
	using this_type = SharedObjWrap<VBase>;
	using VBase::VBase;

	virtual ~SharedObjWrap() noexcept = default;

	friend std::default_delete<this_type>;

	virtual void Release() const  noexcept // dtor of Object is virtual, so destructing from here is OK
	{
		MG_CHECK(!IsOwned());
		delete this;
	}
};

using SharedObj = SharedObjWrap<Object>;

template <typename T>
class SharedThing final : public SharedBase
{
public:
	template <typename ...Args>
	SharedThing(Args&& ...args)
		: thing(std::forward<Args>(args)...)
	{}
	T thing;

	void Release() const  noexcept // SharedThing is final, so destructing from here without virtual dtor is OK
	{
		MG_CHECK(!IsOwned());
		delete this;
	}
};

template <typename T, typename ...Args>
auto make_SharedThing(Args&& ...args) -> SharedPtr<SharedThing<T>>
{ 
	auto* pointer_to_shared_thing = new SharedThing<T>( std::forward<Args>(args)... );
	return { pointer_to_shared_thing, newly_obj{} };
}

template <typename T>
using SharedThingPtr = SharedPtr<SharedThing<T>>;

#endif // __RTC_PTR_SHAREDOBJ_H
