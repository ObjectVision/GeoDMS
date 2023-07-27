#pragma once

#include "ShvDllPch.h"

#include "GridCoord.h"

#include "dbg/SeverityType.h"
#include "geo/Conversions.h"
#include "geo/PointOrder.h"
#include "ser/PairStream.h"
#include "ser/RangeStream.h"

#include "AbstrUnit.h"

#include "StgBase.h"
#include "GridStorageManager.h"

#include "ViewPort.h"

const CrdType GRID_EXTENTS_MARGIN = 0.01;
const UInt32  MIN_GRIDLINE_SIZE = 25;

#if defined(MG_DEBUG)
#define MG_DEBUG_COORD
#endif

//----------------------------------------------------------------------
// class  : GridCoord
//----------------------------------------------------------------------

GridCoord::GridCoord(ViewPort* owner, const grid_coord_key& key) //, GPoint clientSize, const CrdTransformation& w2vTr)
	:	m_Owner(owner->shared_from_base<ViewPort>())
		,	m_Key(key)
		,	m_SubPixelFactors(-1.0, -1.0)
{
#if defined(MG_DEBUG_COORD)
	reportF(SeverityTypeID::ST_MajorTrace, "GridCoord::GridCoord(%s)", AsString(key).c_str());
#endif
//	Init(clientSize, w2vTr);
}

GridCoord::~GridCoord()
{
#if defined(MG_DEBUG_COORD)
	reportF(SeverityTypeID::ST_MajorTrace, "GridCoord::~GridCoord(%s)", AsString(m_Key).c_str());
#endif
	auto owner = m_Owner.lock();
	if (owner)
		owner->m_GridCoordMap.erase(m_Key);
}

void GridCoord::Init(GPoint deviceSize, const CrdTransformation& w2dTr)
{
	m_DeviceSize     = deviceSize;
	m_World2DeviceTr = w2dTr;
	m_IsDirty        = true;
}

bool GridCoord::Empty() const
{
	assert(!m_IsDirty);
	return m_ClippedRelDeviceRect.empty(); 
}

void CalcGridNrs(UInt32* gridCoords, UInt32* linedCoords, CrdType currGridCrd, CrdType deltaGridCrd, CrdType subPixelFactor, UInt32 nrGridPos, UInt32 nrViewPos)
{
	assert(nrViewPos);

	assert(currGridCrd <  nrGridPos);
	assert(currGridCrd >= 0);
	UInt32 currGridPos = RoundDownPositive<4>( currGridCrd ), prevGridPos = currGridPos;

	// if (lineCoords) go back (subPixelFactor) cells to determine showLinesNow
	UInt32 showLinesNow = subPixelFactor;
	CrdType bolGridCrd = currGridCrd;
	while (showLinesNow)
	{
		if (RoundDown<4>(bolGridCrd -= deltaGridCrd) != currGridPos)
			break;
		--showLinesNow;
	}
	while (true)
	{
		assert(currGridPos <= currGridCrd );

		*gridCoords++ = currGridPos;

		if (linedCoords)
		{
			if (!showLinesNow && currGridPos != prevGridPos)
				showLinesNow = subPixelFactor;

			if (showLinesNow==0)
				*linedCoords = currGridPos;
			else
			{
				--showLinesNow;
				*linedCoords = UNDEFINED_VALUE(UInt32);
			}
			++linedCoords;
		}

		if (!--nrViewPos)
			return;

		currGridCrd += deltaGridCrd;
		assert(currGridCrd >= 0);
		assert(currGridCrd <  nrGridPos);

		prevGridPos = currGridPos;
		currGridPos = RoundDownPositive<4>( currGridCrd );
	}
}

bool MustShowLines(CrdType deltaGridCrd, CrdType subPixelFactor)
{
	return (1.0 / abs(deltaGridCrd*subPixelFactor) > MIN_GRIDLINE_SIZE);
}

void CalcGridNrs(grid_coord_array& gridCoords, grid_coord_array& linedCoords, CrdType firstGridCrd, CrdType deltaGridCrd, CrdType subPixelFactor, UInt32 nrGridPos, UInt32 nrViewPos)
{
	bool showLines = MustShowLines(deltaGridCrd, subPixelFactor);
	gridCoords.resize(nrViewPos);
	linedCoords.resize(showLines ? nrViewPos : 0);
	if (nrViewPos)
		CalcGridNrs(
			&*gridCoords.begin()
		,	showLines ? &*linedCoords.begin() : nullptr
		,	firstGridCrd
		,	deltaGridCrd, subPixelFactor
		,	nrGridPos, nrViewPos
		);
}

void GridCoord::Recalc()
{
	// determine transformation and cliprect
	CrdTransformation grid2DeviceTr = m_Key.first;
	grid2DeviceTr *= m_World2DeviceTr;

	auto vp = m_Owner.lock();

	m_Orientation = grid2DeviceTr.Orientation();

	CrdRect gridCRect = Deflate(Convert<CrdRect>(m_Key.second), CrdPoint(GRID_EXTENTS_MARGIN, GRID_EXTENTS_MARGIN));
	CrdRect viewDRect = grid2DeviceTr.Apply(gridCRect); // grid in device coordinates

	CrdRect clientDeviceRect = CrdRect(CrdPoint(0,0), g2dms_order<CrdType>(m_DeviceSize) );

	CrdRect gridClipRect = gridCRect & grid2DeviceTr.Reverse(clientDeviceRect); // (viewport & reduced grid) in grid coords

	m_GridOrigin.Col() = IsRightLeft(m_Orientation) ? gridClipRect.second.Col() : gridClipRect.first.Col();
	m_GridOrigin.Row() = IsBottomTop(m_Orientation) ? gridClipRect.second.Row() : gridClipRect.first.Row();
	m_GridOrigin -= Convert<CrdPoint>( GetGridRect().first ); // make row & col nrs zero based; Roworder is usually reversed (O_BottomTop)
	m_GridCellSize = grid2DeviceTr.V2WFactor();

	clientDeviceRect &= viewDRect;                                          // (viewport & grid) in device coords
	if (clientDeviceRect.empty())
		m_ClippedRelDeviceRect = GRect();
	else
	{
		m_ClippedRelDeviceRect = Rect2GRect( RoundUp<4>(clientDeviceRect), CrdPoint(1.0, 1.0) ); 
		assert(m_GridOrigin.Row() >= 0);
		assert(m_GridOrigin.Col() >= 0);
		assert(m_ClippedRelDeviceRect.Size().x >= 0);
		assert(m_ClippedRelDeviceRect.Size().y >= 0);

		assert((m_GridCellSize.Row() >= 0) || (m_GridOrigin.Row() + (m_ClippedRelDeviceRect.Size().y - 1) * m_GridCellSize.Row() >= 0));
		assert((m_GridCellSize.Col() >= 0) || (m_GridOrigin.Col() + (m_ClippedRelDeviceRect.Size().x - 1) * m_GridCellSize.Col() >= 0));
	}
}

void GridCoord::UpdateToScale(DPoint subPixelFactors)
{
	if (subPixelFactors != m_SubPixelFactors)
		m_IsDirty = true;
	if (!m_IsDirty)
		return;

	m_SubPixelFactors = subPixelFactors;
	Recalc();
	m_IsDirty = false;

// process
	GPoint viewSize = m_ClippedRelDeviceRect.Size();
	IPoint gridSize = Size(GetGridRect());
	assert(gridSize.first  >= 0);
	assert(gridSize.second >= 0);
	assert(viewSize.x >= 0);
	assert(viewSize.y >= 0);


#if defined(MG_DEBUG_COORD)
	reportF(SeverityTypeID::ST_MajorTrace, "GridCoord::Update(%s)", AsString(m_Key).c_str());
#endif

	CalcGridNrs(m_GridCols, m_LinedCols, m_GridOrigin.Col(), m_GridCellSize.Col(), subPixelFactors.first , gridSize.Col(), viewSize.x);
	CalcGridNrs(m_GridRows, m_LinedRows, m_GridOrigin.Row(), m_GridCellSize.Row(), subPixelFactors.second, gridSize.Row(), viewSize.y);
}

void AdjustGridNrs(grid_coord_array& gridCoords, grid_coord_array& linedCoords, Int32 deltaBegin, Int32 deltaSecond, CrdType gridOriginCrd, CrdType gridCellSize, CrdType subPixelFactor, UInt32 nrGridPos)
{
	assert(subPixelFactor > 0.0); // Update was called before
	bool showLines = MustShowLines(gridCellSize, subPixelFactor);

	assert(linedCoords.size() == (showLines ? gridCoords.size() : 0));
	if (deltaBegin > 0) 
	{
		gridCoords.erase (gridCoords.begin(),  gridCoords.begin() + deltaBegin);
		if (showLines)
			linedCoords.erase (linedCoords.begin(),  linedCoords.begin() + deltaBegin);
	}
	else if (deltaBegin < 0) 
	{
		gridCoords.insert(gridCoords.begin(), - deltaBegin, 0);
		if (showLines)
			linedCoords.insert(linedCoords.begin(), - deltaBegin, 0);
		CalcGridNrs(
			&gridCoords[0]
		,	showLines ? &*linedCoords.begin() :0
		,	gridOriginCrd
		,	gridCellSize
		,	subPixelFactor
		,	nrGridPos
		,	-deltaBegin
		);
	}

	if (deltaSecond < 0) 
	{
		gridCoords.erase (gridCoords.end() + deltaSecond,  gridCoords.end());
		if (showLines)
			linedCoords.erase (linedCoords.end() + deltaSecond,  linedCoords.end());
	}
	else if (deltaSecond > 0) 
	{
		gridCoords.insert(gridCoords.end(), deltaSecond, 0);
		if (showLines)
			linedCoords.insert(linedCoords.end(), deltaSecond, 0);
		CalcGridNrs(
			&(gridCoords.end()[ - deltaSecond])
		,	linedCoords.empty() ? 0 : &(linedCoords.end()[ - deltaSecond])
		,	gridOriginCrd + (gridCoords.size() - deltaSecond) * gridCellSize
		,	gridCellSize
		,	subPixelFactor
		,	nrGridPos
		,	deltaSecond
		);
	}
}

void GridCoord::OnDeviceScroll(const GPoint& delta)
{
	m_World2DeviceTr += g2dms_order<CrdType>(delta);

	if (m_IsDirty)
		return;
	if (Empty())
	{
		m_IsDirty = true;
		return;
	}

	GRect oldClippedRelRect = m_ClippedRelDeviceRect;
	Recalc();
	if (Empty())
	{
		m_GridCols.clear();
		m_GridRows.clear();
		return;
	}

	oldClippedRelRect += delta;

	IPoint gridSize  = Size(GetGridRect());

	//==========

	GType deltaLeft  = m_ClippedRelDeviceRect.Left () - oldClippedRelRect.Left ();
	GType deltaRight = m_ClippedRelDeviceRect.Right() - oldClippedRelRect.Right();

	AdjustGridNrs(m_GridCols, m_LinedCols, deltaLeft, deltaRight, m_GridOrigin.Col(), m_GridCellSize.Col(), m_SubPixelFactors.first, gridSize.Col());

	//==========

	TType deltaTop    = m_ClippedRelDeviceRect.Top   () - oldClippedRelRect.Top   ();
	TType deltaBottom = m_ClippedRelDeviceRect.Bottom() - oldClippedRelRect.Bottom();

	AdjustGridNrs(m_GridRows, m_LinedRows, deltaTop, deltaBottom, m_GridOrigin.Row(), m_GridCellSize.Row(),  m_SubPixelFactors.second, gridSize.Row());
}

const grid_rowcol_id* GridCoord::GetGridRowPtr(view_rowcol_id currViewRelRow, bool withLines) const
{
	assert(!IsDirty()); 
	if (Empty())
		return nullptr;

	assert(currViewRelRow >= view_rowcol_id(m_ClippedRelDeviceRect.Top()));
	currViewRelRow -= m_ClippedRelDeviceRect.top; 
	assert(currViewRelRow <= m_GridRows.size()); 
	if (withLines && !m_LinedRows.empty())
		return begin_ptr(m_LinedRows) + currViewRelRow;
	return begin_ptr(m_GridRows) + currViewRelRow; 
} 

const grid_rowcol_id* GridCoord::GetGridColPtr(view_rowcol_id currViewRelCol, bool withLines) const
{
	dms_assert(!IsDirty()); 
	if (Empty())
		return nullptr;

//	if (m_Owner)
//		currViewRelCol -= m_Owner->GetCurrClientAbsPos().x;
	dms_assert(currViewRelCol >= view_rowcol_id(m_ClippedRelDeviceRect.Left())); 
	currViewRelCol -= m_ClippedRelDeviceRect.left; 
	dms_assert(currViewRelCol <= m_GridCols.size()); 
	if (withLines && !m_LinedCols.empty())
		return begin_ptr(m_LinedCols) + currViewRelCol;
	return begin_ptr(m_GridCols) + currViewRelCol; 
} 

const grid_rowcol_id* GridCoord::GetGridRowPtrFromAbs(view_rowcol_id currViewAbsRow, bool withLines) const
{
	auto owner = m_Owner.lock();
	if (owner)
		currViewAbsRow -= owner->GetCurrClientAbsDevicePos().y;
	return GetGridRowPtr(currViewAbsRow, withLines);
}

const grid_rowcol_id* GridCoord::GetGridColPtrFromAbs(view_rowcol_id currViewAbsCol, bool withLines) const
{
	auto owner = m_Owner.lock();
	if (owner)
		currViewAbsCol -= owner->GetCurrClientAbsDevicePos().x;
	return GetGridColPtr(currViewAbsCol, withLines);
}

#include <algorithm>

UInt32 FindViewPos(const grid_coord_array& coords, Int32 gridCrd, bool reversedOrder)
{
	return 
		(	reversedOrder
			?	std::upper_bound(coords.begin(), coords.end(), gridCrd, std::greater<TType>()) 
			:	std::lower_bound(coords.begin(), coords.end(), gridCrd, std::less   <TType>())
		)
		-	coords.begin();
}

GType GridCoord::FirstViewCol(Int32 col) const 
{
	return FindViewPos(m_GridCols, col - GetGridRect().first.Col(), IsRightLeft(m_Orientation));
}

GType GridCoord::FirstViewRow(Int32 row) const 
{
	return FindViewPos(m_GridRows, row - GetGridRect().first.Row(), IsBottomTop(m_Orientation));
}

IRect GridCoord::GetClippedGridRect(const GRect& viewRelRect) const
{
	if (Empty())
		return IRect();

	IRect result = IRect(
		GetGridCoord(viewRelRect.LeftTop())
	,	GetGridCoord(viewRelRect.RightBottom()-GPoint(1,1))
	);
	result.second += IPoint(1,1);
	return result;
}

GRect GridCoord::GetClippedRelDeviceRect(const IRect& selRect) const 
{ 
	assert(!IsDirty()); 
	GRect result(
		FirstViewCol(selRect.first .Col() ),
		FirstViewRow(selRect.first .Row() ),
		FirstViewCol(selRect.second.Col() ),
		FirstViewRow(selRect.second.Row() )
	);
	if (IsBottomTop(m_Orientation)) omni::swap(result.bottom, result.top ); assert(result.top <= result.bottom); 
	if (IsRightLeft(m_Orientation)) omni::swap(result.right,  result.left); assert(result.left<= result.right ); 
	
	result += m_ClippedRelDeviceRect.LeftTop();
	result &= m_ClippedRelDeviceRect;

	return result;
} 

IPoint GridCoord::GetGridCoord(const GPoint& deviceRelPoint) const
{
	assert(!IsDirty()); 
	assert(!Empty());
	assert(IsIncluding(m_ClippedRelDeviceRect, deviceRelPoint));
	// END OF PRECONDITIONS

	return 
		shp2dms_order(
			IPoint(
				*GetGridColPtr(deviceRelPoint.x, false),
				*GetGridRowPtr(deviceRelPoint.y, false)
			) 
		)	
	+	GetGridRect().first;
}

IPoint GridCoord::GetExtGridCoord(GPoint deviceRelPoint) const
{
	dms_assert(!IsDirty()); 
//	if (Empty())
//		return Undefined();

	if (!IsIncluding(m_ClippedRelDeviceRect, deviceRelPoint))
	{
		auto deviceOffset = deviceRelPoint - m_ClippedRelDeviceRect.LeftTop();
		auto gridPos = m_GridOrigin + m_GridCellSize * shp2dms_order<CrdType>(deviceOffset.x, deviceOffset.y);
		return RoundDown<4>(gridPos) + GetGridRect().first;
	}

	return GetGridCoord(deviceRelPoint);
}

IPoint GridCoord::GetExtGridCoordFromAbs(GPoint clientAbsPoint) const
{	
	auto owner = m_Owner.lock();
	if (owner)
		clientAbsPoint -= owner->GetCurrClientAbsDevicePos();
	return GetExtGridCoord(clientAbsPoint);
}
