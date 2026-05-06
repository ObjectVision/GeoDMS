// Copyright (C) 1998-2025 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "ShvDllPch.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

#include "GridDrawer.h"

#include "dbg/DebugCast.h"
#include "mci/ValueClass.h"
#include "utl/Swapper.h"

#include "DataArray.h"

#include "StgBase.h"

#include "DcHandle.h"
#include "DrawContext.h"
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
#ifdef _WIN32
		return reinterpret_cast<UInt32&>( Convert<RGBQUAD>( x) );
#else
		// RGBQUAD layout: Blue, Green, Red, Reserved (same as DmsColor byte order on little-endian)
		return x;
#endif
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
#ifdef _WIN32
	,	m_BitmapInfo(0)
#endif
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
		if (m_ClassIdUnit->WasFailed(FailType::Data))
			m_ClassIdUnit->ThrowFail();
		m_Count = m_ClassIdUnit->GetCount();
		if (m_Count >= MaxPaletteSize)
		{
			// Too many classes for 8-bit indirect DIB; store palette separately and use 32-bit direct rendering.
			const AbstrDataItem* paletteAttr = m_ColorTheme->GetPaletteAttr();
			DataReadLock paletteLock(paletteAttr);
			auto paletteData = const_array_checked_cast<UInt32>(paletteAttr)->GetDataRead();
			m_LargePaletteColors.resize(m_Count);
			UInt32 providedCount = Min<UInt32>(paletteData.size(), m_Count);
			DmsColor2RgbQuadFunc toRgbQuad;
			for (UInt32 i = 0; i != providedCount; ++i)
				m_LargePaletteColors[i] = toRgbQuad(paletteData[i]);
			for (UInt32 i = providedCount; i != m_Count; ++i)
				m_LargePaletteColors[i] = toRgbQuad(STG_Bmp_GetDefaultColor(i & 0xFF));
			m_LargeNodataColor = toRgbQuad(STG_Bmp_GetDefaultColor(CI_NODATA));
			bitCount = 32;
			m_Count = 0; // 32-bit direct mode; no DIB palette entries
		}
		else
		{
			MakeMin(bitCount, MaxNrIndirectPixelBits);
			if (bitCount >= 8) // undefined is used
				++m_Count;     // reserve one entry for NODATA
			dms_assert(m_Count <= (1U << bitCount));
		}
	}
	else
		m_Count = 0;

	m_BitCount = bitCount;

#ifdef _WIN32
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
#endif

	if (usePalette && !HasLargePalette())
	{
		const AbstrDataItem* paletteAttr = m_ColorTheme->GetPaletteAttr();
		dms_assert(paletteAttr);

		DataReadLock paletteLock(paletteAttr);
		dms_assert(paletteLock.IsLocked());

		auto paletteData = const_array_checked_cast<UInt32>(paletteAttr)->GetDataRead();
		dms_assert(paletteData.size() <= m_Count);
		const UInt32* palettePtr = paletteData.begin();
		const UInt32* paletteEnd = paletteData.end();

		m_PaletteColors.resize(m_Count);
		UInt32 i = 0;

#ifdef _WIN32
		RGBQUAD* colorPtr = m_BitmapInfo->bmiColors;
		for (; palettePtr != paletteEnd; ++colorPtr, ++palettePtr, ++i)
		{
			*colorPtr = Convert<RGBQUAD>( *palettePtr);
			m_PaletteColors[i] = *palettePtr;
		}

		UInt32 paletteCount = m_ClassIdUnit->GetCount();
		for (UInt32 j = paletteData.size(), e = paletteCount; j != e; ++j, ++i)
		{
			auto c = STG_Bmp_GetDefaultColor( j & 0xFF );
			*colorPtr++ = Convert<RGBQUAD>( c );
			m_PaletteColors[i] = c;
		}

		if (paletteCount != m_Count)
		{
			auto c = STG_Bmp_GetDefaultColor(CI_NODATA);
			*colorPtr++ = Convert<RGBQUAD>( c );
			m_PaletteColors[i] = c;
		}
		dms_assert(colorPtr == m_BitmapInfo->bmiColors + m_Count);
#else
		for (; palettePtr != paletteEnd; ++palettePtr, ++i)
			m_PaletteColors[i] = *palettePtr;

		UInt32 paletteCount = m_ClassIdUnit->GetCount();
		for (UInt32 j = paletteData.size(), e = paletteCount; j != e; ++j, ++i)
			m_PaletteColors[i] = STG_Bmp_GetDefaultColor( j & 0xFF );

		if (paletteCount != m_Count)
			m_PaletteColors[i] = STG_Bmp_GetDefaultColor(CI_NODATA);
#endif
	}
}


GridColorPalette::~GridColorPalette()
{
#ifdef _WIN32
	delete [] reinterpret_cast<Byte*>(m_BitmapInfo);
	m_BitmapInfo = NULL;
#endif
}

#ifdef _WIN32
BITMAPINFO* GridColorPalette::GetBitmapInfo(LONG width, LONG height) const
{
	dms_assert(m_BitmapInfo);
	m_BitmapInfo->bmiHeader.biWidth = width;
	m_BitmapInfo->bmiHeader.biHeight= height;
	return m_BitmapInfo;
}
#endif

//----------------------------------------------------------------------
// class  : GridDrawer
//----------------------------------------------------------------------

GridDrawer::GridDrawer(
	const GridCoord*        gridCoords
,	const IndexCollector*   entityIndex
,	const GridColorPalette* colorPalette
,	const SelValuesData*    selValues
,	DrawContext*            drawContext
,	const GRect&            viewExtents
,	tile_id                 t
,	const IRect&            tileRect
)	:	m_GridCoords   (gridCoords)
	,	m_EntityIndex  (entityIndex, t)
	,	m_ColorPalette (colorPalette)
	,	m_SelValues    (selValues)
	,	m_sViewRect    (viewExtents)
	,	m_DC           (drawContext)
	,	m_TileID       (t)
	,	m_TileRect     (tileRect)
#ifdef _WIN32
	,	m_pvBits       (nullptr)
#endif
{
	dms_assert(m_GridCoords);
	dms_assert(colorPalette && colorPalette->IsReady());
	dms_assert(!m_GridCoords->IsDirty());
}
void GridDrawer::Apply() const
{
	dms_assert(!m_sViewRect.empty()); // PRECONDITION, Callers responsibility
	AllocatePixelBuffer();
	dms_assert(m_ColorPalette);
	dms_assert(!m_PixelBuffer.empty());
	if (!m_PixelBuffer.empty())
		m_ColorPalette->m_ClassIdUnit->InviteUnitProcessor(*this);
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
void GridDrawer::FillLargePalette(const Unit<ClassIdType>* classIdUnit
	,	typename sequence_traits<ClassIdType>::const_pointer classIdArray, SizeT classIdArraySize
	,	Range<SizeT> themeArrayIndexRange, bool isLastRun
	) const
{
	dms_assert(m_ColorPalette && m_ColorPalette->HasLargePalette());
	const auto& paletteColors = m_ColorPalette->GetLargePaletteColors();
	const UInt32 nodataColor  = m_ColorPalette->GetLargeNodataColor();
	auto classRange           = classIdUnit->GetRange();

	GridFill<ClassIdType, DmsColor>(this
		, classIdArray, classIdArraySize
		, [&](ClassIdType val) -> DmsColor {
			SizeT idx = Range_GetIndex_checked(classRange, val);
			return (idx < paletteColors.size()) ? paletteColors[idx] : nodataColor;
		}
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
	else if (self->m_ColorPalette->HasLargePalette())
		self->FillLargePalette(classIdUnit, classIdArray, classIdArraySize, themeArrayIndexRange, isLastRun);
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
	if (self->m_ColorPalette->HasLargePalette())
		self->FillLargePalette(classIdUnit, classIdArray, classIdArraySize, themeArrayIndexRange, isLastRun);
	else
		self->FillPaletteIds(classIdUnit, classIdArray, classIdArraySize, themeArrayIndexRange, isLastRun);
}

template <typename ClassIdType>
void GridDrawer::FillClassIds(const Unit<ClassIdType>* classIdUnit) const
{
	dms_assert(m_ColorPalette->GetPaletteCount() || m_ColorPalette->HasLargePalette());

	const Theme* colorTheme = m_ColorPalette->m_ColorTheme;
	dms_assert(colorTheme);
	dms_assert(colorTheme->GetThemeAttr());
	const AbstrUnit* themeDomain = colorTheme->GetThemeAttr()->GetAbstrDomainUnit();
	if (colorTheme->GetThemeAttr()->GetAbstrDomainUnit()->IsTiled()) // thus index -> palette is tiled and cannot be cached into memory
	{
		if (m_EntityIndex)
		{
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
		WeakPtr< const ArrayValueGetter<ClassIdType> > avg = GetArrayValueGetter<ClassIdType>(colorTheme);

		Range<SizeT> themeRange;
		if (m_EntityIndex)
		{
			SizeT tileFirstIndex = themeDomain->GetTileFirstIndex(0);
			themeRange = Range<SizeT>(tileFirstIndex, tileFirstIndex+ themeDomain->GetTileCount(0));
		}

		if (m_ColorPalette->HasLargePalette())
		{
			// Class indices from avg map directly to large palette colors (0-based)
			const auto& paletteColors = m_ColorPalette->GetLargePaletteColors();
			const UInt32 nodataColor  = m_ColorPalette->GetLargeNodataColor();
			GridFill<ClassIdType, DmsColor>(this
			,	avg->GetClassIndexArray(), avg->GetCount()
			,	[&](ClassIdType classIdx) -> DmsColor {
					SizeT i = static_cast<SizeT>(classIdx);
					return (i < paletteColors.size()) ? paletteColors[i] : nodataColor;
				}
			,	themeRange
			,	true
			);
		}
		else
		{
			using PixelType = pixel_type_t<ClassIdType>;
			GridFill<ClassIdType, PixelType>(this
			,	avg->GetClassIndexArray(), avg->GetCount()
			,	ID_Func()
			,	themeRange
			,	true
			);
		}
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

void GridDrawer::AllocatePixelBuffer() const
{
	dms_assert(m_ColorPalette);
	int bitsPerPixel = m_ColorPalette->GetBitCount();
	int width = m_sViewRect.Width();
	int height = m_sViewRect.Height();

	// Calculate row stride (DWORD-aligned)
	int bytesPerRow = ((width * bitsPerPixel + 31) / 32) * 4;
	int bufferSize = bytesPerRow * height;

	m_PixelBuffer.resize(bufferSize, 0);
#ifdef _WIN32
	m_pvBits = m_PixelBuffer.data();
#endif
}

void GridDrawer::CopyToDrawContext(GPoint viewportOffset) const
{
	if (!m_DC || m_PixelBuffer.empty())
		return;

	dms_assert(m_ColorPalette);
	int bitsPerPixel = m_ColorPalette->GetBitCount();
	UInt32 paletteCount = m_ColorPalette->GetPaletteCount();
	const void* palette = (paletteCount > 0) ? m_ColorPalette->GetPaletteColors().data() : nullptr;

	GRect destRect = m_sViewRect + viewportOffset;
	m_DC->DrawImage(destRect, m_PixelBuffer.data(), m_sViewRect.Width(), m_sViewRect.Height(),
		bitsPerPixel, palette, paletteCount);
}

bool GridDrawer::empty() const { return m_sViewRect.empty(); }


