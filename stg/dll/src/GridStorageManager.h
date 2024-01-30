// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

#if !defined(__TIC_STG_GRIDSTORAGEMANAGER_H)
#define __TIC_STG_GRIDSTORAGEMANAGER_H

#include "StgBase.h"
#include <optional>

#include "geo/Conversions.h"
#include "geo/Round.h"
#include "geo/undef_xx.h"
#include "mci/ValueClass.h"
#include "mem/grid.h"
#include "mem/RectCopy.h"
#include "mem/tiledata.h"
#include "TiledUnit.h"
#include "utl/mySPrintF.h"

#include "AbstrDataItem.h"
#include "AbstrDataObject.h"
#include "AbstrUnit.h"
#include "DataArray.h"
#include "ViewPortInfoEx.h"

#include "ImplMain.h"
#include "StgImpl.h"

//  ---------------------------------------------------------------------------

const UInt32 MAX_STRIP_SIZE = 1024;

//  ---------------------------------------------------------------------------

#if defined(MG_DEBUG) // DEBUG, REMOVE

#include "dbg/SeverityType.h"

template <typename Iter>
void CheckValueCount(CharPtr loc, Iter iter, SizeT sz)
{
	UInt32 count = 0, szCopy = sz;
	for (; sz--; )
	{
		if (*iter++ != std::iterator_traits<Iter>::value_type())
			count++;
	}
	reportF(SeverityTypeID::ST_MajorTrace, "CheckValueCount(%1%): %2%/%3%", loc, count, szCopy);
}
#else

template <typename Iter>
void CheckValueCount(CharPtr loc, Iter iter, SizeT sz)
{}

#endif

// *****************************************************************************
// 
// helper funcs
//
// *****************************************************************************

extern TokenID GRID_DATA_ID;
extern TokenID GRID_DOMAIN_ID;
extern TokenID PALETTE_DATA_ID;

STGDLL_CALL const AbstrUnit* GetGridDataDomainRO(const TreeItem* storageHolder);
STGDLL_CALL       AbstrUnit* GetGridDataDomainRW(TreeItem* storageHolder);

STGDLL_CALL bool IsGridDomain(const AbstrUnit* au);
STGDLL_CALL bool HasGridDomain(const AbstrDataItem* adi);
STGDLL_CALL SharedDataItem GetGridData(const TreeItem* storageHolder);
STGDLL_CALL SharedDataItem GetGridData(const TreeItem* storageHolder, bool projectionSpecsAvailable);
STGDLL_CALL SharedUnit GridDomain(const AbstrDataItem* adi);
STGDLL_CALL SharedUnit CheckedGridDomain(const AbstrDataItem* adi);

STGDLL_CALL SharedDataItem GetPaletteData(const TreeItem* storageHolder);

inline bool HasPaletteData(const TreeItem* storageHolder) { return GetPaletteData(storageHolder) != nullptr; }

//  ---------------------------------------------------------------------------

class AbstrGridStorageManager : public NonmappableStorageManager
{
	typedef AbstrStorageManager base_type;
public:
	STGDLL_CALL AbstrUnit* CreateGridDataDomain(const TreeItem* storageHolder) override;
	STGDLL_CALL ActorVisitState VisitSuppliers(SupplierVisitFlag svf, const ActorVisitor& visitor, const TreeItem* storageHolder, const TreeItem* self) const override;
	STGDLL_CALL StorageMetaInfoPtr GetMetaInfo(const TreeItem* storageHolder, TreeItem* curr, StorageAction) const override;
	STGDLL_CALL bool AllowRandomTileAccess() const override { return true;  }
	SharedPtr<AbstrUnit> m_GridDomainUnit;
};

struct GridStorageMetaInfo : GdalMetaInfo
{
	GridStorageMetaInfo(const TreeItem* storageHolder, TreeItem* curr, StorageAction);
	std::optional<ViewPortInfoProvider> m_VPIP;
	SharedStr                           m_SqlString;
	void OnPreLock() override;
};


namespace Grid {

	template <typename ColorType> ColorType Negative(ColorType x) { return ~x; }
	inline Bool Negative(Bool x) { return !x; }

	template <typename CountType> void Increment(CountType& x) { ++x; }
	inline void Increment(bit_reference<1, UInt32> x) { x = true; }

	//  --CLASSES------------------------------------------------------------------

	struct TileCount
	{
		Int32  t_min;
		UInt32 t_cnt, mod_begin, mod_end;
		STGDLL_CALL TileCount(Int32 start, UInt32 diff, UInt32 tileSize);
	};

	//  --FUNCS ------------------------------------------------------------------

	template <typename T, typename Imp>
	void ReadTiles(const Imp& imp, IPoint bufStart, UPoint bufSize,
		T defcolor, // value for reading beyond imageboundary
		typename sequence_traits<T>::pointer buf // output buffer (preallocated)
	)
	{
		auto w = imp.GetWidth();
		auto h = imp.GetHeight();

		UPoint tileSize = imp.GetTileSize(); // size of one tile or strip
		UInt32 tw_aligned = array_traits<T>::ByteAlign(tileSize.X());

		// nr of tiles needed
		TileCount
			txr(bufStart.X(), bufSize.X(), tileSize.X()),
			tyr(bufStart.Y(), bufSize.Y(), tileSize.Y());

		UInt32 tile_wh = tw_aligned*tileSize.Y();
		UInt32 scanlineSize = imp.GetTileByteWidth();

		// one strip of tiles of storage defined values
		OwningPtrSizedArray<T> strip(tile_wh, dont_initialize MG_DEBUG_ALLOCATOR_SRC("GridStoragemanager.ReadTiles: strip"));

		SizeT tileByteSizeLocal = array_traits<T>::ByteSize(tile_wh);
		SizeT tileByteSizeNative = imp.GetTileByteSize();

		void* stripBuff = array_traits<T>::DataBegin(strip.begin());
		OwningPtrSizedArray<char> rawBuffer;

		if (tileByteSizeLocal < tileByteSizeNative)
		{
			rawBuffer = OwningPtrSizedArray<char>(tileByteSizeNative, dont_initialize MG_DEBUG_ALLOCATOR_SRC("GridStoragemanager.ReadTiles: rawBuffer"));
			stripBuff = rawBuffer.begin();
		}
			

		MG_DEBUGCODE(auto bufCopy = buf); // DEBUG
		for (UInt32 ty = 0; ty<tyr.t_cnt; ++ty) // loop through all tiles/strips that intersect with [y, y+dy)
		{
			UInt32 strip_y = tyr.t_min + ty;
			UInt64 tile_y = UInt64(strip_y)*tileSize.Y();

			UInt32 bufxOffset = 0;
			UInt32 rMin = 0;             if (ty == 0)         rMin = tyr.mod_begin;
			UInt32 rSize = tileSize.Y(); if (ty + 1 == tyr.t_cnt) rSize = tyr.mod_end + 1;
			rSize -= rMin;
			// retrieve tile strip
			for (UInt32 tx = 0; tx<txr.t_cnt; tx++) // loop through all tiles/strips that intersect with [x, x+dx) 
			{
				Int32 read_result = 0; // NrOfBytes read
				if (tile_y < h)
				{
					UInt32 strip_x = txr.t_min + tx;
					UInt64 tile_x = UInt64(strip_x)*tileSize.X();
					if (tile_x < w)
					{
						// retrieve tilestrips
						read_result = imp.ReadTile(stripBuff, tile_x, tile_y, strip_y, tileByteSizeNative);

						if (read_result > 0)
						{
							imp.UnpackStrip(stripBuff, read_result, imp.GetNrBitsPerPixel());
							Imp::UnpackStrip(strip.begin(), stripBuff, imp.GetNrBitsPerPixel(), read_result, scanlineSize, tileSize.X(), tileSize.Y());

							dms_assert(UInt32(read_result) <= Max<UInt32>(tileByteSizeLocal, tileByteSizeNative));
							// defcolor for area's outside the read (w,h) rectangle as imp.ReadTile may have painted the city read.
							UInt32 nrReadableCols = w - tile_x;
							UInt32 nrReadRows = Min<UInt32>(tileSize.Y(), h - tile_y);
							if (nrReadableCols < tw_aligned)
							{
								typename sequence_traits<T>::pointer stripPtr = strip.begin() ;
								for (UInt32 row = 0; row != nrReadRows; ++row, stripPtr += tw_aligned)
									fast_fill(stripPtr + nrReadableCols, stripPtr + tw_aligned, defcolor);
							}
							assert(nrReadRows <= tileSize.Y());
							assert(tw_aligned * nrReadRows <= tile_wh);
							UInt32 rowStartIndex = tw_aligned * nrReadRows;
							assert(rowStartIndex <= tile_wh);
							fast_fill(strip.begin() + rowStartIndex, strip.begin() + tile_wh, defcolor);
							//REMOVE							CheckValueCount("After fill completion", strip.begin(), tile_wh); // DEBUG
						}
						else
							read_result = 0;
					}
				}
				UInt32 read_elems = array_traits<T>::NrElemsIn(read_result);
				if (read_elems < tile_wh)
					fast_fill(strip.begin() + read_elems, strip.begin() + tile_wh, defcolor); // dummy result for reading invalid or incomplete tile 

																							  // move pixels to ouput buffer
				UInt32 c = 0;            if (tx == 0)           c = txr.mod_begin;
				UInt32 cSize = tileSize.X(); if (tx + 1 == txr.t_cnt) cSize = txr.mod_end + 1;
				cSize -= c;
				RectCopy(
						UGrid<T>(shp2dms_order(bufSize.X(), rSize), buf)
					,	UGrid<const T>(shp2dms_order(tw_aligned, rSize + rMin), strip.begin())
					,	shp2dms_order(c - bufxOffset, rMin)
					);
				bufxOffset += cSize;
			}
			buf += SizeT(rSize) * bufSize.X();
		}
	};

	template <typename T, typename Imp>
	void ReadData(Imp& imp, const StgViewPortInfo& viewPort2Grid,T defaultColor // value for reading beyond imageboundary
	,	typename sequence_traits<T>::pointer pixels // buffer for pixel output
	,	CharPtr dataSourceName // for diagnostic purpose
	)
	{
		assert(pixels);
		//viewPort2Grid.m
		imp.UnpackCheck(nrbits_of_v<T>, imp.GetNrBitsPerPixel(), "GridData::ReadData", "to", dataSourceName);

		if (viewPort2Grid.IsNonScaling() && nrbits_of_v<T> == imp.GetNrBitsPerPixel())
		{
			IPoint offset = Round<4>(viewPort2Grid.Offset());
			if (!IsDefined(offset))
				throwErrorD("GridStorageManager", "Unknown offset of viewPort");

			if (viewPort2Grid.m_smi)
			{

				std::call_once(viewPort2Grid.m_smi->m_compare_tile_size_flag, [&imp, &viewPort2Grid, dataSourceName]()
					{
						UPoint tileSize = imp.GetTileSize();
						auto tile_size_x = tileSize.X();
						auto tile_size_y = tileSize.Y();
						if (X_GRANULARITY != tile_size_x || Y_GRANULARITY != tile_size_y)
						{
							reportF(SeverityTypeID::ST_Warning
							, "GridStorageManager: Tilesize mismatch between data source %s of item %s: %d,%d and GeoDMS default tiling: %d,%d"
							, dataSourceName
							, viewPort2Grid.m_smi->CurrRI()->GetFullName(), tile_size_x, tile_size_y
							, X_GRANULARITY, Y_GRANULARITY);
						}
					});
			}
			
			ReadTiles<T>(imp, viewPort2Grid.GetViewPortOrigin() + offset, viewPort2Grid.GetViewPortSize(), defaultColor, pixels);
			return;
		}

		IRect viewPortRect = viewPort2Grid.GetViewPortExtents();
		DRect viewRectInGrid = viewPort2Grid.Apply(Deflate(Convert<DRect>(viewPortRect), DPoint(0.5, 0.5)));
		IRect readRectInGrid = RoundDown<4>(viewRectInGrid);
		readRectInGrid.second += IPoint(1, 1);

		//	tilestrip height (tiff pixels)
		UInt32 stripHeight = Min<UInt32>(MAX_STRIP_SIZE, Height(readRectInGrid));
		UInt32 stripWidth = Width(readRectInGrid);
		UInt32 stripBufferSize = stripWidth * stripHeight;
		//	buffer for reading from m_TiffHandle
		OwningPtrSizedArray<T> stripBuffer( stripBufferSize, dont_initialize MG_DEBUG_ALLOCATOR_SRC("GridStorageManager.ReadData: stripBuffer"));
		MG_DEBUGCODE(fast_fill(stripBuffer.begin(), stripBuffer.begin() + stripBufferSize, defaultColor)); // DEBUG
		UPoint currViewPortSize = Size(viewPortRect);

		UInt32 h = imp.GetHeight();
		Double dx = viewPort2Grid.Factor().X();
		Double dy = viewPort2Grid.Factor().Y();

		Int32 currViewPortRow = dy >= 0 ? viewPortRect.first.Row() : viewPortRect.second.Row();

		OwningPtrSizedArray<UInt32> stripBuffOffsets( currViewPortSize.Col(), dont_initialize MG_DEBUG_ALLOCATOR_SRC("GridStorageManager.ReadData: stripBuffOffsets"));
		{
			OwningPtrSizedArray<UInt32>::pointer stripBufOffsetPtr = stripBuffOffsets.begin();
			Double ox = viewPort2Grid.Offset().X() + dx / 2.0;
			for (Int32 c = viewPortRect.first.Col(), ce = viewPortRect.second.Col(); c != ce; ++c)
				*stripBufOffsetPtr++ = RoundDown<4>(c * dx + ox) - readRectInGrid.first.X();
		}
		Double oy = viewPort2Grid.Offset().Y() + dy / 2.0;

		fast_fill(pixels, pixels + Cardinality(currViewPortSize), defaultColor);
		// move pixels
		while (readRectInGrid.first.Row() < readRectInGrid.second.Row())
		{
			// boring tilestrip?
			if (readRectInGrid.first.Row() >= Int32(h)) // TODO: GENERALIZE FOR VERTICAL MIRRORING
				break;

			// read tilestrip into input buffer
			Int32 stripEnd = readRectInGrid.first.Row() + stripHeight;  // TODO: GENERALIZE FOR VERTICAL MIRRORING
			ReadTiles<T>(imp, readRectInGrid.first, shp2dms_order(stripWidth, stripHeight), defaultColor, stripBuffer.begin()); // TODO: GENERALIZE FOR VERTICAL MIRRORING

			//REMOVE			CheckValueCount("stripBuffer after ReadTiles", stripBuffer.begin(), stripBufferSize); // DEBUG

			// write to output buffer
			while (currViewPortSize.Row())
			{
				if (dy < 0.0)
					currViewPortRow--;

				Float64 currYInGrid = dy * currViewPortRow + oy;
				Int32 currRowInGrid = RoundDown<4>(currYInGrid);
				if (currRowInGrid >= stripEnd)
					break;

				dms_assert(currYInGrid >= readRectInGrid.first .Row());
				dms_assert(currYInGrid <= readRectInGrid.second.Row());

				dms_assert(currViewPortSize.Row() > 0);

				dms_assert(currRowInGrid >= readRectInGrid.first.Row());
				dms_assert(currRowInGrid < readRectInGrid.second.Row());

				typename OwningPtrSizedArray<T>::pointer
					pixelPtr = pixels,
					pixelPtrEnd = pixels + currViewPortSize.X(),
					stripBufferRow = stripBuffer.begin() + (SizeT(currRowInGrid) - readRectInGrid.first.Row())*stripWidth;
				MG_CHECK(stripBufferRow + stripWidth <= stripBuffer.begin() + stripBufferSize);
				typename OwningPtr<UInt32>::pointer stripBuffOffsetPtr = stripBuffOffsets.begin();

				for (; pixelPtr != pixelPtrEnd; ++pixelPtr)
				{
					UInt32 stripBuffOffset = *stripBuffOffsetPtr++;
					dms_assert(stripBuffOffset < stripWidth);
					*pixelPtr = stripBufferRow[stripBuffOffset];
				}

				//	next row in viewPort
				currViewPortSize.Row()--;
				if (dy >= 0.0)
					currViewPortRow++;
				pixels = pixelPtrEnd;
			}

			//	next strip
			readRectInGrid.first.Row() += stripHeight; // TODO: GENERALIZE FOR VERTICAL MIRRORING
			stripHeight = Min<UInt32>(MAX_STRIP_SIZE, Height(readRectInGrid));
		}
	};


	template <typename Imp>
	void ReadGridData(Imp& imp, const StgViewPortInfo& vpi, AbstrDataObject* ado, tile_id t
		, CharPtr dataSourceName // for diagnostic purpose
	)
	{
		auto dataHandle = ado->GetDataWriteBegin(t);
		void* dataPtr = dataHandle.get_ptr();
		if (!dataPtr)
			return;
		const ValueClass* streamType = GetStreamType(ado);
		ValueClassID streamTypeID = streamType->GetValueClassID();
		switch (streamType->GetBitSize())
		{
		case 1:  ReadData<Bool  >(imp, vpi, false, mutable_array_cast<Bool>(ado)->GetDataWrite(t).begin(), dataSourceName); return;
		case 2:  ReadData<UInt2 >(imp, vpi, UInt2(0), mutable_array_cast<UInt2>(ado)->GetDataWrite(t).begin(), dataSourceName); return;
		case 4:  ReadData<UInt4 >(imp, vpi, UInt4(0), mutable_array_cast<UInt4>(ado)->GetDataWrite(t).begin(), dataSourceName); return;
		case 8:  ReadData<UInt8 >(imp, vpi, Undef08(streamTypeID), reinterpret_cast<UInt8*>(dataPtr), dataSourceName); return;
		case 16: ReadData<UInt16>(imp, vpi, Undef16(streamTypeID), reinterpret_cast<UInt16*>(dataPtr), dataSourceName); return;
		case 32: ReadData<UInt32>(imp, vpi, Undef32(streamTypeID), reinterpret_cast<UInt32*>(dataPtr), dataSourceName); return;
		case 64: ReadData<Float64>(imp, vpi, Undef64(streamTypeID), reinterpret_cast<Float64*>(dataPtr), dataSourceName); return;
		}
		auto streamTypeName = SharedStr(streamType->GetName());
		throwErrorF("GRID::ReadGridData", "Cannot read raster data as elements of type %s", streamTypeName);
	}

	template <typename CountType, typename ColorType, typename Imp>
	void CountDataImpl(const Imp& imp, const StgViewPortInfo& viewPort2tiff
		, typename sequence_traits<CountType>::pointer pixels
		, CharPtr dataSourceName // for diagnostic purpose
	)
	{
		dms_assert(pixels);

		imp.UnpackCheck(nrbits_of_v<ColorType>, imp.GetNrBitsPerPixel(), "GridData::CountData", "to", dataSourceName);

		ColorType countColor = viewPort2tiff.GetCountColor();
		dms_assert(Negative(countColor) != countColor);
		DRect viewRectInGrid = viewPort2tiff.Apply(Convert<DRect>(viewPort2tiff.GetViewPortExtents()));
		IRect readRect = Round<4>(viewRectInGrid); // & IRect(IPoint(0,0), GetSize());

		UInt32 stripHeight = Min<UInt32>(MAX_STRIP_SIZE, Height(readRect));
		UInt32 stripWidth = Width(readRect);

		//	buffer for reading from m_TiffHandle
		OwningPtrSizedArray<ColorType> stripBuffer(SizeT(stripWidth) * stripHeight, dont_initialize MG_DEBUG_ALLOCATOR_SRC("GridStoragemanager.CountDataImpl: stripBuffer"));

		IPoint viewPortOrigin = viewPort2tiff.GetViewPortOrigin();
		UPoint viewPortSize = viewPort2tiff.GetViewPortSize();

		fast_zero(pixels, pixels + viewPort2tiff.GetNrViewPortCells());// initialize buffer;
		OwningPtrSizedArray<UInt32> viewRectOffsets( stripWidth, dont_initialize MG_DEBUG_ALLOCATOR_SRC("GridStoragemanager.CountDataImpl: stripWidth"));
		{
			OwningPtrSizedArray<UInt32>::pointer viewRectOffsetPtr = viewRectOffsets.begin();
			Double finvx = 1.0 / viewPort2tiff.Factor().X(), ox = viewPort2tiff.Offset().X() - 0.5;
			for (UInt32 c = readRect.first.Col(); c != readRect.second.Col(); ++viewRectOffsetPtr, ++c)
			{
				UInt32 viewOffset = RoundDown<4>((c - ox) * finvx) - viewPortOrigin.X(); // offset in x direction in view buffer (first element = 0) 
				dms_assert(viewOffset < viewPort2tiff.GetViewPortSize().X());
				*viewRectOffsetPtr = viewOffset;
			}
		}
		Double finvy = 1.0 / viewPort2tiff.Factor().Y(), oy = viewPort2tiff.Offset().Y() - 0.5;

		while (readRect.first.Row() < readRect.second.Row())
		{
			// read tilestrip into input buffer
			ReadTiles<ColorType>(imp, readRect.first, shp2dms_order(stripWidth, stripHeight), Negative(countColor), stripBuffer.begin()); // TODO: GENERALIZE FOR VERTICAL MIRRORING

			for (UInt32 rowBase = readRect.first.Row(), stripEnd = rowBase + stripHeight; rowBase < stripEnd; ++rowBase)
			{
				dms_assert(viewPortSize.Row() > 0);

				typename OwningPtrSizedArray<CountType>::pointer
					pixelPtr = pixels + (SizeT(RoundDown<4>((rowBase - oy) * finvy)) - viewPortOrigin.Y()) * viewPortSize.X(),
					pixelPtrEnd = pixels + viewPortSize.X();
				typename OwningPtrSizedArray<ColorType>::pointer
					stripBufferRow = stripBuffer.begin() + (SizeT(rowBase) - readRect.first.Row()) * stripWidth;

				for (UInt32 c = 0; c != stripWidth; ++c)
					if (*stripBufferRow++ == countColor)
						Increment(pixelPtr[viewRectOffsets[c]]);
			}
			readRect.first.Row() += stripHeight; // TODO: GENERALIZE FOR VERTICAL MIRRORING
			stripHeight = Min<UInt32>(MAX_STRIP_SIZE, Height(readRect));
		}
	};

	template <typename CountType, typename Imp>
	void CountData(const Imp& imp, const StgViewPortInfo& viewPort2tiff, typename sequence_traits<CountType>::pointer pixels
		, CharPtr dataSourceName // for diagnostic purpose
	)
	{
		UInt32 bpp = imp.GetNrBitsPerPixel();
		switch (bpp)
		{
		case  1: CountDataImpl<CountType, Bool  >(imp, viewPort2tiff, pixels, dataSourceName); return;
			//		case  2: CountDataImpl<CountType, UInt2 >(viewPort2tiff, pixels); return;
		case  4: CountDataImpl<CountType, UInt4 >(imp, viewPort2tiff, pixels, dataSourceName); return;
		case  8: CountDataImpl<CountType, UInt8 >(imp, viewPort2tiff, pixels, dataSourceName); return;
		case 16: CountDataImpl<CountType, UInt16>(imp, viewPort2tiff, pixels, dataSourceName); return;
		case 32: CountDataImpl<CountType, UInt32>(imp, viewPort2tiff, pixels, dataSourceName); return;
		}
		throwErrorF("GRID::CountData", "reading %d bits per pixel not supported", bpp);
	}

	template <typename Imp>
	void ReadGridCounts(const Imp& imp, const StgViewPortInfo& vpi, AbstrDataObject* ado, tile_id t
		, CharPtr dataSourceName // for diagnostic purpose
	)
	{
		UInt32 bitsPerPixel = GetStreamType(ado)->GetBitSize();

		switch (bitsPerPixel) {
		case  1: CountData<Bool  >(imp, vpi, mutable_array_cast<Bool>(ado)->GetDataWrite(t).begin(), dataSourceName); return;
		case  8: CountData<UInt8 >(imp, vpi, reinterpret_cast<UInt8 *>(ado->GetDataWriteBegin(t).get_ptr()), dataSourceName); return;
		case 16: CountData<UInt16>(imp, vpi, reinterpret_cast<UInt16*>(ado->GetDataWriteBegin(t).get_ptr()), dataSourceName); return;
		case 32: CountData<UInt32>(imp, vpi, reinterpret_cast<UInt32*>(ado->GetDataWriteBegin(t).get_ptr()), dataSourceName); return;
		}
		auto streamTypeName = SharedStr(GetStreamType(ado)->GetName());
		throwErrorF("GRID::ReadGridCounts", "Cannot count raster data into elements of type %s", streamTypeName);
	}


	template <typename T, typename Imp>
	void WriteTiles(Imp& imp, const Range<IPoint>& entireRect, tile_id nrTiles, const Range<IPoint>* segmInfo
		, const TileFunctor<T>* tilesArray
		, CharPtr dataSourceName // for diagnostic purpose
	)
	{
		UInt32 w = imp.GetWidth();
		UInt32 h = imp.GetHeight();

		UPoint tileSize = imp.GetTileSize(); // size of one tile or strip
		UInt32 tw_aligned = array_traits<T>::ByteAlign(tileSize.X());

		imp.UnpackCheck(nrbits_of_v<T>, imp.GetNrBitsPerPixel(), "GridData::WriteData", "from", dataSourceName);

		// nr of tiles
		Int32
			x = entireRect.first.X(),
			y = entireRect.first.Y();
		UPoint entireSize = Size(entireRect);

		TileCount
			txr(x, entireSize.X(), tileSize.X()),
			tyr(y, entireSize.Y(), tileSize.Y());
		UInt32 tile_wh = tw_aligned*tileSize.Y();
		UInt32 scanlineSize = imp.GetTileByteWidth();

		// one strip of tiles 
		OwningPtrSizedArray<T> strip(tile_wh, dont_initialize MG_DEBUG_ALLOCATOR_SRC("GridStorageManager.WriteTiles: strip"));
		UInt32 tileByteSize = array_traits<T>::ByteSize(tile_wh);
		TileCRef dmsTileLock;

		UInt32 tile_y = tyr.t_min * tileSize.Y();
		for (UInt32 ty = 0; ty<tyr.t_cnt; ++ty, tile_y += tileSize.Y()) if (tile_y < h)
		{
			UInt32 rMin = 0;            if (ty == 0)         rMin = tyr.mod_begin;
			UInt32 rMax = tileSize.Y(); if (ty + 1 == tyr.t_cnt) rMax = tyr.mod_end + 1;
			UInt32 rSize = rMax - rMin;
			dms_assert(rSize);

			TGridBase<T, UInt32> stripGrid(UPoint(shp2dms_order(tw_aligned, tileSize.Y())), strip.begin());

			UInt32 tile_x = txr.t_min * tileSize.X();
			for (UInt32 tx = 0; tx<txr.t_cnt; ++tx, tile_x += tileSize.X()) if (tile_x < w)
			{

				UInt32 c = 0;            if (tx == 0)           c = txr.mod_begin;
				UInt32 ce = tileSize.X(); if (tx + 1 == txr.t_cnt) ce = txr.mod_end + 1;
				UInt32 cSize = ce - c;
				dms_assert(cSize);

				// zero all pixels outside entireRect for every fresh tile
				fast_zero(strip.begin(), strip.end());

				IRect tiffTileRect(shp2dms_order(tile_x, tile_y), shp2dms_order(tile_x, tile_y) + tileSize);
				for (tile_id t = 0; t != nrTiles; ++t)
					if (IsIntersecting(tiffTileRect, segmInfo[t]))
					{
						RectCopy(stripGrid, UGrid<const T>(Size(segmInfo[t]), tilesArray->GetTile(t).begin()), tiffTileRect.first - segmInfo[t].first);
					}

				// write tile
				Int32 write_result = array_traits<T>::ByteSize(tile_wh);
				imp.PackStrip(array_traits<T>::DataBegin(strip.begin()), write_result, imp.GetNrBitsPerPixel());
				dms_assert(UInt32(write_result) <= tileByteSize);
				write_result = imp.WriteTile(array_traits<T>::DataBegin(strip.begin()), tile_x, tile_y);
			}
		}
	};

	template <typename Imp>
	void WriteGridData(Imp& imp, const StgViewPortInfo& vpi, const TreeItem* storageHolder, const AbstrDataItem* adi
		, const ValueClass* streamType
		, CharPtr dataSourceName // for diagnostic purpose
	)
	{
		assert(adi);
		assert(adi->GetDataRefLockCount()); // Read lock already set

		MG_CHECK(vpi.IsNonScaling());

		UInt8 bitsPerPixel = streamType->GetBitSize();
		UInt8 samplesPerPixel = 1;
		UInt8 bitsPerSample = bitsPerPixel / samplesPerPixel;

		imp.SetDataMode(bitsPerSample, samplesPerPixel, HasPaletteData(storageHolder), SAMPLEFORMAT(adi->GetAbstrValuesUnit()->GetValueType()));

		auto trd = adi->GetCurrRefObj()->GetTiledRangeData();
		IRect entireRect = ThrowingConvert<IRect>(trd->GetRangeAsI64Rect());
		UInt32
			width = Width(entireRect),
			height = Height(entireRect);
		imp.SetTiled();
		imp.SetWidth(width);
		imp.SetHeight(height);

		IPoint displacement = Round<4>(vpi.Offset());
		entireRect += displacement; // corner zero based for TiffImp

		IRect*  segmentationInfoPtr = nullptr;
		tile_id segmentationInfoCount = trd->GetNrTiles();
		std::unique_ptr<IRect[]> segmentationInfoCopy;
		if (segmentationInfoCount >= 1)
		{
			segmentationInfoCopy.reset(new IRect[segmentationInfoCount]);
			for (tile_id t = 0; t != segmentationInfoCount; ++t)
				segmentationInfoCopy.get()[t] = ThrowingConvert<IRect>(trd->GetTileRangeAsI64Rect(t)) + displacement; // translate to TiffImp coordinates
			segmentationInfoPtr = segmentationInfoCopy.get();
		}
		
		// complete rows (don't cache)
		const AbstrDataObject* ado = adi->GetRefObj();
		switch (streamType->GetValueClassID())
		{
		case ValueClassID::VT_Bool:    WriteTiles< Bool  >(imp, entireRect, segmentationInfoCount, segmentationInfoPtr, const_array_cast< Bool  >(ado), dataSourceName); return;
		case ValueClassID::VT_UInt2:   WriteTiles< UInt2 >(imp, entireRect, segmentationInfoCount, segmentationInfoPtr, const_array_cast< UInt2 >(ado), dataSourceName); return;
		case ValueClassID::VT_UInt4:   WriteTiles< UInt4 >(imp, entireRect, segmentationInfoCount, segmentationInfoPtr, const_array_cast< UInt4 >(ado), dataSourceName); return;

		case ValueClassID::VT_UInt8:   WriteTiles< UInt8 >(imp, entireRect, segmentationInfoCount, segmentationInfoPtr, const_array_cast< UInt8 >(ado), dataSourceName); return;
		case ValueClassID::VT_Int8:    WriteTiles< UInt8 >(imp, entireRect, segmentationInfoCount, segmentationInfoPtr, reinterpret_cast<const TileFunctor<UInt8 >*>(const_array_cast<  Int8 >(ado)), dataSourceName); return;

		case ValueClassID::VT_UInt16:  WriteTiles< UInt16>(imp, entireRect, segmentationInfoCount, segmentationInfoPtr, const_array_cast< UInt16>(ado), dataSourceName); return;
		case ValueClassID::VT_Int16:   WriteTiles< UInt16>(imp, entireRect, segmentationInfoCount, segmentationInfoPtr, reinterpret_cast<const TileFunctor<UInt16>*>(const_array_cast<  Int16>(ado)), dataSourceName); return;

		case ValueClassID::VT_Float32: WriteTiles< UInt32>(imp, entireRect, segmentationInfoCount, segmentationInfoPtr, reinterpret_cast<const TileFunctor<UInt32>*>(const_array_cast<Float32>(ado)), dataSourceName); return;
		case ValueClassID::VT_UInt32:  WriteTiles< UInt32>(imp, entireRect, segmentationInfoCount, segmentationInfoPtr, const_array_cast< UInt32>(ado), dataSourceName); return;
		case ValueClassID::VT_Int32:   WriteTiles< UInt32>(imp, entireRect, segmentationInfoCount, segmentationInfoPtr, reinterpret_cast<const TileFunctor<UInt32>*>(const_array_cast<  Int32>(ado)), dataSourceName); return;
		case ValueClassID::VT_Float64: WriteTiles<Float64>(imp, entireRect, segmentationInfoCount, segmentationInfoPtr, const_array_cast<Float64>(ado), dataSourceName); return;
		case ValueClassID::VT_Int64:   WriteTiles<Float64>(imp, entireRect, segmentationInfoCount, segmentationInfoPtr, reinterpret_cast<const TileFunctor<Float64>*>(const_array_cast<Int64>(ado)), dataSourceName); return;
		case ValueClassID::VT_UInt64:  WriteTiles<Float64>(imp, entireRect, segmentationInfoCount, segmentationInfoPtr, reinterpret_cast<const TileFunctor<Float64>*>(const_array_cast<UInt64>(ado)), dataSourceName); return;
		}
		auto streamTypeName = SharedStr(streamType->GetName());
		throwErrorF("GRID::WriteGridData", "cannot store %s elements as raster data ", streamTypeName);
	}

} // namespace Grid


#endif // __TIC_STG_GRIDSTORAGEMANAGER_H
