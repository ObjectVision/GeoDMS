// Copyright (C) 1998-2026 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

#if !defined(__TIC_TREEITEMCLASS_H)
#define __TIC_TREEITEMCLASS_H

//----------------------------------------------------------------------
// used modules and forward class references
//----------------------------------------------------------------------

#include "mci/Class.h"

//----------------------------------------------------------------------
// TreeItemClass 
//----------------------------------------------------------------------

struct TreeItemClass : Class
{
private:
	typedef Class base_type;
public:
	TIC_CALL TreeItemClass(Constructor cFunc, const Class* baseCls, TokenID typeID);

	static Object* CreateFromXml(Object* context, struct XmlElement& elem);

	DECL_RTTI(TIC_CALL, MetaClass)
};


#define IMPL_TREEITEMCLASS(cls, typeName, CreateFunc) \
	const TreeItemClass* cls::GetStaticClass() \
	{ \
		static TreeItemClass s_Cls(CreateFunc, base_type::GetStaticClass(), GetTokenID_st(typeName) ); \
		return &s_Cls; \
	} 

#define IMPL_DYNC_TREEITEMCLASS(cls, typeName) IMPL_RTTI(cls, TreeItemClass) IMPL_TREEITEMCLASS(cls, typeName, CreateFunc<cls>)

//----------------------------------------------------------------------
// streaming utility functions
//----------------------------------------------------------------------

#endif // __TIC_TREEITEMCLASS_H
