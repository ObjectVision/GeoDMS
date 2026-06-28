// Copyright (C) 1998-2026 Object Vision b.v. 
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
#include "ptr/SharedTreePtr.h" // migration option B(a): shared_tree_ptr wrapper for the TreeItem-family typedefs

#if !defined(_MSC_VER)
#	define TIC_CALL
#elif defined(DMTIC_EXPORTS)
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

enum   NotificationCode : int;
enum   class DrlType : UInt8;
enum class CalcRole : UInt8;
enum   DataCheckMode : int;

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

// SharedPtr  (migration (a): TreeItem family -> shared_tree_ptr over std::shared_ptr)
using SharedTreeItem = shared_tree_ptr<const TreeItem>;
using SharedMutableTreeItem = shared_tree_ptr<TreeItem>;
using SharedDataItem = shared_tree_ptr<const AbstrDataItem>;
using SharedUnit     = shared_tree_ptr<const AbstrUnit>;

// Non-owning std::weak_ptr-backed back-references to tree-owned units (a data item refers to its
// domain/values units but does NOT own them; the config/cache tree owns them). Real weak liveness
// via .lock() -- no dangling raw back-pointer when the unit's owner drops first.
using WeakUnit       = weak_tree_ptr<const AbstrUnit>;

// InterestPtr
template <typename CPtr> struct InterestPtr;
using SharedTreeItemInterestPtr = InterestPtr<SharedTreeItem>;
using SharedDataItemInterestPtr = InterestPtr<SharedDataItem>;
using SharedUnitInterestPtr     = InterestPtr<SharedUnit>;

using SharedMutableDataItem = shared_tree_ptr<AbstrDataItem>;
using SharedMutableDataItemInterestPtr = InterestPtr<SharedMutableDataItem>  ;

using SharedMutableUnit = shared_tree_ptr<AbstrUnit>;
using SharedMutableUnitInterestPtr = InterestPtr<SharedMutableUnit>;

struct DataController;
using DataControllerRef = SharedPtr<const DataController>;
using FutureData = InterestPtr<DataControllerRef>;

using GuiReadLock = TileCRef;
using GuiReadLockPair = Point<GuiReadLock>;

using TStateChangeNotificationFunc = void (*)(ClientHandle clientHandle, const TreeItem* self, NotificationCode notificationCode);
using TSupplCallbackFunc = bool (*)(ClientHandle clientHandle, const TreeItem* supplier);

using UnitLabelScalePair = std::pair<TokenID, Float64>;
using GetUnitlabeledScalePairFuncType = auto (*)(TokenID) ->UnitLabelScalePair;

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

using ConstUnitRef = shared_tree_ptr<const AbstrUnit>;
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
template <typename T> inline auto AsDataItem(OwningPtr<T>& self) { return OwningPtr<AbstrDataItem>( AsDataItem(self.release())); }

template <typename T> inline bool IsDataItem       (const SharedPtr<T>& self) { return IsDataItem(self.get()); }
template <typename T> inline auto AsDataItem       (const SharedPtr<T>& self) { return MakeSharedFromBorrowedObjectPtr(AsDataItem(self.get())); }
template <typename T> inline auto AsDynamicDataItem(const SharedPtr<T>& self) { return MakeSharedFromBorrowedObjectPtr(AsDynamicDataItem(self.get())); }
template <typename T> inline auto AsCheckedDataItem(const SharedPtr<T>& self) { return MakeSharedFromBorrowedObjectPtr(AsCheckedDataItem(self.get())); }
template <typename T> inline auto AsCertainDataItem(const SharedPtr<T>& self) { return MakeSharedFromBorrowedObjectPtr(AsCertainDataItem(self.get())); }

// migration (a): same helpers over the shared_tree_ptr family (result borrows via existing_obj)
template <typename T> inline bool IsDataItem       (const shared_tree_ptr<T>& self) { return IsDataItem(self.get()); }
template <typename T> inline auto AsDataItem       (const shared_tree_ptr<T>& self) { auto* r = AsDataItem(self.get());        return shared_tree_ptr<std::remove_pointer_t<decltype(r)>>(r, existing_obj{}); }
template <typename T> inline auto AsDynamicDataItem(const shared_tree_ptr<T>& self) { auto* r = AsDynamicDataItem(self.get()); return shared_tree_ptr<std::remove_pointer_t<decltype(r)>>(r, existing_obj{}); }
template <typename T> inline auto AsCheckedDataItem(const shared_tree_ptr<T>& self) { auto* r = AsCheckedDataItem(self.get()); return shared_tree_ptr<std::remove_pointer_t<decltype(r)>>(r, existing_obj{}); }
template <typename T> inline auto AsCertainDataItem(const shared_tree_ptr<T>& self) { auto* r = AsCertainDataItem(self.get()); return shared_tree_ptr<std::remove_pointer_t<decltype(r)>>(r, existing_obj{}); }

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

//----------------------------------------------------------------------
// enum classes
//----------------------------------------------------------------------

enum class StorageReadOnlySetting { Default, ReadOnly, ReadWrite };


#endif // __TIC_TICBASE_H
