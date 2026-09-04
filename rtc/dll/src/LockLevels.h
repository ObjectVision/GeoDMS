// Copyright (C) 1998-2026 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

#if !defined(__RTC_LOCKLEVELS_H)
#define __RTC_LOCKLEVELS_H

#define NOMINMAX

#include "Parallel.h"

using LockLevels = ord_level_type;

enum class ord_level_type : UInt32;

inline constexpr ord_level_type lowest_of(ord_level_type a, ord_level_type b) { return a < b ? a : b; }
inline constexpr ord_level_type highest_of(ord_level_type a, ord_level_type b) { return a > b ? a : b; }
inline constexpr int highest_of(int a, int b) { return a > b ? a : b; }

enum class ord_level_type : UInt32
{
	// #1233 P5: every section has its OWN ordinal since the second annotation wave. Two sections that share
	// an ordinal can never be nested in a Debug-covered path (Allow refuses equal levels), so their relative
	// order was never defined -- and a Release-only path nesting such a pair in both directions on two threads
	// deadlocks without a diagnostic. With distinct ordinals every pair has one checked order. The order within
	// a former family follows the nestings that are known (noted per entry); where none is known the order is
	// a choice, and the first Debug run that nests the pair the other way round will say so.
	// Gaps are left so a new section can be placed without renumbering.

	// outermost: the whole-session counter and the wms tile cache, taken with nothing else held
	SessionUsageCounter = 1,		// s_SessionUsageCounter (counted); raw try_lock_shared/unlock_shared, invisible to Allow
	WmsTileCache = 2,				// TileCache::s_ImageAccess (shv WmsLayer)

	// Not a section: the per-item ceiling a production WAIT declares (#1233 P2). Entered at item level 1 by the
	// item-counter waits, AwaitAncestorWrites, ReadLockInit and OperationContext::Join, so that a thread which
	// holds any global section -- a TokenStr's registry-shared usage above all -- is refused at the point where
	// it would start waiting on another thread's production. Its ordinal is immaterial (per-item levels are
	// never ordered); it sits here to say what it is.
	ItemProductionWait = 50,

	// geo / shv / stg / clc sections (former 93..95 families)
	BoundingBoxCache1 = 60,			// cs_BB: the bounding-box cache registry
	AbstrStorage = 61,
	SpecificOperatorGroup = 62,		// polygon-overlay insert sections
	DataViewQueue = 63,
	UpdateActionSet = 64,			// sm_UAS (shv GraphicObject)
	Storage = 65,					// s_OdbcSection
	BoundingBoxCache2 = 66,
	SpecificOperator = 68,			// cs_SpatialRefBlockCreation, polygon addition sections
	DataRefContainer = 69,			// s_DataItemRefContainer; inner to Storage as before

	// tile and item machinery (former 96..97)
	PrepareDataUsageLock = 70,		// cs_lock_map, per-item: outer to every global section (see level_type::Allow)
	TileShadow = 72,
	Tile = 73,						// a tile lock may schedule/join operation contexts: outer to ThreadMessing
	ItemRegister = 74,				// sg_ActorLockMap, per-item
	ThreadMessing = 75,				// cs_ThreadMessing; takes CountSection, FailSection and OperContextAccess inside
	DataFlagsLock = 76,				// sg_DataFlagsLockMap, per-item

	// the former 98 family; CountSection first because IncInterestCount takes it and the rest are leaves
	CountSection = 78,				// sg_CountSection
	FailSection = 79,				// sc_FailSection; takes MoveSupplInterest inside
	OperContextAccess = 80,			// cs_OperContextAccess; taken under ThreadMessing
	ActiveProducerSet = 81,
	TreeItemFlags = 82,
	GDALComponent = 83,				// gdalSection; the GDAL error handler may read tokens and report: outer to IndexedString

	// the former 99 family; IndexedString is the INNERMOST of them so that a token can be read under any of the
	// others, and none of them may be taken while a TokenStr is held
	UpdatingInterestSet = 85,		// sd_UpdatingInterestSet; taken under CountSection (#1233 P3)
	OperationContext = 86,			// cs_OcAdm
	TileAccessMap = 87,
	MoveSupplInterest = 88,			// sc_MoveSupplInterestSection; takes NotifyTargetCount inside
	ExplainAccess = 89,				// scs_ExplainAccess
	IndexedString = 90,				// the token registry (counted): shared to read, exclusive to register

	// the former 100 family; CountedMutexSection innermost because every counted_mutex op -- the registry's
	// and the session counter's -- takes it
	NotifyTargetCount = 92,			// sc_NotifyTargetCount; the TContextNotification callback runs under it
	RegisterAccess = 93,				// s_RegAccess
	LispObjCache = 94,
	CountedMutexSection = 95,		// s_CountedMutexSection

	// the former 101 pair
	ObjectRegister = 97,				// cs_ORT
	ItemCounter = 98,				// cs_lockCounterUpdate (tic/ItemLocks.cpp): the item production read/write counter

	DebugOutStream = 100,			// g_DebugStream
	OperationQueue = 101,

	// Not a section: the ceiling DMS_ENTERS_NOTHING declares. Inner to every real level, so every
	// real acquire is outer to it and therefore refused.
	EntersNothing = 0xFFFFFFFF,
};


//==============================================================
// Declared lock-level ceilings (#1227)
//==============================================================
//
// DMS_ENTERS(level, mode) declares the OUTERMOST lock level the enclosing scope may take, directly
// or through anything it calls -- with `mode` saying whether it may take that outermost level
// shared or exclusively. Everything inner to it (higher ordinal, see the enum above) stays
// allowed; anything outer is the ordering violation.
//
// It is not new bookkeeping: the declaration enters the same thread-local level a real acquire
// enters, and level_type::Allow does the checking. See Parallel.h for why declaring is what makes
// the ordering checkable across virtual and indirect calls at all, for the rule that a scope
// should only declare what it does not itself take, and for the two shapes this cannot express.
//
// Three rules, each checkable where it is written:
//
//  - Plain functions. A scope declaring DMS_ENTERS(L, mode) may take L itself at a mode no
//    stronger than declared, and anything inner to L; so it may only call functions whose own
//    declaration is L at that mode or inner. Declare exactly the outermost acquire made.
//  - Virtual functions. The annotation on the BASE declaration is the contract: the outermost
//    level any override is permitted to reach. An override may declare a stricter (inner) level
//    but never an outer one, and every call site is checked against the base. That is what keeps
//    the analysis from needing to know the dynamic type.
//  - Function pointers and std::function. The ceiling belongs to the PARAMETER, not to the
//    callee: DMS_CALLEE_ENTERS on the parameter is what the caller's lambda is checked against,
//    and the callee is checked against its own. Neither side needs to know the other. The same
//    macro annotates a virtual on its base declaration and any opaque plain function; its
//    sibling DMS_CALLEE_ENTERS_NOTHING claims the callee takes no leveled lock at all. Both
//    live in RtcBase.h (they expand to nothing and need none of this header), so leaf headers
//    can carry the contract without the include.
//
// DMS_ENTERS expands to a Debug-only scope object; a Release build sees nothing. The
// DMS_CALLEE_ENTERS pair (RtcBase.h) expands to nothing at all -- no compiler we build with can
// check a declaration annotation, so it records the contract for the reader and for the syntactic
// pass over statically resolvable call edges that is the other half of the scheme. What keeps a
// written contract honest today is the caller's DMS_ENTERS: an annotated callee that lies is
// caught the first time it runs under a ceiling in Debug.

// Spelled out at the call site so that a reader sees which mode is meant, rather than a bare bool.
inline constexpr bool dms_shared_v = true;
inline constexpr bool dms_exclusive_v = false;

#if defined(MG_DEBUG_LOCKLEVEL)

#define DMS_ENTERS_STR2(x) #x
#define DMS_ENTERS_STR(x) DMS_ENTERS_STR2(x)
#define DMS_ENTERS(LEVEL, MODE) DmsLockCeiling mg_lock_ceiling_(LEVEL, MODE, "DMS_ENTERS(" #LEVEL ") at " __FILE__ ":" DMS_ENTERS_STR(__LINE__))
// For a scope whose outermost acquire is a per-item lock (cs_lock_map); see DmsLockCeiling.
#define DMS_ENTERS_ITEM(LEVEL, MODE) DmsLockCeiling mg_lock_ceiling_(LEVEL, MODE, "DMS_ENTERS_ITEM(" #LEVEL ") at " __FILE__ ":" DMS_ENTERS_STR(__LINE__), item_level_type(1))

#else

#define DMS_ENTERS(LEVEL, MODE) ((void)0)
#define DMS_ENTERS_ITEM(LEVEL, MODE) ((void)0)

#endif

// Declares that the enclosing scope takes no lock level at all.
#define DMS_ENTERS_NOTHING DMS_ENTERS(ord_level_type::EntersNothing, dms_exclusive_v)

// DMS_CALLEE_ENTERS / DMS_CALLEE_ENTERS_NOTHING: see RtcBase.h.

//==============================================================

#endif // __RTC_LOCKLEVELS_H
