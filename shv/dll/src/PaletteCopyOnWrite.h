// Copyright (C) 1998-2026 Object Vision b.v.
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

#if !defined(__SHV_PALETTECOPYONWRITE_H)
#define __SHV_PALETTECOPYONWRITE_H

//----------------------------------------------------------------------
// used modules and forward class references
//----------------------------------------------------------------------

#include "ShvBase.h"

#include "DataLocks.h" // SharedDataItemInterestPtr

class AbstrDataItem;
class DataView;

//----------------------------------------------------------------------
// copy-on-write for the legend and the palette editor (issue #634)
//----------------------------------------------------------------------

// A colour, a label or a class break that the configuration calculates cannot be written: the item
// belongs to the project and is shared with every other view that uses it. Rather than refusing the
// edit, the legend and the palette editor edit a view-local copy of it, the same rule #734 applies
// to a whole classification.

// True when adi cannot be written as it is, but a copy of it could be made instead. Called while
// drawing, while setting the cursor and while building menus, so it creates nothing and takes no
// locks.
bool PaletteCoW_CanCopy(const DataView* dv, const AbstrDataItem* adi);

// adi itself when it is already editable; otherwise a copy of the values adi currently computes,
// created under /Desktops/<desktop>/ViewData/<parent of adi>/ with adi's DialogType and no
// calculation rule of its own. An earlier copy of the same attribute is reused. Returns an empty
// ptr when no copy can be made.
//
// Because the copy sits in the desktop tree and carries no rule, #734 accepts it afterwards: a
// later Paste Classbreaks overwrites it in place instead of regenerating the classification again.
SharedDataItemInterestPtr PaletteCoW_GetOrCreateCopy(DataView* dv, const AbstrDataItem* adi);

#endif // !defined(__SHV_PALETTECOPYONWRITE_H)
