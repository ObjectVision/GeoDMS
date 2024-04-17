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

class SharedObj : public Object,  public SharedBase
{	
protected:
	SharedObj() = default;
	virtual ~SharedObj() noexcept = default;

public:
	friend std::default_delete<SharedObj>;

	void Release() const // dtor of Object is virtual, so destructing from here is OK
	{
		if (DecRef())
			return;
		delete this;
	}
	auto DelayedRelease() -> zombie_destroyer // dtor of Object is virtual, so destructing from here is OK
	{
		if (DecRef())
			return {};
		return zombie_destroyer( this );
	}
};

struct TileBase : SharedObj
{
	RTC_CALL virtual void DecoupleShadowFromOwner();
};

template <typename T>
class SharedThing : public SharedObj
{
public:
	template <typename ...Args>
	SharedThing(Args&& ...args)
		: thing(std::forward<Args>(args)...)
	{}
	T thing;
};

template <typename T>
SharedPtr<SharedThing<T>> make_SharedThing(T&& thing) 
{ 
	auto* pointer_to_shared_thing = new SharedThing<T>( std::forward<T>(thing) );
	return pointer_to_shared_thing;
}

#endif // __RTC_PTR_SHAREDOBJ_H
