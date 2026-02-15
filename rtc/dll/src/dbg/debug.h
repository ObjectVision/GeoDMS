// Copyright (C) 1998-2026 Object Vision B.V. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif


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

#	define  DMS_START(cls, fnc, act) CDebugContextHandle debugContext(cls, fnc, act)
#	define  DMS_TRACE(s)             if (debugContext.m_Active) { MG_TRACE s; }
#	define  DMS_TEST(name, cond)     DMS_Test(name, #cond, cond) 

#if defined( MG_DEBUG )
#	define  DBG_START(cls, fnc, act) DMS_START(cls, fnc, act)
#	define  MGD_TRACE(s)             MG_TRACE(s)
#	define  DBG_TRACE(s)             DMS_TRACE(s)
#	define  DBG_TEST(name, cond)     DMS_TEST(name, cond) 
#else
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