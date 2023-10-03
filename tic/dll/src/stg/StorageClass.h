// Copyright (C) 2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

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
