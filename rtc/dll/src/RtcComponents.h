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
// and sym DLLs could derive from them. After the rtc+sym+tic merge all deriving
// statics live inside DmRtc (dumpbin 2026-08: no downstream binary imported
// these), so the 2026-08 de-export pass removed the decoration.
//
// A module OUTSIDE DmRtc does not need to hold a component at all: it links
// against Rtc.dll, so the loader runs Rtc's own initializers -- including the
// components DmRtc constructs -- before any initializer in the dependent module.
// Such a static gets its subsystem for free and uses the Late* variant (see
// StaticLateTokenID in sym/Token.h), which only verifies that guarantee in Debug
// via IsUp() below. That keeps these ctors/dtors unexported.
//----------------------------------------------------------------------

#include "RtcBase.h"

// These are Schwarz counters: the first instance brings its subsystem up, the
// last one takes it down. That accounting only holds when EVERY live instance
// owns exactly one reference, so a copy must count as a new instance. The
// implicitly generated copy constructor does NOT increment, so copying a
// component-derived object (e.g. `auto id = someStaticTokenID;`, which deduces
// StaticTokenID rather than TokenID) used to hand out an unowned reference and
// the eventual destructor drove the count one too low -- tearing the subsystem
// down while other instances were still alive. The explicit copy/move ctors
// below delegate to the default one, so each instance holds its own reference.
struct ElemAllocComponent
{
	ElemAllocComponent();
	ElemAllocComponent(const ElemAllocComponent&) : ElemAllocComponent() {}
	ElemAllocComponent(ElemAllocComponent&&) noexcept : ElemAllocComponent() {}
	ElemAllocComponent& operator =(const ElemAllocComponent&) { return *this; } // already holds a reference
	ElemAllocComponent& operator =(ElemAllocComponent&&) noexcept { return *this; }
	~ElemAllocComponent();
};

struct IndexedStringsComponent : ElemAllocComponent
{
	IndexedStringsComponent();
	IndexedStringsComponent(const IndexedStringsComponent&) : IndexedStringsComponent() {}
	IndexedStringsComponent(IndexedStringsComponent&&) noexcept : IndexedStringsComponent() {}
	IndexedStringsComponent& operator =(const IndexedStringsComponent&) { return *this; }
	IndexedStringsComponent& operator =(IndexedStringsComponent&&) noexcept { return *this; }
	~IndexedStringsComponent();
};

struct TokenComponent : IndexedStringsComponent
{
	TokenComponent();
	TokenComponent(const TokenComponent&) : TokenComponent() {}
	TokenComponent(TokenComponent&&) noexcept : TokenComponent() {}
	TokenComponent& operator =(const TokenComponent&) { return *this; }
	TokenComponent& operator =(TokenComponent&&) noexcept { return *this; }
	~TokenComponent();

#if defined(MG_DEBUG)
	// exported: the Debug-only precondition check of StaticLateTokenID, which lives
	// in stg, stx and qtgui. True once any TokenComponent is alive, i.e. once the
	// token subsystem is up. Not a substitute for holding a component: a static
	// INSIDE DmRtc must still derive from TokenComponent, because its own
	// initializer may run before DmRtc's.
	RTC_CALL static bool IsUp();
#endif
};

#endif // __RTC_COMPONENTS_H
