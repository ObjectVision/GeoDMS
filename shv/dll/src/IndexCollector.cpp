// Copyright (C) 1998-2025 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "ShvDllPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

#include "IndexCollector.h"

#include "mci/CompositeCast.h"
#include "mci/ValueClass.h"
#include "mci/ValueWrap.h"
#include "mci/ValueClassID.h"
#include "set/VectorFunc.h"

#include "LispList.h"

#include "AbstrCalculator.h"
#include "DataArray.h"
#include "Unit.h"
#include "DataController.h"
#include "LispTreeType.h"

#include "Theme.h"

namespace {

	typedef std::map<index_collector_key, IndexCollector*> index_collector_map;
	index_collector_map s_IndexCollectorMap;
}

SharedPtr<IndexCollector> IndexCollector::Create(index_collector_key key)
{
	if  (!key.first && !key.second)
		return {};
	IndexCollector*& ref = s_IndexCollectorMap[key];
	if (!ref)
		ref = new IndexCollector(key);
	return ref;
}

SharedPtr<IndexCollector> IndexCollector::Create(const Theme* featureTheme)
{
	if  (!featureTheme)
		return {};
	return Create(index_collector_key(featureTheme->GetClassification(), featureTheme->GetThemeAttr()));
}


IndexCollector::IndexCollector(index_collector_key key)
	:	m_ExtKeyAttr(make_shared_tree(key.first, existing_obj{})) // = featureTheme->GetClassification() = Feature -> ExtKey (borrow tree-owned attr)
	,	m_GeoRelAttr(make_shared_tree(key.second, existing_obj{})) // = featureTheme->GetThemeAttr()      = Entity  -> ExtKey (borrow tree-owned attr)
{
	assert(key.first || key.second);
	PreparedDataReadLock lck1(key.first , "IndexCollector::ctor");
	PreparedDataReadLock lck2(key.second, "IndexCollector::ctor");

	LispRef expr; // Feature -> Entity
	const AbstrUnit* idValues; // entity_id

	if (!key.second)
	{
		expr = key.first->GetCheckedKeyExpr();
		idValues = key.first->GetAbstrValuesUnit();
	}
	else
	{
		idValues = key.second->GetAbstrDomainUnit();
		if (key.first)
			expr= List3<LispRef>(LispRef("rlookup"), key.first->GetCheckedKeyExpr(), key.second->GetCheckedKeyExpr());
		else
			expr= List2<LispRef>(LispRef("invert"), key.second->GetCheckedKeyExpr());
		expr = AbstrCalculator::RewriteExprTop(expr);
	}
	if (!idValues->GetValueType()->IsNumeric() || idValues->GetRangeAsFloat64().first != 0)
		expr = AbstrCalculator::RewriteExprTop(List2<LispRef>(LispRef(token::ordinal), expr)); //, idValues->GetAsLispRef()));
	if (idValues->GetValueType()->GetCrdClass() != ValueWrap<entity_id>::GetStaticClass())
		expr = AbstrCalculator::RewriteExprTop(List2<LispRef>(LispRef(ValueWrap<entity_id>::GetStaticClass()->GetNameID()), expr)); //, idValues->GetAsLispRef()));
	
	m_DC = GetOrCreateDataController(expr);

	SharedTreeItem resultHolder; // keeps the MakeResult item alive for the rest of this ctor
	const AbstrDataItem* adi;
	if (HasGeoRel())
	{
		resultHolder = m_DC->MakeResult();
		adi = AsDataItem(resultHolder.get());
	}
	else
		adi = m_ExtKeyAttr.get();
	MG_CHECK(adi);

	auto adu = adi->GetAbstrDomainUnit();
	MG_CHECK(adu);
	auto rangeItem = adu->GetCurrRangeItem();
	MG_CHECK(rangeItem);
	m_TileData = AsUnit(rangeItem.get())->GetTiledRangeData();
	MG_CHECK(m_TileData);
}

IndexCollector::~IndexCollector()
{
	s_IndexCollectorMap.erase(index_collector_key(m_ExtKeyAttr.get(), m_GeoRelAttr.get()));
}

void IndexCollector::Release()
{
	delete this;
}

DataReadLock IndexCollector::GetDataItemReadLock() const
{
	auto res = m_DC->CallCalcResult();
	if (!res)
		throwErrorF("IndexCollector", "Cannot calculate data for {}", AsString(m_DC->GetLispRef()).c_str());
	auto item = res->GetCurr();
	MG_CHECK(item);
	PreparedDataReadLock lock(AsDataItem(item.get()), "IndexCollector::GetDataItemReadLock");

	return lock;
}

auto IndexCollector::GetDataRead(tile_id t) const -> DataArray<entity_id>::locked_cseq_t
{
	auto lock = GetDataItemReadLock();

	if (!lock.GetRefObj())
		throwErrorF("IndexCollector", "Cannot create data for {}", AsString(m_DC->GetLispRef()).c_str());
	return const_array_checked_cast<entity_id>(lock.GetRefObj())->GetDataRead(t);
}

tile_loc IndexCollector::GetTileDataLocation(SizeT index) const
{	
	return m_TileData->GetTileDataLocation(index);
}

tile_id IndexCollector::GetNrTiles() const
{
	dms_assert(HasExtKey());
	return m_ExtKeyAttr->GetAbstrDomainUnit()->GetNrTiles();
}

/*
entity_id IndexCollector::GetEntityIndex(feature_id featureIndex) const
{
	assert(HasExtKey() || HasGeoRel());

	if (featureIndex >= m_Array.size())
		return UNDEFINED_VALUE(entity_id);
	return m_Array[featureIndex];
}
*/

feature_id IndexCollector::GetFeatureIndex(entity_id entityIndex) const
{
	assert(!HasExtKey());
	assert( HasGeoRel());
	assert(m_GeoRelAttr->GetDataRefLockCount() > 0);

	return m_GeoRelAttr->GetRefObj()->GetValueAsSizeT(entityIndex);
}

const AbstrUnit* IndexCollector::GetFeatureDomain() const
{
	if (m_ExtKeyAttr)
		return m_ExtKeyAttr->GetAbstrDomainUnit();
	return m_GeoRelAttr->GetAbstrValuesUnit();
}

//----------------------------------------------------------------------
// struct  : LockedIndexCollectorPtr
//----------------------------------------------------------------------

#include "LockedIndexCollectorPtr.h"

LockedIndexCollectorPtr::LockedIndexCollectorPtr(const IndexCollector* ptr)
{
	if (ptr)
		m_Lock = ptr->GetDataItemReadLock();
}

OptionalIndexCollectorAray::OptionalIndexCollectorAray(const IndexCollector* ptr, tile_id t)
{
	if (ptr)
		this->emplace( ptr->GetDataRead(t) );
}

