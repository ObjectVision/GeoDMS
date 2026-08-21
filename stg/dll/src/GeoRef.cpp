// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////

#include "StoragePCH.h"
#include "ImplMain.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif

// Geo-reference file I/O: the affine grid transformation from a grid data
// item, Write/ReadGeoRefFile and GetImageToWorldTransformFromFile.
// Split from stg DllMain.cpp (2026-08).

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

// ------------------------------------------------------------------------
//
// Helper functions
//
// ------------------------------------------------------------------------

#include "FilePtrHandle.h"
auto GetAffineTransformationFromGridDataItem(const AbstrDataItem* grid_adi, bool offset_to_top_left_cell) -> Transformation<Float64> {
	assert(grid_adi);

	auto grid_adu = grid_adi->GetAbstrDomainUnit();
	MG_CHECK(grid_adu);

	auto [grid_begin, grid_end] = grid_adu->GetRangeAsDRect();

	auto grid_projection = grid_adu->GetProjection();
	auto factor = DPoint(1.0, 1.0);
	if (grid_projection)
		factor = grid_projection->Factor();

	DPoint gridOrigin, f2;
	if (factor.X() < 0) {
		f2.X() = -factor.X();
		gridOrigin.Col() = grid_end.Col();
	}
	else
	{
		f2.X() = factor.X();
		gridOrigin.Col() = grid_begin.Col();
	}

	if (factor.Y() > 0) {
		f2.Y() = -factor.Y();
		gridOrigin.Row() = grid_end.Row();
	}
	else
	{
		f2.Y() = factor.Y();
		gridOrigin.Row() = grid_begin.Row();
	}

	auto offset = gridOrigin * factor;
	if (grid_projection)
		offset += grid_projection->Offset();
	
	if (offset_to_top_left_cell)
		offset += 0.5 * f2;
	
	auto affine_transformation = Transformation(offset, factor);
	return affine_transformation;
}

FileResult WriteGeoRefFile(const AbstrDataItem* grid_adi, WeakStr geoRefFileName)
{
	dms_assert(grid_adi);
	const AbstrUnit* colDomain = grid_adi->GetAbstrDomainUnit();
	dms_assert(grid_adi);

	FilePtrHandle bmpwHnd; 
	auto r = bmpwHnd.OpenFH(geoRefFileName, FCM_CreateAlways, true, NR_PAGES_HDRFILE);

	if (!r)
		return r;

	DMS_CALL_BEGIN
		// MapObjects coordinate system is in cartesian order (LeftBottom -> RightTop)
		// and wants offset of center of TopLeft cell, positive x factor and negative y factor.
		// In DMS we defined grid as a series of half-open intervals starting from BottomLeft corner of BottomLeft cell.
		const UnitProjection* colProj = colDomain->GetProjection(); // note that this doesn't have to be the composite projection
		auto affine_transformation = GetAffineTransformationFromGridDataItem(grid_adi);
		
		fprintf(bmpwHnd, "%.9G\n", affine_transformation.Factor().X());
		fprintf(bmpwHnd, "%.9G\n", Float64(0.0));
		fprintf(bmpwHnd, "%.9G\n", Float64(0.0));
		fprintf(bmpwHnd, "%.9G\n", affine_transformation.Factor().Y());
		fprintf(bmpwHnd, "%.9G\n", affine_transformation.Offset().X());
		fprintf(bmpwHnd, "%.9G\n", affine_transformation.Offset().Y());

		return {};

	DMS_CALL_END

	return std::unexpected(GetLastErrorMsgStr());
}

#include "ser/MoreStreamBuff.h"

void ReadGeoRefFile(WeakStr geoRefFileName, AbstrUnit* uDomain, const AbstrUnit* uBase)
{
	assert(uDomain != uBase);
	std::vector<Byte> buffer;
	{
		FilePtrHandle file;

		auto r = file.OpenFH(geoRefFileName, FCM_OpenReadOnly, false, NR_PAGES_HDRFILE);
		if (!r)
			r.Throw("ReadGeoRefFile");
		SizeT size = file.GetFileSize();
		buffer.resize(size + 1, 0);
		MG_CHECK(size == 0 || fread(begin_ptr(buffer), size, 1, file) == 1);
	}
	assert(buffer.size());

	std::replace(buffer.begin(), buffer.end(), ',','.');
	MemoInpStreamBuff inpBuf(begin_ptr( buffer ), end_ptr( buffer ) );
	FormattedInpStream str(&inpBuf);

	DPoint factor, dummy, offset;

	str >> factor.X() >> dummy.X() >> dummy.Y() >> factor.Y() >> offset.X() >> offset.Y();
	assert(factor.X() > 0);
	assert(factor.Y() < 0);

	uBase->UpdateMetaInfo();
	uDomain->SetProjection(new UnitProjection(AsUnit(uBase->GetCurrUltimateItem()).get(), offset - 0.5 * factor, factor));
}

SharedUnit FindProjectionRef(const TreeItem* storageHolder, const AbstrUnit* gridDataDomain)
{
	SharedUnit uBase;
	SharedStr coordRef = dialogDataPropDefPtr->GetValue(gridDataDomain);
	if (!coordRef.empty())
	{
		auto coordItem = gridDataDomain->FindItem(coordRef);
		if (!coordItem && !HasMapType(gridDataDomain))
			gridDataDomain->throwItemErrorF("Cannot find DialogData reference '{}'", coordRef.c_str());
		if (IsUnit(coordItem))
			uBase = AsUnit(coordItem);
	}
	if (uBase == nullptr && storageHolder != gridDataDomain)
	{
		coordRef = dialogDataPropDefPtr->GetValue(storageHolder);
		if (!coordRef.empty())
		{
			auto coordItem = storageHolder->FindItem(coordRef);
			if (!coordItem && !HasMapType(storageHolder))
				storageHolder->throwItemErrorF("Cannot find DialogData reference '{}'", coordRef.c_str());
			if (IsUnit(coordItem))
				uBase = AsUnit(coordItem);
		}
	}
	return uBase;
}

SharedUnit FindProjectionBase(const TreeItem* storageHolder, const AbstrUnit* gridDataDomain)
{
	assert(storageHolder); // PRECONDITION
	if (!gridDataDomain)
		return {};
	if (!storageHolder->DoesContain(gridDataDomain) && (gridDataDomain->GetTreeParent() || !gridDataDomain->IsPassor() ) )
		return {};
	auto uBase = FindProjectionRef(storageHolder, gridDataDomain);
	if (uBase == nullptr)
	{
		uBase = make_shared_tree(AsDynamicUnit(storageHolder), existing_obj{});
		if (!uBase && IsDataItem(storageHolder))
			uBase = make_shared_tree(AsDataItem(storageHolder)->GetAbstrDomainUnit(), existing_obj{});
		if (uBase)
		{
			const UnitProjection* prj = uBase->GetProjection();
			if (prj)
				uBase = make_shared_tree(prj->GetBaseUnit(), existing_obj{});
			else
				uBase = nullptr; // avoid self-referencing
		}
	}

	if (uBase && uBase->GetNrDimensions() != 2)
	{
		auto coordItemName = SharedStr(uBase->GetFullName());
		storageHolder->throwItemErrorF("Found coordinate base '{}' is not a geometric domain",  coordItemName.c_str());
	}
	if (uBase)
	{
		uBase->UpdateMetaInfo();
		auto uRef = uBase->GetCurrUltimateItem();
		if (uRef)
			uBase = AsUnit(uRef);
 	}

	return uBase;
}

void GetImageToWorldTransformFromFile(TreeItem* storageHolder, WeakStr geoRefFileName)
{
	assert(storageHolder); // PRECONDITION
/* 
	const AbstrUnit* gridDataDomainRO = GetGridDataDomainRO(storageHolder); 
	if (!storageHolder->DoesContain(gridDataDomainRO))
		return;
*/

	AbstrUnit* gridDataDomainRW = GetGridDataDomainRW(storageHolder);
	if (!gridDataDomainRW)
		return;

	auto uBase = FindProjectionBase(storageHolder, gridDataDomainRW);
	if (!uBase)
		return;

	ReadGeoRefFile(geoRefFileName, gridDataDomainRW, uBase.get());
}

