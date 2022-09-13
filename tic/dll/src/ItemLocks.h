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

	ItemReadLock& operator = (ItemReadLock&& rhs) noexcept = default;

	ItemReadLock(const ItemReadLock& rhs) = delete;
	void operator = (const ItemReadLock& rhs) = delete;

	bool has_ptr() const { return m_Ptr.has_ptr(); }

	SharedTreeItemInterestPtr m_Ptr;
};

struct ItemWriteLock // held by creator to manage its unreadyness to prevent other threads from premature consumption
{
	TIC_CALL ItemWriteLock() noexcept;
	
	TIC_CALL ItemWriteLock(TreeItem* item, std::weak_ptr<OperationContext> ocb = std::weak_ptr<OperationContext>());
	TIC_CALL ~ItemWriteLock() noexcept;

	ItemWriteLock(ItemWriteLock&& rhs) = default;
	ItemWriteLock& operator = (ItemWriteLock&& rhs) = default;

	// don't calll these
	ItemWriteLock(const ItemWriteLock&) = delete;
	void operator = (const ItemWriteLock& rhs) = delete;

	operator bool() const { return has_ptr(); }
	const TreeItem* GetItem() const { return m_ItemPtr;  }
	TIC_CALL std::shared_ptr<OperationContext> GetProducer() const
	{
		dms_assert(has_ptr());
		if (!has_ptr())
			return {};
		return m_ItemPtr->m_Producer.lock();
	}


private:
	bool has_ptr() const { return m_ItemPtr; }

	SharedPtr<const TreeItem> m_ItemPtr;
};

TIC_CALL Int32 GetItemLockCount(const TreeItem* item);
TIC_CALL bool IsReadLocked(const TreeItem* item);
TIC_CALL bool IsCalculating(const TreeItem* item);
TIC_CALL bool IsDataCurrReady(const TreeItem* item);
TIC_CALL bool IsDataReady(const TreeItem* item);
TIC_CALL bool IsAllDataReady(const TreeItem* item);
TIC_CALL bool CheckDataReady(const TreeItem* item);
TIC_CALL bool CheckAllSubDataReady(const TreeItem* item);
TIC_CALL bool IsAllocated(const TreeItem* item);
//REMOVE TIC_CALL bool IsDcReady(const DataController* dc, const TreeItem* cacheRoot, const TreeItem* cacheItem);
TIC_CALL bool IsCalculatingOrReady(const TreeItem* item);
TIC_CALL bool CheckCalculatingOrReady(const TreeItem* item);
TIC_CALL bool IsCalculatingOrReady(const DataController* dc, const TreeItem* cacheRoot, const TreeItem* cacheItem);
TIC_CALL bool IsInWriteLock(const TreeItem* item);
TIC_CALL bool WaitForReadyOrSuspendTrigger(const TreeItem* ti);
TIC_CALL bool WaitReady(const TreeItem* item);
TIC_CALL bool RunTask(const TreeItem* item);

TIC_CALL std::shared_ptr<OperationContext> GetOperationContext(const TreeItem* item);


#endif //!defined(__TIC_ITEMLOCKS_H)
