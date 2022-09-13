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
#pragma hdrstop

#include "AbstrStreamManager.h"

#include "act/TriggerOperator.h"
#include "dbg/Debug.h"
#include "ser/BinaryStream.h"
#include "utl/mySPrintF.h"

#include "AbstrDataItem.h"
#include "AbstrDataObject.h"
#include "DataLocks.h"
#include "AbstrUnit.h"
#include "ItemLocks.h"
#include "TiledRangeData.h"
#include "TileLock.h"
#include "TreeItem.h"
#include "TreeItemClass.h"
#include "TreeItemContextHandle.h"

// *****************************************************************************
// 
// AbstrStreamManager
//
// *****************************************************************************


AbstrStreamManager::AbstrStreamManager() // link VTBL in TIC dll
{}

std::unique_ptr<OutStreamBuff> AbstrStreamManager::OpenOutStream(const StorageMetaInfo& smi, CharPtr path, tile_id t)
{ 
	auto nameStr = GetNameStr();
	CDebugContextHandle dch1(nameStr.c_str(), "OpenOutStream", true);

	dms_assert(IsOpenForWrite());
	return DoOpenOutStream(smi, path, t); 
}

std::unique_ptr<InpStreamBuff> AbstrStreamManager::OpenInpStream(const StorageMetaInfo& smi, CharPtr path) const
{ 
	auto nameStr = GetNameStr();
	CDebugContextHandle dch1(nameStr.c_str(), "OpenInpStream", false);

	dms_assert(IsOpen());
	return DoOpenInpStream(smi, path);
}

SharedStr GetRelativeName(const StorageMetaInfo* smi)
{
	return smi->CurrRI()->GetRelativeName(smi->StorageHolder());
}

SharedStr GetRelativeName(const StorageMetaInfo* smi, tile_id t)
{
	SharedStr result = GetRelativeName(smi);
	if (t && t!=no_tile)
		result += myArrayPrintF<10>(".%d", t);
	return result;
}

bool AbstrStreamManager::ReadDataItem(const StorageMetaInfo& smi, AbstrDataObject* borrowedReadResultHolder, tile_id t)
{
	TreeItemContextHandle och1(smi.CurrWD(), "DataItem");
	dms_assert( DoesExist(smi.StorageHolder()) );
	auto f = OpenInpStream(smi, ::GetRelativeName(&smi, t).c_str() );
	if (! f )
		return false;

	BinaryInpStream ar(f.get()); 
	borrowedReadResultHolder->DoReadData(ar, t);

	return ! SuspendTrigger::DidSuspend();
}

bool AbstrStreamManager::WriteDataItem(StorageMetaInfoPtr&& smi)
{
	StorageWriteHandle hnd(std::move(smi));

	SharedPtr<const AbstrDataItem> adi = hnd.MetaInfo()->CurrRD();
	MG_CHECK(adi);

	SharedPtr<const AbstrDataObject> ado = adi->GetRefObj();
	MG_CHECK(ado);

	for (tile_id t=0, tn = ado->GetTiledRangeData()->GetNrTiles(); t!=tn; ++t)
	{
		auto ft = ado->GetFutureAbstrTile(t); // hold on to resource to avoid calculating twice
		auto f( OpenOutStream(*hnd.MetaInfo(), ::GetRelativeName(hnd.MetaInfo(), t).c_str(), t) );
		if (! f )
			return false;
			
		BinaryOutStream ar( f.get() ); 
		ado->DoWriteData(ar, t);
	}

	return true;
}

bool AbstrStreamManager::ReadUnitRange(const StorageMetaInfo& smi) const
{
	const TreeItem* storageHolder = smi.StorageHolder();
	dms_assert(storageHolder);

	dms_assert(IsOpen());
	dms_assert(DoesExist(smi.StorageHolder()));

	auto f = OpenInpStream(smi, ::GetRelativeName(&smi).c_str() );
	if ( !f )
		return false;
	dms_assert(smi.CurrRI() == smi.CurrRI()->GetCurrUltimateItem());
	const_cast<TreeItem*>(smi.CurrRI())->LoadBlobStream( f.get() );
	return true;
}

bool AbstrStreamManager::WriteUnitRange(StorageMetaInfoPtr&& smi)
{
	StorageWriteHandle storageHandle(std::move(smi)); 
	if (!IsOpenForWrite())
		return false; // or throw since there seems to be a problem and not a suspension

	auto f = OpenOutStream(*storageHandle.MetaInfo(), ::GetRelativeName(storageHandle.MetaInfo()).c_str(), no_tile);

	if (!f)
		return false; // or throw since there seems to be a problem and not a suspension

	const TreeItem* uti = storageHandle.MetaInfo()->CurrRI()->GetCurrRangeItem();
	dms_assert(uti->GetInterestCount());
	dms_assert(IsCalculatingOrReady(uti));
	ItemReadLock lock(uti); // or return false when Wait is requested.
	uti->StoreBlobStream( f.get() );
	return true;
}

void AbstrStreamManager::DoUpdateTree (const TreeItem* storageHolder, TreeItem* curr, SyncMode sm) const 
{
	AbstrStorageManager::DoUpdateTree(storageHolder, curr, sm);

	if (curr != storageHolder)
		return; // noop

	UpdateMarker::ChangeSourceLock changeStamp( storageHolder, "DoUpdateTree");
}
