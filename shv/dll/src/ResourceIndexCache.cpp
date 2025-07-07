// Copyright (C) 1998-2025 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "ShvDllPch.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

#include "ResourceIndexCache.h"

#include "AbstrUnit.h"

#include "Theme.h"
#include "ThemeValueGetter.h"

#include "gdal/gdal_base.h"

//----------------------------------------------------------------------
// struct  : PenIndexCache
//----------------------------------------------------------------------

ResourceIndexCache::ResourceIndexCache(
			const Theme* pixelWidthTheme   // Character Height in #Points (1 Point = (1/72) inch = 0.3527777 mm)
		,	const Theme* worldWidthTheme  // Additional zoom-level dependent factor of Character Height in World Coord Units
		,	Float64 defPixelSize
		,	Float64 defWorldSize
		,	const AbstrUnit* entityDomain
		,	const AbstrUnit* projectionBaseUnit
		)
	:	m_EntityDomainCount(entityDomain->GetCount() )
	,	m_PixelWidthValueGetter(NULL)
	,	m_WorldWidthValueGetter(NULL)
	,	m_DefaultPixelWidth(defPixelSize)
	,	m_DefaultWorldWidth(defWorldSize)
	,	m_LastNrPixelsPerWorldUnit(-1.0) 
	,	m_LastSubPixelFactor      (-1.0)
{
	if (pixelWidthTheme)
	{
		if (pixelWidthTheme->IsAspectParameter())
		{
			Float64 pixelWidthParam = pixelWidthTheme->GetNumericAspectValue();
			if (IsDefined(pixelWidthParam))
				m_DefaultPixelWidth = pixelWidthParam;
		}
		else
			m_PixelWidthValueGetter = pixelWidthTheme->GetValueGetter();
	}

	if (worldWidthTheme)
	{
		if (worldWidthTheme->IsAspectParameter())
		{
			Float64 worldWidthParam = worldWidthTheme->GetNumericAspectValue();
			if (IsDefined(worldWidthParam))
				m_DefaultWorldWidth = worldWidthParam;
		}
		else
			m_WorldWidthValueGetter = worldWidthTheme->GetValueGetter();
	}
	else if (m_DefaultWorldWidth > 0)
		m_DefaultWorldWidth /= GetUnitSizeInMeters(projectionBaseUnit); // assume that the given default world width was in meters and we need Coordinate Units
}

bool ResourceIndexCache::CompareValueGetter(const AbstrThemeValueGetter* additionalTheme) const
{
	if (!additionalTheme || additionalTheme->IsParameterGetter())
		return true;
	if (m_CompatibleTheme)
	{
		if (!m_CompatibleTheme->HasSameThemeGuarantee(additionalTheme))
		{
			m_CompatibleTheme = nullptr;
			return false;
		}
	}
	else
		m_CompatibleTheme = additionalTheme ;
	return true;
}

//	BREAK HERE, CROSS BOUNDARY VARS: valueGetters, defaults

bool ResourceIndexCache::IsDifferent(Float64 nrPixelsPerWorldUnit, Float64 subPixelFactor) const
{
	assert( nrPixelsPerWorldUnit > 0.0);
	assert( subPixelFactor       > 0.0);

	if (m_LastNrPixelsPerWorldUnit == nrPixelsPerWorldUnit && m_LastSubPixelFactor == subPixelFactor)
		return false;

	m_LastNrPixelsPerWorldUnit = nrPixelsPerWorldUnit;
	m_LastSubPixelFactor       = subPixelFactor;
	return true;
}


UInt32 ResourceIndexCache::GetKeyIndex(entity_id entityId) const
{
	assert(IsDefined(entityId));
	assert(m_LastSubPixelFactor > 0); // UpdateForZoomLevel must have been called at least once

	if (m_KeyIndices.empty())
		return 0;

	assert(entityId < m_EntityDomainCount);

	if (m_CompatibleTheme)
	{
		entityId = m_CompatibleTheme->GetClassIndex(entityId);
		if (!IsDefined(entityId)) 
			entityId = m_KeyIndices.size()-1; // inserted by AddUndefinedKey
	}
	if (entityId >= m_KeyIndices.size())
		entityId = 0;
	assert(entityId < m_KeyIndices.size());
	return m_KeyIndices[entityId];
}

Int32 ResourceIndexCache::GetWidth(entity_id e) const
{
	assert(m_LastSubPixelFactor >= 0);
	assert(m_LastNrPixelsPerWorldUnit >= 0);

	Float64  penPixelWidth = m_DefaultPixelWidth;
	Float64  penWorldWidth = m_DefaultWorldWidth;

	if (m_PixelWidthValueGetter)
		penPixelWidth = m_PixelWidthValueGetter->CreatePaletteGetter()->GetNumericValue(e);
	if (m_WorldWidthValueGetter)
		penWorldWidth = m_WorldWidthValueGetter->CreatePaletteGetter()->GetNumericValue(e);

	assert(penPixelWidth >= 0.0);
	assert(penWorldWidth >= 0.0);
	Int32 totalSize = penPixelWidth * m_LastSubPixelFactor + penWorldWidth * m_LastNrPixelsPerWorldUnit;
	assert(totalSize >= 0);
	return totalSize;
}

