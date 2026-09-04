// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////

#include "ShvDllPCH.h"
#include "act/UpdateMark.h" // UpdateMarker

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif

// Assorted shv helpers (see ShvUtils.h): values-unit and classification
// lookups, ViewData properties, status text and selection utilities.

#include <format>
#include "ShvUtils.h"

#include "dbg/debug.h"
#include "dbg/DebugContext.h"
#include "vt/BaseBounds.h"
#include "vt/Conversions.h"
#include "vt/Pair.h"
#include "mci/Class.h"
#include "mci/CompositeCast.h"
#include "mci/ValueClass.h"
#include "mci/ValueWrap.h"
#include "utl/Environment.h"
#include "utl/PlatformError.h"
#include "utl/IncrementalLock.h"
#include "utl/StrFormat.h"
#include "utl/splitPath.h"

#include "AbstrDataItem.h"
#include "AbstrDataObject.h"
#include "AbstrUnit.h"
#include "DataArray.h"
#include "DataArrayValue.h"
#include "DataItemClass.h"
#include "DataLocks.h"
#include "Projection.h"
#include "PropFuncs.h"
#include "SessionData.h"
#include "TicInterface.h"
#include "TreeItemClass.h"
#include "Unit.h"
#include "UnitClass.h"
#include "UnitProcessor.h"

#include "StgBase.h"

#include "CalcClassBreaks.h"
#include "ValuesTable.h"

#include "GeoTypes.h"

#include "Aspect.h"

#include "DataView.h"
#include "DcHandle.h"
#include "GraphicObject.h"
#include "LayerClass.h"
#include "Theme.h"

#ifdef _WIN32
#include "shellscalingapi.h"
#endif


// Desktop/exports/viewdata containers with unique/copy name generation, the
// config section, the DataContainer helpers, and palette + class-break
// creation (the reason this TU pulls clc's CalcClassBreaks.h).
// Split from ShvUtils.cpp (2026-08).

//----------------------------------------------------------------------
// desktop data section
//----------------------------------------------------------------------

inline SharedMutableTreeItem SafeCreateItem(TreeItem* context, TokenID pathID)
{
	MG_DEBUGCODE(StaticMtIncrementalLock<gd_TokenCreationBlockCount> tokenCreationLock; )
		auto result = context->CreateItem(pathID);
	dms_assert(result);
	return result;
}

inline SharedMutableTreeItem SafeCreateItemFromPath(TreeItem* context, CharPtr path)
{
	MG_DEBUGCODE(StaticMtIncrementalLock<gd_TokenCreationBlockCount> tokenCreationLock; )
	auto result = context->CreateItemFromPath(path);
	dms_assert(result);
	return result;
}

static StaticLateTokenID desktopsID("Desktops");
static StaticLateTokenID defaultID ("Default");
static StaticLateTokenID viewDataID("ViewData");
static StaticLateTokenID exportsID ("Exports");

TreeItem* GetDefaultDesktopContainer(const TreeItem* ti)
{
	assert(ti);
	assert(!ti->IsCacheItem());
	const TreeItem* pi = nullptr;
	while ((pi = ti->GetTreeParent().get()))
		ti = pi;
	auto desktops = const_cast<TreeItem*>(ti)->CreateItem(desktopsID);
	return desktops->CreateItem(defaultID).get();
}

TreeItem* GetExportsContainer(TreeItem* desktopItem)
{
	assert(desktopItem && !desktopItem->IsCacheItem());
	auto result = desktopItem->CreateItem(exportsID);
	assert(result && !result->IsCacheItem());
	return result.get();
}

TreeItem* GetViewDataContainer(TreeItem* desktopItem)
{
	assert(desktopItem && !desktopItem->IsCacheItem());
	auto result = desktopItem->CreateItem(viewDataID);
	assert(result && !result->IsCacheItem());
	return result.get();
}

TreeItem* CreateContainer_impl(TreeItem* container, const TreeItem* item)
{
	assert(item);
	if (!item->IsCacheItem())
	{
		if (IsUnit(item) && AsUnit(item)->IsDefaultUnit())
		{
			auto result = SafeCreateItem(container, AsUnit(item)->GetValueType()->GetNameID());
			assert(result);
			return result.get();
		}

		if (container->DoesContain(item))
			return const_cast<TreeItem*>(item);
		if (!item->IsCacheItem())
		{
			auto configRoot = item->GetRoot();
			auto result = SafeCreateItemFromPath(container, item->GetRelativeName(configRoot).c_str());
			assert(result);
			return result.get();
		}
	}

	item = item->GetUltimateItem().get();
	assert(item);
	auto name = std::format("I{:x}", std::size_t(item));
	return container->CreateItem(GetTokenID(name.c_str())).get();
}

TreeItem* CreateContainer(TreeItem* container, const TreeItem* item)
{
	auto result = CreateContainer_impl(container, item);
	result->UpdateMetaInfo();
	return result;
}

TreeItem* CreateDesktopContainer(TreeItem* desktopItem, const TreeItem* item)
{
	return CreateContainer(GetViewDataContainer(desktopItem), item);
}

static StaticLateTokenID paletteDomainID("PaletteDomain");

SharedMutableUnitInterestPtr CreatePaletteDomain(TreeItem* themeContainer, SizeT n)
{
	SharedMutableUnit paletteDomain = Unit<UInt8>::GetStaticClass()->CreateUnit(themeContainer, paletteDomainID);
	ItemWriteLock  xx(paletteDomain.get());
	if (!paletteDomain->GetTSF(USF_HasConfigRange))
	{
		paletteDomain->SetTSF(USF_HasConfigRange);
		paletteDomain->SetCount(n);
	}
	return paletteDomain;
}

UInt32 InterpolateColor(UInt32 firstColor, UInt32 lastColor, SizeT n, SizeT i)
{
	return CombineRGB(
		InterpolateValue<UInt8>(GetRed  (firstColor), GetRed  (lastColor), n, i),
		InterpolateValue<UInt8>(GetGreen(firstColor), GetGreen(lastColor), n, i),
		InterpolateValue<UInt8>(GetBlue (firstColor), GetBlue (lastColor), n, i)
	);
}

TreeItem* CreatePaletteContainer(DataView* dv, const AbstrUnit* paletteDomain)
{
	if (!paletteDomain->IsEndogenous())
		return const_cast<AbstrUnit*>(paletteDomain);

	return CreateDesktopContainer(dv->GetDesktopContext(), paletteDomain);
}

SharedDataItemInterestPtr CreateColorPalette(DataView* dv, const AbstrUnit* paletteDomain, AspectNr aNr, DmsColor clr)
{
	TreeItem* paletteContainer = CreatePaletteContainer(dv, paletteDomain);
	TokenID name = GetAspectNameID(aNr);
	SharedMutableDataItem result = make_shared_tree(AsDynamicDataItem(paletteContainer->GetSubTreeItemByID(name)), existing_obj{});
	if (!result)
		result = CreateDataItem(paletteContainer, name, paletteDomain, Unit<UInt32>::GetStaticClass()->CreateDefault()); // owning shared (co-owned with paletteContainer)
	dms_assert(result);

	TreeItem_SetDialogType(result.get(), GetAspectNameID(aNr));

	result->DisableStorage();
	result->UpdateMetaInfo();

	paletteDomain->PrepareDataUsage(DrlType::Certain);
	auto n = paletteDomain->GetCount();
	DataWriteLock lock(result.get());
	for (row_id i = 0; i != n; ++i)
		lock->SetValue<UInt32>(i, clr);
	lock.Commit();
	dms_assert(result->HasConfigData());

	return result.get();
}
SharedDataItemInterestPtr CreateSystemColorPalette(DataView* dv, const AbstrUnit* paletteDomain, AspectNr aNr, bool ramp, bool always, bool unique, const Float64* first, const Float64* last)
{
	assert(!SuspendTrigger::DidSuspend());

	bool hasZero = false;
	bool bothSigns = false;
	SizeT nrNegative = 0, nrBreaks = last - first;
	if (nrBreaks)
	{
		dms_assert(ramp);
		bothSigns = *first * last[-1] <= 0;
		if (bothSigns)
		{
			while (nrNegative < nrBreaks && first[nrNegative] < 0)
				++nrNegative;
			if (nrNegative < nrBreaks && first[nrNegative] == 0)
				hasZero = true;
		}
	}
	TreeItem* paletteContainer = CreatePaletteContainer(dv, paletteDomain);
	SharedMutableDataItem result = make_shared_tree(AsDynamicDataItem(paletteContainer->GetSubTreeItemByID(GetAspectNameID(aNr))), existing_obj{});
	TokenID name = GetAspectNameID(aNr);
	if (always || !result)
	{
		if (unique)
		{
			auto uniqueNameStr = SharedStr(name);
			name = UniqueName(paletteContainer, uniqueNameStr.c_str());
		}
		result = CreateDataItem(paletteContainer, name, paletteDomain, Unit<UInt32>::GetStaticClass()->CreateDefault() ); // owning shared (co-owned with paletteContainer)
		TreeItem_SetDialogType(result.get(), GetAspectNameID(aNr) );

		result->DisableStorage();
		result->UpdateMetaInfo();
		paletteDomain->PrepareDataUsage(DrlType::Certain);
		SizeT n = paletteDomain->GetCount();
		DataWriteLock lock(result.get());
		if (n == 2)
		{
			lock->SetValue<UInt32>(0, DmsWhite);
			lock->SetValue<UInt32>(1, unique ? dv->GetNextDmsColor() : DmsRed);
		}
		else if (ramp)
		{
			DmsColor firstColor = STG_Bmp_GetDefaultColor(CI_RAMPSTART);
			DmsColor lastColor  = STG_Bmp_GetDefaultColor(CI_RAMPEND);
			if (bothSigns)
			{
				DmsColor white = STG_Bmp_GetDefaultColor(CI_BACKGROUND);
				SizeT i = 0;
				for (; i != nrNegative; ++i)
					lock->SetValue<UInt32>(i, InterpolateColor(firstColor, white, nrNegative, i));
				if (hasZero)
					lock->SetValue<UInt32>(i++, white);
				SizeT nrNonPositive = i;
				SizeT nrPositive = n - nrNonPositive;
				for (; i != n; ++i)
					lock->SetValue<UInt32>(i, InterpolateColor(white, lastColor, nrPositive, i - nrNonPositive + 1));
			}
			else
			{
				SizeT denominator = (n > 1) ? n - 1 : 1;
				for (SizeT i = 0; i != n; ++i)
					lock->SetValue<UInt32>(i, InterpolateColor(firstColor, lastColor, denominator, i));
			}
		}
		else 
		{
			for (SizeT i = 0; i != n; ++i)
				lock->SetValue<UInt32>(i, STG_Bmp_GetDefaultColor(i % CI_NRCOLORS) );
		}
		lock.Commit();
		dms_assert(result->HasConfigData());
	}
	return result.get();
}

SharedDataItemInterestPtr CreateSystemLabelPalette(DataView* dv, const AbstrUnit* paletteDomain, AspectNr aNr, bool always)
{
	dms_assert(!paletteDomain->WasFailed(FailType::Data));
	TreeItem* paletteContainer = CreatePaletteContainer(dv, paletteDomain);
	SharedDataItemInterestPtr result = AsDynamicDataItem( paletteContainer->GetSubTreeItemByID(GetAspectNameID(aNr)) );

	if (always || !result)
	{
		SizeT n = paletteDomain->GetPreparedCount();
		SharedMutableDataItem newResult = CreateDataItem(paletteContainer, GetAspectNameID(aNr), paletteDomain, Unit<SharedStr>::GetStaticClass()->CreateDefault() ); // owning shared (co-owned with paletteContainer)
		TreeItem_SetDialogType(newResult.get(), GetAspectNameID(aNr) );

		newResult->DisableStorage();
//			newResult->SetConfigData();
		newResult->UpdateMetaInfo();
		result = newResult.get();
		DataWriteLock lock(newResult.get());
		auto resultData = mutable_array_cast<SharedStr>(lock)->GetDataWrite(no_tile, dms_rw_mode::write_only_all);

		visit<typelists::domain_types>(paletteDomain, [n, &resultData]<typename V>(const Unit<V>* pd)
			{
				auto domainRange = pd->GetRange();
				for (SizeT i = 0; i != n; ++i)
					resultData[i] = AsString(Range_GetValue_checked(domainRange, i));
			}
		);

		lock.Commit();
	}
	return result;
}

NewBreakAttrItems CreateBreakAttr(DataView* dv, const AbstrUnit* thematicUnit, const TreeItem* themeIndicator, SizeT n)
{
	TreeItem* themeContainer = CreateDesktopContainer(dv->GetDesktopContext(), GetUltimateSourceItem(themeIndicator));
	NewBreakAttrItems result;
	result.paletteDomain = CreatePaletteDomain(themeContainer, n);
	result.breakAttr = CreateDataItem(
		result.paletteDomain.get_ptr()
	, GetTokenID_mt("ClassBreaks")
	, result.paletteDomain
	,	thematicUnit
	);

	MakeClassBreakAttr(result.breakAttr);
	return result;
}

SharedDataItemInterestPtr CreateEqualIntervalBreakAttr(std::weak_ptr<DataView> dv_wptr, const AbstrUnit* themeUnit)
{
	auto dv = dv_wptr.lock(); if (!dv) return nullptr;

	auto [paletteDomain, breakAttr] = CreateBreakAttr(dv.get(), themeUnit, themeUnit, DEFAULT_MAX_NR_BREAKS);

	Range<Float64> range = themeUnit->GetRangeAsFloat64();
	MakeRange(range.first, range.second);
	ValueCountPairContainer sortedUniqueValueCache;
	sortedUniqueValueCache.reserve(2 MG_DEBUG_ALLOCATOR_SRC("CreateEqualIntervalBreakAttr"));
	sortedUniqueValueCache.push_back(ValueCountPair<Float64>(range.first,  1) MG_DEBUG_ALLOCATOR_SRC("CreateEqualIntervalBreakAttr"));
	sortedUniqueValueCache.push_back(ValueCountPair<Float64>(range.second, 1) MG_DEBUG_ALLOCATOR_SRC("CreateEqualIntervalBreakAttr"));

	ClassifyEqualInterval(breakAttr.get_ptr(), sortedUniqueValueCache, themeUnit->GetTiledRangeData().get());

	return breakAttr.get_ptr();
}

SharedDataItemInterestPtr CreateEqualCountBreakAttr(DataView* dv, const AbstrDataItem* thematicAttr)
{
	SizeT count = thematicAttr->GetAbstrDomainUnit()->GetCount();

	CountsResultType sortedUniqueValueCache;
	if (count)
		sortedUniqueValueCache = PrepareWeededCounts(thematicAttr, MAX_PAIR_COUNT);

	auto [paletteDomain, breakAttr] = CreateBreakAttr(dv, thematicAttr->GetAbstrValuesUnit(), thematicAttr, Min<SizeT>(sortedUniqueValueCache.first.size(), DEFAULT_MAX_NR_BREAKS));


	if (!sortedUniqueValueCache.first.empty())
		ClassifyEqualCount(breakAttr.get_ptr(), sortedUniqueValueCache.first, sortedUniqueValueCache.second.get());

	return breakAttr.get_ptr();
}

void CreateNonzeroJenksFisherBreakAttr(std::weak_ptr<DataView> dv_wptr, const AbstrDataItem* thematicAttr, ItemWriteLock&& iwlPaletteDomain, AbstrDataItem* breakAttr, ItemWriteLock&& iwlBreakAttr, AspectNr aNr)
{
	dms_assert(thematicAttr);
	SizeT count = thematicAttr->GetAbstrDomainUnit()->GetCount();

	CountsResultType sortedUniqueValueCache;
	SharedPtr<const SharedObj> thematicValuesRangeData;
	if (count)
	{
		ItemReadLock readLock(thematicAttr->GetCurrRangeItem());
		DataReadLock lck(thematicAttr);
		sortedUniqueValueCache = GetWeededCounts<ClassBreakValueType, typelists::num_objects, CountType>(thematicAttr, MAX_PAIR_COUNT);
		thematicValuesRangeData = sortedUniqueValueCache.second;
	}

	std::shared_ptr<AbstrUnit> paletteDomain = make_shared_tree(const_cast<AbstrUnit*>(breakAttr->GetAbstrDomainUnit()), existing_obj{});
	std::shared_ptr<AbstrDataItem> breakAttrPtr = make_shared_tree(breakAttr, existing_obj{});

	SizeT nrBreaks = Min<SizeT>(sortedUniqueValueCache.first.size(), DEFAULT_MAX_NR_BREAKS);
	auto result = ClassifyJenksFisher(sortedUniqueValueCache.first, nrBreaks, true); // callsClassifyUniqueValues if breakAttr.size() >= sortedUniqueValueCache.size()

	auto dv = dv_wptr.lock(); if (!dv) return;

	auto siwlPaletteDomain = std::make_shared<ItemWriteLock>(std::move(iwlPaletteDomain));
	auto siwlBreakAttr = std::make_shared<ItemWriteLock>(std::move(iwlBreakAttr)); // TODO G8: Can this be moved into a functor's data field directly? Requires no functor copy!

	// SetCount for a domain that already has attributes can only be called from the MainThread
	auto setTheResultsAction = [paletteDomain
			, siwlMovedPaletteDomain = std::move(siwlPaletteDomain)
			, breakAttrPtr, siwlBreakAttr = std::move(siwlBreakAttr)
			, nrBreaks
			, resultCopy = std::move(result)
			, thematicValuesRangeData, aNr, dv_wptr
			]() 
		{
			paletteDomain->SetCount(nrBreaks);
			auto tsActive = UpdateMarker::GetFreshTS(MG_DEBUG_TS_SOURCE_CODE("CreateNonzeroJenksFisherBreakAttr"));
			paletteDomain->MarkTS(tsActive);

			// Alleviate restriction on breakAttr write-access to avoid dead-lock, which requires mutable=synchronized=unique access to the ItemWriteLock
			// Could the move fix the dangling writeLock on PaletteDomain issue ? No, since the spawning thread doesn't write, except for when the destructor could run, which has unique access, guaranteed by shared_ptr.
			*siwlMovedPaletteDomain = ItemWriteLock(); 
			auto paletteInterest = SharedTreeItemInterestPtr(paletteDomain.get());
			auto tryReadLock = ItemReadLock(std::move(paletteInterest), try_token);
			if (!tryReadLock.has_ptr())
				return; // no accces because of other classifying action, pray for the other action to fill this palette

			FillBreakAttrFromArray(breakAttrPtr.get(), resultCopy, thematicValuesRangeData.get());
			auto dv = dv_wptr.lock(); if (!dv) return;
			if (aNr != AN_AspectCount)
				CreatePaletteData(dv.get(), paletteDomain.get(), aNr, true, true, begin_ptr( resultCopy ), end_ptr( resultCopy ));
			if (aNr != AN_LabelText)
				CreatePaletteData(dv.get(), paletteDomain.get(), AN_LabelText, true, true, begin_ptr(resultCopy), end_ptr(resultCopy));
		}
	;
//	setTheResultsAction();
	dv->PostGuiOper(std::move(setTheResultsAction));
}

const AbstrDataItem* GetSystemPalette(const AbstrUnit* paletteDomain, AspectNr aNr)
{
	assert(paletteDomain);
	return  AsDynamicDataItem( paletteDomain->GetConstSubTreeItemByID(GetAspectNameID(aNr)).get());
}

//----------------------------------------------------------------------
// config section
//----------------------------------------------------------------------

MG_DEBUGCODE( static bool gd_AdminModeKnown = false; )
static bool g_AdminMode;

bool HasAdminMode()
{
	dbg_assert(gd_AdminModeKnown);
	return g_AdminMode;
}

SHV_CALL void SHV_SetAdminMode(bool v)
{
	MG_DEBUGCODE( gd_AdminModeKnown = true; )
	g_AdminMode = v;
}

//----------------------------------------------------------------------
// DataContainer section
//----------------------------------------------------------------------

const AbstrUnit* SHV_DataContainer_GetDomain(const TreeItem* ti, UInt32 level, bool adminMode)
{
	if (!ti || (!adminMode && ti->GetTSF(TSF_InHidden))) return nullptr;

	if (IsDataItem(ti))
	{
		try { 
			return AsDataItem(ti)->GetAbstrDomainUnit();
		} 
		catch (...) {}
		return nullptr;
	}

	if (level) 
	{
		--level;

		for (ti = ti->GetFirstSubItem(); ti; ti = ti->GetNextItem()) 
		{
			const AbstrUnit* result = SHV_DataContainer_GetDomain(ti, level, adminMode);
			if (result)
				return result;
		}
	}
	return nullptr;
}

UInt32 SHV_DataContainer_GetItemCount(const TreeItem* ti, const AbstrUnit* domain, UInt32 level, bool adminMode)
{
	assert(domain);
	if (!ti || (!adminMode && ti->GetTSF(TSF_InHidden))) return 0;

	UInt32 result =0;

	domain->UpdateMetaInfo();
	ti->UpdateMetaInfo();

	if (IsDataItem(ti) && AsDataItem(ti)->GetAbstrDomainUnit()->UnifyDomain(domain))
		++result;

	if (!level) 
		return result; 

	--level;

	for (ti = ti->GetFirstSubItem(); ti; ti = ti->GetNextItem()) 
		result += SHV_DataContainer_GetItemCount(ti, domain, level, adminMode);

	return result;
}

auto DataContainer_NextItem(const TreeItem* ti, const TreeItem* si, const AbstrUnit* domain, bool adminMode) -> const AbstrDataItem*
{
	assert(ti);
	while ((si = ti->WalkConstSubTree(si)))
	{
		// skip hidden items
		if (!adminMode)
			while (si->GetTSF(TSF_InHidden))
			{
				if (si == ti)
					return nullptr;
				const TreeItem* next;
				while ((next = si->GetNextItem()) == nullptr) // skip sub-tree
				{
					si = si->GetTreeParent().get();
					if (si == ti)
						return nullptr;
					assert(si);
				}
				si = next;
			}

		// return dataItem if compatible
		assert(si);
		if (IsDataItem(si))
		{
			auto adi = AsDataItem(si);
			if (adi->GetAbstrDomainUnit()->UnifyDomain(domain))
				return adi;
		}
	}
	return nullptr;
}

const AbstrDataItem* SHV_DataContainer_GetItem(const TreeItem* ti, const AbstrUnit* domain, UInt32 k, UInt32 level, bool adminMode)
{
	assert(domain);
	if (!ti || (!adminMode && ti->GetTSF(TSF_InHidden))) return 0;

	UInt32 result =0;
	if (IsDataItem(ti) && AsDataItem(ti)->GetAbstrDomainUnit()->UnifyDomain(domain))
	{
		if (!k)
			return AsDataItem(ti);
		--k;
	}

	if (!level) 
		return 0;
	--level;

	for (ti = ti->GetFirstSubItem(); ti; ti = ti->GetNextItem()) 
	{
		UInt32 n = SHV_DataContainer_GetItemCount(ti, domain, level, adminMode);
		if (k < n)
			return SHV_DataContainer_GetItem(ti, domain, k, level, adminMode);
		k -= n;
	}
	return nullptr;
}

