// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "TicPCH.h"

// MmdStorageManager.cpp: implementation of the MmdStorageManager class.
//
//////////////////////////////////////////////////////////////////////

#include "stg/MemoryMappedDataStorageManager.h"

#include "dbg/debug.h"
#include "dbg/SeverityType.h"
#include "mci/ValueClass.h"
#include "ser/FileStreamBuff.h"
#include "utl/Environment.h"
#include "utl/mySPrintF.h"
#include "utl/SplitPath.h"
#include "xml/XMLOut.h"

#include "AbstrDataItem.h"
#include "AbstrDataObject.h"
#include "DataStoreManagerCaller.h"
#include "TreeItemProps.h"

#include "stg/StorageClass.h"

TIC_CALL AppendTreeFromConfigurationFuncPtr s_AppendTreeFromConfigurationPtr = nullptr;

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
	return DelimitedConcat(GetNameStr().c_str(), MakeFileName(name).c_str());
}

std::mutex sc_SFWAPtrAccess;

auto MmdStorageManager::GetSFWA() const->std::shared_ptr<SafeFileWriterArray>
{
	auto lock = std::lock_guard(sc_SFWAPtrAccess);
	if (!m_SFWA)
		m_SFWA = DSM::GetSafeFileWriterArray();
	return m_SFWA;
}

FileDateTime MmdStorageManager::GetLastChangeDateTime(const TreeItem* storageHolder, CharPtr path) const
{
	if (DoesExist(storageHolder)) // TODO: lock deze file vanaf hier.
	{
		auto sfwa = GetSFWA(); MG_CHECK(sfwa);
		m_FileTime = GetFileOrDirDateTime(sfwa->GetWorkingFileName(GetFullFileName(path), FCM_OpenReadOnly));
	}
	return m_FileTime; 
}

bool MmdStorageManager::DoCheckExistence(const TreeItem* storageHolder, const TreeItem* storageItem) const
{
	if (!storageItem)
		return AbstrStorageManager::DoCheckExistence(storageHolder, storageItem);

	auto relName = storageItem->GetRelativeName(storageHolder);
	auto sfwa = DSM::GetSafeFileWriterArray(); MG_CHECK(sfwa);
	return IsFileOrDirAccessible(sfwa.get()->GetWorkingFileName(GetFullFileName(relName.c_str()), FCM_OpenReadOnly));
}

void MmdStorageManager::DoUpdateTree(const TreeItem* storageHolder, TreeItem* curr, SyncMode sm) const
{
	if (curr != storageHolder)
		return;
	auto dirFileName = GetFullFileName("0Dictionary.dms");
	auto sfwa = DSM::GetSafeFileWriterArray(); MG_CHECK(sfwa);
	auto workingFileName = sfwa.get()->GetWorkingFileName(dirFileName, FCM_OpenReadOnly);

	if (!IsFileOrDirAccessible(workingFileName))
		return;
	if (!s_AppendTreeFromConfigurationPtr)
		throwErrorD("MmdStorageManager::DoUpdateTree", "s_AppendTreeFromConfigurationPtr is not set");

	s_AppendTreeFromConfigurationPtr(workingFileName.c_str(), curr);
}

void MmdStorageManager::DoWriteTree(const TreeItem* storageHolder)
{
	if (!storageHolder)
		return;

	auto dirFileName = GetFullFileName("0Dictionary.dms");
//	auto sfwa = DSM::GetSafeFileWriterArray(); MG_CHECK(sfwa);
//	auto osb = FileOutStreamBuff(dirFileName, sfwa.get(), true);

	auto osb = FileOutStreamBuff(dirFileName, nullptr, true);
	auto out = OutStream_DMS(&osb, calcRulePropDefPtr);

	storageHolder->XML_Dump(&out, false);
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
