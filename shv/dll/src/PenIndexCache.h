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
#pragma once

#ifndef __SHV_PENINDEXCACHE_H
#define __SHV_PENINDEXCACHE_H

#include "ResourceIndexCache.h"

#include "geo/Color.h"

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

//template <> struct has_undefines<PenKeyType> : boost::false_type {};
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
	);

	void UpdateForZoomLevel(Float64 worldUnitsPerPixel, Float64 subPixelFactor) const;

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
	void MakeKeyIndex() const;

	DmsColor m_DefaultPenColor;
	Int16    m_DefaultPenStyle;

	WeakPtr<const AbstrThemeValueGetter> m_PenColorValueGetter;
	WeakPtr<const AbstrThemeValueGetter> m_PenStyleValueGetter;

	mutable std::vector<PenKeyType> m_Keys;
};


//----------------------------------------------------------------------
// struct  : PenArray
//----------------------------------------------------------------------

#include "DcHandle.h"

struct PenArray
{
	PenArray(HDC hDC, const PenIndexCache*& indexer);
	~PenArray();

	bool SelectPen(UInt32 index);
	SizeT size() const { return m_Collection.size(); }

private:
	typedef GdiHandle<HPEN>                         SafePenHandle; // derived from boost::noncopyable
	typedef std::vector<SafePenHandle>              PenHandleCollection;

	PenHandleCollection m_Collection;
	HDC                 m_hDC;
	HPEN                m_OrgHPen;
};


#endif // __SHV_PENINDEXCACHE_H

