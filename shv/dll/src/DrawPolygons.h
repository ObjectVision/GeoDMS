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

#if !defined(__SHV_DRAWPOLYGONS_H)
#define __SHV_DRAWPOLYGONS_H

#include "geo/CalcWidth.h"
#include "geo/PointIndexBuffer.h"

#include "BoundingBoxCache.h"
#include "CounterStacks.h"
#include "GeoTypes.h"
#include "GraphVisitor.h"
#include "IndexCollector.h"
#include "Theme.h"
#include "ThemeValueGetter.h"
#include "RemoveAdjacentsAndSpikes.h"

#define MIN_WORLD_SIZE 0.5

typedef std::vector<GPoint>         pointBuffer_t;
//typedef std::vector<Range<UInt32> > pointIndexBuffer_t;

template <typename PI>
void fillPointBuffer(pointBuffer_t& buf, PI ii, PI ie, CrdTransformation transformer)
{
	UInt32 nrPoints = ie - ii;

	buf.resize(nrPoints);

	pointBuffer_t::iterator 
		bi = buf.begin();
	for(;ii!=ie; ++ii, ++bi)
		*bi = DPoint2GPoint( transformer.Apply(*ii) );
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
	typedef typename BoundingBoxCache<ScalarType>::RectArrayType RectArrayType;
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
	,	const BoundingBoxCache<ScalarType>* boundingBoxCache
	,	tile_id t, SizeT tileIndexBase
	,	typename polygon_traits<ScalarType>::CPolySeqType featureData
	,	WeakPtr<const IndexCollector> indexCollector
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

	ScalarType minWorldWidth  = MIN_WORLD_SIZE / zoomLevel;
	ScalarType minWorldHeight = minWorldWidth;

	GdiHandle<HPEN>         invisiblePen(CreatePen(PS_NULL, 0, RGB(0, 0, 0)));
	GdiObjectSelector<HPEN> penSelector(d.GetDC(), invisiblePen);

	lfs_assert(rectArray.size() == featureData.size());

	ResumableCounter itemCounter(d.GetCounterStacks(), true);

	typename p_traits::RectArrayType::const_iterator ri = rectArray.begin() + itemCounter.Value();
	for (typename p_traits::CPolyIterType b = featureData.begin(), e = featureData.end(), i=b+itemCounter.Value();
		i != e; 
		++i, ++ri)
	{
		if (!((i-b) % AbstrBoundingBoxCache::c_BlockSize))
			while (!IsIntersecting(clipRect, blockArray[(i-b) / BoundingBoxCache<ScalarType>::c_BlockSize]))
			{
				i  += BoundingBoxCache<ScalarType>::c_BlockSize;
				if (!(i<e)) goto exitFill;
				itemCounter += BoundingBoxCache<ScalarType>::c_BlockSize;
				if (itemCounter.MustBreakOrSuspend()) return true;
				ri += BoundingBoxCache<ScalarType>::c_BlockSize;
			}
		if	(	i->size() >= 3 
			&&	IsIntersecting(clipRect, *ri )
			&&	_Width (*ri) >= minWorldWidth
			&&	_Height(*ri) >= minWorldHeight
			)
		{
			COLORREF brushColor = defBrushColor;
			Int32    hatchStyle = -1;

			entity_id entityIndex = (i - b) + tileIndexBase;
			if (indexCollector)
			{
				entityIndex = indexCollector->GetEntityIndex(entityIndex);
				if (!IsDefined(entityIndex))
					goto nextFill;
			}

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
bool DrawPolygons(
	const GraphicPolygonLayer* layer,
	const FeatureDrawer&       fd,
	const AbstrDataItem*       featureItem,
	const PenIndexCache*       penIndices)
{
	typedef polygon_traits<ScalarType> p_traits;
	const GraphDrawer& d = fd.m_Drawer;

	const AbstrDataObject* featureData = featureItem->GetRefObj();
	const typename p_traits::DataArrayType* da = debug_cast<const typename p_traits::DataArrayType*>(featureData);
	tile_id tn = featureItem->GetAbstrDomainUnit()->GetNrTiles();

	const BoundingBoxCache<ScalarType>* boundingBoxCache =  GetBoundingBoxCache<ScalarType>(layer);

	pointBuffer_t      pointBuffer;

	CrdType zoomLevel = Abs(d.GetTransformation().ZoomLevel());
	dms_assert(zoomLevel > 1.0e-30); // we assume that nothing remains visible on such a small scale to avoid numerical overflow in the following inversion

	ScalarType minWorldWidth  = MIN_WORLD_SIZE / zoomLevel;
	ScalarType minWorldHeight = minWorldWidth;

	typename p_traits::RangeType clipRect = Convert<typename p_traits::RangeType>( layer->GetWorldClipRect(d) );

	WeakPtr<const IndexCollector> indexCollector = fd.GetIndexCollector();
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
				typename p_traits::LockedSeqType lockedData = da->GetLockedDataRead(t);
				SizeT tileIndexBase = featureItem->GetAbstrDomainUnit()->GetTileFirstIndex(t);

				if (DrawPolygonInterior(
						(transparentBrush)
							?	TRANSPARENT_COLORREF
							:	DmsColor2COLORREF(layer->GetDefaultOrThemeColor(AN_BrushColor) ) // green as default color for polygons isn't really used since we have a theme
					,	layer->GetEnabledTheme(AN_BrushColor).get()
					,	layer->GetEnabledTheme(AN_HatchStyle).get()
					,	d
					,   boundingBoxCache
					,	t, tileIndexBase
					,	lockedData
					,	indexCollector
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
			PenArray pa(d.GetDC(), penIndices, true);

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
				SizeT tileIndexBase = featureItem->GetAbstrDomainUnit()->GetTileFirstIndex(t);

				ResumableCounter itemCounter(d.GetCounterStacks(), true);
				typename p_traits::RectArrayType::const_iterator ri = rectArray.begin() + itemCounter.Value();

				for (typename p_traits::CPolyIterType i = b + itemCounter.Value(); i != e; ++i, ++ri)
				{
					if (!((i-b) & (BoundingBoxCache<ScalarType>::c_BlockSize -1)))
						while (!IsIntersecting(clipRect, blockArray[(i-b) / BoundingBoxCache<ScalarType>::c_BlockSize]))
						{
							i  += BoundingBoxCache<ScalarType>::c_BlockSize;
							if (!(i<e)) goto exitDrawBorders;
							itemCounter += BoundingBoxCache<ScalarType>::c_BlockSize;
							if (itemCounter.MustBreakOrSuspend()) 
								return true;
							ri += BoundingBoxCache<ScalarType>::c_BlockSize;
						}
					if	(	i->size() >= 3
						&&	IsIntersecting(clipRect, *ri)
						&&	(_Width (*ri) >= minWorldWidth || _Height(*ri) >= minWorldHeight)
						)
					{
						if (penIndices || selectedOnly)
						{
							SizeT entityIndex = (i - b) + tileIndexBase;
							if (indexCollector)
								entityIndex = indexCollector->GetEntityIndex(entityIndex);
							if (!IsDefined(entityIndex))
								goto nextBorder;
							if (penIndices   && ! pa.SelectPen(penIndices->GetKeyIndex(entityIndex) ) )
								goto nextBorder;
							if (selectedOnly && !(selectionsArray && SelectionID( selectionsArray[entityIndex])))
								goto nextBorder;
						}

						fillPointBuffer     (pointBuffer,      i->begin(), i->end(), d.GetTransformation());
						fillPointIndexBuffer(pointIndexBuffer, i->begin(), i->end());

						pointBuffer_t::iterator 
							bi = pointBuffer.begin();

						// draw Polyline for each island and lake; identified by repetition of start-point
						pointIndexBuffer_t::const_iterator 
							ii = pointIndexBuffer.begin(),
							ie = pointIndexBuffer.end();

						lfs_assert(i->size());
						const typename p_traits::PointType* iBegin = begin_ptr(*i);
						for (; ii != ie; ++ii)
						{
							dms_assert(ii->first <= ii->second);
							dms_assert(ii->second <= pointBuffer.size());

							auto bufferOffset    = bi + ii->first;
							auto bufferOffsetEnd = bi + ii->second;

							bufferOffsetEnd = std::unique(bufferOffset, bufferOffsetEnd);
							UInt32 lineSize        =  bufferOffsetEnd - bufferOffset;
							if	(	lineSize >= 2
								)
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
			SizeT tileIndexBase = featureItem->GetAbstrDomainUnit()->GetTileFirstIndex(t);

			ResumableCounter itemCounter(d.GetCounterStacks(), true);
			typename p_traits::RectArrayType::const_iterator ri = rectArray.begin() + itemCounter.Value();
			ScanPointCalcResource<ScalarType> calcResource;

			for (typename p_traits::CPolyIterType i=b+itemCounter.Value(); i != e; ++i, ++ri)
			{
				if (!((i-b) & (BoundingBoxCache<ScalarType>::c_BlockSize -1)))
					while (!IsIntersecting(clipRect, blockArray[(i-b) / BoundingBoxCache<ScalarType>::c_BlockSize]))
					{
						i  += BoundingBoxCache<ScalarType>::c_BlockSize;
						if (!(i<e)) goto exitLabelDraw;
						itemCounter += BoundingBoxCache<ScalarType>::c_BlockSize;
						if (itemCounter.MustBreakOrSuspend()) return true;
						ri += BoundingBoxCache<ScalarType>::c_BlockSize;
					}
				UInt32 nrPoints = i->size();		
				if	(	nrPoints >= 3
					&&	IsIntersecting(clipRect, *ri )
					&&	_Width (*ri) >= minWorldWidth
					&&	_Height(*ri) >= minWorldHeight
					)
				{
					typename p_traits::PointType centroid = (nrPoints < 4096)
						?	CentroidOrMid<ScalarType,typename p_traits::PointType>(*i, calcResource)
						: Center(*ri)
					;
	//				dms_assert(IsIncluding(*ri, centroid));

					UInt32 entityIndex = (i - b) + tileIndexBase;
					if (indexCollector)
					{
						entityIndex = indexCollector->GetEntityIndex(entityIndex);
						if (!IsDefined(entityIndex))
							goto nextLabel;
					}
					if (selectedOnly && !(selectionsArray && SelectionID(selectionsArray[entityIndex])))
						goto nextLabel;
					ld.DrawLabel(entityIndex, Convert<GPoint>(d.GetTransformation().Apply(centroid)));
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
