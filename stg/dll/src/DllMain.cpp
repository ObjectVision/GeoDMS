// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "StoragePCH.h"
#include "ImplMain.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

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
#include "DataStoreManagerCaller.h"
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
RTC_CALL bool s_IsDetectingIncInterest;

STGDLL_CALL void DMS_Stg_Load()
{
}


const ValueClass* GetStreamType(const AbstrDataObject* ado)
{
	return ado->GetValuesType();
}

const ValueClass* GetStreamType(const AbstrDataItem* adi)
{
	return adi->GetDynamicObjClass()->GetValuesType();
}

const AbstrDataItem* TreeItem_AsColumnItem(const TreeItem* ti, bool allowVoid, bool readOnly)
{
	const AbstrDataItem* adi = AsDynamicDataItem(ti);
	if (!adi ||  adi->IsDisabledStorage() || (readOnly && adi->HasCalculator()))
		return nullptr;
	if (!allowVoid && adi->GetAbstrDomainUnit()->GetValueType() == ValueWrap<Void>::GetStaticClass())
		return nullptr;
	return adi;
}

bool TableDomain_IsAttr(const AbstrUnit* domain, const AbstrDataItem* adi)
{
	dms_assert(domain);
	return adi && adi ->GetAbstrDomainUnit() == domain;
}

void TableDomain_TestDataItem(const AbstrUnit*& domain, const TreeItem* ti, bool allowVoid, bool readOnly)
{
	const AbstrDataItem* dataItem = TreeItem_AsColumnItem(ti, allowVoid, readOnly);
	if (dataItem)
	{
		if (domain && !TableDomain_IsAttr(domain, dataItem))
			dataItem->Fail("storageManager requires all dataItems to have the same domain", FailType::Data);
		else
			domain = dataItem->GetAbstrDomainUnit();
	}
}

const AbstrUnit* StorageHolder_GetTableDomain(const TreeItem* storageHolder)
{
	bool readOnly = storageReadOnlyPropDefPtr->GetValue(storageHolder);
	const AbstrUnit* domain = nullptr;

	for (	
			const TreeItem* subItem = storageHolder->_GetFirstSubItem();  // prevent recursive call to UpdateMetaInfo
			subItem; 
			subItem = subItem->GetNextItem()
		)
		TableDomain_TestDataItem(domain, subItem, false, readOnly);
	if (domain) goto exit;

	domain = const_unit_dynacast<UInt32>(storageHolder);
	if (domain) goto exit;

	TableDomain_TestDataItem(domain, storageHolder, true, readOnly);
	if (domain) goto exit;

	for (	
			const TreeItem* subItem = storageHolder->_GetFirstSubItem();  // prevent recursive call to UpdateMetaInfo
			subItem; 
			subItem = subItem->GetNextItem()
		)
	{
		TableDomain_TestDataItem(domain, subItem, true, readOnly);
		if (domain) goto exit;
	}
	storageHolder->ThrowFail("Storage expected an unabiguous domain", FailType::MetaInfo);

exit:
	dms_assert(domain); // POSTCONDITION
	return domain;
}


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

// ------------------------------------------------------------------------
//
// TNameSet for mapping to/from limited namespace
//
// ------------------------------------------------------------------------

SharedStr ToUpperCase(SharedStr src)
{
	for (auto& ch: src) // non-const begin and end quarantee unique access
		ch = std::toupper(ch);
	return src;
}

TNameSet::TNameSet(const UInt32 len)
	:	m_Len(len)
	{}

void TNameSet::InsertIfColumn(const TreeItem* storageHolder, const AbstrUnit* tableDomain)
{
	bool readOnly = storageReadOnlyPropDefPtr->GetValue(storageHolder);

	const AbstrDataItem* adi = TreeItem_AsColumnItem(storageHolder, true, readOnly);
	if (!adi)
		return;

	if (TableDomain_IsAttr(tableDomain, adi))
		InsertItem(adi);
}

bool TNameSet::EqualName(CharPtr n1, CharPtr n2)
{
	return !stricmp(n1, n2);
}

void NextName(SharedStr& name)
{
	auto i = name.send();
	while (i != name.begin())
	{
		--i;
		if (*i < '0' || *i > '9')
		{
			*i = '0';
			return;
		}
		if (*i < '9')
		{
			++*i;
			return;
		}
		*i = '0';
	}
	throwErrorD(nullptr, "Namespace Full");
}

void TNameSet::InsertItem(const AbstrDataItem* adi)
{
	SharedStr name = SharedStr(adi->GetID());
	auto name2 = name;
	if (name2.ssize() > m_Len)
		name2 = SharedStr(CharPtrRange(name.begin(), name.begin() + m_Len));

	while (HasMappedName(name2.c_str()))
		if (name2.ssize() < m_Len)
			name2 += '0';
		else
			NextName(name2);

	m_Mappings[name] = Couple<SharedStr>(name2, ToUpperCase(name2));
}

bool TNameSet::HasMappedName(CharPtr name)
{
	for (auto& mapping : m_Mappings)
		if (EqualName(mapping.second.first.c_str(), name))
			return true;
	return false;
}


SharedStr TNameSet::FieldNameToMappedName(CharPtr src) const
{
	dms_assert(m_Len > 1);
	dms_assert(strlen(src) <= m_Len);

	std::unique_ptr<char[]> dst(new char[GetMappedNameBufferLength()]);

	char* dstPtr = dst.get();
	char* dstEnd = dstPtr + m_Len;


	if (isdigit(* src)) // first character cannot be numerical
		*dstPtr++ = '_';

	while(dstPtr != dstEnd)
	{
		char ch = *src;
		if (!ch)
			break;

		if (itemNameNextChar_test(ch))
			*dstPtr++ = ch;
		else
			*dstPtr++ = '_';
		++src;
	}
	*dstPtr = char(0);
	return SharedStr(CharPtrRange(dst.get(), dstPtr));
}

SharedStr TNameSet::FieldNameToItemName(CharPtr fieldName) const
{
	SharedStr mappedName = FieldNameToMappedName(fieldName);
	for (auto& mapping : m_Mappings)
		if (EqualName(mapping.second.first.c_str(), mappedName.c_str()))
			return mapping.first;
	throwErrorF("TNameSet", "unknown MappedName {0} for FieldName {1}", mappedName, fieldName);
}

SharedStr TNameSet::ItemNameToFieldName(CharPtr itemName) const
{
	auto posIter = m_Mappings.find(itemName);
	if (posIter == m_Mappings.end())
		throwErrorF("TNameSet", "unknown ItemName {0}", itemName);
	return posIter->second.second;
}

SharedStr TNameSet::ItemNameToMappedName(CharPtr itemName) const
{
	auto posIter = m_Mappings.find(itemName);
	if (posIter == m_Mappings.end())
		return{};
	return posIter->second.first;
}

SharedStr TNameSet::InsertFieldName(CharPtr fieldName)
{
	SharedStr mappedName = FieldNameToMappedName(fieldName);
	if (HasMappedName(mappedName.c_str()))
	{
		throwErrorF("TNameSet", "NameConflict between {0} and FieldName {1}", mappedName, fieldName);
	}
	m_Mappings[mappedName] = Couple<SharedStr>(mappedName, SharedStr(fieldName));
	return mappedName;
}


CharPtr TNameSet::GetItemName(CharPtr fieldName) const
{
	auto mappedName = FieldNameToMappedName(fieldName);

	for (auto & mapping: m_Mappings)
	{
		if ( EqualName(mapping.second.first.c_str(), mappedName.c_str()))
			return mapping.first.begin();
	}
	return nullptr;
}

// ------------------------------------------------------------------------
// Implementation of CreateTreeItemColumnInfo
// ------------------------------------------------------------------------

bool TreeItemIsColumn(TreeItem *ti)
{
	return IsDataItem(ti) && ! AsDataItem(ti)->HasVoidDomainGuarantee();
}

bool CompatibleTypes(const ValueClass* dbCls, const ValueClass* configCls)
{
	return dbCls && configCls &&
		(	(dbCls->IsNumericOrBool() && configCls->IsNumericOrBool())
		||	dbCls == configCls);
}

bool CreateTreeItemColumnInfo(TreeItem* tiTable, CharPtr colName, const AbstrUnit *domainUnit, const ValueClass *dbValuesClass, ValueComposition vc)
{
	if (dbValuesClass == nullptr)
		return false;

	TreeItem* tiColumn = TreeItem_CheckCls(tiTable->GetItem(colName), AbstrDataItem::GetStaticClass());

	if (tiColumn)
	{
		if (!TreeItemIsColumn(tiColumn))
			return false;
		const ValueClass *vCls = AsDataItem(tiColumn)->GetAbstrValuesUnit()->GetValueType();
		bool res = CompatibleTypes(dbValuesClass, vCls);
		if (!res)
		{
			auto msg = mySSPrintF("StorageManager: inconsistent value types; table: {}, column: {}, configured type: {}, database type: {}",
				tiTable->GetFullName(),
				colName,
				vCls->GetName(),
				dbValuesClass->GetName()
			);
			tiColumn->Fail(msg, FailType::Data);
		}
		return res;
	}
	const AbstrUnit* valuesUnit = UnitClass::Find(dbValuesClass)->CreateDefault();
	assert(valuesUnit);
	return CreateDataItem(tiTable, GetTokenID_mt(colName),	domainUnit, valuesUnit, vc) != nullptr;
}

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