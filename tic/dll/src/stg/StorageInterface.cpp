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

#include "AbstrStorageManager.h"
#include "StorageInterface.h"
#include "TreeItemContextHandle.h"
#include "TreeItemClass.h"

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

TIC_CALL void DMS_CONV DMS_TreeItem_SetStorageManager(TreeItem* storageHolder, CharPtr storageName, CharPtr storageType, bool readOnly)
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

		return IString::Create(AbstrStorageManager::GetFullStorageName(context, SharedStr(relativeStorageName)).c_str());

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

