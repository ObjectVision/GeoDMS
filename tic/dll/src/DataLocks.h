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

#if !defined(__TIC_DATALOCKS_H)
#define __TIC_DATALOCKS_H

//----------------------------------------------------------------------
// used modules and forward class references
//----------------------------------------------------------------------

#include "AbstrDataItem.h"
#include "ptr/InterestHolders.h"
#include "ser/FileCreationMode.h"
#include "ItemLocks.h"
#include "TileLock.h"

enum class DrlType : UInt8 { UpdateNever = 0, Suspendible = 1, Certain = 2, ThrowOnFail = 4, UpdateMask = 0x0003, CertainOrThrow = Certain + ThrowOnFail };

//----------------------------------------------------------------------
// FileData primitives; TODO G8.5:Move to DataStoreManager
//----------------------------------------------------------------------

auto CreateFileData(AbstrDataItem* adi, bool mustClear MG_DEBUG_ALLOCATOR_SRC_ARG)->std::unique_ptr<AbstrDataObject>;
auto OpenFileData(const AbstrDataItem* adi, SharedStr filenameBase, SafeFileWriterArray* sfwa MG_DEBUG_ALLOCATOR_SRC_ARG)->std::unique_ptr<const AbstrDataObject>;

//----------------------------------------------------------------------
// DataReadLockAtom: manages m_DataLockCount and unique consideration of LoadBlob and CreateFileData
//----------------------------------------------------------------------

struct DataReadLockAtom
{
	DataReadLockAtom() = default;
	TIC_CALL DataReadLockAtom(DataReadLockAtom&&) noexcept;
	TIC_CALL DataReadLockAtom(const AbstrDataItem* item);
	TIC_CALL ~DataReadLockAtom() noexcept;

//	void swap(DataReadLockAtom& oth) noexcept;
//	void operator = (DataReadLockAtom&& rhs) noexcept { swap(rhs); }
	DataReadLockAtom& operator = (DataReadLockAtom&& rhs) noexcept = default;

	const AbstrDataItem* GetItem() const { return m_Item; }

private:
	SharedPtr<const AbstrDataItem> m_Item;
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

	DataReadLock& operator = (DataReadLock&& rhs)  noexcept = default;

	const AbstrDataObject* GetRefObj () const { return get(); }

	bool IsLocked() const { return m_DRLA.GetItem(); }

private:
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
auto const_array_checkedcast(const DataReadHandle& drh) -> const TileFunctor<V>*
{
	return checked_cast<const TileFunctor<V>*>(drh.get());
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
	TIC_CALL PreparedDataReadLock(const AbstrDataItem* item);
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

	DataWriteLock& operator =(DataWriteLock&& rhs) noexcept = default;
	bool IsLocked() const { return get() != nullptr; }
	AbstrDataItem* GetItem() { return m_adi; } // TODO G8: REMOVE

	TIC_CALL void Commit();

private:
	DataWriteLock(const DataWriteLock&) = delete;
	DataWriteLock& operator = (const DataWriteLock&) = delete;

	SharedPtr<AbstrDataItem> m_adi;  // TODO G8: REMOVE
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
	if (!HasInterest())
		InterestRetainContextBase::Add(this);
	PreparedDataReadLock lck(this);
	return GetValue<V>(index);
}

template <typename V> SizeT AbstrDataItem::LockAndCountValues(param_type_t<typename sequence_traits<V>::value_type> value) const
{
	SharedActorInterestPtr tii(this);
	PreparedDataReadLock lck(this);
	return CountValues<V>(value);
}


#endif //!defined(__RTC_SET_RESOURCECOLLECTION_H)
