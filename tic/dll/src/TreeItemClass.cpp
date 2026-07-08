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

TreeItemClass::TreeItemClass(Constructor cFunc, const Class* baseCls, TokenID typeID, SharedConstructor sFunc)
	: 	Class(cFunc, baseCls, typeID, sFunc)
{
}

static StaticTokenID nameTokenID("name");

std::shared_ptr<Actor> TreeItemClass::CreateFromXml(Object* context, struct XmlElement& elem)
{
	CharPtr name = elem.GetAttrValue(nameTokenID);
	if (!context)
		return TreeItem::CreateConfigRoot(GetTokenID_mt(name)); // SharedMutableTreeItem -> owning std::shared_ptr<Actor>
	CheckPtr(context, TreeItem::GetStaticClass(), "TreeItemClass::CreateFromXml");
	TreeItem* container= debug_cast<TreeItem*>(context);
	// the new child is co-owned by its parent; return its std::shared_ptr (control block flows through)
	return container->CreateItemFromPath(name);
}


IMPL_RTTI_METACLASS(TreeItemClass, "TreeItem", TreeItemClass::CreateFromXml)

