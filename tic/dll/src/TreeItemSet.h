// Copyright (C) 1998-2026 Object Vision B.V. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif


#ifndef __TIC_TREEITEMSET_H
#define __TIC_TREEITEMSET_H

#include "TicBase.h"

#include <set>

//----------------------------------------------------------------------
// TreeItem composites
//----------------------------------------------------------------------

using TreeItemCPtrArray = std::vector< const TreeItem* >;
using TreeItemCRefArray = std::vector< SharedPtr<const TreeItem> >;

// *****************************************************************************
// Section:     TreeItemSet
// *****************************************************************************

struct TreeItemSetType : std::set<const TreeItem*> { std::mutex critical_section;  };
struct TreeItemVectorType  : TreeItemCPtrArray              {};

#endif //!defined(__TIC_TREEITEMSET_H)
