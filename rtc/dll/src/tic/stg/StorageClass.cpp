// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////

#include "TicPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif

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
