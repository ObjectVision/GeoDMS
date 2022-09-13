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
#pragma once

#if !defined(__STG_STORAGEREGISTRY_H)
#define __STG_STORAGEREGISTRY_H

#include "StorageInterface.h"
#include "mci/Class.h"

/*
 *	class: StorageClass
 *
 *	purpose: Class Factory for implementation classes of AbstrStorageManager
 *
 *	each storage manager instantiates one instance of StorageClass
 */

struct StorageClass : public Class
{
private:
	typedef Class base_type;
public:
	TIC_CALL StorageClass(Constructor cFunc, TokenID id);
	TIC_CALL ~StorageClass();

	static AbstrStorageManagerRef CreateStorageManager(CharPtr name, TokenID typeID, bool readOnly, bool throwOnFailure, item_level_type itemLevel);

	TIC_CALL static UInt32        GetNrClasses();
	TIC_CALL static StorageClass* Get(UInt32 classNr);
private:
	static StorageClass* Find(TokenID classID);

	DECL_RTTI(TIC_CALL, MetaClass)
};

#define IMPL_STORAGECLASS(CLS, TYPENAME, CREATE_FUNC) \
	const StorageClass* CLS::GetStaticClass() \
	{ \
		static StorageClass s_StaticCls(CREATE_FUNC, GetTokenID_st(TYPENAME) ); \
		return &s_StaticCls; \
	} 

#define IMPL_DYNC_STORAGECLASS(CLS, TYPENAME) \
	IMPL_RTTI(CLS, StorageClass) \
	IMPL_STORAGECLASS(CLS, TYPENAME, CreateFunc<CLS>)


#endif  // __STG_STORAGEREGISTRY_H
