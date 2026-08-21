// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

/*
 *  DataReadLock / DataWriteLock: RAII access to the data object of an
 *  AbstrDataItem -- a read lock triggers calculation or file loading as
 *  needed, a write lock administers preparation and commit -- plus the
 *  FileData open/close primitives.
 */

#if !defined(__TIC_DATALOCKS_H)
#define __TIC_DATALOCKS_H

//----------------------------------------------------------------------
// used modules and forward class references
//----------------------------------------------------------------------

#include "AbstrDataItem.h"
#include "act/InterestRetainContext.h"
#include "dbg/Diagnostics.h"
#include "ser/FileCreationMode.h"
#include "ItemLocks.h"

enum class DrlType : UInt8 { UpdateNever = 0, Suspendible = 1, Certain = 2, ThrowOnFail = 4, UpdateMask = 0x0003, CertainOrThrow = Certain + ThrowOnFail };

//----------------------------------------------------------------------
// FileData primitives
//
// These were lifted out of the DataStoreManager, and a "TODO G8.5: move back" stood here
// until 2026-08. There is nothing to move them back to: the DataStoreManager owned the
// CalcCache and went with it (#1189). They now serve memory-mapped storages, keyed by
// storage path plus relative item name rather than by expression, so tic is where they
// belong. See doc/development/g8-todos.md.
//----------------------------------------------------------------------

auto OpenFileData(const AbstrDataItem* adi, const SharedObj* abstrValuesRangeData, SharedStr filenameBase)
-> std::unique_ptr<const AbstrDataObject>;

//----------------------------------------------------------------------
// DataReadLockAtom: manages m_DataLockCount and unique consideration of LoadBlob and CreateFileData
//----------------------------------------------------------------------

struct DataReadLockAtom
{
	DataReadLockAtom() = default;
	TIC_CALL DataReadLockAtom(DataReadLockAtom&&) noexcept;
	TIC_CALL DataReadLockAtom(const AbstrDataItem* item);
	TIC_CALL ~DataReadLockAtom() noexcept;

	// NOT '= default': the data-lock-count release lives in ~DataReadLockAtom. A defaulted move-assignment
	// reset-moves m_Item (shared_tree_ptr = std reset-not-swap) and LEAKS the count it replaces. (The old
	// intrusive SharedPtr move-assignment was swap-based, so '= default' used to route the old lock through
	// the moved-from temporary's dtor; std::shared_ptr semantics broke that.) Release first.
	DataReadLockAtom& operator = (DataReadLockAtom&& rhs) noexcept;

	const AbstrDataItem* GetItem() const { return m_Item.get(); }

private:
	void release() noexcept; // shared by ~DataReadLockAtom and move-assignment
	std::shared_ptr<const AbstrDataItem> m_Item;
};

//----------------------------------------------------------------------
// DataReadLock
//----------------------------------------------------------------------

struct DataReadLock : SharedPtr<const AbstrDataObject>
{
	DataReadLock()  noexcept = default;
	DataReadLock(DataReadLock&& rhs)  noexcept = default;
	~DataReadLock() noexcept = default;

	TIC_CALL DataReadLock(const AbstrDataItem* item);  // prepare only true when called from PreparedDataLock

	// NOT '= default': a defaulted memberwise reset-move (a) leaks the count-bearing members' locks
	// (m_RefPtrLock -> m_ItemCount + s_SessionUsageCounter share; the read-lock members were swap-based
	// pre-migration) AND (b) would reset m_KeepItemAlive FIRST (declaration order), possibly freeing the
	// item while m_RefPtrLock / m_DRLA counts are still held. Release counts-before-owner (matching the
	// member-ordering note below and ~DataReadLock), then adopt rhs. See ItemReadLock for the shared-lock leak.
	TIC_CALL DataReadLock& operator = (DataReadLock&& rhs)  noexcept;

	const AbstrDataObject* GetRefObj () const { return get(); }

	// A DataReadLock constructed on a null/expired item is EMPTY (tolerated by design). Use these at points
	// that require the lock to actually hold data, so a raced-away item fails the operation (MG_CHECK throw)
	// instead of deferring a null deref to first use.
	const AbstrDataObject* GetRefObjOrThrow() const { auto refObj = get(); MG_CHECK(refObj); return refObj; }
	const AbstrDataItem*   GetItemOrThrow  () const { auto item = m_DRLA.GetItem(); MG_CHECK(item); return item; }

	bool IsLocked() const { return m_DRLA.GetItem(); }

private:
	// Owns the locked item and is declared FIRST, so it is destructed LAST (after m_RefPtrLock and m_DRLA).
	// This guarantees neither count-bearing lock is ever the item's last owner: m_DRLA (m_DataLockCount) and
	// m_RefPtrLock (m_ItemCount) release their counts AND drop their owning refs first, and only then does this
	// plain owner drop the final ref -- so when ~AbstrDataItem/ClearDataObject runs, both counts are already 0.
	// Needed because ownership is now downward (parent->child): without this, a lock's count-holder could be the
	// last owner and destroy the item while its own count was still held (tripping MG_CHECK(m_ItemCount==0)).
	std::shared_ptr<const AbstrDataItem> m_KeepItemAlive;
	ItemReadLock                       m_RefPtrLock; // TODO G8: REMOVE
	DataReadLockAtom                   m_DRLA;
};

using DataReadHandle = DataReadLock; // TODO G8.5: RENAME DataReadLock -> DataReadHandle

template<typename V> const TileFunctor<V>*
const_array_cast(const DataReadHandle& drh)
{
	return debug_valcast<const TileFunctor<V>*>(drh.get());
}

template<typename V> const TileFunctor<V>*
const_opt_array_cast(const DataReadHandle& drh)
{
	if (drh.get() == nullptr)
		return nullptr;
	return debug_valcast<const TileFunctor<V>*>(drh.get());
}

template<typename V>
auto const_array_dynacast(const DataReadHandle& drh) -> const TileFunctor<V>*
{
	return dynamic_cast<const TileFunctor<V>*>(drh.get());
}

template<typename V>
auto const_array_checked_cast(const DataReadHandle& drh) -> const TileFunctor<V>*
{
	return checked_cast<const TileFunctor<V>*>(drh.get());
}

template<typename V>
auto const_array_checked_valcast(const DataReadHandle& drh) -> const TileFunctor<V>*
{
	return checked_valcast<const TileFunctor<V>*>(drh.get());
}

template<typename V>
auto const_opt_array_checkedcast(const DataReadHandle& drh) -> const TileFunctor<V>*
{
	if (drh.get() == nullptr)
		return nullptr;
	return checked_cast<const TileFunctor<V>*>(drh.get());
}

//----------------------------------------------------------------------
// PreparedDataReadLock, Blocks Suspension and Prepares data before creating a DataReadLock
//----------------------------------------------------------------------

#include "act/TriggerOperator.h"

struct PreparedDataReadLock : SuspendTrigger::FencedBlocker, DataReadLock
{
	TIC_CALL PreparedDataReadLock(const AbstrDataItem* item, CharPtr blockingAction);
};

//----------------------------------------------------------------------
// DataWriteLock
//----------------------------------------------------------------------

// Simple base class that makes sure lock is incremented in destructor once it is decremented in constructur

struct DataWriteLock : SharedPtr<AbstrDataObject>
{
	DataWriteLock() = default;
	TIC_CALL DataWriteLock(AbstrDataItem*, dms_rw_mode rwm = dms_rw_mode::write_only_all, const SharedObj* abstrValuesRangeData = nullptr); // was lockTile 

	TIC_CALL DataWriteLock(DataWriteLock&&) noexcept = default;
	TIC_CALL ~DataWriteLock();

	// NOT '= default': DataWrite_Unlock lives in ~DataWriteLock, keyed on m_adi. A defaulted move-assignment
	// reset-moves m_adi (std shared_tree_ptr) and LEAKS the write lock it replaces (the old intrusive SharedPtr
	// move was swap-based, routing the old lock through the moved-from temporary's dtor). Release first.
	TIC_CALL DataWriteLock& operator =(DataWriteLock&& rhs) noexcept;
	bool IsLocked() const { return get() != nullptr; }
	AbstrDataItem* GetItem() { return m_adi.get(); } // TODO G8: REMOVE

	// See DataReadLock::GetRefObjOrThrow: an empty lock (null item at construction) fails here, not at deref.
	AbstrDataObject* GetRefObjOrThrow() const { auto refObj = get(); MG_CHECK(refObj); return refObj; }
	AbstrDataItem*   GetItemOrThrow  () const { auto item = m_adi.get(); MG_CHECK(item); return item; }

	TIC_CALL void Commit();

private:
	DataWriteLock(const DataWriteLock&) = delete;
	DataWriteLock& operator = (const DataWriteLock&) = delete;

	void release() noexcept; // shared by ~DataWriteLock and move-assignment
	std::shared_ptr<AbstrDataItem> m_adi;
};

template<typename V> TileFunctor<V>*
mutable_array_cast(DataWriteLock const& lock)
{
	return debug_valcast<TileFunctor<V>*>(lock.get());
}

using DataWriteHandle = DataWriteLock; // TODO G8.5: RENAME DataWriteLock -> DataWriteHandle

template<typename V>
auto mutable_array_dynacast(DataWriteLock const& lock) -> TileFunctor<V>*
{
	return dynamic_cast<TileFunctor<V>*>(lock.get());
}

template<typename V>
auto mutable_array_checkedcast(DataWriteLock const& lock) -> TileFunctor<V>*
{
	return checked_cast<TileFunctor<V>*>(lock.get());
}


//----------------------------------------------------------------------
// Lock dependent template members of AbstrDataItem
//----------------------------------------------------------------------

#include "AbstrDataObject.h"

template <typename V> V AbstrDataItem::LockAndGetValue(SizeT index) const
{
	assert(IsMetaThread());

	InterestRetainContextBase base;
	if (!HasInterest())
		base.Add(this);
	PreparedDataReadLock lck(this, "AbstrDataItem::LockAndGetValue");
	return GetValue<V>(index);
}

#endif // __TIC_DATALOCKS_H
