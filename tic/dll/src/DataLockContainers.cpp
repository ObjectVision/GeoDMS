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
#include "TicPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

#include "DataLockContainers.h"

//----------------------------------------------------------------------
// class  : DataReadLockContainer
//----------------------------------------------------------------------

bool DataReadLockContainer::Add(const AbstrDataItem* adi, DrlType drlType)
{
	if	(!adi)
		return true;

	dms_assert(drlType != DrlType::Certain || SuspendTrigger::BlockerBase::IsBlocked()); // Callers responsibility
	dms_assert((drlType != DrlType::Suspendible) || !SuspendTrigger::DidSuspend()); // should have been acted upon

	if (!adi->PrepareDataUsage(drlType))
		return false;

	SharedPtr<const TreeItem> adiCurrItem = adi->GetCurrUltimateItem();
	dms_assert(adiCurrItem->GetInterestCount());
	if (!WaitForReadyOrSuspendTrigger(adiCurrItem))
		return false;

	dms_assert(!SuspendTrigger::DidSuspend()); // PRECONDITION for DataReadLock, POSTCONDITION for WaitForReadyOrSuspendTrigger

	DataReadLock newLock(adi);
	if (newLock.IsLocked())
		push_back(std::move(newLock));
	assert(!newLock.IsLocked()); // ownership transferred?
	return true;
}

//----------------------------------------------------------------------
// class  : DataWriteLockContainer
//----------------------------------------------------------------------

void DataWriteLockContainer::Add(AbstrDataItem* adi, dms_rw_mode rwm)
{
	m_Locks.push_back( DataWriteLock(adi, rwm)	);
}

void DataWriteLockContainer::Commit()
{
	for(auto i = m_Locks.begin(), e = m_Locks.end(); i!=e; ++i)
		i->Commit();
}

