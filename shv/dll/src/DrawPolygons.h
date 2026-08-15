// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

/*
 *  Template drawing routines for polygon feature layers: DrawPolygons
 *  renders the (selected) features of a GraphicPolygonLayer through the
 *  FeatureDrawer/DrawContext machinery, using point-index buffers, the
 *  pen-index cache, and suspendible counting via CounterStacks.
 */

#if !defined(__SHV_DRAWPOLYGONS_H)
#define __SHV_DRAWPOLYGONS_H

#include "geo/CalcWidth.h"
#include "geo/PointIndexBuffer.h"
#include "geo/PointOrder.h"

#include "BoundingBoxCache.h"
#include "CounterStacks.h"
#include "DrawContext.h"
#include "FeatureLayer.h"
#include "LabelDrawer.h"
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

// Clip a closed polygon ring (world coords) to the in-front half-plane (refSign * w >= wEps) in WORLD space
// -- where w == CrdTransformation::ApplyDenom is LINEAR in (x,y) -- then project the result to device GPoints.
// Used only for the INTERIOR fill under a PROJECTIVE (tilted) view: a vertex beyond the projective horizon
// (w <= 0) otherwise divides by ~0 in the perspective map and the polygon balloons into a screen-filling slab.
// Sutherland-Hodgman against the single horizon edge; the clip edge along the horizon is kept, which is correct
// for a filled area (it bounds the visible part of the polygon at the horizon). wEps > 0 keeps the projected
// near-horizon vertices finite (within GDI's coordinate range); within the visible rect w stays well above it,
// so on-screen geometry is never clipped. Affine/axis-separable views keep the exact 1:1 fillPointBuffer path.
template <typename PI>
void fillPointBufferHorizonClipped(pointBuffer_t& buf, PI ii, PI ie, const CrdTransformation& tr, double refSign, double wEps)
{
	buf.clear();
	if (ii == ie)
		return;
	auto signedW = [&](const DPoint& p) { return refSign * tr.ApplyDenom(p); };

	DPoint pPrev = DPoint(*(ie - 1));      // closed ring: start from the last vertex
	double wPrev = signedW(pPrev);
	for (PI cur = ii; cur != ie; ++cur)
	{
		DPoint  pCur = DPoint(*cur);
		double  wCur = signedW(pCur);
		bool    inPrev = wPrev >= wEps, inCur = wCur >= wEps;
		if (inCur != inPrev)                 // edge crosses the horizon -> insert the intersection
		{
			double t = (wEps - wPrev) / (wCur - wPrev);
			buf.push_back(DPoint2GPoint(pPrev + (pCur - pPrev) * t, tr));
		}
		if (inCur)
			buf.push_back(DPoint2GPoint(pCur, tr));
		pPrev = pCur;
		wPrev = wCur;
	}
}

// Horizon-clip an OPEN polyline (a polygon-border ring or an arc) under a PROJECTIVE view and draw it.
// Unlike the filled-interior clip, no spurious edge along the horizon is added: the in-front portions are
// emitted as separate open polylines and the behind-horizon parts are dropped. Without this, a ring/arc with
// a vertex beyond the horizon (w <= 0) projects to a wild device coordinate and draws as a line shooting
// across the whole viewport. `scratch` is a reusable device-point buffer. Affine views never call this.
template <typename PI>
void drawHorizonClippedPolyline(DrawContext* drawCtx, pointBuffer_t& scratch, PI ii, PI ie,
	const CrdTransformation& tr, double refSign, double wEps,
	DmsColor penColor, int penWidth, DmsPenStyle penStyle)
{
	if (ii == ie)
		return;
	auto signedW = [&](const DPoint& p) { return refSign * tr.ApplyDenom(p); };
	auto flush   = [&]() { if (scratch.size() >= 2) drawCtx->DrawPolyline(scratch.data(), scratch.size(), penColor, penWidth, penStyle); scratch.clear(); };

	scratch.clear();
	DPoint pPrev = DPoint(*ii);
	double wPrev = signedW(pPrev);
	bool   inPrev = wPrev >= wEps;
	if (inPrev)
		scratch.push_back(DPoint2GPoint(pPrev, tr));
	for (PI cur = ii + 1; cur != ie; ++cur)
	{
		DPoint pCur = DPoint(*cur);
		double wCur = signedW(pCur);
		bool   inCur = wCur >= wEps;
		if (inCur != inPrev)                       // crossing: add the horizon intersection point
		{
			double t = (wEps - wPrev) / (wCur - wPrev);
			scratch.push_back(DPoint2GPoint(pPrev + (pCur - pPrev) * t, tr));
			if (!inCur)                            // leaving the in-front side -> finish this run
				flush();
		}
		if (inCur)
			scratch.push_back(DPoint2GPoint(pCur, tr));
		pPrev = pCur;
		wPrev = wCur;
		inPrev = inCur;
	}
	flush();
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

	CrdType zoomLevel = d.GetWorldZoomLevel();

	dms_assert(zoomLevel > 1.0e-30); // we assume that nothing remains visible on such a small scale to avoid numerical overflow in the following inversion

	// Under a projective (tilted) view, clip polygon interiors against the horizon (see fillPointBufferHorizonClipped);
	// a vertex beyond the horizon would otherwise balloon the filled polygon into a screen-wide slab. Computed once.
	// Sample the in-front horizon sign at the TRUE view centre = Reverse(device-clip centre), which is always in
	// front. Do NOT use Center(clipRect): clipRect is the AABB of the back-projected device rect, whose centre
	// drifts toward/past the horizon at steep tilt -> the sign flips -> the clip inverts and discards the (in-front)
	// polygon, so it vanished at the steepest tilt levels. Affine views keep the exact 1:1 fill.
	const bool   isProjective = d.GetTransformation().IsProjective();
	double horizonRefSign = 1.0;
	if (isProjective)
	{
		auto viewCentreWorld = d.GetTransformation().Reverse(Center(g2dms_order<CrdType>(d.GetAbsClipDeviceRect())));
		if (d.GetTransformation().ApplyDenom(viewCentreWorld) < 0)
			horizonRefSign = -1.0;
	}
	const double horizonWEps = 0.01;

	ScalarType minWorldWidth  = s_DrawingSizeTresholdInPixels / zoomLevel;
	ScalarType minWorldHeight = minWorldWidth;

	auto* drawContext = d.GetDrawContext();

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
				brushColor = GetFocusClr(); // centralized focus highlight (issue #1039)
			else if (isSelected)
				brushColor = GetSelectedClr();
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

				typename sequence_traits<typename p_traits::PointType>::const_pointer
					pointArrayBegin = i->begin(),
					pointArrayEnd   = i->end();

				if (isProjective)
					fillPointBufferHorizonClipped(pointBuffer, pointArrayBegin, pointArrayEnd, d.GetTransformation(), horizonRefSign, horizonWEps);
				else
					fillPointBuffer(pointBuffer, pointArrayBegin, pointArrayEnd, d.GetTransformation());

				remove_adjacents_and_spikes(pointBuffer);
				if (pointBuffer.size() >= 3)
					drawContext->DrawPolygon(
						pointBuffer.data(),
						pointBuffer.size(),
						COLORREF2DmsColor(brushColor),
						static_cast<DmsHatchStyle>(hatchStyle)
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

					reportF(ST_MinorTrace, "bufferOffset {}", bufferOffset);

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

template <typename ScalarType>
bool DrawPolygons(const GraphicPolygonLayer* layer, const FeatureDrawer& fd, const AbstrDataItem* featureItem, const PenIndexCache* penIndices)
{
	typedef polygon_traits<ScalarType> p_traits;
	const GraphDrawer& d = fd.m_Drawer;

	const AbstrDataObject* featureData = featureItem->GetRefObj().get();
	auto da = const_array_cast<typename p_traits::PolygonType>(featureData);
	auto trd = da->GetTiledRangeData();
	tile_id tn = trd->GetNrTiles();

	auto boundingBoxCache =  GetSequenceBoundingBoxCache<ScalarType>(layer);

	pointBuffer_t pointBuffer;

	CrdType zoomLevel = d.GetWorldZoomLevel();
	assert(zoomLevel > 1.0e-30); // we assume that nothing remains visible on such a small scale to avoid numerical overflow in the following inversion

	ScalarType minWorldWidth  = s_DrawingSizeTresholdInPixels / zoomLevel;
	ScalarType minWorldHeight = minWorldWidth;

	typename p_traits::RangeType clipRect = Convert<typename p_traits::RangeType>( layer->GetWorldClipRect(d) );

	// Projective (tilted) view: horizon-clip border rings (see drawHorizonClippedPolyline) so an outline whose
	// far vertices fall beyond the horizon does not shoot a line across the whole viewport. Sign sampled at the
	// true view centre = Reverse(device-clip centre) (always in front; Center(clipRect) drifts past the horizon
	// at steep tilt and flips it). Affine views keep the exact 1:1 border path.
	const bool   isProjective = d.GetTransformation().IsProjective();
	double horizonRefSign = 1.0;
	if (isProjective)
	{
		auto viewCentreWorld = d.GetTransformation().Reverse(Center(g2dms_order<CrdType>(d.GetAbsClipDeviceRect())));
		if (d.GetTransformation().ApplyDenom(viewCentreWorld) < 0)
			horizonRefSign = -1.0;
	}
	const double horizonWEps = 0.01;

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
					, trd.get(), t
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
		index_range_vector_t pointIndexBuffer;
		if (penIndices && !layer->IsDisabledAspectGroup(AG_Pen))
		{
			penIndices->UpdateForZoomLevel(d.GetWorldZoomLevel(), d.GetSubPixelFactor());
			auto* drawCtx = d.GetDrawContext();

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
						UInt32 penKeyIndex = 0;
						if (penIndices || selectedOnly)
						{
							SizeT entityIndex = trd->GetRowIndex(t, itemCounter);
							entityIndex = fd.m_IndexCollector.GetEntityIndex(entityIndex);
							if (!IsDefined(entityIndex))
								goto nextBorder;
							if (penIndices)
							{
								penKeyIndex = penIndices->GetKeyIndex(entityIndex);
								if (!penIndices->IsPenVisible(penKeyIndex))
									goto nextBorder;
							}
							if (selectedOnly && !(selectionsArray && SelectionID( selectionsArray[entityIndex])))
								goto nextBorder;
						}

						pointIndexBuffer.resize(0);
						fillPointIndexBuffer(pointIndexBuffer, featurePtr->begin(), featurePtr->end());

						const auto& penKey = penIndices->GetPenKey(penKeyIndex);
						DmsColor penColor = penKey.m_Color;
						int penWidth = penKey.m_Width;
						DmsPenStyle penStyle = static_cast<DmsPenStyle>(penKey.m_Style);

						// draw Polyline for each island and lake; identified by repetition of start-point
						auto ii = pointIndexBuffer.begin(), ie = pointIndexBuffer.end();
						lfs_assert(featurePtr->size());

						if (isProjective)
						{
							// horizon-clip each ring as open polylines over the WORLD points (no 1:1 device buffer,
							// since clipping changes the vertex count); reuse pointBuffer as scratch.
							auto pBegin = featurePtr->begin();
							for (; ii != ie; ++ii)
								drawHorizonClippedPolyline(drawCtx, pointBuffer, pBegin + ii->first, pBegin + ii->second,
									d.GetTransformation(), horizonRefSign, horizonWEps, penColor, penWidth, penStyle);
						}
						else
						{
							fillPointBuffer(pointBuffer, featurePtr->begin(), featurePtr->end(), d.GetTransformation());
							auto bi = pointBuffer.begin();
							for (; ii != ie; ++ii)
							{
								assert(ii->first <= ii->second);
								assert(ii->second <= pointBuffer.size());

								auto bufferOffset    = bi + ii->first;
								auto bufferOffsetEnd = bi + ii->second;

								bufferOffsetEnd = std::unique(bufferOffset, bufferOffsetEnd);
								UInt32 lineSize =  bufferOffsetEnd - bufferOffset;
								if	(lineSize >= 2)
									drawCtx->DrawPolyline(
										&*bufferOffset,
										lineSize,
										penColor,
										penWidth,
										penStyle
									);
							}
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
			lfs_assert(rectArray.size() == SizeT(e-b));

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
