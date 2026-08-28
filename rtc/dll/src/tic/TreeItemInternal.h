// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

/*
 *  Internal to the TreeItem component: what the TreeItem*.cpp translation units share
 *  with each other and with nothing else. TreeItem.cpp was split by functional role
 *  (path resolution, meta-info update, data usage, XML dump, function spec), which
 *  turned a handful of file-local helpers into cross-TU ones; they are declared here
 *  rather than in the hot TreeItem.h, so that churn in them costs no PCH.
 *
 *  Not part of the Tic interface -- do not include this outside rtc/dll/src/tic.
 */

#if !defined(__TIC_TREEITEMINTERNAL_H)
#define __TIC_TREEITEMINTERNAL_H

#include "TicBase.h"

// a non-owning interest count on a raw TreeItem pointer, as used to keep an item alive
// across a call that may otherwise drop the last reference
using TreeItemInterestPtr = InterestPtr<const TreeItem*>;

// apply a calculation rule that turns out to be a meta function or a template
// instantiation to its holder; shared by TreeItem::MakeCalculator (TreeItem.cpp) and
// the meta-info update (TreeItemMetaInfo.cpp)
void ApplyCalculator(TreeItem* holder, const AbstrCalculator* ac);

// "'<check>' of <guardian>", the text that identifies one guardian's IntegrityCheck in a
// diagnostic; shared by the guardian-closure construction (TreeItemMetaInfo.cpp) and
// TreeItem::VisitSuppliers, which names the guardian in the context of an error raised
// while building the check.
SharedStr TreeItem_IntegrityCheckText(const TreeItem* guardian);

#endif // __TIC_TREEITEMINTERNAL_H
