// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////

#if !defined(__TIC_FREEDATAMANAGER_H)
#define __TIC_FREEDATAMANAGER_H

#include "mci/ValueClass.h"
#include "mci/ValueWrap.h"

#include "TreeItemSet.h"
#include "AbstrDataObject.h"
#include "AbstrDataItem.h"
#include "DataItemClass.h"

// ====================== Various Predicates

// Below this size a data object is cheap enough to keep in memory rather than drop; see
// AbstrDataItem::TryCleanupMemImpl.
#define KEEPMEM_MAX_NR_BYTES          128

// Decides whether a cache item's tile file is persistent or temporary; see CreateFileData in
// DataLocks.cpp. The IsFileable/IsFileableSize family that used to sit beside it selected
// CalcCache candidates by size and was retired with the CalcCache in the 8.0 series.
bool MustStorePersistent(const TreeItem* adi);

#endif !defined(__TIC_FREEDATAMANAGER_H)
