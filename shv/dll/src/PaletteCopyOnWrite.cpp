// Copyright (C) 1998-2026 Object Vision b.v.
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////
//
// Copy-on-write for the data a layer control shows (issue #634).
//
// A legend cell whose attribute the configuration calculates used to be uneditable: the item is
// shared with every other view that uses the same classification, so writing it is not an option.
// Instead of refusing the edit, the first edit copies the values the configuration currently
// computes into a view-local item under /Desktops/<desktop>/ViewData and re-themes onto that, the
// same rule issue #734 applies to a whole classification.
//
/////////////////////////////////////////////////////////////////////////////

#include "ShvDllPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

#include "PaletteCopyOnWrite.h"

#include "act/UpdateMark.h"
#include "dbg/SeverityType.h"
#include "mci/CompositeCast.h"
#include "ptr/SharedTreePtr.h"
#include "utl/StrFormat.h"

#include "AbstrDataItem.h"
#include "AbstrDataObject.h"
#include "AbstrUnit.h"
#include "DataItemClass.h"
#include "DataLocks.h"
#include "PropFuncs.h"
#include "TreeItemProps.h"

#include "DataView.h"
#include "ShvUtils.h"

//----------------------------------------------------------------------
// the predicate
//----------------------------------------------------------------------

bool PaletteCoW_CanCopy(const DataView* dv, const AbstrDataItem* adi)
{
	if (!dv || !adi)
		return false;

	if (adi->IsEditable())
		return false;                       // writable as it is; there is nothing to copy

	if (adi->IsCacheItem() || !adi->GetTreeParent())
		return false;                       // no stable place in the tree to mirror it to

	if (adi->WasFailed(FailType::MetaInfo))
		return false;

	if (!adi->GetAbstrDomainUnit() || !adi->GetAbstrValuesUnit())
		return false;

	// An item that the GeoDMS itself generated for this desktop and that is still not editable is
	// not a configured palette but a calculated view attribute (the Count, Area and SelCount
	// columns of a legend); copying those would only produce a meaningless editable pcount.
	auto desktopItem = dv->GetDesktopContext();
	if (!desktopItem || desktopItem->DoesContain(adi))
		return false;

	return true;
}

//----------------------------------------------------------------------
// the copy
//----------------------------------------------------------------------

SharedDataItemInterestPtr PaletteCoW_GetOrCreateCopy(DataView* dv, const AbstrDataItem* adi)
{
	if (adi && adi->IsEditable())
		return const_cast<AbstrDataItem*>(adi);

	if (!PaletteCoW_CanCopy(dv, adi))
		return {};

	auto domain = adi->GetAbstrDomainUnit();
	auto values = adi->GetAbstrValuesUnit();

	TreeItem* container = CreateDesktopContainer(dv->GetDesktopContext(), adi->GetTreeParent().get());
	if (!container)
		return {};

	// An earlier edit of this same attribute already made the copy; edit that one further, so that
	// a second colour change does not silently start from the configured values again.
	TokenID name = adi->GetID();
	if (auto existing = AsDynamicDataItem(container->GetSubTreeItemByID(name)))
	{
		if	(	existing->GetAbstrDomainUnit() == domain
			&&	existing->GetAbstrValuesUnit() == values
			&&	existing->GetValueComposition() == adi->GetValueComposition()
			&&	existing->IsEditable()
			)
			return existing;

		// something else already occupies the name; do not touch it
		name = UniqueName(container, SharedStr(name).c_str());
	}

	// One epoch tick for the whole copy: the item is created and seeded together, so that no
	// consumer can observe the empty in-between state.
	UpdateMarker::ChangeSourceLock changeStamp(
		UpdateMarker::GetActiveTS(MG_DEBUG_TS_SOURCE_CODE("PaletteCoW_GetOrCreateCopy"))
	,	"PaletteCoW_GetOrCreateCopy"
	);

	SharedMutableDataItem result = CreateDataItem(container, name, domain, values, adi->GetValueComposition());
	if (auto dialogType = TreeItem_GetDialogType(adi))
		TreeItem_SetDialogType(result.get(), dialogType);   // keeps FindAspectAttr and #734 recognizing it

	result->DisableStorage();
	result->UpdateMetaInfo();

	domain->PrepareDataUsage(DrlType::Certain);

	PreparedDataReadLock srcLock(adi, "PaletteCoW_GetOrCreateCopy");
	DataWriteLock dwl(result.get());
	CopyData(adi->GetRefObj().get(), dwl.get());
	dwl.Commit();

	reportF(SeverityTypeID::ST_Warning
	,	"Edit: {} is configured or calculated and is shared with every view that uses it; "
		"{} was created for this desktop and is edited instead (issue #634)"
	,	adi->GetFullName()
	,	result->GetFullName()
	);

	return result.get();
}
