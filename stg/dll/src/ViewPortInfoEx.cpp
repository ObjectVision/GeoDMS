// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////

#include "StoragePCH.h"
#include "ImplMain.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif

// ViewPortInfoEx<Int>: the grid-to-viewport mapping info used by the grid
// storage managers and geo's Poly2Grid/RasterMerge.
// Split from stg DllMain.cpp (2026-08); declared in ViewPortInfoEx.h.

#include "dbg/DebugCast.h"
#include "dbg/DmsCatch.h"
#include "vt/Round.h"
#include "geom/Transform.h"
#include "vt/Conversions.h"
#include "mci/CompositeCast.h"
#include "mci/ValueClass.h"
#include "mci/ValueWrap.h"
#include "utl/Environment.h"
#include "utl/StrFormat.h"
#include "utl/Encodes.h"
#include "xct/DmsException.h"

#include "AbstrDataItem.h"
#include "AbstrDataObject.h"
#include "DataLocks.h"
#include "Metric.h"
#include "Projection.h"
#include "PropFuncs.h"
#include "TreeItemClass.h"
#include "TreeItemProps.h"
#include "Unit.h"
#include "UnitClass.h"

#if defined(_MSC_VER)
#include <minmax.h>
#include <share.h>
#endif

#include "NameSet.h"
#include "ViewPortInfoEx.h"
#include "GridStorageManager.h"

extern RTC_CALL bool s_IsDetectingIncInterest; // defined in rtc act/Actor.cpp

// ------------------------------------------------------------------------
// Implementation of ViewPortInfo
// ------------------------------------------------------------------------

template <typename Int>
ViewPortInfoEx<Int>::ViewPortInfoEx(const TreeItem* context, const AbstrUnit* currDomain, tile_id tc, const AbstrUnit* gridDomain, tile_id tg, StorageMetaInfoPtr smi, bool correctGridOffset, bool mustCheck, countcolor_t cc, bool queryActualGridDomain)
{
	assert(queryActualGridDomain || !IsDefined(tg));
	assert(!gridDomain || gridDomain == gridDomain->GetCurrRangeItem().get());
	assert(!currDomain || currDomain == currDomain->GetCurrRangeItem().get());
	assert(queryActualGridDomain || !correctGridOffset);
	assert(queryActualGridDomain || tg == no_tile);
	assert(!correctGridOffset || queryActualGridDomain);

	this->m_smi = smi;

	if (queryActualGridDomain && gridDomain)
		m_GridExtents = ThrowingConvert<rect_type>(gridDomain->GetTileSizeAsI64Rect(tg));
	else
		m_GridExtents = rect_type(MIN_VALUE(typename ViewPortInfoEx::point_type), MAX_VALUE(typename ViewPortInfoEx::point_type));

	const AbstrUnit* currBase = GetCurrWorldCrdUnitFromGeoUnit(currDomain);
	const AbstrUnit* gridBase = GetCurrWorldCrdUnitFromGeoUnit(gridDomain);

	if (mustCheck)
	{
		if (currBase && gridBase && !currBase->UnifyDomain(gridBase,"", "", UM_AllowAllEqualCount))
		{
			MG_CHECK(currDomain);
			MG_CHECK(gridDomain);

			context->throwItemErrorF("ProjectionBase {} of {} incompatible with ProjectionBase {} of {}."
				, currBase->GetName().c_str()
				, currDomain->GetName().c_str()
				, gridBase->GetName().c_str()
				, gridDomain->GetName().c_str()
			);
		}
		if (tc == no_tile)
			return;
	}
	else
	{
		assert(!currBase || currBase->Was(ProgressState::MetaInfo));
		assert(!gridBase || gridBase->Was(ProgressState::MetaInfo));
	}

	//	Projections in world coords
	const UnitProjection* currProj = currDomain ? currDomain->GetCurrProjection() : nullptr;
	const UnitProjection* gridProj = gridDomain ? gridDomain->GetCurrProjection() : nullptr;

	CrdTransformation fileRasterProj2World = UnitProjection::GetCompositeTransform(gridProj);
	CrdTransformation viewPortProj2World   = UnitProjection::GetCompositeTransform(currProj);

	MG_CHECK(!fileRasterProj2World.IsSingular());
	CrdTransformation viewPortProj2fileRasterProj = viewPortProj2World / fileRasterProj2World;

	*typesafe_cast<CrdTransformation*>(this) = viewPortProj2fileRasterProj;
	if (correctGridOffset)
	*typesafe_cast<CrdTransformation*>(this) -= Convert<DPoint>(GetTopLeft(m_GridExtents, viewPortProj2fileRasterProj.Orientation())); // camouflage of non-zero based grids

	if (currDomain)
	{
		try {
			auto tileRange = currDomain->GetTiledRangeData()->GetTileRangeAsI64Rect(tc);
			this->m_ViewPortExtents = ThrowingConvert<rect_type>(tileRange);
			MG_CHECK(IsDefined(this->m_ViewPortExtents.first.first));
			MG_CHECK(IsDefined(this->m_ViewPortExtents.first.second));
			MG_CHECK(IsDefined(this->m_ViewPortExtents.second.first));
			MG_CHECK(IsDefined(this->m_ViewPortExtents.second.second));
		}
		catch (...)
		{
			auto err = catchException(true)->Why();
			err = GetFirstLine(err);
			auto msg = mySSPrintF("ViewPortInfo failure of tile {}: {}\nCheck the definition of the target domain"
				, tc, err
			);
			currDomain->throwItemError(msg);
		}
	}
	else
		this->m_ViewPortExtents = rect_type(MIN_VALUE(typename ViewPortInfoEx::point_type), MAX_VALUE(typename ViewPortInfoEx::point_type));
	this->m_CountColor = cc;
}

template <typename Int>
void ViewPortInfoEx<Int>::SetWritability(AbstrDataItem* adi) const
{
// REMOVE, CLEAN-UP THIS FUNCTION AND ALL ITS CALLERS.
//	if (!Is1to1())
//		storageReadOnlyPropDefPtr->SetValue(adi, true);
}

ViewPortInfoProvider::ViewPortInfoProvider(const TreeItem * storageHolder, const AbstrDataItem* adi, bool mayCreateDomain, bool queryActualRange)
	: m_ADI(make_shared_tree(adi, existing_obj{})) // borrow the tree-owned data item (co-own its real control block)
	, m_QueryActualGridDomain(queryActualRange)
{
	// PRECONDIDION
	assert(storageHolder);
	assert(adi);

	// Domain of column & grid
	auto allowDomainCalculation = tmp_swapper(s_IsDetectingIncInterest, false);

	SharedUnitInterestPtr currDomain = CheckedGridDomain(adi); dms_assert(currDomain);
	SharedUnitInterestPtr gridDomain = GetGridDataDomainRO(storageHolder);
	if (!gridDomain && mayCreateDomain)
		if (auto nmsm = dynamic_cast<NonmappableStorageManager*>(storageHolder->GetStorageManager()))
			gridDomain = nmsm->CreateGridDataDomain(storageHolder);

	if (!gridDomain)
		gridDomain = currDomain;

	m_CurrDomain = currDomain;
	m_GridDomain = gridDomain;


	const AbstrUnit* currBase = GetWorldCrdUnitFromGeoUnit(currDomain);
	const AbstrUnit* gridBase = GetWorldCrdUnitFromGeoUnit(gridDomain);

	if (currBase && gridBase) 
	{
		auto sr1 = currBase->GetSpatialReference();
		auto sr2 = gridBase->GetSpatialReference();
		if (sr1 && sr2 && sr1 != sr2)
			adi->throwItemErrorF("SpatialReference {} of {} incompatible with SpatialReference {} of {}."
			, sr1, currDomain->GetName().c_str()
			, sr2, gridDomain->GetName().c_str()
			);
	}
	

	//	extents of the file raster
	gridDomain->PrepareDataUsage(DrlType::Certain);
	currDomain->PrepareDataUsage(DrlType::Certain);

	SharedStr sql = TreeItemPropertyValue(adi, sqlStringPropDefPtr);
	bool sqlIsNumeric = true;
	for (char const& c : std::string(sql.c_str())) {
		if (std::isdigit(c) == 0)
			sqlIsNumeric = false;
	}

	if (sql.empty())
		m_CountColor = -1;
	else if (sqlIsNumeric)
		m_CountColor = atoi(sql.c_str());
	else
		m_CountColor = -1;
}

ViewPortInfoEx<Int32> ViewPortInfoProvider::GetViewportInfoEx(tile_id tc, StorageMetaInfoPtr smi, tile_id tg) const
{
	FixedContextHandle provideExceptionContext("in constructing a ViewPortInfo<Int32> (for transfering data from one tiling to another)");
	auto curr_range_unit = AsUnit(m_CurrDomain->GetCurrRangeItem());
	auto grid_range_unit = AsUnit(m_GridDomain->GetCurrRangeItem());

	return ViewPortInfoEx<Int32>(m_ADI.get(), curr_range_unit.get(), tc, grid_range_unit.get(), tg, smi, true, false, m_CountColor, m_QueryActualGridDomain);
}

template struct ViewPortInfoEx<Int32>;
template struct ViewPortInfoEx<Int64>;
