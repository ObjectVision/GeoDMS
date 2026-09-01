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
	OperationContext = MOST_INNER_LOCK,
	UpdatingInterestSet = MOST_INNER_LOCK - 1,

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
	LispObjCache = IndexedString + 1,
	MoveSupplInterest = FailSection + 1,

	// level c+3 == MOST_INNER_LOCK + 1
	GDALComponent = IndexedString -1,
	FLispUsageCache = IndexedString - 1,
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
//  - Plain functions. A scope declaring DMS_ENTERS(L) may only call functions that take a level
//    inner to L -- or L itself, if both are shared.
//  - Virtual functions. The annotation on the BASE declaration is the contract: the outermost
//    level any override is permitted to reach. An override may declare a stricter (inner) level
//    but never an outer one, and every call site is checked against the base. That is what keeps
//    the analysis from needing to know the dynamic type.
//  - Function pointers and std::function. The ceiling belongs to the PARAMETER, not to the
//    callee: DMS_CALLEE_ENTERS on the parameter is what the caller's lambda is checked against,
//    and the callee is checked against its own. Neither side needs to know the other.
//
// DMS_ENTERS expands to a Debug-only scope object; a Release build sees nothing. DMS_CALLEE_ENTERS
// expands to nothing at all -- no compiler we build with can check a parameter annotation, so it
// records the contract for the reader and for the syntactic pass over statically resolvable call
// edges that is the other half of the scheme.

// Spelled out at the call site so that a reader sees which mode is meant, rather than a bare bool.
inline constexpr bool dms_shared_v = true;
inline constexpr bool dms_exclusive_v = false;

#if defined(MG_DEBUG_LOCKLEVEL)

#define DMS_ENTERS(LEVEL, MODE) DmsLockCeiling mg_lock_ceiling_(LEVEL, MODE, "DMS_ENTERS(" #LEVEL ")")

#else

#define DMS_ENTERS(LEVEL, MODE) ((void)0)

#endif

// Declares that the enclosing scope takes no lock level at all.
#define DMS_ENTERS_NOTHING DMS_ENTERS(ord_level_type::EntersNothing, dms_exclusive_v)

// Parameter annotation for a function-pointer / std::function parameter; see above.
#define DMS_CALLEE_ENTERS(LEVEL)

//==============================================================

#endif // __RTC_LOCKLEVELS_H
