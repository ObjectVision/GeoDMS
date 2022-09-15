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

#include "FontIndexCache.h"

#include "mci/CompositeCast.h"

#include "DataArray.h"

#include "rlookup.h"

#include "Theme.h"
#include "ThemeValueGetter.h"

#include <boost/tuple/tuple_comparison.hpp>

//----------------------------------------------------------------------
// struct  : FontIndexCache
//----------------------------------------------------------------------

FontIndexCache::FontIndexCache(
		const Theme* fontSizeTheme
	,	const Theme* worldSizeTheme
	,	const Theme* fontNameTheme
	,	const Theme* fontAngleTheme
	,	const AbstrUnit* entityDomain
	,	Float64 defFontSize
	,	Float64 defWorldSize
	,	TokenID defFontNameID
	,	UInt16  defFontAngle
	)	:	ResourceIndexCache(fontSizeTheme, worldSizeTheme, defFontSize, defWorldSize, entityDomain)
		,	m_DefaultFontNameId(defFontNameID)
		,	m_DefaultFontAngle (defFontAngle )
		,	m_LastNrPointsPerPixel(-1.0) 
		,	m_FontNameValueGetter (NULL)
		,	m_FontAngleValueGetter(NULL)
{
	if (fontNameTheme)
	{
		if (fontNameTheme->IsAspectParameter())
		{
			TokenID fontNameIdParam = GetTokenID_mt(fontNameTheme->GetTextAspectValue().c_str());
			if (IsDefined(fontNameIdParam))
				m_DefaultFontNameId = fontNameIdParam;
		}
		else
		{
			m_FontNameValueGetter = fontNameTheme->GetValueGetter();

			auto fontNameStrings = const_array_cast<SharedStr>(m_FontNameValueGetter->GetPaletteAttr())->GetDataRead();
			m_FontNameTokens.reserve(fontNameStrings.size());

			for(
				DataArrayBase<SharedStr>::const_iterator
					fontNameStringPtr = fontNameStrings.begin(),
					fontNameStringEnd = fontNameStrings.end();
				fontNameStringPtr != fontNameStringEnd;
				++fontNameStringPtr
			)	
				m_FontNameTokens.push_back(
					GetTokenID_mt(
						begin_ptr( *fontNameStringPtr ), 
						end_ptr  ( *fontNameStringPtr )
					)
				);	
		}
	}
	if (fontAngleTheme)
	{
		if (fontAngleTheme->IsAspectParameter())
			m_DefaultFontAngle = fontAngleTheme->GetNumericAspectValue();
		else
			m_FontAngleValueGetter = fontAngleTheme->GetValueGetter();
	}
}

//	BREAK HERE, CROSS BOUNDARY VARS: fontNameTokens, valueGetters, defaults

const AbstrUnit* FontIndexCache::GetCommonClassIdUnit() const
{
	m_CompatibleTheme = 0;
	if	(	CompareValueGetter(m_PixelWidthValueGetter)
		&&	CompareValueGetter(m_WorldWidthValueGetter)
		&&	CompareValueGetter(m_FontNameValueGetter  )
		&&	CompareValueGetter(m_FontAngleValueGetter )
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
void FontIndexCache::UpdateForZoomLevel(Float64 nrPixelsPerWorldUnit, Float64 subPixelFactor) const
{
	dms_assert( nrPixelsPerWorldUnit > 0.0);
	dms_assert( subPixelFactor       > 0.0);

	Float64 nrPointsPerPixel = (72.0 / 96.0) * subPixelFactor;
	dms_assert(nrPointsPerPixel     > 0.0);

	if	((!IsDifferent(nrPixelsPerWorldUnit, subPixelFactor)) && m_LastNrPointsPerPixel == nrPointsPerPixel) // maybe we now render for a different Device (such as Printer)
	{	
		dms_assert(m_KeyIndices.size() > 0); // PostCondition (still) remains
		return;
	}

	m_Keys.clear(); // we are gonna refill this one

	m_LastNrPointsPerPixel = nrPointsPerPixel;

	if (m_EntityDomainCount &&	(m_PixelWidthValueGetter || m_WorldWidthValueGetter || m_FontNameValueGetter || m_FontAngleValueGetter))
	{
		const AbstrUnit* classIdUnit = GetCommonClassIdUnit();
		if (classIdUnit)
		{
			//	OPTIMIZATION FOR WHEN ALL THEMES USE THE SAME OR NO ENTITY CLASSIFICATION
			AddKeys(
				m_PixelWidthValueGetter ? m_PixelWidthValueGetter->CreatePaletteGetter() : 0
			,	m_WorldWidthValueGetter ? m_WorldWidthValueGetter->CreatePaletteGetter() : 0
			,	m_FontNameValueGetter   ? m_FontNameValueGetter  ->CreatePaletteGetter() : 0
			,	m_FontAngleValueGetter  ? m_FontAngleValueGetter ->CreatePaletteGetter() : 0
			,	classIdUnit->GetCount()
			);
			AddUndefinedKey();
		}
		else
			AddKeys(m_PixelWidthValueGetter, m_WorldWidthValueGetter, m_FontNameValueGetter, m_FontAngleValueGetter, m_EntityDomainCount);
	}
	else
	{
		m_Keys.reserve(1);

		AddKey(m_DefaultPixelWidth, m_DefaultWorldWidth, m_DefaultFontNameId, m_DefaultFontAngle);
	}
	MakeKeyIndex();
	dms_assert(m_KeyIndices.size() > 0); // PostCondition
}

void FontIndexCache::AddKeys(const AbstrThemeValueGetter* sizeValueGetter, const AbstrThemeValueGetter* worldSizeValueGetter, const AbstrThemeValueGetter* nameValueGetter, const AbstrThemeValueGetter* angleValueGetter, UInt32 n) const
{
	TokenID fontNameID = m_DefaultFontNameId;
	Float64 fontSize   = m_DefaultPixelWidth, 
			worldSize  = m_DefaultWorldWidth;
	UInt16  angle      = m_DefaultFontAngle;

	// make a FontKey for each EntityIndex
	m_Keys.reserve(n);

	for (UInt32 i = 0; i != n; ++i)
	{
		if (sizeValueGetter)
			fontSize  = sizeValueGetter->GetNumericValue(i);
		if (worldSizeValueGetter)
			worldSize  = worldSizeValueGetter->GetNumericValue(i);
		if (nameValueGetter)
		{
			entity_id fontClass = nameValueGetter->GetClassIndex(i);
			fontNameID  = (fontClass < m_FontNameTokens.size())
				?	m_FontNameTokens[fontClass]
				:	TokenID::GetUndefinedID();
		}
		if (angleValueGetter)
			angle = angleValueGetter->GetNumericValue(i);

		AddKey(fontSize, worldSize, fontNameID, angle);
	}
}

void FontIndexCache::AddKey(Float64 fontSize, Float64 worldSize, TokenID fontNameID, UInt16 angle) const
{
	if (IsDefined(fontNameID) && IsDefined(fontSize) && IsDefined(worldSize) && IsDefined(angle))
	{
		dms_assert(fontSize >= 0.0);
		dms_assert(worldSize >= 0.0);

		dms_assert(m_LastNrPointsPerPixel     >= 0);
		dms_assert(m_LastNrPixelsPerWorldUnit >= 0);

		Int32 totalFontSize = Round<4>( m_LastNrPointsPerPixel * (fontSize + m_LastNrPixelsPerWorldUnit* worldSize) );
		dms_assert(totalFontSize >= 0);
		if (totalFontSize == 0)  // avoid multiple versions of hidden font.
		{
			fontNameID = TokenID::GetEmptyID();
			angle = 0;
		}

		m_Keys.push_back(
			FontKeyType(
				totalFontSize,
				fontNameID,
				angle
			)
		);
	}
	else
		AddUndefinedKey();
}

void FontIndexCache::AddUndefinedKey() const
{
	m_Keys.push_back( FontKeyType(0, TokenID::GetEmptyID(), 0) ); // Extra Font for features with Undefined ClassId
}

void FontIndexCache::MakeKeyIndex() const
{
	std::vector<FontKeyType> orgFontKeys = m_Keys;

	std::sort(m_Keys.begin(), m_Keys.end());
	m_Keys.erase(
		std::unique(m_Keys.begin(), m_Keys.end()),
		m_Keys.end()
	);

	m_KeyIndices.resize(orgFontKeys.size());
	rlookup2index_array_unchecked(m_KeyIndices,
		orgFontKeys,
		m_Keys
	);
}

Int32 FontIndexCache::GetMaxFontSize() const
{
	dms_assert(m_Keys.size() > 0);
	return m_Keys.back().get<0>();
}

//----------------------------------------------------------------------
// struct  : FontArray
//----------------------------------------------------------------------

FontArray::FontArray(const FontIndexCache* indexCache, bool sizesAreCellHeights)
{
	dms_assert(indexCache != 0);
	dms_assert(indexCache->m_Keys.size() > 0);

	m_FontArray.reserve(indexCache->m_Keys.size());

	LOGFONT fontInfo;

	// LONG values
//	fontInfo.lfHeight        = 0; -MulDiv(PointSize, GetDeviceCaps(hDC, LOGPIXELSY), 72);=
	fontInfo.lfWidth         = 0; // match against the digitization aspect ratio of the available fonts 
//   	fontInfo.lfEscapement    = 0;
//   	fontInfo.lfOrientation   = 0;
	fontInfo.lfWeight        = FW_NORMAL; // 400; FW_DONTCARE;
	// BYTE values
   	fontInfo.lfItalic        = false;
	fontInfo.lfUnderline     = false;
	fontInfo.lfStrikeOut     = false;
	fontInfo.lfCharSet       = DEFAULT_CHARSET;
	fontInfo.lfOutPrecision  = OUT_TT_PRECIS; // OUT_DEFAULT_PRECIS; 
	fontInfo.lfClipPrecision = CLIP_DEFAULT_PRECIS; 
	fontInfo.lfQuality       = PROOF_QUALITY; // DEFAULT_QUALITY; DRAFT_QUALITY; PROOF_QUALITY
	fontInfo.lfPitchAndFamily= DEFAULT_PITCH   // FIXED_PITCH or VARIABLE_PITCH; 
	                         | FF_DONTCARE; //FF_DECORATIVE, FF_MODERN, FF_ROMAN, FF_SCRIPT, FF_SWISS
//	TCHAR fontInfo.lfFaceName [LF_FACESIZE]; 

	static_assert( LF_FACESIZE == 32 );

	for (auto i = indexCache->m_Keys.begin(), e = indexCache->m_Keys.end(); i!=e; ++i)
	{
		Int32  fontSize  = i->get<0>();
		if (fontSize == 0)
			m_FontArray.push_back( GdiHandle<HFONT>() ); // add handle without resource
		else
		{
			UInt16  fontAngle = i->get<2>();
			CharPtr fontName  = GetTokenStr(i->get<1>()).c_str();
			strncpy(fontInfo.lfFaceName, fontName, LF_FACESIZE);
			fontInfo.lfFaceName[LF_FACESIZE-1] = 0;
			fontInfo.lfHeight = (sizesAreCellHeights)
				?	fontSize
				:	- fontSize;
		   	fontInfo.lfEscapement    = fontAngle;
		   	fontInfo.lfOrientation   = fontAngle;

			//	CreatePointFont(UInt32(fontSize) * 10, fontName, NULL)		
			m_FontArray.push_back( GdiHandle<HFONT>( CreateFontIndirect(&fontInfo) ) );
		}
	}
	dms_assert(size() > 0);
}

HFONT FontArray::GetFontHandle(UInt32 index) const
{
	dms_assert( index < m_FontArray.size());
	return m_FontArray[index];
}

//----------------------------------------------------------------------
// struct  : SelectingFontArray
//----------------------------------------------------------------------

SelectingFontArray::SelectingFontArray(HDC hDC, const FontIndexCache* indexCache, bool sizesAreCellHeights)
	:	FontArray(indexCache, sizesAreCellHeights)
	,	m_hDC(hDC)
	,	m_OrgHFont(NULL) // zodat dat weer terug te zetten is in destructor, bewaar pas bij eerste Selectie
{
	dms_assert(hDC);
}

SelectingFontArray::~SelectingFontArray()
{
	dms_assert( m_hDC );

	// try not to destroy a HFONT that is currently selected in the DC
	if (m_OrgHFont)
	{
		SelectObject(m_hDC, m_OrgHFont);
	}
	// default destructor of ResourceContainer will do the rest.
}

bool SelectingFontArray::SelectSingleton()
{
	dms_assert( m_hDC );

	if (! IsSingleton() )
		return false;
	SelectFontHandle(0);
	return true;
}

bool SelectingFontArray::SelectFontHandle(UInt32 index)
{
	dms_assert( m_hDC );

	HFONT fontHandle = GetFontHandle(index);

	if (fontHandle == NULL)
		return false;

	HGDIOBJ prevFontHandle = SelectObject(m_hDC, fontHandle);
	if (m_OrgHFont == NULL)
		m_OrgHFont = reinterpret_cast<HFONT>(prevFontHandle);
	return true;
}
