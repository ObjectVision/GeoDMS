// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////
#pragma once

#ifndef __SHV_INDEXCACHE_H
#define __SHV_INDEXCACHE_H

#include "vt/BaseBounds.h"
#include "ptr/WeakPtr.h"

#include <vector>

#include "ShvBase.h"
#include "rlookup.h"

class Theme;
class AbstrUnit;
class AbstrThemeValueGetter;

//----------------------------------------------------------------------
// struct  : ResourceIndexCache
//----------------------------------------------------------------------

struct ResourceIndexCache
{
	resource_index_t GetKeyIndex(entity_id entityId) const;
	Int32 GetWidth(entity_id e) const;
	auto GetDefaultPixelWidth()->Float64 { return m_DefaultPixelWidth; }

protected: 
	ResourceIndexCache(
		const Theme* pixelSizeTheme   // Character Height in #Points (1 Point = (1/72) inch = 0.3527777 mm)
	,	const Theme* worldSizeTheme  // Additional zoom-level dependent factor of Character Height in World Coord Units
	,	Float64 defPixelSize
	,	Float64 defWorldSize
	,	const AbstrUnit* entityDomain
	,	const AbstrUnit* projectionBaseUnit
	);

	bool CompareValueGetter(const AbstrThemeValueGetter* additionalTheme) const;
	bool IsDifferent(Float64 nrPixelsPerWorldUnit, Float64 subPixelFactor) const;

	entity_id m_EntityDomainCount;
	Float64   m_DefaultPixelWidth, m_DefaultWorldWidth;

	WeakPtr<const AbstrThemeValueGetter> m_PixelWidthValueGetter;
	WeakPtr<const AbstrThemeValueGetter> m_WorldWidthValueGetter;

	mutable WeakPtr<const AbstrThemeValueGetter> m_CompatibleTheme;

	mutable	Float64                 m_LastNrPixelsPerWorldUnit;
	mutable	Float64                 m_LastSubPixelFactor;

	mutable std::vector<resource_index_t> m_KeyIndices;
};

template<typename KeyType> 
void MakeKeyIndex(std::vector<resource_index_t>& keyIndices, KeyType& keys)
{
	assert(keys.size());
	auto orgKeys = keys;

	std::sort(keys.begin(), keys.end());
	keys.erase(std::unique(keys.begin(), keys.end()), keys.end());

	if (keys.size() > 1)
	{
		keyIndices.resize(orgKeys.size());
		rlookup2index_array_non_null_values(keyIndices, orgKeys, keys);
	}
	else
		keyIndices.clear();
}


#endif // __SHV_INDEXCACHE_H

