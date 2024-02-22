#pragma once

#include "ShvDllPch.h"

#include "GridDrawer.h"

#include "dbg/DebugCast.h"
#include "mci/ValueClass.h"
#include "utl/Swapper.h"

#include "DataArray.h"

#include "StgBase.h"

#include "DcHandle.h"
#include "IndexCollector.h"
#include "GridCoord.h"
#include "GridFill.h"
#include "Theme.h"
#include "ThemeValueGetter.h"

//----------------------------------------------------------------------
// struct: SelValuesData
//----------------------------------------------------------------------

void SelValuesData::MoveTo(IPoint newLoc)
{
	m_Rect.second += (newLoc - m_Rect.first);
	m_Rect.first   = newLoc;
}

//----------------------------------------------------------------------
// struct ID_Func
//----------------------------------------------------------------------

struct ID_Func
{
	template <typename ClassIdType>
	ClassIdType operator()(ClassIdType x) const
	{
		return x;
	}
};

//----------------------------------------------------------------------
// struct DmsColor2RgbQuadFunc
//----------------------------------------------------------------------

struct DmsColor2RgbQuadFunc
{
	UInt32 operator()(DmsColor x) const
	{
		return reinterpret_cast<UInt32&>( Convert<RGBQUAD>( x) );
	}
};

//----------------------------------------------------------------------
// struct ConvertThemeValue2ClassIndexFunc<ClassIdType>
//----------------------------------------------------------------------

template <typename ClassIdType>
struct ConvertThemeValue2ClassIndexFunc
{
	typedef typename Unit<ClassIdType>::range_t range_t;
	ConvertThemeValue2ClassIndexFunc(
		const range_t& classDataRange
	)	:	m_ClassDataRange(classDataRange)
	{}

	ClassIdType operator() (ClassIdType currGridValue) const
	{
		return Range_GetIndex_checked(m_ClassDataRange, currGridValue);
	}

	range_t m_ClassDataRange;
};

//----------------------------------------------------------------------
// class  : GridPalette
//----------------------------------------------------------------------

GridColorPalette::GridColorPalette(const Theme* colorTheme)
	:	m_ColorTheme (colorTheme)
	,	m_ClassIdUnit(colorTheme ? colorTheme->GetClassIdUnit() : nullptr)
	,	m_BitmapInfo(0)
{
	bool usePalette = colorTheme && colorTheme->GetPaletteAttr();

	if (usePalette && !m_ColorTheme->GetPaletteAttr()->PrepareData())
		return;

	UInt32 bitCount = m_ClassIdUnit ? m_ClassIdUnit->GetValueType()->GetBitSize() : 32;
	dms_assert(bitCount);
	if (bitCount > 1)
	{
		if (bitCount <= 4) bitCount = 4; else
		if (bitCount <= 8) bitCount = 8; else
		if (bitCount <=16) bitCount =16; else
		if (bitCount <=24) bitCount =24; else
		if (bitCount <=32) bitCount =32;
	}

	if (usePalette && m_ClassIdUnit)
	{
		m_ClassIdUnit->PrepareDataUsage(DrlType::Certain);
		if (m_ClassIdUnit->WasFailed(FR_Data))
			m_ClassIdUnit->ThrowFail();
		m_Count = m_ClassIdUnit->GetCount();
		if (m_Count >= MaxPaletteSize) 
			throwErrorD("GridDraw", "Palette too large to represent in an indirect bitmap");

		MakeMin(bitCount, MaxNrIndirectPixelBits);
		if (bitCount >= 8) // undefined is used
			++m_Count;     // reserve one entry for NODATA

		dms_assert(m_Count <= (1U << bitCount));
	}
	else
		m_Count = 0;

	m_BitmapInfo = reinterpret_cast<BITMAPINFO*>(new Byte[sizeof(BITMAPINFOHEADER) + (m_Count * sizeof(RGBQUAD))] );

	BITMAPINFOHEADER& bmiHeader = m_BitmapInfo->bmiHeader;
	bmiHeader.biSize          = sizeof(BITMAPINFOHEADER); dms_assert(CharPtr(&bmiHeader) + bmiHeader.biSize == CharPtr(m_BitmapInfo->bmiColors) );
	bmiHeader.biWidth         = 0; // will be set in CreateDIBSectionFromPalette
	bmiHeader.biHeight        = 0; // will be set in CreateDIBSectionFromPalette
	bmiHeader.biPlanes        = 1;
	bmiHeader.biBitCount      = bitCount;
	bmiHeader.biCompression   = BI_RGB;
	bmiHeader.biSizeImage     = 0;
	bmiHeader.biXPelsPerMeter = 0;
	bmiHeader.biYPelsPerMeter = 0;
	bmiHeader.biClrUsed       = m_Count;
	bmiHeader.biClrImportant  = m_Count;

	if (usePalette)
	{
		const AbstrDataItem* paletteAttr = m_ColorTheme->GetPaletteAttr();
		dms_assert(paletteAttr);

		DataReadLock paletteLock(paletteAttr);
		dms_assert(paletteLock.IsLocked());

		auto paletteData = const_array_checkedcast<UInt32>(paletteAttr)->GetDataRead();
		dms_assert(paletteData.size() <= m_Count);
		const UInt32* palettePtr = paletteData.begin();
		const UInt32* paletteEnd = paletteData.end();

		RGBQUAD* colorPtr = m_BitmapInfo->bmiColors;

		for (; palettePtr != paletteEnd; ++colorPtr, ++palettePtr)
			*colorPtr = Convert<RGBQUAD>( *palettePtr);

		UInt32 paletteCount = m_ClassIdUnit->GetCount();

		// NYI: use default colors if PaletteAttr is not or partially provided
		for (UInt32 i = paletteData.size(), e = paletteCount; i != e; ++i)
			*colorPtr++ = Convert<RGBQUAD>( STG_Bmp_GetDefaultColor( i & 0xFF ));

		// provide color for NODATA
		if (paletteCount != m_Count)
			*colorPtr++ = Convert<RGBQUAD>( STG_Bmp_GetDefaultColor(CI_NODATA) );
		dms_assert(colorPtr == m_BitmapInfo->bmiColors + m_Count);
	}
}


GridColorPalette::~GridColorPalette()
{
	delete [] reinterpret_cast<Byte*>(m_BitmapInfo);
	m_BitmapInfo = NULL;
}

BITMAPINFO* GridColorPalette::GetBitmapInfo(LONG width, LONG height) const
{
	dms_assert(m_BitmapInfo);
	m_BitmapInfo->bmiHeader.biWidth = width;
	m_BitmapInfo->bmiHeader.biHeight= height;
	return m_BitmapInfo;
}

//----------------------------------------------------------------------
// class  : GridDrawer
//----------------------------------------------------------------------

GridDrawer::GridDrawer(
	const GridCoord*        gridCoords
,	const IndexCollector*   entityIndex
,	const GridColorPalette* colorPalette
,	const SelValuesData*    selValues
,	HDC                     hDC
,	const GRect&            viewExtents
,	tile_id                 t
,	const IRect&            tileRect
)	:	m_GridCoords   (gridCoords)
	,	m_EntityIndex  (entityIndex, t)
	,	m_ColorPalette (colorPalette)
	,	m_SelValues    (selValues)
	,	m_sViewRect    (viewExtents)
	,	m_hDC          (hDC)
	,	m_TileID       (t)
	,	m_TileRect     (tileRect)
	,	m_pvBits       (nullptr)
{
	dms_assert(m_GridCoords);
	dms_assert(colorPalette && colorPalette->IsReady());
	dms_assert(!m_GridCoords->IsDirty());
}

GdiHandle<HBITMAP> GridDrawer::Apply() const
{
	dms_assert(!m_sViewRect.empty()); // PRECONDITION, Callers responsibility
	GdiHandle<HBITMAP> hBitmap( CreateDIBSectionFromPalette() );
	dms_assert(m_ColorPalette);
	dms_assert(m_pvBits != nullptr);
	if ((m_pvBits != nullptr))
		m_ColorPalette->m_ClassIdUnit->InviteUnitProcessor(*this);

	return hBitmap;
}

template <typename ClassIdType>
void GridDrawer::FillPaletteIds(const Unit<ClassIdType>* classIdUnit
	,	typename sequence_traits<ClassIdType>::const_pointer classIdArray, SizeT classIdArraySize
	,	Range<SizeT> themeArrayIndexRange,	bool isLastRun
	) const
{
	using PixelType = pixel_type_t<ClassIdType>;
	dms_assert(m_ColorPalette);
	dms_assert(m_ColorPalette->GetPaletteCount());

	GridFill<ClassIdType, PixelType>(this
		, classIdArray, classIdArraySize
		, ConvertThemeValue2ClassIndexFunc<ClassIdType>(classIdUnit->GetRange())
		, themeArrayIndexRange, isLastRun
	);
}

template <typename ClassIdType>
void GridDrawer::FillTrueColor(const Unit<ClassIdType>* classIdUnit
	, typename sequence_traits<ClassIdType>::const_pointer classIdArray, SizeT classIdArraySize
	, bool isLastRun) const
{
	dms_assert(m_ColorPalette && !m_ColorPalette->GetPaletteCount());

	GridFill<ClassIdType, ClassIdType>(this
		, classIdArray, classIdArraySize
		, DmsColor2RgbQuadFunc()
		, Range<SizeT>(), isLastRun
	);
}

void GridDrawer::FillDirect(const UInt32* classIdArray, SizeT classIdArraySize, bool isLastRun) const
{
	dms_assert(m_ColorPalette && !m_ColorPalette->GetPaletteCount());

	GridFill<UInt32, UInt32>(
		this, classIdArray, classIdArraySize
		, [](UInt32 c) { return c; }
		, Range<SizeT>(), isLastRun
		);
}

template <typename ClassIdType> typename boost::enable_if_c<sizeof(ClassIdType) == 4>::type
GridDrawer_VisitImplDirect(
		const GridDrawer* self
	,	const Unit<ClassIdType>* classIdUnit
	,	typename sequence_traits<ClassIdType>::const_pointer classIdArray, SizeT classIdArraySize
	,	Range<SizeT> themeArrayIndexRange,	bool isLastRun
	)
{
	if (self->m_ColorPalette->GetPaletteCount())
		self->FillPaletteIds(classIdUnit, classIdArray, classIdArraySize, themeArrayIndexRange, isLastRun);
	else
		self->FillTrueColor(classIdUnit, classIdArray, classIdArraySize, isLastRun);
}

template <typename ClassIdType> typename boost::enable_if_c<sizeof(ClassIdType) != 4>::type
GridDrawer_VisitImplDirect(
		const GridDrawer* self
	,	const Unit<ClassIdType>* classIdUnit
	,	typename sequence_traits<ClassIdType>::const_pointer classIdArray, SizeT classIdArraySize
	,	Range<SizeT> themeArrayIndexRange, bool isLastRun
	)
{
	self->FillPaletteIds(classIdUnit, classIdArray, classIdArraySize, themeArrayIndexRange, isLastRun);
}

template <typename ClassIdType>
void GridDrawer::FillClassIds(const Unit<ClassIdType>* classIdUnit) const
{
	dms_assert(m_ColorPalette->GetPaletteCount());

	const Theme* colorTheme = m_ColorPalette->m_ColorTheme;
	dms_assert(colorTheme);
	dms_assert(colorTheme->GetThemeAttr());
	const AbstrUnit* themeDomain = colorTheme->GetThemeAttr()->GetAbstrDomainUnit();
	if (colorTheme->GetThemeAttr()->GetAbstrDomainUnit()->IsTiled()) // thus index -> palette is tiled and cannot be cached into memory
	{
		if (m_EntityIndex)
		{
			dms_assert(m_EntityIndex->HasExtKey() || m_EntityIndex->HasGeoRel());
			for (tile_id t=0, tn=themeDomain->GetNrTiles(); t!=tn; ++t)
			{
				SizeT tileFirstIndex= themeDomain->GetTileFirstIndex(t);
				GetValueGetter(colorTheme)->GridFillDispatch(this, t, Range<SizeT>(tileFirstIndex, tileFirstIndex+ themeDomain->GetTileCount(t)), t+1==tn);
			}
		}
		else
			GetValueGetter(colorTheme)->GridFillDispatch(this, m_TileID, Range<SizeT>(), true);
	}
	else
	{
		using PixelType = pixel_type_t<ClassIdType>;
		WeakPtr< const ArrayValueGetter<ClassIdType> > avg = GetArrayValueGetter<ClassIdType>(colorTheme);

		Range<SizeT> themeRange;
		if (m_EntityIndex)
		{
			SizeT tileFirstIndex = themeDomain->GetTileFirstIndex(no_tile);
			themeRange = Range<SizeT>(tileFirstIndex, tileFirstIndex+ themeDomain->GetTileCount(no_tile));
		}
		GridFill<ClassIdType, PixelType>(this
		,	avg->GetClassIndexArray(), avg->GetCount()
		,	ID_Func()
		,	themeRange
		,	true
		);

	}
}

template <typename ClassIdType>
void GridDrawer::_VisitImpl(const Unit<ClassIdType>* classIdUnit) const
{
	const Theme* colorTheme = m_ColorPalette->m_ColorTheme;
	dms_assert(colorTheme);
	if (colorTheme->GetClassification())
		FillClassIds(classIdUnit);
	else
	{
		dms_assert(colorTheme->GetThemeAttr());

		auto colorThemeTileFunctor = const_array_cast<ClassIdType>(colorTheme->GetThemeAttr());
		if (m_EntityIndex)
		{
			dms_assert(m_EntityIndex->HasExtKey() || m_EntityIndex->HasGeoRel());
			auto tileRanges = colorThemeTileFunctor->GetTiledRangeData();
			for (tile_id t=0, tn= tileRanges->GetNrTiles(); t!=tn; ++t)
			{
				SizeT tileFirstIndex = tileRanges->GetFirstRowIndex(t);
				auto colorThemeTile = colorThemeTileFunctor->GetTile(t);
				GridDrawer_VisitImplDirect(this, classIdUnit
				,	colorThemeTile.begin(), colorThemeTile.size()
				,	Range<SizeT>(tileFirstIndex, tileFirstIndex+tileRanges->GetTileSize(t)), t+1==tn
				);
			}
		}
		else
		{
			auto colorThemeTile = colorThemeTileFunctor->GetTile(m_TileID);
			GridDrawer_VisitImplDirect(this, classIdUnit
				, colorThemeTile.begin(), colorThemeTile.size()
				, Range<SizeT>(), true
			);
		}
	}
}

template <typename ClassIdType>
void GridDrawer::VisitImpl(const Unit<ClassIdType>* classIdUnit) const
{
	_VisitImpl(classIdUnit);
}

template <>
void GridDrawer::VisitImpl(const Unit<ClassID>* classIdUnit) const // override for ClassID that is the only type that can be used to indicate selections
{
	if (m_SelValues)
	{	
		GridFill<ClassID, ClassID>(this, m_SelValues->m_Data, m_SelValues->m_Size, ID_Func(), Range<SizeT>(), true);
	}
	else 
		_VisitImpl(classIdUnit);
}


#define INSTANTIATE(E) \
void GridDrawer::Visit(const Unit<E>* classIdUnit) const { VisitImpl<E>(classIdUnit); }
INSTANTIATE_DOMAIN_INTS
#undef INSTANTIATE

HBITMAP GridDrawer::CreateDIBSectionFromPalette() const
{
	dms_assert(m_ColorPalette);
	BITMAPINFO* bitmapInfo = m_ColorPalette->GetBitmapInfo(m_sViewRect.Width(), m_sViewRect.Height());

	dms_assert(m_pvBits == nullptr);
	HBITMAP bitmap =
		CreateDIBSection(
			m_hDC,
			bitmapInfo,
			DIB_RGB_COLORS,
			&m_pvBits,
			NULL, 0
		);
	dms_assert(m_pvBits != nullptr);
	dms_assert(bitmap != 0);
	return bitmap;
}

void GridDrawer::CopyDIBSection(HBITMAP hBitmap, GPoint viewportOffset, DWORD dwRop) const
{
	CompatibleDcHandle memDC(NULL, 0);
	GdiObjectSelector<HBITMAP> selectBitmap(memDC, hBitmap);

	GRect bitmapRect = m_sViewRect + viewportOffset;
	BitBlt(m_hDC, 
		bitmapRect.left, 
		bitmapRect.top , 
		bitmapRect.Width(), 
		bitmapRect.Height(), 
		memDC, 0, 0, dwRop
	);
}

bool GridDrawer::empty() const { return m_sViewRect.empty(); }


