// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

// include file for standard system include files,
//  or project specific include files that are used frequently, but
//      are changed infrequently
//

#if defined(_MSC_VER)
#pragma once
#endif

#if !defined(__TIC_TICBASE_H)
#define __TIC_TICBASE_H

//----------------------------------------------------------------------
// used modules and forward class references
//----------------------------------------------------------------------

#include "RtcBase.h"
#include "SymBase.h"

#if defined(DMTIC_EXPORTS)
#	define TIC_CALL __declspec(dllexport)
#else
#	define TIC_CALL __declspec(dllimport)
#endif
// TODO G8: REMOVE TICTOC_CALL if it doesn't need to be TIC_CALL by using TIC local constructors
#define TICTOC_CALL TIC_CALL


struct TreeItem;
struct TreeItemSetType;
struct TreeItemVectorType;
class AbstrUnit;
class AbstrDataItem;
class AbstrDataObject;
struct AbstrTileRangeData;

using AbstrParam = AbstrDataItem;

template <typename V> class Unit;

struct AbstrTileRangeData;
template <typename V> struct TiledRangeData;
template <typename V> struct TileFunctor;
template <typename V> using DataArray = TileFunctor<V>; // TODO G8: SUBSTITUTE AWAY

struct TreeItemClass;
class  DataItemClass;
class  UnitClass;

struct UnitMetric;
struct UnitProjection;
struct DataItemRefContainer;
struct UnitProcessor;

struct DataReadLock;
struct DataWriteLock;

enum   NotificationCode;
enum   class DrlType : UInt8;
enum class CalcRole : UInt8;
enum   DataCheckMode;

// Forward Declarations
class AbstrStorageManager;
class MmdStorageManager;
class NonmappableStorageManager;
class AbstrStreamManager;
struct StorageMetaInfo;
using StorageMetaInfoPtr = std::shared_ptr<StorageMetaInfo>;
struct StorageReadHandle;
class AbstrCalculator;

// Name value indentation level
using prop_name = TokenID; // cannot contain name delimiters, e.g. "attr1"
using prop_level = int;
using prop_value = SharedStr;
using prop_name_value = std::pair<prop_name, prop_value>;
using level_propvalue = std::pair<prop_level, prop_name_value>;
using prop_tables = std::vector<level_propvalue>;

// SharedPtr
using SharedTreeItem = SharedPtr<const TreeItem>;
using SharedDataItem = SharedPtr<const AbstrDataItem>;
using SharedUnit     = SharedPtr<const AbstrUnit>;

// InterestPtr
template <typename CPtr> struct InterestPtr;
using SharedTreeItemInterestPtr = InterestPtr<SharedTreeItem>;
using SharedDataItemInterestPtr = InterestPtr<SharedDataItem>;
using SharedUnitInterestPtr     = InterestPtr<SharedUnit>;

using SharedMutableDataItem = SharedPtr<AbstrDataItem>;
using SharedMutableDataItemInterestPtr = InterestPtr<SharedMutableDataItem>  ;

using SharedMutableUnit = SharedPtr<AbstrUnit>;
using SharedMutableUnitInterestPtr = InterestPtr<SharedMutableUnit>;

struct DataController;
using DataControllerRef = SharedPtr<const DataController>;
using FutureData = InterestPtr<DataControllerRef>;

using GuiReadLock = TileCRef;
using GuiReadLockPair = Point<GuiReadLock>;

using TStateChangeNotificationFunc = void (*)(ClientHandle clientHandle, const TreeItem* self, NotificationCode notificationCode);
using TSupplCallbackFunc = bool (*)(ClientHandle clientHandle, const TreeItem* supplier);

template <typename T> struct OwningPtrSizedArray;
using BlobBuffer = OwningPtrSizedArray<Byte>;

namespace Explain {
	struct Context;
}

using AbstrCalculatorRef = SharedPtr<const AbstrCalculator> ;
using BestItemRef = std::pair<SharedTreeItem, SharedStr>;
using AbstrStorageManagerRef = SharedPtr<AbstrStorageManager>;
using NonmappableStorageManagerRef = SharedPtr<NonmappableStorageManager>;

struct AbstrOperGroup;
class Operator;

// *****************************************************************************
// ArgSeqType
// *****************************************************************************

using ArgSeqType = std::vector<const TreeItem*>;
struct  TreeItemDualRef;

using ConstUnitRef = SharedPtr<const AbstrUnit>;
class Operator;

using arg_index = UInt32;
using og_index  = UInt32;

//----------------------------------------------------------------------
// casting
//----------------------------------------------------------------------

template <typename T> inline bool                 IsDataItem(const T* self) { return AsDynamicDataItem(self) != nullptr; }
template <typename T> inline const AbstrDataItem* AsDataItem(const T* self) { return debug_cast<const AbstrDataItem*>(self); }
template <typename T> inline       AbstrDataItem* AsDataItem(T* self) { return debug_cast<AbstrDataItem*>(self); }
template <typename T> inline const AbstrDataItem* AsDynamicDataItem(const T* self) { return dynamic_cast<const AbstrDataItem*>(self); }
template <typename T> inline       AbstrDataItem* AsDynamicDataItem(T* self) { return dynamic_cast<AbstrDataItem*>(self); }
template <typename T> inline const AbstrDataItem* AsCheckedDataItem(const T* self) { return checked_cast<const AbstrDataItem*>(self); }
template <typename T> inline       AbstrDataItem* AsCheckedDataItem(T* self) { return checked_cast<AbstrDataItem*>(self); }
template <typename T> inline const AbstrDataItem* AsCertainDataItem(const T* self) { return checked_valcast<const AbstrDataItem*>(self); }
template <typename T> inline       AbstrDataItem* AsCertainDataItem(T* self) { return checked_valcast<      AbstrDataItem*>(self); }

template <typename T> inline bool IsDataItem(const SharedPtr<T>& self) { return IsDataItem(self.get()); }
template <typename T> inline auto AsDataItem(const SharedPtr<T>& self) { return MakeShared(AsDataItem(self.get())); }
template <typename T> inline auto AsDynamicDataItem(const SharedPtr<T>& self) { return MakeShared(AsDynamicDataItem(self.get())); }
template <typename T> inline auto AsCheckedDataItem(const SharedPtr<T>& self) { return MakeShared(AsCheckedDataItem(self.get())); }
template <typename T> inline auto AsCertainDataItem(const SharedPtr<T>& self) { return MakeShared(AsCertainDataItem(self.get())); }

//----------------------------------------------------------------------
// class  : TreeItemAdmLock, inherit from specific Adm's to ensure order of initialization
//----------------------------------------------------------------------

#if defined(MG_DEBUG_DATA)

struct TreeItemAdmLock
{
	TreeItemAdmLock();
	~TreeItemAdmLock();

	static void Report();
};

#endif


#endif // __TIC_TICBASE_H
