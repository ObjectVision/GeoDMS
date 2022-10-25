//<HEADER> 
/*
Data & Model Server (DMS) is a server written in C++ for DSS applications. 
Version: see srv/dms/rtc/dll/src/RtcVersion.h for version info.

Copyright (C) 1998-2004  YUSE GSO Object Vision BV. 

Documentation on using the Data & Model Server software can be found at:
http://www.ObjectVision.nl/DMS/

See additional guidelines and notes in srv/dms/Readme-srv.txt 

This library is free software; you can use, redistribute, and/or
modify it under the terms of the GNU General Public License version 2 
(the License) as published by the Free Software Foundation,
provided that this entire header notice and readme-srv.txt is preserved.

See LICENSE.TXT for terms of distribution or look at our web site:
http://www.objectvision.nl/DMS/License.txt
or alternatively at: http://www.gnu.org/copyleft/gpl.html

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
General Public License for more details. However, specific warranties might be
granted by an additional written contract for support, assistance and/or development
*/
//</HEADER>

#include "StoragePCH.h"
#pragma hdrstop

#include "dbg/DebugCast.h"
#include "dbg/DmsCatch.h"
#include "geo/Round.h"
#include "geo/Transform.h"
#include "geo/Conversions.h"
#include "mci/CompositeCast.h"
#include "mci/ValueClass.h"
#include "mci/ValueWrap.h"
#include "utl/Environment.h"

#include "AbstrDataItem.h"
#include "AbstrDataObject.h"
#include "DataLocks.h"
#include "DataStoreManagerCaller.h"
#include "Metric.h"
#include "Projection.h"

#include "TreeItemClass.h"
#include "TreeItemProps.h"
#include "Unit.h"
#include "UnitClass.h"

#include <minmax.h>

#include <share.h>

#include "NameSet.h"
#include "ViewPortInfoEx.h"
#include "GridStorageManager.h"

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
			dataItem->Fail("storageManager requires all dataItems to have the same domain", FR_Data);
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
	storageHolder->ThrowFail("Storage expected an unabiguous domain", FR_MetaInfo);

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

bool WriteGeoRefFile(const AbstrDataItem* diGrid, WeakStr geoRefFileName)
{
	dms_assert(diGrid);
	const AbstrUnit* colDomain = diGrid->GetAbstrDomainUnit();
	dms_assert(diGrid);

	auto [gridBegin, gridEnd] = colDomain->GetRangeAsDRect();
	
	FilePtrHandle bmpwHnd; bmpwHnd.OpenFH(geoRefFileName, DSM::GetSafeFileWriterArray(diGrid), FCM_CreateAlways, true, NR_PAGES_HDRFILE);

	if (bmpwHnd == NULL)
		return false;

	DMS_CALL_BEGIN

		// MapObjects coordinate system is in cartesian order (LeftBottom -> RightTop)
		// and wants offset of center of TopLeft cell, positive x factor and negative y factor.
		// In DMS we defined grid as a series of half-open intervals starting from BottomLeft corner of BottomLeft cell.

		const UnitProjection* colProj = colDomain->GetProjection(); // note that this doesn't have to be the composite projection

		DPoint factor = (colProj) ? colProj->Factor() : DPoint(1.0, 1.0), f2 = factor;
			if (factor.X() < 0) { f2.X() = -factor.X(); gridBegin.Col() = gridEnd.Col(); }
			if (factor.Y() > 0) { f2.Y() = -factor.Y(); gridBegin.Row() = gridEnd.Row(); }

		DPoint offset = ((colProj) ? colProj->Offset() : DPoint()) + gridBegin * factor + 0.5 * f2;

		fprintf(bmpwHnd, "%.9G\n", f2.X());
		fprintf(bmpwHnd, "%.9G\n", Float64(0.0));
		fprintf(bmpwHnd, "%.9G\n", Float64(0.0));
		fprintf(bmpwHnd, "%.9G\n", f2.Y());
		fprintf(bmpwHnd, "%.9G\n", offset.X());
		fprintf(bmpwHnd, "%.9G\n", offset.Y());

	DMS_CALL_END

	return true;
}

#include "ser/MoreStreamBuff.h"

void ReadGeoRefFile(WeakStr geoRefFileName, AbstrUnit* uDomain, const AbstrUnit* uBase)
{
	dms_assert(uDomain != uBase);
	std::vector<Byte> buffer;
	{
		FilePtrHandle file;

		file.OpenFH(
			geoRefFileName
		,	DSM::GetSafeFileWriterArray(uDomain)
		,	FCM_OpenReadOnly, false, NR_PAGES_HDRFILE
		);
		if(file)
		{
			SizeT size = file.GetFileSize();
			buffer.resize(size+1, 0);
			fread( begin_ptr( buffer ), size, 1, file);
		}
	}
	if (buffer.size())
	{
		std::replace(buffer.begin(), buffer.end(), ',','.');
		MemoInpStreamBuff inpBuf(begin_ptr( buffer ), end_ptr( buffer ) );
		FormattedInpStream str(&inpBuf);

		DPoint factor, dummy, offset;

		str >> factor.X() >> dummy.X() >> dummy.Y() >> factor.Y() >> offset.X() >> offset.Y();
		dms_assert(factor.X() > 0);
		dms_assert(factor.Y() < 0);

		uBase->UpdateMetaInfo();
		uDomain->SetProjection(new UnitProjection(AsUnit(uBase->GetCurrUltimateItem()), offset - 0.5 * factor, factor));

		return;
	}
	uDomain->throwItemErrorF("Error reading geographic reference info from %s", geoRefFileName.c_str());
}

const AbstrUnit* FindProjectionBase(const TreeItem* storageHolder, const AbstrUnit* gridDataDomain)
{
	dms_assert(storageHolder); // PRECONDITION
	if (!gridDataDomain)
		return nullptr;
	if (!storageHolder->DoesContain(gridDataDomain) && (gridDataDomain->GetTreeParent() || !gridDataDomain->IsPassor() ) )
		return nullptr;

	SharedStr coordRef = dialogDataPropDefPtr->GetValue(gridDataDomain);
	if (coordRef.empty() && storageHolder != gridDataDomain)
		coordRef = dialogDataPropDefPtr->GetValue(storageHolder);

	const AbstrUnit* uBase = nullptr;
	if (coordRef.empty())
	{
		uBase = AsDynamicUnit(storageHolder);
		if (!uBase && IsDataItem(storageHolder))
			uBase = AsDataItem(storageHolder)->GetAbstrDomainUnit();
		if (uBase)
		{
			const UnitProjection* prj = uBase->GetProjection();
			if (prj)
				uBase = prj->GetBaseUnit();
			else
				uBase = nullptr; // no self referring thing
		}
	}
	else
	{
		if (gridDataDomain->GetTreeParent())
			storageHolder = gridDataDomain->GetTreeParent();
		auto coordItem = storageHolder->FindItem(coordRef);
		if (!coordItem)
			storageHolder->throwItemErrorF("Cannot find DialogData reference '%s'", coordRef.c_str());
		if (IsUnit(coordItem))
			uBase = AsUnit(coordItem);
	}
	if (uBase && uBase->GetNrDimensions() != 2)
	{
		auto coordItemName = SharedStr(uBase->GetFullName());
		storageHolder->throwItemErrorF("Found coordinate base '%s' is not a geometric domain",  coordItemName.c_str());
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

void ReadProjection(TreeItem* storageHolder, WeakStr geoRefFileName)
{
	dms_assert(storageHolder); // PRECONDITION

	AbstrUnit* gridDataDomain = GetGridDataDomainRW(storageHolder); 

	const AbstrUnit* uBase = FindProjectionBase(storageHolder, gridDataDomain );
	if (!uBase)
		return;

	ReadGeoRefFile(geoRefFileName, gridDataDomain, uBase);
}

// ------------------------------------------------------------------------
//
// TNameSet for mapping to/from limited namespace
//
// ------------------------------------------------------------------------

SharedStr ToUpperCase(SharedStr src)
{
	SharedStr result(src.begin(), src.send());
	typedef char* charPtr;
	for (charPtr chPtr = result.begin(), chEnd = result.send(); chPtr != chEnd; ++chPtr)
		*chPtr = toupper(*chPtr);
	return result;
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
		name2 = SharedStr(name.begin(), name.begin() + m_Len);

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


	if (*src >= '0' && *src <= '9')
		*dstPtr++ = '_';

	while(dstPtr != dstEnd)
	{
		char ch = *src;
		if (!ch)
			break;

		if ((ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9'))
			*dstPtr++ = ch;
		else
			*dstPtr++ = '_';
		++src;
	}
	*dstPtr = char(0);
	return SharedStr(dst.get(), dstPtr);
}

SharedStr TNameSet::FieldNameToItemName(CharPtr fieldName) const
{
	SharedStr mappedName = FieldNameToMappedName(fieldName);
	for (auto& mapping : m_Mappings)
		if (EqualName(mapping.second.first.c_str(), mappedName.c_str()))
			return mapping.first;
	throwErrorF("TNameSet", "unknown MappedName %1% for FieldName %2%", mappedName, fieldName);
}

SharedStr TNameSet::ItemNameToFieldName(CharPtr itemName) const
{
	auto posIter = m_Mappings.find(itemName);
	if (posIter == m_Mappings.end())
		throwErrorF("TNameSet", "unknown ItemName %1%", itemName);
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
		throwErrorF("TNameSet", "NameConflict between %1% and FieldName %2%", mappedName, fieldName);
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

bool CreateTreeItemColumnInfo(TreeItem* tiTable, CharPtr colName, const AbstrUnit *domainUnit, const ValueClass *dbValuesClass)
{
	if (dbValuesClass == nullptr)
		return false;

	TreeItem* tiColumn = tiTable->GetItem(colName)->CheckCls(AbstrDataItem::GetStaticClass());

	if (tiColumn)
	{
		if (!TreeItemIsColumn(tiColumn))
			return false;
		const ValueClass *vc = debug_cast<AbstrDataItem*>(tiColumn)->GetAbstrValuesUnit()->GetValueType();
		bool res = CompatibleTypes(dbValuesClass, vc);
		if (!res)
		{
			auto msg = mySSPrintF("StorageManager: inconsistent value types; table: %s, column: %s, configured type: %s, database type: %s",
				tiTable->GetFullName(),
				colName,
				vc->GetName(),
				dbValuesClass->GetName()
			);
			tiColumn->Fail(msg, FR_Data);
		}
		return res;
	}
	const AbstrUnit* valuesUnit = UnitClass::Find(dbValuesClass)->CreateDefault();
	dms_assert(valuesUnit);
	return CreateDataItem(tiTable, GetTokenID_mt(colName),	domainUnit, valuesUnit) != nullptr;
}

// ------------------------------------------------------------------------
// Implementation of ViewPortInfo
// ------------------------------------------------------------------------

template <typename Int>
ViewPortInfoEx<Int>::ViewPortInfoEx(const TreeItem* context, const AbstrUnit* currDomain, tile_id tc, const AbstrUnit* gridDomain, tile_id tg, bool correctGridOffset, bool mustCheck, countcolor_t cc, bool queryActualGridDomain)
{
	dms_assert(queryActualGridDomain || !IsDefined(tg));
	dms_assert(!gridDomain || gridDomain == gridDomain->GetCurrRangeItem());
	dms_assert(!currDomain || currDomain == currDomain->GetCurrRangeItem());
	dms_assert(queryActualGridDomain || !correctGridOffset);
	dms_assert(queryActualGridDomain || tg == no_tile);
	dms_assert(!correctGridOffset || queryActualGridDomain);

	if (queryActualGridDomain && gridDomain)
		m_GridExtents = ThrowingConvert<rect_type>(gridDomain->GetTileSizeAsI64Rect(tg));
	else
		m_GridExtents = rect_type(MIN_VALUE(ViewPortInfoEx::point_type), MAX_VALUE(ViewPortInfoEx::point_type));

	const UnitProjection* currProj = currDomain ? currDomain->GetCurrProjection() : nullptr;
	const UnitProjection* gridProj = gridDomain ? gridDomain->GetCurrProjection() : nullptr;
	const AbstrUnit* currBase = currProj ? currProj->GetCompositeBase() : currDomain;
	const AbstrUnit* gridBase = gridProj ? gridProj->GetCompositeBase() : gridDomain;

	if (mustCheck)
	{
		if (!currBase->UnifyDomain(gridBase, UM_AllowAllEqualCount))
			context->throwItemErrorF("ProjectionBase %s incompatible with ProjectionBase %s of GridDomain", currBase->GetName().c_str(), gridBase->GetName().c_str());
		if (tc == no_tile)
			return;
	}
	else
	{
		dms_assert(!currBase || currBase->Was(PS_MetaInfo));
		dms_assert(!gridBase || gridBase->Was(PS_MetaInfo));
	}

	//	Projections in world coords
	CrdTransformation fileRasterProj2World = UnitProjection::GetCompositeTransform(gridProj);
	CrdTransformation viewPortProj2World   = UnitProjection::GetCompositeTransform(currProj);

	MG_CHECK(!fileRasterProj2World.IsSingular());
	CrdTransformation viewPortProj2fileRasterProj = viewPortProj2World / fileRasterProj2World;

	*typesafe_cast<CrdTransformation*>(this) = viewPortProj2fileRasterProj;
	if (correctGridOffset)
	*typesafe_cast<CrdTransformation*>(this) -= Convert<DPoint>(GetTopLeft(m_GridExtents, viewPortProj2fileRasterProj.Orientation())); // camouflage of non-zero based grids

	if (currDomain)
		this->m_ViewPortExtents = ThrowingConvert<rect_type>(currDomain->GetTiledRangeData()->GetTileRangeAsI64Rect(tc));
	else
		this->m_ViewPortExtents = rect_type(MIN_VALUE(ViewPortInfoEx::point_type), MAX_VALUE(ViewPortInfoEx::point_type));
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
	: m_ADI(adi)
	, m_QueryActualGridDomain(queryActualRange)
{
	// PRECONDIDION
	dms_assert(storageHolder);
	dms_assert(adi);

	// Domain of column & grid
	SharedUnitInterestPtr currDomain = CheckedGridDomain(adi); dms_assert(currDomain);
	SharedUnitInterestPtr gridDomain = GetGridDataDomainRO(storageHolder);
	if (!gridDomain && mayCreateDomain)
		gridDomain = storageHolder->GetStorageManager()->CreateGridDataDomain(storageHolder);
	if (!gridDomain)
		gridDomain = currDomain;

	m_CurrDomain = currDomain;
	m_GridDomain = gridDomain;

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

ViewPortInfoEx<Int32> ViewPortInfoProvider::GetViewportInfoEx(tile_id tc, tile_id tg) const
{
	FixedContextHandle provideExceptionContext("in constructing a ViewPortInfo<Int32> (for transfering data from one tiling to another)");
	return ViewPortInfoEx<Int32>(m_ADI, AsUnit(m_CurrDomain->GetCurrRangeItem()), tc, AsUnit(m_GridDomain->GetCurrRangeItem()), tg, true, false, m_CountColor, m_QueryActualGridDomain);
}

template ViewPortInfoEx<Int32>;
template ViewPortInfoEx<Int64>;