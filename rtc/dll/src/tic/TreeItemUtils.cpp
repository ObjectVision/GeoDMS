// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////

#include "TicPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif

// Assorted TreeItem helper functions: naming and path resolution, and
// state-change notification support.

#include "TreeItemUtils.h"

#include "utl/mySPrintF.h"
#include "utl/splitPath.h"

#include "AbstrCalculator.h"
#include "LispTreeType.h"
#include "StateChangeNotification.h"
#include "Unit.h"

//----------------------------------------------------------------------
// used modules and forward class references
//----------------------------------------------------------------------

//----------------------------------------------------------------------
// functions 
//----------------------------------------------------------------------

auto _GetHistoricUltimateItem(const TreeItem* ti) noexcept -> std::shared_ptr<const TreeItem>
{
	assert(ti);

	while (true)
	{
		auto refItem = ti->mc_RefItem.lock();
		if (!refItem)
			return make_shared_tree(ti, no_zombies{}); // re-own the tree-managed item (no_zombies: empty if ti is mid-destruction, e.g. SetKeepDataState from ~AbstrDataItem -- existing_obj would throw bad_weak_ptr there)
		ti = refItem.get();
	}
}

auto _GetCurrUltimateItem(const TreeItem* ti) noexcept -> std::shared_ptr<const TreeItem>
{
	assert(ti);
	dbg_assert(ti->CheckMetaInfoReadyOrPassor());

	return _GetHistoricUltimateItem(ti);
}

auto _GetCurrRangeItem(const TreeItem* ti)  noexcept -> std::shared_ptr<const TreeItem>
{
	return _GetCurrUltimateItem(ti);
}

auto _GetUltimateItem(const TreeItem* ti)  noexcept -> std::shared_ptr<const TreeItem>
{
	assert(ti);
	while (true)
	{
		auto refItem = ti->GetReferredItem();
		if (!refItem)
			return make_shared_tree(ti, no_zombies{}); // re-own the tree-managed item (no_zombies: empty if ti is mid-destruction; existing_obj would throw bad_weak_ptr)
		ti = refItem.get();
	}
}

bool HasVisibleSubItems(const TreeItem* refItem)  noexcept
{
	while (true)
	{
		if (refItem->HasSubItems())
			return true;
		auto ref2 = refItem->GetReferredItem();
		if (!ref2)
			return false;
		refItem = ref2.get();
	}
}


item_origin GetItemOrigin(const TreeItem* ti)
{
	assert(ti);

	// order matters: an item within a template is shown as such even when it has a calculation
	// rule, and a calculation rule prevails over the storage of an enclosing container.
	if (ti->InTemplate())
		return item_origin::template_def;
	if (ti->HasCalculator())
		return item_origin::calculated;
	if (ti->GetStorageParent(false))
		return item_origin::exogenic;
	return item_origin::container;
}

static std::array<DmsColor, UInt32(item_origin::count)> s_OriginTextColors = sc_DefaultOriginTextColors;

DmsColor GetItemOriginTextColor(item_origin io)
{
	assert(io < item_origin::count);
	return s_OriginTextColors[UInt32(io)];
}

DmsColor GetItemOriginTextColor(const TreeItem* ti)
{
	return GetItemOriginTextColor(GetItemOrigin(ti));
}

void SetItemOriginTextColor(item_origin io, DmsColor clr)
{
	assert(io < item_origin::count);
	clr &= MAX_COLOR;
	s_OriginTextColors[UInt32(io)] = clr;
}

NotificationCode NotificationCodeFromProblem(FailType ft)
{
	switch (ft)
	{
	case FailType::Data:      return NC2_DataFailed;
	case FailType::Validate:  return NC2_CheckFailed;
	case FailType::Committed: return NC2_CommitFailed;
	}
	return NC2_MetaFailed;
}

SharedStr GetPartialName(const TreeItem* themeDisplayItem, UInt32 nameLevel)
{
	dms_assert(themeDisplayItem);
	dms_assert(!themeDisplayItem->IsCacheItem());

	const TreeItem* themeDisplayItemCopy = themeDisplayItem;
	SharedStr result;

	for (; nameLevel; --nameLevel)
	{
		auto parent = themeDisplayItem->GetTreeParent();
		if (!parent)
			return themeDisplayItemCopy->GetFullName();

		result = DelimitedConcat(themeDisplayItem->GetName().c_str(), result.c_str());
		themeDisplayItem = parent.get();
		dms_assert(themeDisplayItem); // not root
	}
	return result;
}

const AbstrDataItem* GeometrySubItem(const TreeItem* ti)
{
	dms_assert(ti);
	ti->UpdateMetaInfo();
	const TreeItem* si = const_cast<TreeItem*>(ti)->GetSubTreeItemByID(token::geometry);
	if (!IsDataItem(si))
		return nullptr;
	auto gi = AsDataItem(si);
	if (gi->GetAbstrValuesUnit()->GetValueType()->GetNrDims() != 2)
		return nullptr;
	return gi;
}

#include "PropFuncs.h"

bool IsThisMappable(const TreeItem* ti)
{
	dms_assert(ti);
	return HasMapType(ti) || GeometrySubItem(ti);
}

auto GetMappingItem(const TreeItem* ti) -> const TreeItem*
{
	dms_assert(ti); // PRECONDITION
	do
	{
		dms_assert(!SuspendTrigger::DidSuspend());
		if (IsThisMappable(ti))
			return ti;
		ti = ti->GetReferredItem().get();
	} while (ti);
	return nullptr;
}


