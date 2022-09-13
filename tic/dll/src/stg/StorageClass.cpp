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

#include "dbg/DebugCast.h"
#include "stg/StorageClass.h"
#include "mci/register.h"

#include "stg/AbstrStorageManager.h"
#include "stg/AsmUtil.h"
#include "stg/StorageClass.h"

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


AbstrStorageManagerRef StorageClass::CreateStorageManager(CharPtr name, TokenID typeID, bool readOnly, bool throwOnFailure, item_level_type itemLevel)
{
	StorageClass* cls = Find(typeID);
	if (!cls)
	{
		if (!throwOnFailure)
			return {};
		throwStorageError(ASM_E_UNKNOWNSTORAGECLASS, GetTokenStr(typeID).c_str());
	}
	AbstrStorageManagerRef result = debug_cast<AbstrStorageManager*>(cls->CreateObj());
	dms_assert(result);
	result->InitStorageManager(name, readOnly, itemLevel);
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
