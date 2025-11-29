// Copyright (C) 1998-2025 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "TicPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

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
#include "TicInterface.h"
#include "TreeItemProps.h"

#include "stg/StorageClass.h"

TIC_CALL AppendTreeFromConfigurationFuncPtr s_AppendTreeFromConfigurationPtr = nullptr;

//////////////////////////////////////////////////////////////////////
// MmdStorageManager implementation
//////////////////////////////////////////////////////////////////////

SharedStr MmdStorageManager::GetFullFileName(CharPtr name) const
{
	return DelimitedConcat(GetNameStr().c_str(), MakeFileName(name).c_str());
}

FileDateTime MmdStorageManager::GetLastChangeDateTime(const TreeItem* storageHolder, CharPtr path) const
{
	if (DoesExist(storageHolder)) // TODO: lock deze file vanaf hier.
	{
		m_FileTime = GetFileOrDirDateTime(GetFullFileName(path));
	}
	return m_FileTime; 
}

bool MmdStorageManager::DoCheckExistence(const TreeItem* storageHolder, const TreeItem* storageItem) const
{
	if (!storageItem)
		return AbstrStorageManager::DoCheckExistence(storageHolder, storageItem);

	auto relName = storageItem->GetRelativeName(storageHolder);
	return IsFileOrDirAccessible(GetFullFileName(relName.c_str()));
}

void MmdStorageManager::DoUpdateTree(const TreeItem* storageHolder, TreeItem* curr, SyncMode sm) const
{
	if (curr != storageHolder) // only update the root item
		return;
	if (curr->HasCalculator()) // don't read schema info if the item has a calculator; this is the production case
		return;
	if (curr->_GetFirstSubItem() && !storageReadOnlyPropDefPtr->GetValue(storageHolder))
		return; 

	auto dictFileName = GetFullFileName("0Dictionary.dms");

	if (!IsFileOrDirAccessible(dictFileName))
		return;
	if (!s_AppendTreeFromConfigurationPtr)
		throwErrorD("MmdStorageManager::DoUpdateTree", "s_AppendTreeFromConfigurationPtr is not set");

	s_AppendTreeFromConfigurationPtr(dictFileName.c_str(), curr);
}

void MmdStorageManager::DoWriteTree(const TreeItem* storageHolder)
{
	if (!storageHolder)
		return;

	ExportMetaInfo(storageHolder, storageHolder);

	auto dictFileName = GetFullFileName("0Dictionary.dms");

	auto osb = FileOutStreamBuff(dictFileName, true);
	auto out = OutStream_DMS(&osb, calcRulePropDefPtr);

	TreeItem_XML_Dump(storageHolder, &out, false);
}

bool IsInMMD(const AbstrDataItem* cacheItem)
{
	auto configItem = SharedPtr<const AbstrDataItem>((cacheItem->m_BackRef && IsDataItem(cacheItem->m_BackRef)) ? AsDataItem(cacheItem->m_BackRef) : cacheItem);
	if (auto sp = configItem->GetCurrStorageParent(true))
	{
		auto sm = sp->GetStorageManager();
		assert(sm);
		if (auto mmd = dynamic_cast<MmdStorageManager*>(sm))
			return true;
	}
	return false;
}

//----------------------------------------------------------------------
// instantiation and registration
//----------------------------------------------------------------------

IMPL_DYNC_STORAGECLASS(MmdStorageManager, "MMD")
