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
	OperationContext = MOST_INNER_LOCK - 1,
	OperationContextDeque = OperationContext - 1,
	UpdatingInterestSet = MOST_INNER_LOCK - 1,

//	FailSection = IndexedString + 1,
	TileShadow = MOST_INNER_LOCK - 3,
	Tile = TileShadow + 1,
	SafeFileWriterArray = MOST_INNER_LOCK,

	SpecificOperator = TileShadow - 1,
	SpecificOperatorGroup = SpecificOperator - 1,
	DataViewQueue = SpecificOperator - 1,
	UpdateActionSet = DataViewQueue,
	Storage = SpecificOperator - 1,
	AbstrStorage = Storage - 1,
	BoundingBoxCache2 = SpecificOperator - 1,
	BoundingBoxCache1 = BoundingBoxCache2 - 1,

	DataRefContainer = 1,

	// level c
	ThreadMessing = TileShadow+1, //lowest_of(CountSection - 1, IndexedString - 1), // calls CountSections.
	CountSection = ThreadMessing+1,

	// level c+1
	FailSection = ThreadMessing + 1,
	OperContextAccess = ThreadMessing + 1,

	// level c+2
	TileAccessMap = highest_of(ThreadMessing, CountSection) + 1, // can be used in SymbObjSection and in CountSection
	IndexedString = CountSection + 1,
	LispObjCache = IndexedString + 1,
	MoveSupplInterest = FailSection + 1,

	// level c+3
	GDALComponent = IndexedString + 1,
	FLispUsageCache = IndexedString - 1,
//	SymbObjSection = IndexedString + 1, // can be used while IndexedString is locked
	NotifyTargetCount = MoveSupplInterest + 1,

	// level c+4
	ObjectRegister = LispObjCache + 1, // can be used in SymbObjSection

	// level c+5
	DebugOutStream = ObjectRegister + 1,

	// level c+6
	OperationQueue = DebugOutStream + 1,
};



//==============================================================

#endif // __RTC_LOCKLEVELS_H
