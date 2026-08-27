// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////
#pragma once

#ifndef __SHV_PENINDEXCACHE_H
#define __SHV_PENINDEXCACHE_H

#include "ResourceIndexCache.h"

#include "vt/color.h"

#include<utility>

typedef Int16 PenStyle;

const int DEFAULT_PEN_PIXEL_WIDTH = 1;
const int DEFAULT_PEN_WORLD_WIDTH = 1;

//----------------------------------------------------------------------
// struct  : PenKeyType
//----------------------------------------------------------------------

struct PenKeyType
{
	PenKeyType(Int32 width, DmsColor color, PenStyle penStyle)
		:	m_Width(width), m_Color(color), m_Style(penStyle)
	{}

	Int32    m_Width; // Width of Pen in Pixels; 
	DmsColor m_Color; // Pen Color
	PenStyle m_Style; // Pen Style;
	bool operator <(const PenKeyType& rhs) const 
	{
		return	
			m_Width < rhs.m_Width 
		||	(	m_Width == rhs.m_Width 
			&&	(	m_Color < rhs.m_Color
				||	(	m_Color == rhs.m_Color
					&&	m_Style < rhs.m_Style
					)
				)
			);
	}
	bool operator ==(const PenKeyType& rhs) const
	{
		return	m_Color == rhs.m_Color
			&&	m_Width == rhs.m_Width 
			&&	m_Style == rhs.m_Style;
	}
};

template <> constexpr bool has_undefines_v<PenKeyType> = false;

//----------------------------------------------------------------------
// struct  : PenIndexCache
//----------------------------------------------------------------------

struct PenIndexCache : ResourceIndexCache
{
	PenIndexCache(
		const Theme* penPixelWidthTheme
	,	const Theme* penWorldWidthTheme
	,	const Theme* penColorTheme
	,	const Theme* penStyleTheme
	,	const AbstrUnit* entityDomain
	,	DmsColor defaultPenColor
	,	const AbstrUnit* projectionBaseUnit
	);

	void UpdateForZoomLevel(Float64 worldUnitsPerPixel, Float64 subPixelFactor) const;

	const PenKeyType& GetPenKey(UInt32 keyIndex) const
	{
		dms_assert(keyIndex < m_Keys.size());
		return m_Keys[keyIndex];
	}

	bool IsPenVisible(UInt32 keyIndex) const
	{
		auto& pk = GetPenKey(keyIndex);
		return pk.m_Style != 5; // DmsPenStyle::Null
	}

#if defined(MG_DEBUG)
	UInt32 GetKeyIndex(UInt32 entityId) const
	{
		entityId = ResourceIndexCache::GetKeyIndex(entityId);
		dms_assert(entityId < m_Keys.size());
		return entityId;
	}
#endif

private: friend struct PenArray;
	const AbstrUnit* GetCommonClassIdUnit() const;
	void AddKeys(const AbstrThemeValueGetter* pixelwidth, const AbstrThemeValueGetter*  worldWidth, const AbstrThemeValueGetter* penColor, const AbstrThemeValueGetter* penStyle, entity_id n) const;
	void AddKey(Float64 penSize, Float64 worldSize, DmsColor penColor, PenStyle penStyle) const;
	void AddUndefinedKey() const;
	
	DmsColor m_DefaultPenColor;
	Int16    m_DefaultPenStyle;

	WeakPtr<const AbstrThemeValueGetter> m_PenColorValueGetter;
	WeakPtr<const AbstrThemeValueGetter> m_PenStyleValueGetter;

	mutable std::vector<PenKeyType> m_Keys;
};


//----------------------------------------------------------------------
// struct  : PenArray (Win32-only: GDI pen management)
//----------------------------------------------------------------------

#ifdef _WIN32

#include "DcHandle.h"

struct PenArray
{
	using SafePenHandle = GdiHandle<HPEN>;

	PenArray(HDC hDC, const PenIndexCache*& indexer);
	~PenArray();

	void ResetPen();
	bool SelectPen(UInt32 index);
	void SetSpecificPen(HPEN pen);

	SizeT size() const { return m_Collection.size(); }

private:
	void SetPen(HPEN pen);

	std::vector<SafePenHandle> m_Collection;
	HDC                        m_hDC;
	HPEN                       m_OrgHPen = nullptr; // zodat dat weer terug te zetten is in destructor, bewaar pas bij eerste Selectie
	bool                       m_CurrPenIsExceptional = false;

};

#endif // _WIN32


#endif // __SHV_PENINDEXCACHE_H

