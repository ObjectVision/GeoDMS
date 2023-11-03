// Copyright (C) 2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "GeoPCH.h"
#pragma hdrstop

#include "BoundingBoxCache.h"

#include "set/QuickContainers.h"
#include "LockLevels.h"

//----------------------------------------------------------------------
// class  : AbstrBoundingBoxCache
//----------------------------------------------------------------------

std::map<const AbstrDataObject*, const AbstrBoundingBoxCache*> g_BB_Register;
leveled_critical_section cs_BB(item_level_type(0), ord_level_type::BoundingBoxCache1, "BoundingBoxCache");

AbstrBoundingBoxCache::AbstrBoundingBoxCache(const AbstrDataObject* featureData)
	:	m_FeatureData(featureData)
{}

AbstrBoundingBoxCache::~AbstrBoundingBoxCache()
{
	if (m_HasBeenRegistered)
	{
		leveled_critical_section::scoped_lock lockBB_register(cs_BB);
		g_BB_Register.erase(m_FeatureData);
	}
}

DRect AbstrBoundingBoxCache::GetBounds(tile_id t, tile_offset featureID) const
{
	throwIllegalAbstract(MG_POS, "AbstrBoundingBoxCache::GetBounds");
}

DRect AbstrBoundingBoxCache::GetBounds(SizeT featureID) const
{
	tile_loc tl = m_FeatureData->GetTiledLocation(featureID);
	return GetBounds(tl.first, tl.second);
}



