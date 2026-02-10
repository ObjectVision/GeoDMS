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
};



//==============================================================

#endif // __RTC_LOCKLEVELS_H
