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

#include "ShvDllPch.h"

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


#include "Theme.h"

namespace {

	typedef std::map<index_collector_key, IndexCollector*> index_collector_map;
	index_collector_map s_IndexCollectorMap;
}

SharedPtr<IndexCollector> IndexCollector::Create(index_collector_key key)
{
	if  (!key.first && !key.second)
		return 0;
	IndexCollector*& ref = s_IndexCollectorMap[key];
	if (!ref)
		ref = new IndexCollector(key);
	return ref;
}

SharedPtr<IndexCollector> IndexCollector::Create(const Theme* featureTheme)
{
	if  (!featureTheme)
		return nullptr;
	return Create(index_collector_key(featureTheme->GetClassification(), featureTheme->GetThemeAttr()));
}


IndexCollector::IndexCollector(index_collector_key key)
	:	m_ExtKeyAttr   ( key.first  ) // = featureTheme->GetClassification() = Feature -> ExtKey
	,	m_GeoRelAttr   ( key.second ) // = featureTheme->GetThemeAttr()      = Entity  -> ExtKey
{
	dms_assert(key.first || key.second);
	PreparedDataReadLock lck1(key.first); 
	PreparedDataReadLock lck2(key.second); 

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
		expr = AbstrCalculator::RewriteExprTop(List2<LispRef>(LispRef("ordinal"), expr)); //, idValues->GetAsLispRef()));
	if (idValues->GetValueType()->GetCrdClass() != ValueWrap<entity_id>::GetStaticClass())
		expr = AbstrCalculator::RewriteExprTop(List2<LispRef>(LispRef(ValueWrap<entity_id>::GetStaticClass()->GetID()), expr)); //, idValues->GetAsLispRef()));
	
	m_DC = GetOrCreateDataController(expr);
}

IndexCollector::~IndexCollector()
{
	s_IndexCollectorMap.erase(index_collector_key(m_ExtKeyAttr, m_GeoRelAttr));
}

void IndexCollector::Release()
{
	if (!DecRef())
		delete this;
}

DataReadLock IndexCollector::GetDataItemReadLock(tile_id t) const
{
	auto res = m_DC->CalcResult();
	PreparedDataReadLock lock(AsDataItem(res->GetOld()));

	if (!lock.GetRefObj())
		throwErrorF("IndexCollector", "Cannot create data for %s", AsString(m_DC->GetLispRef()).c_str());

	return lock;
}

tile_loc IndexCollector::GetTiledLocation(SizeT index) const
{
	const AbstrDataItem* adi;
	if (HasGeoRel())
		adi = AsDataItem(m_DC->MakeResult().get_ptr());
	else
		adi = m_ExtKeyAttr;
	dms_assert(adi);
	
	return AsUnit(adi->GetAbstrDomainUnit()->GetCurrRangeItem())->GetTiledRangeData()->GetTiledLocation(index);
}

tile_id IndexCollector::GetNrTiles() const
{
	dms_assert(HasExtKey());
	return m_ExtKeyAttr->GetAbstrDomainUnit()->GetNrTiles();
}

entity_id IndexCollector::GetEntityIndex(feature_id featureIndex) const
{
	dms_assert(HasExtKey() || HasGeoRel());
	
	auto tl = GetTiledLocation(featureIndex);
	auto drl = GetDataItemReadLock(tl.first);
	auto data = const_array_cast<entity_id>(drl.GetRefObj())->GetTile(tl.first);
	dms_assert(tl.second < data.size());
	return data[tl.second];
}

feature_id IndexCollector::GetFeatureIndex(entity_id entityIndex) const
{
	dms_assert(!HasExtKey());
	dms_assert( HasGeoRel());
	dms_assert(m_GeoRelAttr->GetDataRefLockCount() > 0);

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

LockedIndexCollectorPtr::LockedIndexCollectorPtr(const IndexCollector* ptr, tile_id t)
	:	WeakPtr<const IndexCollector>(ptr)
{
	if (ptr)
		m_Lock = ptr->GetDataItemReadLock(t);
}

LockedIndexCollectorPtr::~LockedIndexCollectorPtr()
{}

