// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "TicPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

#include "TreeItemClass.h"

#include "dbg/CheckPtr.h"
#include "dbg/DebugCast.h"
#include "xml/XmlParser.h"

//----------------------------------------------------------------------
// class  : TreeItemClass
//----------------------------------------------------------------------

TreeItemClass::TreeItemClass(Constructor cFunc, const Class* baseCls, TokenID typeID)
	: 	Class(cFunc, baseCls, typeID)
{
}

static TokenID nameTokenID = GetTokenID_st("name");

Object* TreeItemClass::CreateFromXml(Object* context, struct XmlElement& elem)
{
	CharPtr name = elem.GetAttrValue(nameTokenID);
	if (!context)
		return TreeItem::CreateConfigRoot(GetTokenID_mt(name));
	CheckPtr(context, TreeItem::GetStaticClass(), "TreeItemClass::CreateFromXml");
	TreeItem* container= debug_cast<TreeItem*>(context);
	return container->CreateItemFromPath(name);
}


IMPL_RTTI_METACLASS(TreeItemClass, "TREEITEM", TreeItemClass::CreateFromXml)

