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
#include "StoragePCH.h"
#pragma hdrstop


// *****************************************************************************
//
// Implementations of StrStorageManager
//
// *****************************************************************************

#include "StrStorageManager.h"

#include "act/ActorVisitor.h"
#include "dbg/debug.h"
#include "mci/CompositeCast.h"
#include "mci/ValueClass.h"
#include "stg/StorageClass.h"
#include "utl/SplitPath.h"

#include "AbstrDataItem.h"
#include "DataArray.h"
#include "DataLocks.h"
#include "DataStoreManagerCaller.h"
#include "Unit.h"
#include "UnitClass.h"

#include "FilePtrHandle.h"


// ================ Read / Write data
bool StrStorageManager::ReadDataItem (const StorageMetaInfo& smi, AbstrDataObject* borrowedReadResultHolder, tile_id t)
{
	MG_CHECK(t == 0);

	std::size_t dataSize; 
	AbstrDataObject::data_write_begin_handle dataBeginHolder;
	void* dataBegin;
	const TreeItem* storageHolder = smi.StorageHolder();
	AbstrDataItem* adi = smi.CurrWD();
	dms_assert(adi);
	AbstrDataObject* ado = borrowedReadResultHolder;
	dms_assert(ado);
	DataArray<SharedStr>* sdo = dynamic_cast<DataArray<SharedStr>*>(ado);
	for (SizeT i=0, n = GetNrFiles(storageHolder, adi); i!=n; ++i) {
		FilePtrHandle file;
		if (! file.OpenFH(GetFileName(storageHolder, adi, i), DSM::GetSafeFileWriterArray(storageHolder), FCM_OpenReadOnly, false, NR_PAGES_DIRECTIO))
			return false;

		dms::filesize_t fileSize = file.GetFileSize();
		if (sdo)
		{
			auto sdoData = sdo->GetWritableTile(t, dms_rw_mode::read_write);
			dms_assert(sdoData.size() == n);

			sdoData[i].resize_uninitialized(fileSize);
			dataBegin = sdoData[i].begin();
			dataSize  = fileSize;
		}
		else
		{
			MG_CHECK(adi->GetValueComposition() == ValueComposition::Single);
			dataBeginHolder = ado->GetDataWriteBegin(i);
			dataBegin = dataBeginHolder.get_ptr();
			dataSize  = ado->GetNrTileBytesNow(i, false);

			MG_CHECK(dataSize <= fileSize);
		}
		fread(dataBegin, dataSize, 1, file);
	}
	return true;
}

bool StrStorageManager::WriteDataItem(StorageMetaInfoPtr&& smiHolder)
{
	auto smi = smiHolder.get();
	StorageWriteHandle hnd(std::move(smiHolder));

	const AbstrDataItem  * adi = smi->CurrRD();
	const AbstrDataObject* ado = adi->GetRefObj();
	const TreeItem* storageHolder = smi->StorageHolder();

	auto sda = const_array_dynacast<SharedStr>(ado);
	if (sda)
	{
		auto sdData = sda->GetDataRead();
		auto n = GetNrFiles(smi->StorageHolder(), adi);
		dms_assert(sdData.size() == n);
		for (SizeT i = 0; i != n; ++i)
		{
			FilePtrHandle file;
			if (!file.OpenFH(GetFileName(storageHolder, adi, i), DSM::GetSafeFileWriterArray(storageHolder), FCM_CreateAlways, false, NR_PAGES_DIRECTIO))
				return false;

			auto dataBegin = sdData[i].begin();
			auto dataSize  = sdData[i].size();
			fwrite(dataBegin, dataSize, 1, file);
		}
	}
	else
	{
		auto n = GetNrFiles(smi->StorageHolder(), adi);
		for (SizeT i = 0; i != n; ++i)
		{
			FilePtrHandle file;
			if (!file.OpenFH(GetFileName(storageHolder, adi, i), DSM::GetSafeFileWriterArray(storageHolder), FCM_CreateAlways, false, NR_PAGES_DIRECTIO))
				return false;
			MG_CHECK(adi->GetValueComposition() == ValueComposition::Single);

			auto dataBegin = ado->GetDataReadBegin(i); // TODO G8: maake van dataBegin een tileHandle met void pointer
			auto dataSize = ado->GetNrTileBytesNow(i, false);
			fwrite(dataBegin, dataSize, 1, file);
		}
	}
	return true;
}


// Unclear in this context, but obligatory
void StrStorageManager::DoUpdateTree(const TreeItem* storageHolder, TreeItem* curr, SyncMode sm) const
{
	AbstrStorageManager::DoUpdateTree(storageHolder, curr, sm);

	if (curr != storageHolder)
		return; // noop

	if (	!	IsDataItem(storageHolder)
	   ||	(	!	AsDataItem(storageHolder)->HasVoidDomainGuarantee() )
			&&	AsDataItem(storageHolder)->GetAbstrValuesUnit()->GetUnitClass() == Unit<SharedStr>::GetStaticClass()
	   )
		throwItemError("StrStorageManager requires as storageHolder a parameter<SharedStr> or an attribute<valuesUnit with fixed element size>");

	UpdateMarker::ChangeSourceLock changeStamp( storageHolder, "DoUpdateTree");
	curr->SetFreeDataState(true);
}

SharedStr StrStorageManager::GetFileName(const TreeItem* storageHolder, const TreeItem* curr, SizeT recNo) const
{
	return GetNameStr();
}

SizeT StrStorageManager::GetNrFiles (const TreeItem* storageHolder, const TreeItem* curr) const
{
	return 1;
}


// *****************************************************************************
//
// Implementations of StrFilesStorageManager
//
// *****************************************************************************

const AbstrDataItem* StrFilesStorageManager::GetFileNameAttr(const TreeItem* storageHolder, const TreeItem* self) const
{
	dms_assert(storageHolder == self);
	if (!m_FileNameAttr) {
		if (storageHolder == self)
			storageHolder = storageHolder->GetTreeParent();
		dms_assert(storageHolder);
		auto fileNameItem = storageHolder->FindItem("FileName");
		if (!fileNameItem)
			storageHolder->throwItemError("StrFilesStorageManager requires an attribute<string> FileName with the same domain as this to be in its parent namespace");
		m_FileNameAttr = checked_cast<const AbstrDataItem*>(fileNameItem);
	}
	return m_FileNameAttr;
}

CharPtrRange AsRange(const SA_ConstReference<char>& rhs) { return CharPtrRange(rhs.begin(), rhs.end()); }

SharedStr StrFilesStorageManager::GetFileName(const TreeItem* storageHolder, const TreeItem* self, SizeT recNo) const
{ 
	return DelimitedConcat(GetNameStr().AsRange(),
		AsRange( const_array_cast<SharedStr>(GetFileNameAttr(storageHolder, self))->GetDataRead()[recNo] )
	);
}

SizeT StrFilesStorageManager::GetNrFiles (const TreeItem* storageHolder, const TreeItem* curr) const
{
	return GetFileNameAttr(storageHolder, curr)->GetAbstrDomainUnit()->GetCount();
}

StorageMetaInfoPtr StrFilesStorageManager::GetMetaInfo(const TreeItem* storageHolder, TreeItem* curr, StorageAction sa) const
{
	dms_assert(storageHolder == curr);
	dms_assert(IsDataItem(curr));
	if (!IsDataItem(curr) || !GetFileNameAttr(storageHolder, curr)->PrepareDataUsage(DrlType::Certain))
		return{};
	return base_type::GetMetaInfo(storageHolder, curr, sa);
}

bool StrFilesStorageManager::ReadDataItem(const StorageMetaInfo& smi, AbstrDataObject* borrowedReadResultHolder, tile_id t)
{
	DataReadLock drl(GetFileNameAttr(smi.StorageHolder(), smi.CurrRD()));
	return base_type::ReadDataItem(smi, borrowedReadResultHolder, t);
}

bool StrFilesStorageManager::WriteDataItem(StorageMetaInfoPtr&& smi)
{
	PreparedDataReadLock drl(GetFileNameAttr(smi->StorageHolder(), smi->CurrRD()));
	return base_type::WriteDataItem(std::move(smi));
}

ActorVisitState StrFilesStorageManager::VisitSuppliers(SupplierVisitFlag svf, const ActorVisitor& visitor, const TreeItem* storageHolder, const TreeItem* self) const
{
	if (IsDataItem(self))
	{
		const TreeItem* fileNameAttr = GetFileNameAttr(storageHolder, self);
		if (fileNameAttr != self && fileNameAttr != storageHolder)
			if (visitor.Visit(fileNameAttr) != AVS_Ready)
				return AVS_SuspendedOrFailed;
	}
	return base_type::VisitSuppliers(svf, visitor, storageHolder, self);
}

void StrFilesStorageManager::DoUpdateTree(const TreeItem* storageHolder, TreeItem* curr, SyncMode sm) const
{
	AbstrStorageManager::DoUpdateTree(storageHolder, curr, sm);
	m_FileNameAttr.reset(); // 

	if (curr != storageHolder)
		throwItemError("StrFilesStorageManager does not allow sub items");
	if (	!	IsDataItem(storageHolder) 
		||		AsDataItem(storageHolder)->GetAbstrValuesUnit()->GetUnitClass() != Unit<SharedStr>::GetStaticClass()
	)
		throwItemError("StrFilesStorageManager requires an attribute<SharedStr> as storageManager");
	AsDataItem(storageHolder)->GetAbstrDomainUnit()->UnifyDomain(GetFileNameAttr(storageHolder, curr)->GetAbstrDomainUnit()
		, "Domain of StorageHolder", "Domain of attribute<String> FileName", UM_Throw);
}

IMPL_DYNC_STORAGECLASS(StrStorageManager, "str");
IMPL_DYNC_STORAGECLASS(StrFilesStorageManager, "strfiles");

