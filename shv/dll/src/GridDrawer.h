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
class  DrawContext;

#ifdef _WIN32
template <typename HandleType> struct GdiHandle;
#endif

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
	UInt32         GetBitCount()     const { return m_BitCount; }
	const std::vector<UInt32>& GetPaletteColors() const { return m_PaletteColors; }
#ifdef _WIN32
	bool           IsReady        () const { return m_BitmapInfo; }
	BITMAPINFO*    GetBitmapInfo(LONG width, LONG height) const;
#else
	bool           IsReady        () const { return m_BitCount > 0; }
#endif

private:
#ifdef _WIN32
	BITMAPINFO*             m_BitmapInfo;
#endif
	UInt32                  m_BitCount = 0;
	UInt32                  m_Count = 0;
	std::vector<UInt32>     m_PaletteColors;
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
	,	DrawContext*            drawContext
	,	const GRect&            viewExtents
	,	tile_id                 t        = no_tile
	,	const IRect&            tileRect = IRect()
	);

	void Apply() const;

	template <typename ClassIdType> void  VisitImpl(const Unit<ClassIdType>* classIdUnit) const;
	template <typename ClassIdType> void _VisitImpl(const Unit<ClassIdType>* classIdUnit) const;
	template <typename ClassIdType> void  FillPaletteIds(const Unit<ClassIdType>* classIdUnit, typename sequence_traits<ClassIdType>::const_pointer classIdArray, SizeT classIdArraySize, Range<SizeT> themeArrayIndexRange, bool isLastRun) const;
	template <typename ClassIdType> void  FillClassify  (const Unit<ClassIdType>* classIdUnit) const;
	template <typename ClassIdType> void  FillTrueColor (const Unit<ClassIdType>* classIdUnit, typename sequence_traits<ClassIdType>::const_pointer classIdArray, SizeT classIdArraySize, bool isLastRun) const;
	template <typename ClassIdType> void  FillClassIds  (const Unit<ClassIdType>* classIdUnit) const;
	void  FillDirect(const UInt32* classIdArray, SizeT classIdArraySize, bool isLastRun) const;

	#define INSTANTIATE(E) \
	void Visit(const Unit<E>* classIdUnit) const override;
	INSTANTIATE_DOMAIN_INTS
	#undef INSTANTIATE

	void AllocatePixelBuffer() const;
	void CopyToDrawContext(GPoint viewportOffset) const;

	bool empty() const;

	const GridCoord*        m_GridCoords;
	OptionalIndexCollectorAray m_EntityIndex;
	const GridColorPalette* m_ColorPalette;
	const SelValuesData*    m_SelValues;

	mutable GRect           m_sViewRect;
	DrawContext*            m_DC;

	#ifdef _WIN32
	mutable void*           m_pvBits; // DWORD Alignment per row
#endif
	mutable std::vector<Byte> m_PixelBuffer; // portable pixel storage
	mutable tile_id         m_TileID;
	mutable IRect           m_TileRect;
};

#endif // __GRIDDRAWER_H

