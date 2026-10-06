// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "ShvDllPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

#include "FontIndexCache.h"

#include "mci/CompositeCast.h"

#include "DataArray.h"

#include "rlookup.h"

#include "Theme.h"
#include "ThemeValueGetter.h"

//----------------------------------------------------------------------
// struct  : FontIndexCache
//----------------------------------------------------------------------

FontIndexCache::FontIndexCache(
		const Theme* fontSizeTheme
	,	const Theme* worldSizeTheme
	,	const Theme* fontNameTheme
	,	const Theme* fontAngleTheme
	,	const AbstrUnit* entityDomain
	,	const AbstrUnit* projectionBaseUnit
	,	Float64 defFontSize
	,	Float64 defWorldSize
	,	TokenID defFontNameID
	,	UInt16  defFontAngle
	)	:	ResourceIndexCache(fontSizeTheme, worldSizeTheme, defFontSize, defWorldSize, entityDomain, projectionBaseUnit)
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
	assert( nrPixelsPerWorldUnit > 0.0);
	assert( subPixelFactor       > 0.0);

	Float64 nrPointsPerPixel = /*(72.0 / 96.0) * */ subPixelFactor;
	assert(nrPointsPerPixel     > 0.0);

	if	((!IsDifferent(nrPixelsPerWorldUnit, subPixelFactor)) && m_LastNrPointsPerPixel == nrPointsPerPixel) // maybe we now render for a different Device (such as Printer)
	{	
		assert(m_KeyIndices.size() > 0 || m_Keys.size() == 1); // PostCondition (still) remains
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
				m_PixelWidthValueGetter ? m_PixelWidthValueGetter->CreatePaletteGetter() : nullptr
			,	m_WorldWidthValueGetter ? m_WorldWidthValueGetter->CreatePaletteGetter() : nullptr
			,	m_FontNameValueGetter   ? m_FontNameValueGetter  ->CreatePaletteGetter() : nullptr
			,	m_FontAngleValueGetter  ? m_FontAngleValueGetter ->CreatePaletteGetter() : nullptr
			,	classIdUnit->GetCount()
			);
			AddUndefinedKey();
		}
		else
			AddKeys(m_PixelWidthValueGetter, m_WorldWidthValueGetter, m_FontNameValueGetter, m_FontAngleValueGetter, m_EntityDomainCount);
		assert(m_Keys.size() > 0); // PostCondition
	}
	else
	{
		m_Keys.reserve(1);

		AddKey(m_DefaultPixelWidth, m_DefaultWorldWidth, m_DefaultFontNameId, m_DefaultFontAngle);
		assert(m_Keys.size() > 0); // PostCondition
	}
	MakeKeyIndex(m_KeyIndices, m_Keys);
	assert(m_KeyIndices.size() > 0 || m_Keys.size() == 1); // PostCondition
}

void FontIndexCache::AddKeys(const AbstrThemeValueGetter* sizeValueGetter, const AbstrThemeValueGetter* worldSizeValueGetter, const AbstrThemeValueGetter* nameValueGetter, const AbstrThemeValueGetter* angleValueGetter, UInt32 n) const
{
	TokenID fontNameID = m_DefaultFontNameId;
	Float64 fontSize   = m_DefaultPixelWidth, // does scale with monitor dependent scale factor
			worldSize  = m_DefaultWorldWidth;
	UInt16  angle      = m_DefaultFontAngle;

	// make a FontKey for each EntityIndex
	m_Keys.reserve(n);

	for (UInt32 i = 0; i != n; ++i)
	{
		if (sizeValueGetter)
			fontSize = sizeValueGetter->GetNumericValue(i) * m_LastSubPixelFactor;
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
		assert(fontSize >= 0.0);
		assert(worldSize >= 0.0);

		assert(m_LastNrPointsPerPixel     >= 0); // does scale with monitor dependent scale factor
		assert(m_LastNrPixelsPerWorldUnit >= 0);

		Int32 totalFontSize = Round<4>(m_LastNrPointsPerPixel * fontSize + m_LastNrPixelsPerWorldUnit * worldSize);
		if (totalFontSize == 0)  // avoid multiple versions of hidden font.
		{
			fontNameID = TokenID::GetEmptyID();
			angle = 0;
		}

		m_Keys.emplace_back(totalFontSize, fontNameID, angle);
	}
	else
		AddUndefinedKey();
}

void FontIndexCache::AddUndefinedKey() const
{
	m_Keys.emplace_back(0, TokenID::GetEmptyID(), 0); // Extra Font for features with Undefined ClassId
}

Int32 FontIndexCache::GetMaxFontSize() const
{
	dms_assert(m_Keys.size() > 0);
	return std::get<0>(m_Keys.back());
}
