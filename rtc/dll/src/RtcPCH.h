// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#pragma once

#if !defined(__RTC_PCH)
#define __RTC_PCH

#if !defined(DMRTC_EXPORTS) && !defined(DMRTC_STATIC)
#pragma message("RtcPCH.h included without DMRTC_EXPORTS or DMRTC_STATIC defined; is this included from the right code-unit?")
#define DMRTC_EXPORTS
#endif

#include "RtcBase.h"
#include "dbg/Check.h"
#include "dbg/debug.h"

//----------------------------------------------------------------------
// RtcLock
//----------------------------------------------------------------------

struct ElemAllocComponent
{
	ElemAllocComponent();
	~ElemAllocComponent();
};

struct RtcStreamLock : ElemAllocComponent
{
	RtcStreamLock();
	~RtcStreamLock();
};

struct IndexedStringsComponent: ElemAllocComponent
{
	IndexedStringsComponent();
   ~IndexedStringsComponent();
};

struct TokenComponent : IndexedStringsComponent
{
	TokenComponent();
   ~TokenComponent();
};

struct RtcReportLock : RtcStreamLock, TokenComponent
{
	RtcReportLock();
   ~RtcReportLock();
};

//----------------------------------------------------------------------
// Section      : IString, used for returning string-handles to ClientAppl
//----------------------------------------------------------------------

#if defined(MG_DEBUG)

struct IStringComponentLock : RtcStreamLock
{
	RTC_CALL IStringComponentLock();
	RTC_CALL ~IStringComponentLock();
};


#endif

//----------------------------------------------------------------------


#endif // __RTC_PCH
