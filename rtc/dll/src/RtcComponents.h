// Copyright (C) 1998-2026 Object Vision b.v.
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

#if !defined(__RTC_COMPONENTS_H)
#define __RTC_COMPONENTS_H

//----------------------------------------------------------------------
// RTC lifecycle components
//
// Each component's constructor brings a subsystem up (ref-counted, idempotent)
// and its destructor tears it down when the last user goes. Deriving a
// statically-initialized object from the component it depends on guarantees the
// subsystem is constructed first, regardless of cross-translation-unit static
// initialization order -- which is undefined within a single DLL. See
// StaticTokenID in set/Token.h for the canonical example.
//
// The ctors/dtors were RTC_CALL-exported so objects in the then-separate tic
// and sym DLLs could derive from them. Since the rtc+sym+tic merge all deriving
// statics live inside DmRtc (dumpbin 2026-08: no downstream binary imports
// these), so the export decoration is only kept until the de-export pass of
// doc/development/tu-reorg-and-export-surface-2026-08.md removes it.
//----------------------------------------------------------------------

#include "RtcBase.h"

struct ElemAllocComponent
{
	RTC_CALL ElemAllocComponent();
	RTC_CALL ~ElemAllocComponent();
};

struct IndexedStringsComponent : ElemAllocComponent
{
	RTC_CALL IndexedStringsComponent();
	RTC_CALL ~IndexedStringsComponent();
};

struct TokenComponent : IndexedStringsComponent
{
	RTC_CALL TokenComponent();
	RTC_CALL ~TokenComponent();
};

#endif // __RTC_COMPONENTS_H
