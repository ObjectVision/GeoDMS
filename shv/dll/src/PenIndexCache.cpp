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

#include "PenIndexCache.h"

#include "mci/CompositeCast.h"

#include "DataArray.h"

#include "rlookup.h"

#include "Theme.h"
#include "ThemeValueGetter.h"

#define PS_GEOMETRIC_STYLE_MASK (0x07|0x00000300|0x00003000)

//#define PS_SOLID            0
//#define PS_DASH             1       /* -------  */
//#define PS_DOT              2       /* .......  */
//#define PS_DASHDOT          3       /* _._._._  */
//#define PS_DASHDOTDOT       4       /* _.._.._  */
//#define PS_NULL             5
//#define PS_INSIDEFRAME      6
//#define PS_USERSTYLE        7

//#define PS_ENDCAP_ROUND     0x00000000
//#define PS_ENDCAP_SQUARE    0x00000100
//#define PS_ENDCAP_FLAT      0x00000200
//#define PS_ENDCAP_MASK      0x00000F00

//#define PS_JOIN_ROUND       0x00000000
//#define PS_JOIN_BEVEL       0x00001000
//#define PS_JOIN_MITER       0x00002000
//#define PS_JOIN_MASK        0x0000F000

//----------------------------------------------------------------------
// struct  : PenIndexCache
//----------------------------------------------------------------------

PenIndexCache::PenIndexCache(
		const Theme* penPixelWidthTheme
	,	const Theme* penWorldWidthTheme
	,	const Theme* penColorTheme
	,	const Theme* penStyleTheme
	,	const AbstrUnit* entityDomain
	,	DmsColor defaultPenColor
	,	const AbstrUnit* projectionBaseUnit
	)	: ResourceIndexCache(penPixelWidthTheme, penWorldWidthTheme, DEFAULT_PEN_PIXEL_WIDTH, DEFAULT_PEN_WORLD_WIDTH, entityDomain, projectionBaseUnit)
		,	m_DefaultPenColor(defaultPenColor)
		,	m_DefaultPenStyle(PS_SOLID)
		,	m_PenColorValueGetter (NULL)
		,	m_PenStyleValueGetter (NULL)
{
	if (penColorTheme)
	{
		if(penColorTheme->IsAspectParameter())
		{
			DmsColor penColorParam = penColorTheme->GetColorAspectValue();
			if (IsDefined(penColorParam))
				m_DefaultPenColor = penColorParam;
		}
		else
			m_PenColorValueGetter = penColorTheme->GetValueGetter();
	}

	if (penStyleTheme)
	{
		if(penStyleTheme->IsAspectParameter())
		{
			Int32 penStyleParam = penStyleTheme->GetOrdinalAspectValue();
			if (IsDefined(penStyleParam))
				m_DefaultPenStyle = penStyleParam;
		}
		else
			m_PenStyleValueGetter = penStyleTheme->GetValueGetter();
	}
}

//	BREAK HERE, CROSS BOUNDARY VARS: valueGetters, defaults

const AbstrUnit* PenIndexCache::GetCommonClassIdUnit() const
{
	m_CompatibleTheme = 0;
	if	(	CompareValueGetter(m_PixelWidthValueGetter)
		&&	CompareValueGetter(m_WorldWidthValueGetter)
		&&	CompareValueGetter(m_PenColorValueGetter)
		&&	CompareValueGetter(m_PenStyleValueGetter)
		&&	m_CompatibleTheme)
	{
		const AbstrUnit* resultClasses = m_CompatibleTheme->GetPaletteAttr()->GetAbstrDomainUnit();
		if (resultClasses->GetCount() < m_EntityDomainCount)
			return resultClasses;
		else
			m_CompatibleTheme.reset();
	}
	dms_assert(m_CompatibleTheme.is_null());
	return 0;
}

//	PARAMETERIZE ON (DC, worldUnitsPerPixel), redo when worldUnitsPerPixel has changed
void PenIndexCache::UpdateForZoomLevel(Float64 nrPixelsPerWorldUnit, Float64 subPixelFactor) const
{
	dms_assert( nrPixelsPerWorldUnit > 0.0);
	dms_assert( subPixelFactor       > 0.0);

	if	(!IsDifferent(nrPixelsPerWorldUnit, subPixelFactor)) // maybe we now render for a different Device (such as Printer)
	{	
		dms_assert(m_KeyIndices.size() > 0); // PostCondition (still) remains
		return;
	}

	m_Keys.clear(); // we are gonna refill this one

	if	(	m_EntityDomainCount 
		&&	(	m_PenColorValueGetter 
			||	m_PenStyleValueGetter 
			||	m_PixelWidthValueGetter 
			||	m_WorldWidthValueGetter
			)
		)
	{
		const AbstrUnit* classIdUnit = GetCommonClassIdUnit();
		if (classIdUnit)
		{
			//	OPTIMIZATION FOR WHEN ALL THEMES USE THE SAME OR NO ENTITY CLASSIFICATION
			AddKeys(
				m_PixelWidthValueGetter ? m_PixelWidthValueGetter->CreatePaletteGetter() : 0
			,	m_WorldWidthValueGetter ? m_WorldWidthValueGetter->CreatePaletteGetter() : 0
			,	m_PenColorValueGetter   ? m_PenColorValueGetter  ->CreatePaletteGetter() : 0
			,	m_PenStyleValueGetter   ? m_PenStyleValueGetter  ->CreatePaletteGetter() : 0
			,	classIdUnit->GetCount()
			);
			AddUndefinedKey();
		}
		else
			//	THE FOLLOWING WORKS ALWAYS, ALSO FOR MULTIPLE ENTITY CLASSIFICATION, 
			AddKeys(m_PixelWidthValueGetter, m_WorldWidthValueGetter, m_PenColorValueGetter, m_PenStyleValueGetter, m_EntityDomainCount);
	}
	else
	{
		m_Keys.reserve(1);

		AddKey(m_DefaultPixelWidth, m_DefaultWorldWidth, m_DefaultPenColor, m_DefaultPenStyle);
	}
	MakeKeyIndex();
	dms_assert(m_KeyIndices.size() > 0); // PostCondition
}

void PenIndexCache::AddKeys(const AbstrThemeValueGetter* pixelWidthVG, const AbstrThemeValueGetter*  worldWidthVG, const AbstrThemeValueGetter* penColorVG, const AbstrThemeValueGetter* penStyleVG, entity_id n) const
{
	DmsColor penColor      = m_DefaultPenColor;
	PenStyle penStyle      = m_DefaultPenStyle;
	Float64  penPixelWidth = m_DefaultPixelWidth;
	Float64  penWorldWidth = m_DefaultWorldWidth;
	// make a PenKey for each EntityIndex
	m_Keys.reserve(n);

	for (entity_id i = 0; i != n; ++i)
	{
		if (pixelWidthVG)
			penPixelWidth  = pixelWidthVG->GetNumericValue(i);
		if (worldWidthVG)
			penWorldWidth  = worldWidthVG->GetNumericValue(i);
		if (penColorVG)
			penColor  = penColorVG->GetColorValue(i);
		if (penStyleVG)
			penStyle  = penStyleVG->GetOrdinalValue(i);

		AddKey(penPixelWidth, penWorldWidth, penColor, penStyle);
	}
}

void PenIndexCache::AddKey(Float64 penSize, Float64 worldSize, DmsColor penColor, PenStyle penStyle) const
{
	if	(	IsDefined(penColor) 
		&&	IsDefined(penStyle) 
		&&	IsDefined(penSize) 
		&&	IsDefined(worldSize)
		)
	{
		assert(penSize >= 0.0);
		assert(worldSize >= 0.0);
		Int32 totalSize = penSize * m_LastSubPixelFactor + worldSize * m_LastNrPixelsPerWorldUnit;
		assert(totalSize >= 0);

		m_Keys.push_back(
			PenKeyType(
				totalSize,
				penColor,
				(penStyle & PS_GEOMETRIC_STYLE_MASK)
			)
		);
	}
	else
		AddUndefinedKey();
}

void PenIndexCache::AddUndefinedKey() const
{
	m_Keys.push_back( PenKeyType(0, 0, PS_NULL) ); // Extra Pen for features with Undefined ClassId
}

void PenIndexCache::MakeKeyIndex() const
{
	std::vector<PenKeyType> orgKeys = m_Keys;

	std::sort(m_Keys.begin(), m_Keys.end());
	m_Keys.erase(
		std::unique(m_Keys.begin(), m_Keys.end()),
		m_Keys.end()
	);

	m_KeyIndices.resize(orgKeys.size());
	rlookup2index_array_unchecked(m_KeyIndices,
		orgKeys,
		m_Keys
	);
}

//----------------------------------------------------------------------
// struct  : PenArray
//----------------------------------------------------------------------

PenArray::PenArray(HDC hDC, const PenIndexCache*& indexCache, bool dontAssumeUsingOtherPens)
	:	m_hDC(hDC)
{
	assert(hDC != nullptr);
	assert(indexCache != 0);
	assert(indexCache->m_Keys.size() > 0);

	m_Collection.reserve(indexCache->m_Keys.size());

	LOGBRUSH logBrush;
	logBrush.lbColor = 0;
	logBrush.lbHatch = 0;
	logBrush.lbStyle = BS_SOLID;

	DWORD userPenStyleArray[1];
	
	for (auto i = indexCache->m_Keys.begin(), e = indexCache->m_Keys.end(); i!=e; ++i)
	{
		if (i->m_Width <= 1 && i->m_Style != PS_USERSTYLE)
			m_Collection.push_back(
				GdiHandle<HPEN>(
					CreatePen(
						i->m_Style
					,	i->m_Width
					,	i->m_Color
					)
				)
			);
		else
		{
			logBrush.lbColor = i->m_Color;
			if (i->m_Style == PS_USERSTYLE)
			{
				userPenStyleArray[0] = i->m_Width * 2;
				m_Collection.push_back(
					GdiHandle<HPEN>( 
						ExtCreatePen(
							PS_GEOMETRIC|(i->m_Style)|PS_ENDCAP_ROUND|PS_JOIN_ROUND
						,	i->m_Width
						,	&logBrush
						,	1
						,	userPenStyleArray
						)
					)
				);
			}
			else
				m_Collection.push_back(
					GdiHandle<HPEN>( 
						ExtCreatePen(
							PS_GEOMETRIC|(i->m_Style)|PS_ENDCAP_ROUND|PS_JOIN_ROUND
						,	i->m_Width
						,	&logBrush
						,	0, 0
						)
					)
				);
		}
	}
	assert(size() > 0);
	SelectPen(0);
	if (size() == 1 && dontAssumeUsingOtherPens)
		indexCache = nullptr; // don't use this penarrau in selection
}


PenArray::~PenArray()
{
	// try not to destroy a HFONT that is currently selected in the DC
	if (m_OrgHPen)
		SelectObject(m_hDC, m_OrgHPen);
	// default destructor of ResourceContainer will do the rest.
}

bool PenArray::SelectPen(UInt32 index)
{
	HPEN penHandle;
	assert(index < m_Collection.size());
	penHandle = m_Collection[index];
	if (penHandle == nullptr)
		return false;

	HGDIOBJ prevPenHandle = SelectObject(m_hDC, penHandle);
	if (m_OrgHPen == nullptr)
		m_OrgHPen = reinterpret_cast<HPEN>(prevPenHandle);
	return true;
}

