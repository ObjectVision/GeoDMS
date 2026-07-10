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

#include "ptr/InterestHolders.h"

#if !defined(__TIC_ITEMLOCKS_H)
#define __TIC_ITEMLOCKS_H

#include "act/ActorLock.h"
#include "act/UpdateMark.h"
struct OperationContext;
struct DataController;

//----------------------------------------------------------------------
// ItemLocks
//----------------------------------------------------------------------

struct try_token_t {};
constexpr try_token_t try_token;

struct ItemReadLock
{
	TIC_CALL ItemReadLock() noexcept; 
	TIC_CALL ItemReadLock(const TreeItem* item);
	TIC_CALL ItemReadLock(SharedTreeItemInterestPtr&& rhs);
	TIC_CALL ItemReadLock(SharedTreeItemInterestPtr&& rhs, try_token_t);
	TIC_CALL ItemReadLock(ItemReadLock&& rhs)  noexcept;
	TIC_CALL ~ItemReadLock() noexcept;

	// NOT '= default': the read-lock release (cs_lock::ReadFree + s_SessionUsageCounter.unlock_shared) lives in
	// ~ItemReadLock, so a defaulted move-assignment would only reset-move m_Ptr (SharedTreeItemInterestPtr =
	// std reset-not-swap) and LEAK the shared usage lock it replaces (s_SessionUsageCounter.m_Count stays > 0,
	// which then hangs the EnableAutoDelete worker-drain at teardown). The old intrusive SharedPtr move-assign
	// was swap-based, so '= default' used to route the old lock through the moved-from temporary's dtor; std
	// semantics broke that. Release first. (Same fix as ItemWriteLock / DataReadLockAtom / DataWriteLock.)
	TIC_CALL ItemReadLock& operator = (ItemReadLock&& rhs) noexcept;

	ItemReadLock(const ItemReadLock& rhs) = delete;
	void operator = (const ItemReadLock& rhs) = delete;

	bool has_ptr() const { return m_Ptr.has_ptr(); }

private:
	void releaseHeldLock() noexcept; // shared by ~ItemReadLock and move-assignment
public:

	// Approach A (matches the reversed parent<-child ownership): a read lock locks ONLY the item to be read
	// (so it never bumps m_ItemCount on an ancestor that this lock does not own). To still honor "do not read a
	// descendant of an item being (re)produced", cs_lock::ReadLock AWAITS any in-progress write lock on a cache
	// ancestor (joining its producer) without locking it -- a write-locked ancestor is kept alive by its own
	// ItemWriteLock and owns the chain downward, so the await-walk is lifetime-safe.
	SharedTreeItemInterestPtr m_Ptr;
};

struct ItemWriteLock // held by creator to manage its unreadyness to prevent other threads from premature consumption
{
	TIC_CALL ItemWriteLock() noexcept;
	
	TIC_CALL ItemWriteLock(TreeItem* item, std::weak_ptr<OperationContext> ocb = std::weak_ptr<OperationContext>());
	TIC_CALL ~ItemWriteLock() noexcept;

	ItemWriteLock(ItemWriteLock&& rhs) = default;
	// NOT '= default': the write-lock release (treeitem_production_task::unlock_unique) lives in ~ItemWriteLock,
	// so a defaulted move-assignment would only overwrite m_ItemPtr and LEAK the lock it replaces (m_ItemCount
	// stays < 0). Callers rely on `lock = ItemWriteLock()` / `lock = std::move(other)` to RELEASE the prior lock
	// (e.g. ShvUtils CreateNonzeroJenksFisherBreakAttr releasing the palette-domain write lock). Release first.
	TIC_CALL ItemWriteLock& operator = (ItemWriteLock&& rhs) noexcept;

	// don't calll these
	ItemWriteLock(const ItemWriteLock&) = delete;
	void operator = (const ItemWriteLock& rhs) = delete;

	operator bool() const { return has_ptr(); }
	const TreeItem* GetItem() const { return m_ItemPtr.get(); }
	TIC_CALL std::shared_ptr<OperationContext> GetProducer() const
	{
		dms_assert(has_ptr());
		if (!has_ptr())
			return {};
		return m_ItemPtr->m_Producer.lock();
	}


private:
	bool has_ptr() const { return m_ItemPtr != nullptr; }
	void releaseHeldLock() noexcept; // shared by ~ItemWriteLock and move-assignment

	std::shared_ptr<const TreeItem> m_ItemPtr;
};

TIC_CALL Int32 GetItemLockCount(const TreeItem* item);
TIC_CALL bool IsReadLocked(const TreeItem* item);
TIC_CALL bool IsCalculating(const TreeItem* item);
TIC_CALL bool IsDataCurrCompleted(const TreeItem* item);
TIC_CALL bool IsDataCurrReady(const TreeItem* item);
TIC_CALL bool IsDataCurrStandby(const TreeItem* item);
TIC_CALL bool IsDataReady(const TreeItem* item);
TIC_CALL bool IsAllDataCurrStandby(const TreeItem* item);
TIC_CALL bool IsAllInterestedCalculatingOrDataReady(const TreeItem* item);
TIC_CALL bool CheckAllSubDataReady(const TreeItem* item);
TIC_CALL bool IsCalculatingOrReady(const TreeItem* item);
TIC_CALL bool CheckCalculatingOrReady(const TreeItem* item);
TIC_CALL bool IsCalculatingOrReady(const DataController* dc, const TreeItem* cacheRoot, const TreeItem* cacheItem);
TIC_CALL bool IsInWriteLock(const TreeItem* item);
TIC_CALL bool WaitForReadyOrSuspendTrigger(const TreeItem* ti);
TIC_CALL bool WaitReady(const TreeItem* item);
TIC_CALL bool RunTask(const TreeItem* item);
inline   bool CheckDataReady(const TreeItem* item) { return IsDataReady(item); } // Obsolete?

TIC_CALL std::shared_ptr<OperationContext> GetOperationContext(const TreeItem* item);


#endif //!defined(__TIC_ITEMLOCKS_H)
