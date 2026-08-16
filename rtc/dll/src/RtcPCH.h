// Copyright (C) 1998-2024 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

#if !defined(__RTC_PCH)
#define __RTC_PCH

#if !defined(DMRTC_EXPORTS) && !defined(DMRTC_STATIC)
#pragma message("RtcPCH.h included without DMRTC_EXPORTS or DMRTC_STATIC defined; is this included from the right code-unit?")
#define DMRTC_EXPORTS
#endif

#include "RtcBase.h"
#include "dbg/Diagnostics.h"
#include "dbg/debug.h"
#include "sym/Token.h"

//----------------------------------------------------------------------
// RtcLock
//
// ElemAllocComponent / IndexedStringsComponent / TokenComponent and StaticTokenID moved to
// RtcComponents.h resp. sym/Token.h (both included via dbg/Diagnostics.h -> sym/Token.h above) so that
// tic/sym can derive their static objects from the exported components ahead of the DLL merge.
//----------------------------------------------------------------------

struct RtcStreamLock : ElemAllocComponent
{
	RtcStreamLock();
	~RtcStreamLock();
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
