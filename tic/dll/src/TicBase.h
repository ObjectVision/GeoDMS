// Copyright (C) 2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

// include file for standard system include files,
//  or project specific include files that are used frequently, but
//      are changed infrequently
//

#pragma once

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

typedef AbstrDataItem AbstrParam;

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
class AbstrStreamManager;
struct StorageMetaInfo;
using StorageMetaInfoPtr = std::shared_ptr<StorageMetaInfo>;
struct StorageReadHandle;
class AbstrCalculator;

//SharedPtr
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

typedef TileCRef                                      GuiReadLock;
typedef Point<GuiReadLock>                            GuiReadLockPair;

typedef void (*TStateChangeNotificationFunc)(ClientHandle clientHandle, const TreeItem* self, NotificationCode notificationCode);
typedef bool (*TSupplCallbackFunc)(ClientHandle clientHandle, const TreeItem* supplier);


template <typename T> struct OwningPtrSizedArray;
typedef OwningPtrSizedArray<Byte> BlobBuffer;

namespace Explain {
	struct Context;
}

using AbstrCalculatorRef = SharedPtr<const AbstrCalculator> ;
using BestItemRef = std::pair<const TreeItem*, SharedStr>;
using AbstrStorageManagerRef = SharedPtr<AbstrStorageManager>;

struct AbstrOperGroup;
class Operator;

// *****************************************************************************
// ArgSeqType
// *****************************************************************************

using ArgSeqType = std::vector<const TreeItem*>;
struct  TreeItemDualRef;

typedef SharedPtr<const AbstrUnit> ConstUnitRef;
class Operator;

using arg_index = UInt32;
using og_index  = UInt32;

//----------------------------------------------------------------------
// casting
//----------------------------------------------------------------------

template <typename T> inline const AbstrDataItem* AsCheckedDataItem(const T* self) { return checked_cast<const AbstrDataItem*>(self); }
template <typename T> inline       AbstrDataItem* AsCheckedDataItem(T* self) { return checked_cast<AbstrDataItem*>(self); }
template <typename T> inline const AbstrDataItem* AsCertainDataItem(const T* self) { return checked_valcast<const AbstrDataItem*>(self); }
template <typename T> inline       AbstrDataItem* AsCertainDataItem(T* self) { return checked_valcast<      AbstrDataItem*>(self); }
template <typename T> inline const AbstrDataItem* AsDynamicDataItem(const T* self) { return dynamic_cast<const AbstrDataItem*>(self); }
template <typename T> inline       AbstrDataItem* AsDynamicDataItem(T* self) { return dynamic_cast<AbstrDataItem*>(self); }
template <typename T> inline const AbstrDataItem* AsDataItem(const T* self) { return debug_cast<const AbstrDataItem*>(self); }
template <typename T> inline       AbstrDataItem* AsDataItem(T* self) { return debug_cast<AbstrDataItem*>(self); }
template <typename T> inline bool                 IsDataItem(const T* self) { return AsDynamicDataItem(self) != nullptr; }

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
