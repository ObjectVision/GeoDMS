// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "StoragePCH.h"
#include "act/UpdateMark.h" // UpdateMarker

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)


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
#include "utl/splitPath.h"

#include "AbstrDataItem.h"
#include "DataArray.h"
#include "DataLocks.h"
#include "LispTreeType.h" // token::storage_read_attr (#587)
#include "Unit.h"
#include "UnitClass.h"

#include "FilePtrHandle.h"


// ================ Read / Write data
FileResult StrStorageManager::ReadDataItem (StorageMetaInfoPtr smi, AbstrDataObject* borrowedReadResultHolder, tile_id t)
{
	MG_CHECK(t == 0);

	std::size_t dataSize; 
	AbstrDataObject::data_write_begin_handle dataBeginHolder;
	void* dataBegin;
	const TreeItem* storageHolder = smi->StorageHolder();
	const AbstrDataItem* adi = smi->CurrRD().get(); // the described item: file names and composition come from the configuration; the data goes into ado (#587)
	assert(adi);
	AbstrDataObject* ado = borrowedReadResultHolder;
	MG_CHECK(ado);
	DataArray<SharedStr>* sdo = dynamic_cast<DataArray<SharedStr>*>(ado);
	for (SizeT i=0, n = GetNrFiles(storageHolder, adi); i!=n; ++i) {
		FilePtrHandle file;
		auto strFileName = GetFileName(storageHolder, adi, i);
		auto r = file.OpenFH(strFileName, FCM_OpenReadOnly, false, NR_PAGES_DIRECTIO);
		if (!r)
			r.Throw("StrStorageManager");

		dms::filesize_t fileSize = file.GetFileSize();
		if (sdo)
		{
			auto sdoData = sdo->GetWritableTile(t, dms_rw_mode::read_write);
			MG_CHECK(sdoData.size() == n); // n is the count of the FileName attribute's domain; sdoData[i] below is written for i < n

			sdoData[i].resize_uninitialized(fileSize MG_DEBUG_ALLOCATOR_SRC("StrStorageManager::ReadDataItem"));
			dataBegin = sdoData[i].begin();
			dataSize  = fileSize;
		}
		else
		{
			MG_CHECK(adi->GetValueComposition() == ValueComposition::Single);
			dataBeginHolder = ado->GetDataWriteBegin(i, dms_rw_mode::write_only_all);
			dataBegin = dataBeginHolder.get_ptr();
			dataSize  = ado->GetNrTileBytesNow(i, false);

			MG_CHECK(dataSize <= fileSize);
		}
		MG_CHECK(dataSize == 0 || fread(dataBegin, dataSize, 1, file) == 1);
	}
	return {};
}

FileResult StrStorageManager::WriteDataItem(StorageMetaInfoPtr&& smiHolder)
{
	auto smi = smiHolder.get();
	StorageWriteHandle hnd(this, std::move(smiHolder));

	const AbstrDataItem  * adi = smi->CurrRD().get();
	const AbstrDataObject* ado = adi->GetRefObj().get();
	const TreeItem* storageHolder = smi->StorageHolder();

	auto sda = const_array_dynacast<SharedStr>(ado);
	if (sda)
	{
		auto sdData = sda->GetDataRead();
		auto n = GetNrFiles(smi->StorageHolder(), adi);
		MG_CHECK(sdData.size() == n);
		for (SizeT i = 0; i != n; ++i)
		{

			FilePtrHandle file;
			auto r = file.OpenFH(GetFileName(storageHolder, adi, i), FCM_CreateAlways, false, NR_PAGES_DIRECTIO);
			if (!r)
				return r;

			auto dataBegin = sdData[i].begin();
			auto dataSize  = sdData[i].size();
			// a short write (disk full, share dropped) left a truncated file and reported success;
			// fwrite with a size of 0 answers 0, hence the guard, as in ReadDataItem
			MG_CHECK(dataSize == 0 || fwrite(dataBegin, dataSize, 1, file) == 1);
		}
	}
	else
	{
		auto n = GetNrFiles(smi->StorageHolder(), adi);
		for (SizeT i = 0; i != n; ++i)
		{
			FilePtrHandle file;
			auto r = file.OpenFH(GetFileName(storageHolder, adi, i), FCM_CreateAlways, false, NR_PAGES_DIRECTIO);
			if (!r)
				return r;
			MG_CHECK(adi->GetValueComposition() == ValueComposition::Single);

			auto dataBegin = ado->GetDataReadBegin(i); // TODO G8: make dataBegin a tile handle with a void pointer
			auto dataSize = ado->GetNrTileBytesNow(i, false);
			MG_CHECK(dataSize == 0 || fwrite(dataBegin, dataSize, 1, file) == 1);
		}
	}
	return {};
}

void StrStorageManager::DoUpdateTree(const TreeItem* storageHolder, TreeItem* curr, SyncMode sm) const
{
	NonmappableStorageManager::DoUpdateTree(storageHolder, curr, sm);

	if (curr != storageHolder)
		return; // noop

	if (	!	IsDataItem(storageHolder)
	   ||	(	(	!	AsDataItem(storageHolder)->HasVoidDomainGuarantee() )
			&&	AsDataItem(storageHolder)->GetAbstrValuesUnit()->GetUnitClass() == Unit<SharedStr>::GetStaticClass()
			)
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
	assert(storageHolder == self);
	if (!m_FileNameAttr) {
		if (storageHolder == self)
			storageHolder = storageHolder->GetTreeParent().get();
		assert(storageHolder);
		auto fileNameItem = storageHolder->ResolveItemPath("FileName");
		if (!fileNameItem)
			storageHolder->throwItemError("StrFilesStorageManager requires an attribute<string> FileName with the same domain as this to be in its parent namespace");
		m_FileNameAttr = make_shared_tree(AsCheckedDataItem(fileNameItem).get(), existing_obj{});
	}
	return m_FileNameAttr.get();
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

// No GetMetaInfo override: base_type::GetMetaInfo (NonmappableStorageManager) merely builds the
// StorageMetaInfo and needs nothing from FileName. ReadDataItem/WriteDataItem each hold a
// (Prepared)DataReadLock on FileName for the operation; the read has FileName as an argument (below),
// the write reaches FileName through the ExportInfo visit.

// #587: the FileName attribute is an argument of the read, so that it is calculated before the read
// and is part of the read's identity: storage_read_attr(spec, name, domain, 'attr', vu, <FileName key>)
static LispRef AppendArg(LispRef args, LispRef arg)
{
	if (args.EndP())
		return LispRef(arg, LispRef());
	return LispRef(args.Left(), AppendArg(args.Right(), arg));
}

ReadCallSpec StrFilesStorageManager::DescribeReadCall(const TreeItem* storageHolder, const TreeItem* item) const
{
	auto result = base_type::DescribeReadCall(storageHolder, item);
	if (!result.operName || result.operName != token::storage_read_attr)
		return result;
	auto fileNameAttr = GetFileNameAttr(storageHolder, storageHolder);
	fileNameAttr->UpdateMetaInfo();
	result.args = AppendArg(result.args, fileNameAttr->GetCheckedKeyExpr());
	return result;
}

FileResult StrFilesStorageManager::ReadDataItem(StorageMetaInfoPtr smi, AbstrDataObject* borrowedReadResultHolder, tile_id t)
{
	DataReadLock drl(GetFileNameAttr(smi->StorageHolder(), smi->CurrRD().get()));
	return base_type::ReadDataItem(smi, borrowedReadResultHolder, t);
}

FileResult StrFilesStorageManager::WriteDataItem(StorageMetaInfoPtr&& smi)
{
	PreparedDataReadLock drl(GetFileNameAttr(smi->StorageHolder(), smi->CurrRD().get()), "@StrFilesStorageManager::WriteDataItem");
	return base_type::WriteDataItem(std::move(smi));
}

void StrFilesStorageManager::DoUpdateTree(const TreeItem* storageHolder, TreeItem* curr, SyncMode sm) const
{
	NonmappableStorageManager::DoUpdateTree(storageHolder, curr, sm);
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

