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

#ifndef __GRIDDRAWER_H
#define __GRIDDRAWER_H

//----------------------------------------------------------------------
// used modules and forward class references
//----------------------------------------------------------------------

#include "UnitProcessor.h"
#include "ShvUtils.h"

#include "LockedIndexCollectorPtr.h"

struct GridCoord;
struct IndexCollector;
class  Theme;

template <typename HandleType> struct GdiHandle;

//----------------------------------------------------------------------
// struct: SelValuesData
//----------------------------------------------------------------------

struct SelValuesData
{
	UInt32         m_Size;
	IRect          m_Rect;
	ClassID        m_Data[];

	bool IsHidden() const { return !IsDefined(m_Rect.first); }
	void MoveTo(IPoint newLoc);
};

//----------------------------------------------------------------------
// class  : GridPalette
//----------------------------------------------------------------------

struct GridColorPalette 
{
	GridColorPalette(const Theme* colorTheme);
	~GridColorPalette();

	const Theme*            m_ColorTheme;
	const AbstrUnit*        m_ClassIdUnit;

	UInt32         GetPaletteCount() const { return m_Count; }
	bool           IsReady        () const { return m_BitmapInfo; }
	BITMAPINFO*    GetBitmapInfo(LONG width, LONG height) const;

private:
	BITMAPINFO*             m_BitmapInfo;
	UInt32                  m_Count = 0;
};

//----------------------------------------------------------------------
// class  : GridDrawer
//----------------------------------------------------------------------

struct GridDrawer: UnitProcessor
{
	typedef UInt32 AspectType;

	GridDrawer(
		const GridCoord*        gridCoords
	,	const IndexCollector*   entityIndex
	,	const GridColorPalette* colorPalette
	,	const SelValuesData*    selValues
	,	HDC                     hDC
	,	const GRect&            viewExtents
	,	tile_id                 t        = no_tile
	,	const IRect&            tileRect = IRect()
	);

	GdiHandle<HBITMAP> Apply() const;

	template <typename ClassIdType> void  VisitImpl(const Unit<ClassIdType>* classIdUnit) const;
	template <typename ClassIdType> void _VisitImpl(const Unit<ClassIdType>* classIdUnit) const;
	template <typename ClassIdType> void  FillPaletteIds(const Unit<ClassIdType>* classIdUnit, typename sequence_traits<ClassIdType>::const_pointer classIdArray, Range<SizeT> themeArrayIndexRange, bool isLastRun) const;
	template <typename ClassIdType> void  FillClassify  (const Unit<ClassIdType>* classIdUnit) const;
	template <typename ClassIdType> void  FillTrueColor (const Unit<ClassIdType>* classIdUnit, typename sequence_traits<ClassIdType>::const_pointer classIdArray, bool isLastRun) const;
	template <typename ClassIdType> void  FillClassIds  (const Unit<ClassIdType>* classIdUnit) const;
	void  FillDirect(const UInt32* classIdArray, bool isLastRun) const;

	#define INSTANTIATE(E) \
	void Visit(const Unit<E>* classIdUnit) const override;
	INSTANTIATE_DOMAIN_INTS
	#undef INSTANTIATE

	HBITMAP CreateDIBSectionFromPalette() const;

	void CopyDIBSection(HBITMAP  hBitmap, GPoint viwePortOffset, DWORD dwRop) const;

	bool empty() const;

	const GridCoord*        m_GridCoords;
	LockedIndexCollectorPtr m_EntityIndex;
	const GridColorPalette* m_ColorPalette;
	const SelValuesData*    m_SelValues;

	mutable GRect           m_sViewRect;
	HDC                     m_hDC;

	mutable VOID*           m_pvBits; // DWORD Alignment per row
	mutable tile_id         m_TileID;
	mutable IRect           m_TileRect;
};

#endif // __GRIDDRAWER_H

