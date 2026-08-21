// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////
#pragma once

#ifndef __STORAGEINTERFACE_H
#define __STORAGEINTERFACE_H

#include "TicBase.h"

class AbstrStorageManager;
struct TreeItem;

class InpStreamBuff;
class OutStreamBuff;

extern "C" {

TIC_CALL bool         DMS_CONV DMS_TreeItem_StorageDoesExist(const TreeItem* storageHolder, CharPtr storageName, CharPtr storageType);

TIC_CALL CharPtr      DMS_CONV DMS_StorageManager_GetType  (const AbstrStorageManager* self);
TIC_CALL CharPtr      DMS_CONV DMS_StorageManager_GetName  (const AbstrStorageManager* self);
TIC_CALL const Class* DMS_CONV DMS_AbstrStorageManager_GetStaticClass();

/******************************************* Storage Handlers **********************/

TIC_CALL void          DMS_CONV DMS_TreeItem_SetStorageManager(TreeItem* storageHolder, CharPtr storageName, CharPtr storageType, StorageReadOnlySetting readOnly);
TIC_CALL IStringHandle DMS_CONV DMS_TreeItem_GetFullStorageName(TreeItem* context, CharPtr relativeStorageName);
TIC_CALL IStringHandle DMS_CONV DMS_Config_GetFullStorageName(CharPtr subDir, CharPtr relativeStorageName);

}

#endif // __STORAGEINTERFACE_H