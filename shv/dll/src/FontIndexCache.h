// Copyright (C) 2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#ifndef __SHV_FONDINDEXCACHE_H
#define __SHV_FONDINDEXCACHE_H

#include "ResourceIndexCache.h"

#include "set/Token.h"

#include <utility>
#include <boost/tuple/tuple.hpp>

//----------------------------------------------------------------------
// struct  : FontIndexCache
//----------------------------------------------------------------------

using FontKeyType = boost::tuple<Int32, TokenID, UInt16>; // first == Int32Height of Characters in Pixels; second == TokenID van FontName; third == UInt16 is angle in tenths of degrees counter-clockwise from x-basis
template <> constexpr bool has_undefines_v<FontKeyType> = false;

struct FontIndexCache : ResourceIndexCache
{
	FontIndexCache(
		const Theme* fontSizeTheme   // Character Height in #Points (1 Point = (1/72) inch = 0.3527777 mm)
	,	const Theme* worldSizeTheme  // Additional zoom-level dependent factor of Character Height in World Coord Units
	,	const Theme* fontNameTheme   // FontNames as string
	,	const Theme* fontAngleTheme
	,	const AbstrUnit* entityDomain
	,	const AbstrUnit* projectionBaseUnit
	,	Float64 defFontPixelSize
	,	Float64 defFontWorldSize
	,	TokenID defFontNameID
	,	UInt16  defFontAngle
	);

	void UpdateForZoomLevel(Float64 worldUnitsPerPixel, Float64 subPixelFactor) const;

	Int32  GetMaxFontSize() const;
	Float64 GetLastSubPixelFactor() const { return m_LastSubPixelFactor; }

#if defined(MG_DEBUG)
	UInt32 GetKeyIndex(UInt32 entityId) const
	{
		entityId = ResourceIndexCache::GetKeyIndex(entityId);
		dms_assert(entityId < m_Keys.size());
		return entityId;
	}
#endif

private: friend struct FontArray;
	const AbstrUnit* GetCommonClassIdUnit() const;
	void AddKeys(const AbstrThemeValueGetter* sizeValueGetter, const AbstrThemeValueGetter* worldSizeValueGetter, const AbstrThemeValueGetter* nameValueGetter, const AbstrThemeValueGetter* angleValueGetter, UInt32 n) const;
	void AddKey(Float64 fontSize, Float64 worldSize, TokenID fontNameID, UInt16 fontAngle) const;
	void AddUndefinedKey() const;

	TokenID m_DefaultFontNameId;
	UInt16  m_DefaultFontAngle;

	WeakPtr<const AbstrThemeValueGetter> m_FontNameValueGetter;
	WeakPtr<const AbstrThemeValueGetter> m_FontAngleValueGetter;

	std::vector<TokenID> m_FontNameTokens;

	mutable	Float64 m_LastNrPointsPerPixel;

	mutable std::vector<FontKeyType> m_Keys;
};


//----------------------------------------------------------------------
// struct  : FontArray
//----------------------------------------------------------------------

#include "DcHandle.h"

typedef GdiHandle<HFONT>                                SafeFontHandle; // derived from boost::noncopyable
typedef std::vector<SafeFontHandle>                     FontHandleCollection;

struct FontArray
{
	FontArray(const FontIndexCache* indexer, bool sizesAreCellHeights);

	bool IsSingleton() const { return size() == 1; }

	HFONT  GetFontHandle   (UInt32 index) const;
	SizeT size() const { return m_FontArray.size(); }

private:
	FontHandleCollection m_FontArray;
};

//----------------------------------------------------------------------
// struct  : SelectingFontArray
//----------------------------------------------------------------------

struct SelectingFontArray : FontArray
{
	SelectingFontArray(HDC hDC, const FontIndexCache* indexer, bool sizesAreCellHeights);
	~SelectingFontArray();

	bool SelectSingleton(); // selects the only font; use this in an if after construction to forget about indexer
	bool SelectFontHandle(UInt32 index);

private:
	HDC                    m_hDC;
	HFONT                  m_OrgHFont;
};

#endif // __SHV_FONDINDEXCACHE_H

