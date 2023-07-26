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

#if !defined(__RTC_UTL_SWAPPER_H)
#define __RTC_UTL_SWAPPER_H

//----------------------------------------------------------------------
// used modules and forward class references
//----------------------------------------------------------------------

#include <boost/noncopyable.hpp>
#include "utl/swap.h"
#include "geo/ElemTraits.h"

//----------------------------------------------------------------------
// mt_swap(per) helps to do thread safe resource swapping/locking
//----------------------------------------------------------------------

template <class T>
void mt_swap(T& a, T& b)
{
//	using namespace std::swap;
//	NYI: Mutex (mutally exclusion around swap function
	omni::swap(a, b);
//	T tmp = a; a=b; b=tmp;
}

template <class T>
struct mt_swapper : private boost::noncopyable
{
	 mt_swapper(T& resourceHandle, const T& blockValue)
		: m_ResourceHandleRef(resourceHandle)
		, m_ResourceHandleCopy(blockValue) 
	 { 
		 mt_swap(m_ResourceHandleRef, m_ResourceHandleCopy); 
	 }
	~mt_swapper()
	 { 
		 mt_swap(m_ResourceHandleRef, m_ResourceHandleCopy); 
	 }

	 bool     WasBlocked() const { return m_ResourceHandleRef == m_ResourceHandleCopy; }
	 const T& GetValue  () const { return m_ResourceHandleCopy; }

protected:
	T& m_ResourceHandleRef;
	T  m_ResourceHandleCopy;
};

template <class T>
struct tmp_swapper  : private boost::noncopyable
{
	tmp_swapper(T& resourceHandle, typename param_type<T>::type tmpValue)
		:	m_ResourceHandleRef (resourceHandle)
		,	m_ResourceHandleCopy(resourceHandle) 
	 { 
		 resourceHandle = tmpValue;
	 }
	~tmp_swapper() // restore old situation
	 { 
		m_ResourceHandleRef = m_ResourceHandleCopy;
	 }

	using param_t = typename param_type<T>::type;

	param_t PreviousValue() const { return m_ResourceHandleCopy; }

	T& GetRef() { return m_ResourceHandleRef; }

protected:
	T& m_ResourceHandleRef;
	T  m_ResourceHandleCopy;
};

#endif // __RTC_UTL_SWAPPER_H
