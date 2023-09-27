// Copyright (C) 2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "StoragePCH.h"
#pragma hdrstop


// MmdStorageManager.cpp: implementation of the MmdStorageManager class.
//
//////////////////////////////////////////////////////////////////////

#include "mmd/MemoryMappedDataStorageManager.h"

#include "dbg/Debug.h"
#include "dbg/SeverityType.h"
#include "mci/ValueClass.h"
#include "ser/FileStreamBuff.h"
#include "utl/Environment.h"
#include "utl/mySPrintF.h"
#include "utl/SplitPath.h"

#include "AbstrDataItem.h"
#include "AbstrDataObject.h"
#include "DataStoreManagerCaller.h"

#include "stg/StorageClass.h"

//////////////////////////////////////////////////////////////////////
// MmdStorageManager implementation
//////////////////////////////////////////////////////////////////////

MmdStorageManager::~MmdStorageManager()
{
	CloseStorage();
	dms_assert(!m_FssLockFile.IsOpen());
}

void MmdStorageManager::DropStream(const TreeItem* item, CharPtr path)
{
	assert(item);

	reportF(SeverityTypeID::ST_MajorTrace, "Drop  fss(%s,%s)", GetNameStr().c_str(), path);

	auto sfwa = DSM::GetSafeFileWriterArray();
	if (sfwa)
		sfwa->OpenOrCreate(GetFullFileName(path).c_str(), FCM_Delete);
}

SharedStr MmdStorageManager::GetFullFileName(CharPtr name) const
{
	return DelimitedConcat(GetNameStr().c_str(), MakeDataFileName(name).c_str());
}

FileDateTime MmdStorageManager::GetLastChangeDateTime(const TreeItem* storageHolder, CharPtr path) const
{
	if (DoesExist(storageHolder)) // TODO: lock deze file vanaf hier.
	{
		auto sfwa = DSM::GetSafeFileWriterArray(); MG_CHECK(sfwa);
		m_FileTime = GetFileOrDirDateTime(sfwa.get()->GetWorkingFileName(GetFullFileName(path), FCM_OpenReadOnly));
	}
	return m_FileTime; 
}

std::unique_ptr<OutStreamBuff> MmdStorageManager::DoOpenOutStream(const StorageMetaInfo& smi, CharPtr path, tile_id t)
{
	const AbstrDataItem* adi = AsDynamicDataItem(smi.CurrRI());

	dms_assert(!m_IsReadOnly);

	reportF(MsgCategory::storage_write, SeverityTypeID::ST_MajorTrace, "Write fss(%s,%s)", GetNameStr().c_str(), path);

	SharedStr fullName = GetFullFileName(path); 
	auto sfwa = DSM::GetSafeFileWriterArray();
	if (!sfwa)
		return {};

	if (adi)
	{
		assert(t != no_tile);
		const AbstrDataObject* ado = adi->GetRefObj();

		return std::make_unique<MappedFileOutStreamBuff>(fullName, sfwa.get(), ado->GetNrTileBytesNow(t, true));
	}
	assert(t == no_tile);
	return std::make_unique<FileOutStreamBuff>( fullName, sfwa.get(), false);
}

std::unique_ptr<InpStreamBuff> MmdStorageManager::DoOpenInpStream(const StorageMetaInfo& smi, CharPtr path) const
{
	reportF(MsgCategory::storage_read, SeverityTypeID::ST_MajorTrace, "Read  fss(%s,%s)", GetNameStr().c_str(), path);

	dms_assert(IsOpen());

	auto sfwa = DSM::GetSafeFileWriterArray();
	if (!sfwa)
		return {};

	auto result = std::make_unique<MappedFileInpStreamBuff>(GetFullFileName(path), sfwa.get(), false, false);

	if (!result->IsOpen())
		return {};
	return result;
}

void MmdStorageManager::DoOpenStorage(const StorageMetaInfo& smi, dms_rw_mode rwMode) const
{
	dms_assert(!IsOpen());
	dms_assert(rwMode != dms_rw_mode::unspecified);

	SharedStr fileName = DelimitedConcat(GetNameStr().c_str(), "LockFile.fss");
	if (rwMode > dms_rw_mode::read_only)
		m_FssLockFile.OpenRw(fileName, nullptr, 0, rwMode, true, false, true);
	else
		m_FssLockFile.OpenForRead(fileName, nullptr, true, false, true);
}

void MmdStorageManager::DoCloseStorage (bool mustCommit) const
{
	m_FssLockFile.CloseFile();
}

//----------------------------------------------------------------------
// instantiation and registration
//----------------------------------------------------------------------

IMPL_DYNC_STORAGECLASS(MmdStorageManager, "MMD")
