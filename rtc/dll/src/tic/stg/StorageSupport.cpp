// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////

#include "TicPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif

// Small satellites of the tic storage framework, merged (2026-08):
// StorageClass (registry), StorageInterface (C-API), AsmUtil.

// ==== from StorageClass.cpp ====

// StorageClass: the meta-class registry that maps storage-type tokens to
// storage-manager constructors.

#include "dbg/DebugCast.h"
#include "stg/StorageClass.h"
#include "mci/register.h"

#include "stg/AbstrStorageManager.h"
#include "stg/AsmUtil.h"

typedef StaticRegister<StorageClass, TokenID, CompareLtItemIdPtrs<StorageClass> > StorageClasRegisterType; 
static StorageClasRegisterType s_StorageClasses;

TIC_CALL StorageClass::StorageClass(Constructor cFunc, TokenID typeID)
: Class(cFunc, AbstrStorageManager::GetStaticClass(), typeID)
{
	s_StorageClasses.Register(this);
}

TIC_CALL StorageClass::~StorageClass()
{
	s_StorageClasses.Unregister(this);
}


AbstrStorageManagerRef StorageClass::CreateStorageManager(CharPtr name, TokenID typeID, bool readOnly, bool throwOnFailure)
{
	StorageClass* cls = Find(typeID);
	if (!cls)
	{
		if (!throwOnFailure)
			return {};
		throwStorageError(ASM_E_UNKNOWNSTORAGECLASS, GetTokenStr(typeID).c_str());
	}
	auto  result = AbstrStorageManagerRef(debug_cast<AbstrStorageManager*>(cls->CreateObj()), newly_obj{});
	assert(result);
	result->InitStorageManager(name, readOnly);
	return result;
}

StorageClass* StorageClass::Find(TokenID classID)
{
	return s_StorageClasses.Find(classID);
}

UInt32 StorageClass::GetNrClasses()
{
	return s_StorageClasses.Size();
}

StorageClass* StorageClass::Get(UInt32 classNr)
{
	return s_StorageClasses.Begin()[classNr];
}

IMPL_RTTI_METACLASS(StorageClass, "STORAGE", nullptr)


// ==== from StorageInterface.cpp ====

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



// ==== from AsmUtil.cpp ====

#include "utl/StrFormat.h"
#include "xct/DmsException.h"
#include "stg/StorageInterface.h"

#include "stg/AsmUtil.h"

/*
 * asm_state_string
 *
 * global method for determining a string associated with the ASM_STATE
 *
 * Parameters:
 * state(I): the state to return a string for
 *
 * Returns:
 * (const char*): the string for the state
 */

SharedStr asm_state_string(ASM_STATE state, CharPtr storageName)
{
	CharPtr msg = "unknown error";
	switch(state)
	{
	case ASM_E_FILENOTFOUND:
		msg = "file not found";
		break;
	case ASM_E_PATHNOTFOUND:
		msg = "path not found";
		break;
	case ASM_E_FILENOTOPENED:
		msg = "file not opened";
		break;
	case ASM_E_FILEALREADYEXISTS:
		msg = "Storage Manager: file already exists";
		break;
	case ASM_E_LOCKVIOLATION:
		msg = "lock violation";
		break;
	case ASM_E_SHAREVIOLATION:
		msg = "share violation";
		break;
	case ASM_E_ACCESSDENIED:
		msg = "access denied";
		break;
	case ASM_E_DATAREAD:
		msg = "data read error";
		break;
	case ASM_E_MEDIUMFULL:
		msg = "medium full";
		break;
	case ASM_E_WRITEFAULT:
		msg = "write fault";
		break;
	case ASM_E_INSUFFICIENTMEMORY:
		msg = "insufficient memory";
		break;
	case ASM_E_INVALIDOBJECT:
		msg = "invalid object";
		break;
	case ASM_E_UNKNOWNSTORAGECLASS:
		msg = "unknown storage type";
		break;
	}
	return mySSPrintF("Storage Exception {}: {}", storageName, msg);
}

/*
 * throwStorageError
 *
 * global method which constructs an ASMException with the given ASM_STATE
 *
 * Parameters:
 * state(I): the state to construct an exception for
 *
 * throw:
 * ASMException
 */

[[noreturn]] void throwStorageError(ASM_STATE state, CharPtr storageName)
{
	throwErrorD("STG", asm_state_string(state, storageName).c_str() );
}

