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
		m_DefaultWorldWidth = GetUnitSizeInMeters(projectionBaseUnit);
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
	dms_assert( nrPixelsPerWorldUnit > 0.0);
	dms_assert( subPixelFactor       > 0.0);

	if (m_LastNrPixelsPerWorldUnit == nrPixelsPerWorldUnit && m_LastSubPixelFactor == subPixelFactor)
		return false;

	m_LastNrPixelsPerWorldUnit = nrPixelsPerWorldUnit;
	m_LastSubPixelFactor       = subPixelFactor;
	return true;
}


UInt32 ResourceIndexCache::GetKeyIndex(entity_id entityId) const
{
	dms_assert(m_KeyIndices.size() > 0); // UpdateForZoomLevel must have been called at least once
	dms_assert(m_LastSubPixelFactor> 0); // idem
	dms_assert(IsDefined(entityId));

	dms_assert(entityId < m_EntityDomainCount);

	if (m_CompatibleTheme)
	{
		entityId = m_CompatibleTheme->GetClassIndex(entityId);
		if (!IsDefined(entityId)) entityId = m_KeyIndices.size()-1; // inserted by AddUndefinedKey
	}

	dms_assert(entityId < m_KeyIndices.size());
	return m_KeyIndices[entityId];
}

