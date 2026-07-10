// Copyright (C) 1998-2025 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif


#if !defined(__TIC_ITEMUPDATE_H)
#define __TIC_ITEMUPDATE_H

#include "ptr/Interestholders.h"

TIC_CALL bool ItemUpdateImpl(const TreeItem* self, CharPtr context, SharedTreeItemInterestPtr& holder );
TIC_CALL auto TreeUpdateOrReturnFailerImpl(const TreeItem* self, CharPtr context, SharedTreeItemInterestPtr& holder ) -> SharedTreeItem;
TIC_CALL auto Tree_Update_Or_Return_Failer(const TreeItem* self, CharPtr context) -> SharedTreeItem;

#endif // __TIC_ITEMUPDATE_H
