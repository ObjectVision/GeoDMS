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

/* REMOVE
MmdStorageManager::MmdStorageManager()
{
}

MmdStorageManager::~MmdStorageManager()
{
//	CloseStorage();
	assert(!m_MmdLockFile.IsOpen());
}
*/

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

/* REMOVE
void MmdStorageManager::DoOpenStorage(const StorageMetaInfo& smi, dms_rw_mode rwMode) const
{
	assert(!IsOpen());
	assert(rwMode != dms_rw_mode::unspecified);

	SharedStr fileName = DelimitedConcat(GetNameStr().c_str(), "LockFile.fss");
	if (rwMode > dms_rw_mode::read_only)
		m_MmdLockFile.OpenRw(fileName, nullptr, 0, rwMode, true, false, true);
	else
		m_MmdLockFile.OpenForRead(fileName, nullptr, true, false, true);
}

void MmdStorageManager::DoCloseStorage (bool mustCommit) const
{
	m_MmdLockFile.CloseFile();
}
*/

//----------------------------------------------------------------------
// instantiation and registration
//----------------------------------------------------------------------

IMPL_DYNC_STORAGECLASS(MmdStorageManager, "MMD")
