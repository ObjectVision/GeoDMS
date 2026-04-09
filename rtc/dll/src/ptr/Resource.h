// Copyright (C) 1998-2026 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

#if !defined(__RTC_PTR_RESOURCE_H)
#define __RTC_PTR_RESOURCE_H

#include "ptr/OwningPtr.h"

template <typename R> struct Resource;

struct ResourceBase
{
	virtual ~ResourceBase() = 0;

	template <typename R>
	bool IsA() const
	{
		return dynamic_cast<const Resource<R>*>(this);
	};

	template <typename R>
	R& GetAs()
	{
		return debug_refcast<Resource<R>&>(*this);
	};

	template <typename R>
	R* GetOptional()
	{
		return debug_cast<Resource<R>*>(this);
	};


	template <typename R>
	const R& GetAs() const 
	{
		return debug_refcast<const Resource<R>&>(*this);
	};

	template <typename R>
	const R* GetOptional() const
	{
		return debug_cast<const Resource<R>*>(this);
	};


};

inline ResourceBase::~ResourceBase() {}

template <typename R>
struct Resource : ResourceBase, R
{
	template <typename... Args>
	Resource(Args&& ...args)
		:	R(std::forward<Args>(args)...)
	{}
};


using ResourceHandle = std::unique_ptr<ResourceBase>;


template <typename R>
ResourceHandle makeResource(R&& res)
{
	return std::make_unique<Resource<R>>(std::forward<R>(res));
}

inline ResourceHandle makeResource(ResourceHandle&& res)
{
	return std::move(res);
}

template <typename R, typename... Args>
ResourceHandle makeResource(Args&& ...args)
{
	return ResourceHandle(new Resource<R>(std::forward<Args>(args)...));
}

inline void operator <<=(ResourceHandle& oldResource, ResourceHandle&& newResource)
{
	if (!oldResource)
		oldResource = std::move(newResource);
	else if (newResource)
		oldResource = makeResource<std::pair<ResourceHandle, ResourceHandle>>(std::move(oldResource), std::move(newResource));
}

template <typename R>
void operator <<=(ResourceHandle& oldResource, R&& res)
{
	operator <<=(oldResource, makeResource<R>(std::forward<R>(res)));
}

template <typename R>
R& GetAs(ResourceBase* rh)
{
	assert(rh);
	return rh->GetAs<R>();
}

template <typename R>
R& GetAs(ResourceHandle& rh)
{
	assert(rh);
	return rh->GetAs<R>();
}

template <typename R>
const R& GetAs(const ResourceHandle& rh)
{
	assert(rh);
	return rh->GetAs<R>();
}

template <typename R>
const R& GetAs(const ResourceBase* rh)
{
	assert(rh);
	return rh->GetAs<R>();
}

template <typename R>
R& MakeAs(ResourceHandle& rh)
{
	if (!rh || !rh->IsA<R>())
		rh.reset(new Resource<R>);
	return rh->GetAs<R>();
}

template <typename R>
R* GetOptional(ResourceBase* rh)
{
	return rh->GetOptional<R>();
}

template <typename R>
R* GetOptional(ResourceHandle& rh)
{
	return rh->GetOptional<R>();
}

template <typename R>
const R* GetOptional(const ResourceHandle& rh)
{
	return rh->GetOptional<R>();
}

#endif // __RTC_PTR_RESOURCE_H
