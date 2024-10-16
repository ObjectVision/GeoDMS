// Copyright (C) 1998-2024 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

#if !defined(__SHV_DRAWPOLYGONS_H)
#define __SHV_DRAWPOLYGONS_H

#include "geo/CalcWidth.h"
#include "geo/PointIndexBuffer.h"

#include "BoundingBoxCache.h"
#include "CounterStacks.h"
#include "FeatureLayer.h"
#include "GeoTypes.h"
#include "GraphVisitor.h"
#include "IndexCollector.h"
#include "Theme.h"
#include "ThemeValueGetter.h"
#include "RemoveAdjacentsAndSpikes.h"

SHV_CALL Float64 s_DrawingSizeTresholdInPixels = 0.0;

using pointBuffer_t = std::vector<GPoint>;

template <typename PI>
void fillPointBuffer(pointBuffer_t& buf, PI ii, PI ie, CrdTransformation transformer)
{
	UInt32 nrPoints = ie - ii;

	buf.resize(nrPoints);

	pointBuffer_t::iterator 
		bi = buf.begin();
	for(;ii!=ie; ++ii, ++bi)
		*bi = DPoint2GPoint(*ii, transformer);
}

inline void CorrectHatchStyle(Int32& hatchStyle)
{
	if (hatchStyle < -1 || hatchStyle > 5) 
		hatchStyle = -1;
}

template <typename ScalarType>
struct polygon_traits
{
	typedef Point<ScalarType>                                    PointType;
	typedef Range<PointType>                                     RangeType;
	typedef typename sequence_traits<PointType>::container_type  PolygonType;
	typedef DataArray<PolygonType>                               DataArrayType;
	using RectArrayType = typename SequenceBoundingBoxCache<ScalarType>::RectArrayType;
	typedef typename sequence_traits<PolygonType>::cseq_t        CPolySeqType;
	typedef typename DataArrayType::const_iterator               CPolyIterType;
	typedef typename DataArrayType::locked_cseq_t                LockedSeqType;
};

template <typename ScalarType>
bool DrawPolygonInterior(
		COLORREF defBrushColor
	,	const Theme* brushColorTheme
	,	const Theme* hatchStyleTheme
	,	const GraphDrawer& d
	,	const SequenceBoundingBoxCache<ScalarType>* boundingBoxCache
	,	const AbstrTileRangeData* trd, tile_id t
	,	typename polygon_traits<ScalarType>::CPolySeqType featureData
	,	const FeatureDrawer& fd
	,	pointBuffer_t& pointBuffer
	,	bool selectedOnly, SelectionIdCPtr selectionsArray
	,	SizeT fe
)
{
	typedef polygon_traits<ScalarType> p_traits;

	const typename p_traits::RectArrayType& rectArray  = boundingBoxCache->GetBoundsArray(t);
	const typename p_traits::RectArrayType& blockArray = boundingBoxCache->GetBlockBoundArray(t);

	WeakPtr<const AbstrThemeValueGetter> brushColorGetter;
	typename p_traits::RangeType clipRect = Convert<typename p_traits::RangeType>( d.GetWorldClipRect() ); // no need to clip on possible label extents

	if (brushColorTheme)
	{
		if (brushColorTheme->IsAspectParameter())
			defBrushColor = brushColorTheme->GetColorAspectValue();
		else
			brushColorGetter = brushColorTheme->GetValueGetter();
	}

	Int32 defHatchStyle = -1; // solid
	WeakPtr<const AbstrThemeValueGetter> hatchStyleGetter;
	if (hatchStyleTheme)
	{
		if (hatchStyleTheme->IsAspectParameter())
		{
			defHatchStyle = hatchStyleTheme->GetOrdinalAspectValue();
			CorrectHatchStyle(defHatchStyle);
		}
		else
			hatchStyleGetter = hatchStyleTheme->GetValueGetter();
	}

	CrdType zoomLevel = Abs(d.GetTransformation().ZoomLevel());

	dms_assert(zoomLevel > 1.0e-30); // we assume that nothing remains visible on such a small scale to avoid numerical overflow in the following inversion

	ScalarType minWorldWidth  = s_DrawingSizeTresholdInPixels / zoomLevel;
	ScalarType minWorldHeight = minWorldWidth;

	GdiHandle<HPEN>         invisiblePen(CreatePen(PS_NULL, 0, RGB(0, 0, 0)));
	GdiObjectSelector<HPEN> penSelector(d.GetDC(), invisiblePen);

	lfs_assert(rectArray.size() == featureData.size());

	ResumableCounter itemCounter(d.GetCounterStacks(), true);

	for (auto b = featureData.begin(), e = featureData.end(), i= b+itemCounter; i != e; ++i)
	{
		if ((i-b) % AbstrBoundingBoxCache::c_BlockSize == 0)
			while (!IsIntersecting(clipRect, blockArray[(i-b) / AbstrBoundingBoxCache::c_BlockSize]))
			{
				i  += AbstrBoundingBoxCache::c_BlockSize;
				if (!(i<e)) 
					goto exitFill;
				itemCounter += AbstrBoundingBoxCache::c_BlockSize;
				if (itemCounter.MustBreakOrSuspend()) 
					return true;
			}
		auto ri = rectArray.begin() + itemCounter;
		if	(i->size() >= 3 && IsIntersecting(clipRect, *ri ) && Width (*ri) >= minWorldWidth && Height(*ri) >= minWorldHeight)
		{
			COLORREF brushColor = defBrushColor;
			Int32    hatchStyle = -1;

			entity_id entityIndex = trd->GetRowIndex(t, i - b);
			entityIndex = fd.m_IndexCollector.GetEntityIndex(entityIndex);
			if (!IsDefined(entityIndex))
				goto nextFill;

			bool isSelected = selectionsArray && SelectionID( selectionsArray[entityIndex] );
			if (selectedOnly)
			{
				if (!isSelected) goto nextFill;
				isSelected = false;
			}

			if (entityIndex == fe)
				brushColor = ::GetSysColor(COLOR_HIGHLIGHT);
			else if (isSelected)
				brushColor = GetSelectedClr(selectionsArray[entityIndex]);
			else
			{
				hatchStyle = defHatchStyle;
				if (brushColorGetter)
					brushColor = brushColorGetter->GetColorValue(entityIndex);
				if (hatchStyleGetter)
				{
					hatchStyle = hatchStyleGetter->GetOrdinalValue(entityIndex);
					CorrectHatchStyle(hatchStyle);
				}
			}

			if (brushColor != TRANSPARENT_COLORREF)
			{
				CheckColor(brushColor);
				GdiHandle<HBRUSH> brush(
					(hatchStyle == -1)
					?	CreateSolidBrush(brushColor)
					:	CreateHatchBrush(hatchStyle, brushColor)
				);
				GdiObjectSelector<HBRUSH> brushSelector(d.GetDC(), brush);

				typename sequence_traits<typename p_traits::PointType>::const_pointer
					pointArrayBegin = i->begin(),
					pointArrayEnd   = i->end();

				fillPointBuffer(pointBuffer, pointArrayBegin, pointArrayEnd, d.GetTransformation());

				remove_adjacents_and_spikes(pointBuffer);
				if (pointBuffer.size() >= 3)
					CheckedGdiCall(
						Polygon(
							d.GetDC(),
							&*pointBuffer.begin(),
							pointBuffer.size()
						)
					,	"DrawPolygon"
					);

/*	BEGIN NEW
				fillPointIndexBuffer(pointIndexBuffer, pointArrayBegin, pointArrayEnd);

				pointBuffer_t::const_iterator 
					bi = pointBuffer.begin();
				pointIndexBuffer_t::const_iterator 
					ii = pointIndexBuffer.begin(),
					ie = pointIndexBuffer.end();


				while(ii != ie)
				{
					UInt32 bufferOffset = ii->first;

					reportF(ST_MinorTrace, "bufferOffset %d", bufferOffset);

					dms_assert(Area<Float64>(i->begin()+ii->first, i->begin()+ ii->second) >= 0);
					++ii;
					backStack.clear();
					while (ii != ie && Area<Float64>(pointArrayBegin + ii->first, pointArrayBegin + ii->second) <= 0)
					{
						backStack.push_back(pointBuffer[ii->second-1]);
						++ii;
					}

					UInt32
						bufferOffsetEnd    = ii[-1].second,
						bufferOffsetEndEnd = bufferOffsetEnd + backStack.size();


					dms_assert(bufferOffsetEndEnd <= pointBuffer.size());
					swap_range(pointBuffer.begin() + bufferOffsetEnd, pointBuffer.begin() + bufferOffsetEndEnd, backStack.rend());

					if (IsIntersecting(clipRect, RangeType(&*(pointArrayBegin + bufferOffset), &*(pointArrayBegin + bufferOffsetEnd), false)))
						CheckedGdiCall(
							Polygon(
								d.GetDC(), 
								&*(bi + bufferOffset), 
								bufferOffsetEnd - bufferOffset
							)
						,	"DrawPolygon"
						);
					swap_range(pointBuffer.begin() + bufferOffsetEnd, pointBuffer.begin() + bufferOffsetEndEnd, backStack.rend());
				}
// END NEW */
			}
		}
	nextFill:
		++itemCounter; if (itemCounter.MustBreakOrSuspend100()) return true;
	}
exitFill:
	itemCounter.Close();
	return false;
}

#include "FeatureLayer.h"

template <typename ScalarType>
bool DrawPolygons(const GraphicPolygonLayer* layer, const FeatureDrawer& fd, const AbstrDataItem* featureItem, const PenIndexCache* penIndices)
{
	typedef polygon_traits<ScalarType> p_traits;
	const GraphDrawer& d = fd.m_Drawer;

	const AbstrDataObject* featureData = featureItem->GetRefObj();
	auto da = const_array_cast<typename p_traits::PolygonType>(featureData);
	auto trd = da->GetTiledRangeData();
	tile_id tn = trd->GetNrTiles();

	auto boundingBoxCache =  GetSequenceBoundingBoxCache<ScalarType>(layer);

	pointBuffer_t pointBuffer;

	CrdType zoomLevel = Abs(d.GetTransformation().ZoomLevel());
	assert(zoomLevel > 1.0e-30); // we assume that nothing remains visible on such a small scale to avoid numerical overflow in the following inversion

	ScalarType minWorldWidth  = s_DrawingSizeTresholdInPixels / zoomLevel;
	ScalarType minWorldHeight = minWorldWidth;

	typename p_traits::RangeType clipRect = Convert<typename p_traits::RangeType>( layer->GetWorldClipRect(d) );

	SelectionIdCPtr selectionsArray; assert(!selectionsArray);
	if (fd.m_SelValues)
	{
		selectionsArray = fd.m_SelValues.value().begin();
		assert(selectionsArray);
	}

	// draw Interiors
	ResumableCounter mainCount(d.GetCounterStacks(), false);

	SizeT fe = UNDEFINED_VALUE(SizeT);
	if (layer->IsActive())
		fe = layer->GetFocusElemIndex();

	bool selectedOnly = layer->ShowSelectedOnly();

	if (mainCount == 0)
	{
		bool transparentBrush = layer->IsDisabledAspectGroup(AG_Brush);

		if (selectionsArray || !transparentBrush || IsDefined(fe) ) // no BrushColorTheme means transparent polygons
		{
			ResumableCounter tileCounter(d.GetCounterStacks(), true);
			for(tile_id t = tileCounter.Value(); t<tn; ++t)
			{
				auto data = da->GetTile(t);

				if (DrawPolygonInterior(
					(transparentBrush)
					? TRANSPARENT_COLORREF
					: DmsColor2COLORREF(layer->GetDefaultOrThemeColor(AN_BrushColor)) // green as default color for polygons isn't really used since we have a theme
					, layer->GetEnabledTheme(AN_BrushColor).get()
					, layer->GetEnabledTheme(AN_HatchStyle).get()
					, d, boundingBoxCache.get()
					, trd, t
					, data
					,	fd
					,	pointBuffer
					,	selectedOnly, selectionsArray
					,	fe
					)
				)
					return true;
				++tileCounter; if (tileCounter.MustBreakOrSuspend()) return true;
			}
			tileCounter.Close();
		}
		++mainCount; if (mainCount.MustBreakOrSuspend()) return true;
	}

	// draw Boundaries (always)
	if (mainCount == 1)
	{
		pointIndexBuffer_t pointIndexBuffer;
		if (penIndices && !layer->IsDisabledAspectGroup(AG_Pen))
		{
			PenArray pa(d.GetDC(), penIndices);

			ResumableCounter tileCounter(d.GetCounterStacks(), true);
			for(tile_id t = tileCounter.Value(); t<tn; ++t)
			{
				auto data = da->GetTile(t);
				auto ts = data.size();
				auto b = data.begin();

				const auto& rectArray  = boundingBoxCache->GetBoundsArray(t);
				const auto& blockArray = boundingBoxCache->GetBlockBoundArray(t);

				ResumableCounter itemCounter(d.GetCounterStacks(), true);

				while (itemCounter != ts)
				{
					if (itemCounter % AbstrBoundingBoxCache::c_BlockSize == 0)
						while (!IsIntersecting(clipRect, blockArray[itemCounter / AbstrBoundingBoxCache::c_BlockSize]))
						{
							itemCounter += AbstrBoundingBoxCache::c_BlockSize;
							if (itemCounter >= ts) 
								goto exitDrawBorders;
							if (itemCounter.MustBreakOrSuspend()) 
								return true;
						}
					auto featurePtr = b + itemCounter;
					auto ri = rectArray.begin() + itemCounter;
					if (featurePtr->size() >= 3 && IsIntersecting(clipRect, *ri) && Width(*ri) >= minWorldWidth && Height(*ri) >= minWorldHeight)
					{
						if (penIndices || selectedOnly)
						{
							SizeT entityIndex = trd->GetRowIndex(t, itemCounter);
							entityIndex = fd.m_IndexCollector.GetEntityIndex(entityIndex);
							if (!IsDefined(entityIndex))
								goto nextBorder;
							if (penIndices && ! pa.SelectPen(penIndices->GetKeyIndex(entityIndex) ) )
								goto nextBorder;
							if (selectedOnly && !(selectionsArray && SelectionID( selectionsArray[entityIndex])))
								goto nextBorder;
						}

						fillPointBuffer     (pointBuffer, featurePtr->begin(), featurePtr->end(), d.GetTransformation());
						fillPointIndexBuffer(pointIndexBuffer, featurePtr->begin(), featurePtr->end());

						auto bi = pointBuffer.begin();

						// draw Polyline for each island and lake; identified by repetition of start-point
						auto ii = pointIndexBuffer.begin(), ie = pointIndexBuffer.end();

						lfs_assert(featurePtr->size());
						auto iBegin = begin_ptr(*featurePtr);
						for (; ii != ie; ++ii)
						{
							assert(ii->first <= ii->second);
							assert(ii->second <= pointBuffer.size());

							auto bufferOffset    = bi + ii->first;
							auto bufferOffsetEnd = bi + ii->second;

							bufferOffsetEnd = std::unique(bufferOffset, bufferOffsetEnd);
							UInt32 lineSize =  bufferOffsetEnd - bufferOffset;
							if	(lineSize >= 2)
								CheckedGdiCall(
									Polyline(
										d.GetDC(), 
										&*bufferOffset, 
										lineSize
									)
								,	"DrawPolyline"
								);
						}
					}
				nextBorder:
					++itemCounter; if (itemCounter.MustBreakOrSuspend100()) return true;
				}
			exitDrawBorders:
				itemCounter.Close();
				++tileCounter; if (tileCounter.MustBreakOrSuspend()) return true;
			}
			tileCounter.Close();
		}
		++mainCount; if (mainCount.MustBreakOrSuspend()) return true;
	}

	// draw labels
	if (mainCount == 2 && fd.HasLabelText() && !layer->IsDisabledAspectGroup(AG_Label) )
	{
		LabelDrawer ld(fd);               // allocate font(s) required for drawing labels

		ResumableCounter tileCounter(d.GetCounterStacks(), true);
		for(tile_id t = tileCounter.Value(); t<tn; ++t)
		{
			typename p_traits::LockedSeqType lockedData = da->GetLockedDataRead(t);
			typename p_traits::CPolyIterType
				b = lockedData.begin(),
				e = lockedData.end();
			const typename p_traits::RectArrayType& rectArray  = boundingBoxCache->GetBoundsArray(t);
			const typename p_traits::RectArrayType& blockArray = boundingBoxCache->GetBlockBoundArray(t);
			lfs_assert(rectArray.size() == e-b);

			ResumableCounter itemCounter(d.GetCounterStacks(), true);
			typename p_traits::RectArrayType::const_iterator ri = rectArray.begin() + itemCounter.Value();
			ScanPointCalcResource<ScalarType> calcResource;

			for (typename p_traits::CPolyIterType i=b+itemCounter.Value(); i != e; ++i, ++ri)
			{
				if ((i - b) % AbstrBoundingBoxCache::c_BlockSize == 0)
					while (!IsIntersecting(clipRect, blockArray[(i-b) / AbstrBoundingBoxCache::c_BlockSize]))
					{
						i  += AbstrBoundingBoxCache::c_BlockSize;
						if (!(i<e)) goto exitLabelDraw;
						itemCounter += AbstrBoundingBoxCache::c_BlockSize;
						if (itemCounter.MustBreakOrSuspend()) return true;
						ri += AbstrBoundingBoxCache::c_BlockSize;
					}
				UInt32 nrPoints = i->size();		
				if	(	nrPoints >= 3
					&&	IsIntersecting(clipRect, *ri )
					&&	Width (*ri) >= minWorldWidth
					&&	Height(*ri) >= minWorldHeight
					)
				{
					typename p_traits::PointType centroid = (nrPoints < 4096)
						?	CentroidOrMid<ScalarType,typename p_traits::PointType>(*i, calcResource)
						: Center(*ri)
					;
	//				dms_assert(IsIncluding(*ri, centroid));

					auto entityIndex = trd->GetRowIndex(t, i - b);
					entityIndex = fd.m_IndexCollector.GetEntityIndex(entityIndex);
					if (!IsDefined(entityIndex))
						goto nextLabel;


					if (selectedOnly && !(selectionsArray && SelectionID(selectionsArray[entityIndex])))
						goto nextLabel;
					auto dp = d.GetTransformation().Apply(centroid);
					ld.DrawLabel(entityIndex, GPoint(dp.X(), dp.Y()));
				}
			nextLabel:
				++itemCounter; if (itemCounter.MustBreakOrSuspend100()) return true;
			}
	exitLabelDraw:
			itemCounter.Close();
			++tileCounter; if (tileCounter.MustBreakOrSuspend()) return true;
		}
		tileCounter.Close();
	}
	mainCount.Close();
	return false;
}

#endif // __SHV_DRAWPOLYGONS_H
