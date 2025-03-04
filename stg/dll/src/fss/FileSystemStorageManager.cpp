// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "StoragePCH.h"
#include "ImplMain.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)


// FileSystemStorageManager.cpp: implementation of the FileSystemStorageManager class.
//
//////////////////////////////////////////////////////////////////////

#include "fss/FileSystemStorageManager.h"

#include "dbg/debug.h"
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
// FileSystemStorageManager implementation
//////////////////////////////////////////////////////////////////////

FileSystemStorageManager::~FileSystemStorageManager()
{
	CloseStorage();
	assert(!m_FssLockFile.IsOpen());
}

void FileSystemStorageManager::DropStream(const TreeItem* item, CharPtr path)
{
	assert(item);

	reportF(SeverityTypeID::ST_MajorTrace, "Drop  fss(%s,%s)", GetNameStr().c_str(), path);

	KillFileOrDir(GetFullFileName(path));
}

SharedStr FileSystemStorageManager::GetFullFileName(CharPtr name) const
{
	return DelimitedConcat(GetNameStr().c_str(), MakeDataFileName(name).c_str());
}

FileDateTime FileSystemStorageManager::GetLastChangeDateTime(const TreeItem* storageHolder, CharPtr path) const
{
	if (DoesExist(storageHolder)) // TODO: lock deze file vanaf hier.
		m_FileTime = GetFileOrDirDateTime(GetFullFileName(path));

	return m_FileTime; 
}

std::unique_ptr<OutStreamBuff> FileSystemStorageManager::DoOpenOutStream(const StorageMetaInfo& smi, CharPtr path, tile_id t)
{
	const AbstrDataItem* adi = AsDynamicDataItem(smi.CurrRI());

	assert(!m_IsReadOnly);

//	reportF(MsgCategory::storage_write, SeverityTypeID::ST_MajorTrace, "Write fss(%s,%s)", GetNameStr().c_str(), path);

	SharedStr fullName = GetFullFileName(path); 
	if (adi)
	{
		assert(t != no_tile);
		const AbstrDataObject* ado = adi->GetRefObj();

		return std::make_unique<MappedFileOutStreamBuff>(fullName, ado->GetNrTileBytesNow(t, true));
	}
	assert(t == no_tile);
	return std::make_unique<FileOutStreamBuff>( fullName, false);
}

std::unique_ptr<InpStreamBuff> FileSystemStorageManager::DoOpenInpStream(const StorageMetaInfo& smi, CharPtr path) const
{
//	reportF(MsgCategory::storage_read, SeverityTypeID::ST_MajorTrace, "Read  fss(%s,%s)", GetNameStr().c_str(), path);

	assert(IsOpen());

	auto result = std::make_unique<MappedFileInpStreamBuff>(GetFullFileName(path), false, false);

	if (!result->IsOpen())
		return {};
	return result;
}

void FileSystemStorageManager::DoOpenStorage(const StorageMetaInfo& smi, dms_rw_mode rwMode) const
{
	dms_assert(!IsOpen());
	dms_assert(rwMode != dms_rw_mode::unspecified);

	SharedStr fileName = DelimitedConcat(GetNameStr().c_str(), "LockFile.fss");
	if (rwMode > dms_rw_mode::read_only)
		m_FssLockFile.OpenRw(fileName, 0, rwMode, true, false, true);
	else
		m_FssLockFile.OpenForRead(fileName, true, false, true);
}

void FileSystemStorageManager::DoCloseStorage (bool mustCommit) const
{
	m_FssLockFile.CloseFile();
}

//----------------------------------------------------------------------
// instantiation and registration
//----------------------------------------------------------------------

IMPL_DYNC_STORAGECLASS(FileSystemStorageManager, "FSS")
