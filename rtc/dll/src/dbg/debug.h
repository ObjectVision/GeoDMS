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

#if !defined(__DBG_DEBUG_H)
#define __DBG_DEBUG_H

//----------------------------------------------------------------------
// used modules and forward class references
//----------------------------------------------------------------------

#include "ser/format.h"
#include "dbg/DebugContext.h"


/********** Various debug related defined **********/


template<typename ...Args>
void MG_TRACE(CharPtr msg, Args&&... args)
{
	DBG_TraceStr(mgFormat2string<Args...>(msg, std::forward<Args>(args)...).c_str());
}

#	define  DMS_INIT_COUT            CDebugCOutHandle    debugCout
#	define  DMS_START(cls, fnc, act) CDebugContextHandle debugContext(cls, fnc, act)
#	define  DMS_TRACE(s)             if (debugContext.m_Active) { MG_TRACE s; }
#	define  DMS_TEST(name, cond)     DMS_Test(name, #cond, cond) 

#if defined( MG_DEBUG )
#	define  DBG_INIT_COUT            DMS_INIT_COUT
#	define  DBG_START(cls, fnc, act) DMS_START(cls, fnc, act)
#	define  MGD_TRACE(s)             MG_TRACE(s)
#	define  DBG_TRACE(s)             DMS_TRACE(s)
#	define  DBG_TEST(name, cond)     DMS_TEST(name, cond) 
#else
#	define  DBG_INIT_COUT
#	define  DBG_START(a, b, act)
#	define	MGD_TRACE(s)
#	define  DBG_TRACE(s)
#	define  DBG_TEST(name, cond)     true 
#endif

#if defined( MG_DEBUG_DATA )
#	define _DBG_TRACE_DATA(s)        MGD_TRACE(s) 
#else
#	define _DBG_TRACE_DATA(s)
#endif

#if defined(MG_DEBUG_DATA)
#	define MGD_TRACEDATA(Msg)        MGD_TRACE(Msg)
#else
#	define MGD_TRACEDATA(Msg)
#endif


#endif // __DBG_DEBUG_H