// Copyright (C) 1998-2026 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "TicPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

#include "AbstrStorageManager.h"
#include "StorageInterface.h"
#include "TreeItemClass.h"
#include "TreeItemContextHandle.h"

#include "ser/AsString.h"
#include "dbg/DmsCatch.h"

// *****************************************************************************
// Section:     DMS_StorageManager functions
// *****************************************************************************

TIC_CALL bool DMS_CONV DMS_TreeItem_StorageDoesExist(const TreeItem* storageHolder, CharPtr storageName, CharPtr storageType)
{
	DMS_CALL_BEGIN
		TreeItemContextHandle checkPtr(storageHolder, 0, "DMS_TreeItem_StorageDoesExist");

		MG_PRECONDITION(storageType);
		MG_PRECONDITION(storageName);
		
		return AbstrStorageManager::DoesExistEx(storageName, GetTokenID_mt(storageType), storageHolder);

	DMS_CALL_END
	return false;
}

TIC_CALL CharPtr DMS_CONV DMS_StorageManager_GetType(const AbstrStorageManager* storageManager)
{
	DMS_CALL_BEGIN

		ObjectContextHandle checkPtr(storageManager, AbstrStorageManager::GetStaticClass(), "DMS_StorageManager_GetType");

		return storageManager->GetClsName().c_str();

	DMS_CALL_END
	return nullptr;
}

TIC_CALL CharPtr DMS_CONV DMS_StorageManager_GetName(const AbstrStorageManager* storageManager)
{
	DMS_CALL_BEGIN

		ObjectContextHandle checkPtr(storageManager, AbstrStorageManager::GetStaticClass(), "DMS_StorageManager_GetName");

		return storageManager->GetNameStr().c_str();

	DMS_CALL_END
	return nullptr;
}

TIC_CALL void DMS_CONV DMS_TreeItem_SetStorageManager(TreeItem* storageHolder, CharPtr storageName, CharPtr storageType, StorageReadOnlySetting readOnly)
{
	DMS_CALL_BEGIN

		TreeItemContextHandle checkPtr(storageHolder, TreeItem::GetStaticClass(), "DMS_TreeItem_SetStorageManager");

		storageHolder->SetStorageManager(storageName, storageType, readOnly);

	DMS_CALL_END
}

TIC_CALL IStringHandle DMS_CONV DMS_TreeItem_GetFullStorageName(TreeItem* context, CharPtr relativeStorageName)
{
	DMS_CALL_BEGIN

		TreeItemContextHandle checkPtr(context, TreeItem::GetStaticClass(), "DMS_TreeItem_GetFullStorageName");

		return IString::Create(AbstrStorageManager::GetFullStorageName(context, SharedStr(relativeStorageName MG_DEBUG_ALLOCATOR_SRC("DMS_TreeItem_GetFullStorageName"))).c_str());

	DMS_CALL_END
	return 0;
}

TIC_CALL IStringHandle DMS_CONV DMS_Config_GetFullStorageName(CharPtr subDir, CharPtr relativeStorageName)
{
	DMS_CALL_BEGIN

		return IString::Create(AbstrStorageManager::GetFullStorageName(subDir, relativeStorageName).c_str());

	DMS_CALL_END
	return 0;
}

