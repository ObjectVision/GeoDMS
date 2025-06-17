// Copyright (C) 1998-2025 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

#if !defined(__TIC_TREEITEMFLAGS_H)
#define __TIC_TREEITEMFLAGS_H

//----------------------------------------------------------------------
// used modules and forward class references
//----------------------------------------------------------------------

#include "TicBase.h"

#if defined(MG_DEBUG)
#	define CHECK_FLAG_INVARIANTS CheckFlagInvariants()
#else
#	define CHECK_FLAG_INVARIANTS
#endif

//----------------------------------------------------------------------
// flags stored in Actor::m_State
//----------------------------------------------------------------------

static_assert(actor_flag_set::AF_Next == 0x4000);

const UInt32 ASF_MakeCalculatorLock    = 0x0001 * actor_flag_set::AF_Next; 
const UInt32 ASF_WasLoaded             = 0x0002 * actor_flag_set::AF_Next; // TODO G8.5 ? REMOVE AFTER CalcCache restoration
const UInt32 ASF_GetStorageManagerLock = 0x0004 * actor_flag_set::AF_Next;

const UInt32 ASF_GetCalcMetaInfo       = 0x0008 * actor_flag_set::AF_Next;

//----------------------------------------------------------------------
// TreeItemStatusFlags stored in m_StatusFlags
//----------------------------------------------------------------------

typedef UInt32 TreeItemStatusFlags;
typedef TreeItemStatusFlags DataItemStatusFlags;
typedef TreeItemStatusFlags UnitItemStatusFlags;

const TreeItemStatusFlags TSF_IsAutoDeleteDisabled        = 0x0001;
const TreeItemStatusFlags TSF_DataInMem                   = 0x0002;
const TreeItemStatusFlags TSF_IsCacheItem                 = 0x0004;
const TreeItemStatusFlags TSF_IsEndogenous                = 0x0008;

const TreeItemStatusFlags TSF_KeepData                    = 0x0010; // Keep data available (in mem) as if interest remains
const TreeItemStatusFlags TSF_FreeData                    = 0x0020; // Drop when out of interest (don't make persistent)
const TreeItemStatusFlags TSF_DisabledStorage             = 0x0040;
const TreeItemStatusFlags TSF_HasConfigData               = 0x0080;

const TreeItemStatusFlags TSF_InheritedRef                = 0x0100;
const TreeItemStatusFlags TSF_HasPseudonym                = 0x0200;
const TreeItemStatusFlags TSF_HasStoredProps              = 0x0400;
const DataItemStatusFlags TSF_THA_Keep                    = 0x0800; // this data-item is referred to by a repetitive tile users and therefore memoization of all tiles is advised.

//----------------------------------------------------------------------
// DataItemStatusFlags
//----------------------------------------------------------------------

const TreeItemStatusFlags TSF_IsHidden                    = 0x00010000;
const TreeItemStatusFlags TSF_InHidden                    = 0x00020000;
const TreeItemStatusFlags TSF_IsTemplate                  = 0x00040000;
const TreeItemStatusFlags TSF_InTemplate                  = 0x00080000;

const TreeItemStatusFlags TSF_Categorical                 = 0x00100000;
const TreeItemStatusFlags TSF_LazyCalculated              = 0x00200000;
const TreeItemStatusFlags TSF_StoreData                   = 0x00400000; // Also use CalcCache when data is below the data-size threshold
const TreeItemStatusFlags TSF_Depreciated                 = 0x00800000; // unallocated bit

// Unit flags can overlap with Data flags as a TreeItem is never both.
const UnitItemStatusFlags USF_HasSpatialReference         = 0x02000000;
const UnitItemStatusFlags USF_HasConfigRange              = 0x04000000;

// REMOVE, TODO: CONSIDER STORING THE FOLLOWING PER TILE
const int DCM2DSF_SHIFT = 25;
const int DCM_MASK = 0x03U;
const DataItemStatusFlags DSF_HasUndefinedValues     = 0x02000000;
const DataItemStatusFlags DSF_HasOutOfRangeValues    = 0x04000000;
const DataItemStatusFlags DSF_ValuesChecked          = 0x08000000;
const DataItemStatusFlags DSF_CachedByStorageManager = 0x10000000;
const DataItemStatusFlags DSF_HasSortedValues        = 0x20000000;

const int VC2DSF_SHIFT = DCM2DSF_SHIFT + 4;
const int VC_MASK = 0x03U;

struct treeitem_flag_set : flag_set
{
	void SetValueComposition(ValueComposition vc)
	{
		SetBits(VC_MASK << VC2DSF_SHIFT, UInt32(vc) << VC2DSF_SHIFT);
	}

	ValueComposition GetValueComposition() const
	{
		return ValueComposition(GetBits(VC_MASK << VC2DSF_SHIFT) >> VC2DSF_SHIFT);
	}

	void SetDataCheckMode(DataCheckMode dcm)
	{
		SetBits(DCM_MASK << DCM2DSF_SHIFT, (dcm << DCM2DSF_SHIFT) | DSF_ValuesChecked);

	}

	DataCheckMode GetDataCheckMode() const
	{
		assert(Get(DSF_ValuesChecked));
		return DataCheckMode(GetBits(DCM_MASK << DCM2DSF_SHIFT) >> DCM2DSF_SHIFT);
	}
	bool HasSortedValues() const
	{
		return Get(DSF_HasSortedValues);
	}
	void SetHasSortedValues(bool hasSortedValues = true)
	{
		Set(DSF_HasSortedValues, hasSortedValues);
	}
};


#endif // __TIC_TREEITEMFLAGS_H
