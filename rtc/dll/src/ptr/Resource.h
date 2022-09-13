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

#if !defined(__RTC_PTR_RESOURCE_H)
#define __RTC_PTR_RESOURCE_H

#include "ptr/OwningPtr.h"

template <typename R> struct Resource;

struct ResourceBase
{
	virtual ~ResourceBase() = 0
	{}

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

template <typename R>
struct Resource : ResourceBase, R
{
	template <typename... Args>
	Resource(Args&& ...args)
		:	R(std::forward<Args>(args)...)
	{}
};


using ResourceHandle = OwningPtr<ResourceBase>;


template <typename R>
ResourceHandle makeResource(R&& res)
{
	return new Resource<R>(std::forward<R>(res));
}

inline ResourceHandle makeResource(ResourceHandle&& res)
{
	return std::move(res);
}

template <typename R, typename... Args>
ResourceHandle makeResource(Args&& ...args)
{
	return new Resource<R>(std::forward<Args>(args)...);
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

/* REMOVE
template <typename R, typename... Args>
ResourceHandle  operator <<=(ResourceHandle&& oldResource, Args&& ...args)
{
	return  operator <<=(std:move(oldResource), makeResource<R>(std::forward<Args>(args)...));
}
*/

template <typename R>
R& GetAs(ResourceBase* rh)
{
	dms_assert(rh);
	return rh->GetAs<R>();
}

template <typename R>
const R& GetAs(const ResourceBase* rh)
{
	dms_assert(rh);
	return rh->GetAs<R>();
}

template <typename R>
R* GetOptional(ResourceBase* rh)
{
	return rh->GetOptional<R>();
}

#endif // __RTC_PTR_RESOURCE_H
