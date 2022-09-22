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

GridCoord::GridCoord(ViewPort* owner, const grid_coord_key& key, GPoint clientSize, const CrdTransformation& w2vTr)
	:	m_Owner(owner->shared_from_base<ViewPort>())
		,	m_Key(key)
		,	m_SubPixelFactor(-1.0)
{
#if defined(MG_DEBUG_COORD)
	reportF(SeverityTypeID::ST_MajorTrace, "GridCoord::GridCoord(%s)", AsString(key).c_str());
#endif
	Init(clientSize, w2vTr);
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

void GridCoord::Init(GPoint clientSize, const CrdTransformation& w2vTr)
{
	m_ClientSize     = clientSize;
	m_World2ClientTr = w2vTr;
	m_IsDirty        = true;
}

bool GridCoord::Empty() const
{
	dms_assert(!m_IsDirty);
	return m_ClippedRelRect.empty(); 
}

void CalcGridNrs(UInt32* gridCoords, UInt32* linedCoords, CrdType currGridCrd, CrdType deltaGridCrd, CrdType subPixelFactor, UInt32 nrGridPos, UInt32 nrViewPos)
{
	dms_assert(nrViewPos);

	dms_assert(currGridCrd <  nrGridPos);
	dms_assert(currGridCrd >= 0);
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
		dms_assert(currGridPos <= currGridCrd );

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
		dms_assert(currGridCrd >= 0);
		dms_assert(currGridCrd <  nrGridPos);

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
		,	showLines ? &*linedCoords.begin() : 0
		,	firstGridCrd
		,	deltaGridCrd, subPixelFactor
		,	nrGridPos, nrViewPos
		);
}

void GridCoord::Recalc()
{
	// determine transformation and cliprect
	CrdTransformation grid2ClientTr = m_Key.first * m_World2ClientTr;
	if (!m_Owner.lock())
		grid2ClientTr = m_World2ClientTr;
	m_Orientation = grid2ClientTr.Orientation();

	CrdRect gridCRect = Deflate(Convert<CrdRect>(m_Key.second), CrdPoint(GRID_EXTENTS_MARGIN, GRID_EXTENTS_MARGIN));
	CrdRect viewCRect = grid2ClientTr.Apply(gridCRect); // reduced grid in client coordinates

	CrdRect clientRect = CrdRect(CrdPoint(0,0), Convert<CrdPoint>(m_ClientSize) );

	CrdRect gridClipRect = gridCRect &  grid2ClientTr.Reverse(clientRect); // (viewport & reduced grid) in grid coords

	m_GridOrigin.Col() = IsRightLeft(m_Orientation) ? gridClipRect.second.Col() : gridClipRect.first.Col();
	m_GridOrigin.Row() = IsBottomTop(m_Orientation) ? gridClipRect.second.Row() : gridClipRect.first.Row();
	m_GridOrigin -= Convert<CrdPoint>( GetGridRect().first ); // make row & col nrs zero based; Roworder is usually reversed (O_BottomTop)
	m_GridCellSize = grid2ClientTr.V2WFactor();

	clientRect &= viewCRect;                                               // (viewport & grid) in client coords
	if (clientRect.empty())
		m_ClippedRelRect = GRect();
	else
	{
		m_ClippedRelRect = Rect2GRect( RoundUp<4>(clientRect) ); 
		dms_assert(m_GridOrigin.Row() >= 0);
		dms_assert(m_GridOrigin.Col() >= 0);
		dms_assert(m_ClippedRelRect.Size().x >= 0);
		dms_assert(m_ClippedRelRect.Size().y >= 0);

		dms_assert((m_GridCellSize.Row() >= 0) || (m_GridOrigin.Row() + (m_ClippedRelRect.Size().y - 1) * m_GridCellSize.Row() >= 0));
		dms_assert((m_GridCellSize.Col() >= 0) || (m_GridOrigin.Col() + (m_ClippedRelRect.Size().x - 1) * m_GridCellSize.Col() >= 0));
	}
}

void GridCoord::Update(Float64 subPixelFactor)
{
	dms_assert(subPixelFactor > 0.0);
	if (subPixelFactor != m_SubPixelFactor)
		m_IsDirty = true;
	if (!m_IsDirty)
		return;

	m_SubPixelFactor = subPixelFactor;
	Recalc();
	m_IsDirty = false;

// process
	GPoint viewSize = m_ClippedRelRect.Size();
	IPoint gridSize = Size(GetGridRect());
	dms_assert(gridSize.first  >= 0);
	dms_assert(gridSize.second >= 0);
	dms_assert(viewSize.x >= 0);
	dms_assert(viewSize.y >= 0);


#if defined(MG_DEBUG_COORD)
	reportF(SeverityTypeID::ST_MajorTrace, "GridCoord::Update(%s)", AsString(m_Key).c_str());
#endif

	CalcGridNrs(m_GridCols, m_LinedCols, m_GridOrigin.Col(), m_GridCellSize.Col(), subPixelFactor, gridSize.Col(), viewSize.x);
	CalcGridNrs(m_GridRows, m_LinedRows, m_GridOrigin.Row(), m_GridCellSize.Row(), subPixelFactor, gridSize.Row(), viewSize.y);
}

void AdjustGridNrs(grid_coord_array& gridCoords, grid_coord_array& linedCoords, Int32 deltaBegin, Int32 deltaSecond, CrdType gridOriginCrd, CrdType gridCellSize, CrdType subPixelFactor, UInt32 nrGridPos)
{
	dms_assert(subPixelFactor > 0.0); // Update was called before
	bool showLines = MustShowLines(gridCellSize, subPixelFactor);

	dms_assert(linedCoords.size() == (showLines ? gridCoords.size() : 0));
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

void GridCoord::OnScroll(const GPoint& delta)
{
	m_World2ClientTr += Convert<CrdPoint>(delta);

	if (m_IsDirty)
		return;
	if (Empty())
	{
		m_IsDirty = true;
		return;
	}

	GRect oldClippedRelRect = m_ClippedRelRect;
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

	GType deltaLeft  = m_ClippedRelRect.Left () - oldClippedRelRect.Left ();
	GType deltaRight = m_ClippedRelRect.Right() - oldClippedRelRect.Right();

	AdjustGridNrs(m_GridCols, m_LinedCols, deltaLeft, deltaRight, m_GridOrigin.Col(), m_GridCellSize.Col(), m_SubPixelFactor, gridSize.Col());

	//==========

	TType deltaTop    = m_ClippedRelRect.Top   () - oldClippedRelRect.Top   ();
	TType deltaBottom = m_ClippedRelRect.Bottom() - oldClippedRelRect.Bottom();

	AdjustGridNrs(m_GridRows, m_LinedRows, deltaTop, deltaBottom, m_GridOrigin.Row(), m_GridCellSize.Row(),  m_SubPixelFactor, gridSize.Row());
}

const grid_rowcol_id* GridCoord::GetGridRowPtr(view_rowcol_id currViewRelRow, bool withLines) const
{
	dms_assert(!IsDirty()); 
	if (Empty())
		return nullptr;

	dms_assert(currViewRelRow >= view_rowcol_id(m_ClippedRelRect.Top()));
	currViewRelRow -= m_ClippedRelRect.top; 
	dms_assert(currViewRelRow <= m_GridRows.size()); 
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
	dms_assert(currViewRelCol >= view_rowcol_id(m_ClippedRelRect.Left())); 
	currViewRelCol -= m_ClippedRelRect.left; 
	dms_assert(currViewRelCol <= m_GridCols.size()); 
	if (withLines && !m_LinedCols.empty())
		return begin_ptr(m_LinedCols) + currViewRelCol;
	return begin_ptr(m_GridCols) + currViewRelCol; 
} 

const grid_rowcol_id* GridCoord::GetGridRowPtrFromAbs(view_rowcol_id currViewAbsRow, bool withLines) const
{
	auto owner = m_Owner.lock();
	if (owner)
		currViewAbsRow -= owner->GetCurrClientAbsPos().y();
	return GetGridRowPtr(currViewAbsRow, withLines);
}

const grid_rowcol_id* GridCoord::GetGridColPtrFromAbs(view_rowcol_id currViewAbsCol, bool withLines) const
{
	auto owner = m_Owner.lock();
	if (owner)
		currViewAbsCol -= owner->GetCurrClientAbsPos().x();
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
	return FindViewPos(m_GridCols,col - GetGridRect().first.Col(),  IsRightLeft(m_Orientation));
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
		GetGridCoord(viewRelRect.TopLeft())
	,	GetGridCoord(viewRelRect.BottomRight()-GPoint(1,1))
	);
	result.second += IPoint(1,1);
	return result;
}

GRect GridCoord::GetClippedRelRect(const IRect& selRect) const 
{ 
	dms_assert(!IsDirty()); 
	GRect result(
		FirstViewCol(selRect.first .Col() ),
		FirstViewRow(selRect.first .Row() ),
		FirstViewCol(selRect.second.Col() ),
		FirstViewRow(selRect.second.Row() )
	);
	if (IsBottomTop(m_Orientation)) omni::swap(result.bottom, result.top ); dms_assert(result.top <= result.bottom); 
	if (IsRightLeft(m_Orientation)) omni::swap(result.right,  result.left); dms_assert(result.left<= result.right ); 
	
	result += m_ClippedRelRect.TopLeft();
	result &= m_ClippedRelRect;

	return result;
} 

IPoint GridCoord::GetGridCoord(const GPoint& clientRelPoint) const
{
	dms_assert(!IsDirty()); 
	dms_assert(!Empty());
	dms_assert(IsIncluding(m_ClippedRelRect, clientRelPoint));
	// END OF PRECONDITIONS

	return 
		shp2dms_order(
			IPoint(
				*GetGridColPtr(clientRelPoint.x, false), 
				*GetGridRowPtr(clientRelPoint.y, false)
			) 
		)	
	+	GetGridRect().first;
}

IPoint GridCoord::GetExtGridCoord(GPoint clientRelPoint) const
{
	dms_assert(!IsDirty()); 
//	if (Empty())
//		return Undefined();

	if (!IsIncluding(m_ClippedRelRect, clientRelPoint))
		return RoundDown<4>(
			m_GridOrigin 
		+	m_GridCellSize * Convert<CrdPoint>(clientRelPoint - m_ClippedRelRect.TopLeft())
		) + GetGridRect().first;

	return GetGridCoord(clientRelPoint);
}

IPoint GridCoord::GetExtGridCoordFromAbs(GPoint clientAbsPoint) const
{	
	auto owner = m_Owner.lock();
	if (owner)
		clientAbsPoint -= TPoint2GPoint(owner->GetCurrClientAbsPos());
	return GetExtGridCoord(clientAbsPoint);
}
