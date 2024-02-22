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
	void Release() const // dtor of Object is virtual, so destructing from here is OK
	{
		if (!DecRef())
			delete this;
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
