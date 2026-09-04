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
	MOST_INNER_LOCK = 99,
	MOST_MOST_INNER_LOCK = MOST_INNER_LOCK + 1,

	RegisterAccess = MOST_MOST_INNER_LOCK,
	CountedMutexSection = MOST_MOST_INNER_LOCK,

//	IndexedString = MOST_INNER_LOCK - 1,
	ActiveProducerSet = MOST_INNER_LOCK - 1,
//	TileAccessMap = CountSection + 1, // can be used in SymbObjSection and in CountSection

	TreeItemFlags = MOST_INNER_LOCK - 1,
	ItemRegister = MOST_INNER_LOCK - 2,
	// The three cs_lock_map instances (#1233). All three live in the per-item dimension of
	// level_type::Allow, where an item level >= 1 makes each of them outer to every global section
	// and per-item locks are not ordered against each other at all -- so today these ordinals are
	// never compared. Kept distinct anyway: they record the one same-item nesting that is known,
	// PrepareDataUsage(X) enclosing DataWriteLockAtom(X), for a finer rule to build on.
	// ItemRegister itself is the actor lock map.
	PrepareDataUsageLock = ItemRegister - 1,
	DataFlagsLock = ItemRegister + 1,
	OperationContext = MOST_INNER_LOCK,
//	UpdatingInterestSet moved to CountSection + 1 below (#1233 P3): it used to share CountSection's
//	ordinal, so InterestReporter::Report -- which holds CountSection and then takes this one -- had
//	to silence the checker with a LevelCheckBlocker to nest two equal levels. A silenced checker
//	cannot see a thread nesting the pair the other way round, which is the AB/BA it was hiding.
//	All six sections of this lock are leaves (set a pointer, insert into or erase from
//	sd_InterestSet; nothing is taken inside them), and nothing at CountSection + 1 is ever held
//	when they are entered, so the pair simply has an order now, and it is the checked one.

//	FailSection = IndexedString + 1,
	TileShadow = MOST_INNER_LOCK - 3,
	Tile = TileShadow + 1,

	SpecificOperator = TileShadow - 1, // MOST_INNER_LOCK - 4
	SpecificOperatorGroup = SpecificOperator - 1,
	DataViewQueue = SpecificOperator - 1,
	UpdateActionSet = DataViewQueue,
	Storage = SpecificOperator - 1, // MOST_INNER_LOCK - 5
	AbstrStorage = Storage - 1,
	BoundingBoxCache2 = SpecificOperator - 1,
	BoundingBoxCache1 = BoundingBoxCache2 - 1,

	// // MOST_INNER_LOCK - 4
	DataRefContainer = Storage + 1,

	// level c == MOST_INNER_LOCK - 2
	ThreadMessing = TileShadow+1, //lowest_of(CountSection - 1, IndexedString - 1), // calls CountSections.
	CountSection = ThreadMessing+1,

	// level c+1 == MOST_INNER_LOCK - 1
	FailSection = ThreadMessing + 1,
	OperContextAccess = ThreadMessing + 1,

	// level c+2 == MOST_INNER_LOCK
	TileAccessMap = highest_of(ThreadMessing, CountSection) + 1, // can be used in SymbObjSection and in CountSection
	IndexedString = CountSection + 1,
	UpdatingInterestSet = CountSection + 1, // taken while CountSection is held; see above (#1233)
	LispObjCache = IndexedString + 1,
	MoveSupplInterest = FailSection + 1,

	// level c+3 == MOST_INNER_LOCK + 1
	GDALComponent = IndexedString -1,
//	FLispUsageCache: retired for #1227 -- AsFLispSharedStr now reuses a thread_local buffer
//	instead of guarding one global static, so the level it occupied no longer exists.
//	SymbObjSection = IndexedString + 1, // can be used while IndexedString is locked
	NotifyTargetCount = MoveSupplInterest + 1,

	// level c+4 == MOST_INNER_LOCK + 2
	ObjectRegister = LispObjCache + 1, // can be used in SymbObjSection

	// level c+5 == MOST_INNER_LOCK + 3
	DebugOutStream = ObjectRegister + 1,

	// level c+6 == MOST_INNER_LOCK + 4
	OperationQueue = DebugOutStream + 1,

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
