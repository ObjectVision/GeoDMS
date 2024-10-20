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
#include "ShvDllPch.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

#include "SelCaret.h"


#include "dbg/debug.h"

#include "DataArray.h"

#include "DataView.h"
#include "GridCoord.h"
#include "IndexCollector.h"
#include "RegionTower.h"
#include "ViewPort.h"

//----------------------------------------------------------------------
// class  : SelCaret
//----------------------------------------------------------------------

SelCaret::SelCaret(ViewPort* owner,	const sel_caret_key& key, GridCoordPtr gridCoord)
	:	m_Owner(owner->shared_from_base<ViewPort>())
	,	m_EntityIndexCollectorPtr(key.second)
	,	m_EntityIndexCollectorArray(key.second, no_tile)
	,	m_SelAttr(key.first)
	,	m_GridCoords(gridCoord)
	,	m_Ready(false)
{}

SelCaret::~SelCaret()
{
	auto owner = m_Owner.lock(); if (!owner) return;

	ForwardDiff(m_SelCaretRgn);
	owner->m_SelCaretMap.erase(sel_caret_key(m_SelAttr, m_EntityIndexCollectorPtr));
}

std::weak_ptr<ViewPort> SelCaret::GetOwner() const
{
	return m_Owner;
}

void SelCaret::ForwardDiff(const Region& diff)
{
	GetOwner().lock()->GetDataView().lock()->XOrSelCaret(diff);
}

void SelCaret::SetSelCaretRgn(Region&& selCaretRgn)
{
	Region diff = m_SelCaretRgn ^ selCaretRgn;
	if (diff.Empty())
		return;

	m_SelCaretRgn = std::move(selCaretRgn);

	ForwardDiff(diff);
}

void SelCaret::XOrSelCaretRgn(const Region& diff)
{
	m_SelCaretRgn ^= diff;
	ForwardDiff(diff);
}

void SelCaret::OnZoom()
{
	if (!m_Ready)
		return;

	SetSelCaretRgn(Region());
	m_Ready = false;
}

void SelCaret::OnDeviceScroll(const GPoint& delta)
{
	if (!m_Ready)
		return;
	auto owner = m_Owner.lock(); if (!owner) return;

	CrdRect  clipRect = owner->GetCurrClientAbsDeviceRect();
	GRect intClipRect = CrdRect2GRect(clipRect);
	Region clipRgn(intClipRect);

	// Scroll m_SelCaretRgn but don't Set since this is done by DataView::Scroll
	if (m_SelCaretRgn.IsIntersecting(clipRgn))
		m_SelCaretRgn.ScrollDevice(delta, intClipRect, clipRgn);

	// Add scrolled-in stuff to SelCaret
	UpdateRgn(clipRgn - Region(CrdRect2GRect( owner->GetCurrClientAbsDeviceRect()) + delta ));
}

Region SelCaret::UpdateRectImpl(const GRect& updateRect)
{
	dms_assert(m_SelAttr->GetDataRefLockCount() > 0);

	MG_CHECK(IsMetaThread()); // licensed to use statics ?
	static RegionTower allRows, allCols;

	assert(allRows.Empty());
	assert(allCols.Empty());

	// TODO: MT1 support and merge resulting Regions from worker-threads.
	auto selData = const_array_cast<Bool>( m_SelAttr )->GetDataRead(); 

	IPoint gridSize = Size(m_GridCoords->GetGridRect());
	if (gridSize.first <= 0 || gridSize.second <= 0)
		return Region();

	Int32 viewColBegin = updateRect.left;
	Int32 viewColEnd   = updateRect.right;

	Int32 currViewRow  = updateRect.top;
	Int32 viewRowEnd   = updateRect.bottom;

	const UInt32* currGridRowPtr  = m_GridCoords->GetGridRowPtrFromAbs(currViewRow,  false);
	const UInt32* gridColBeginPtr = m_GridCoords->GetGridColPtrFromAbs(viewColBegin, false);
	MG_DEBUGCODE( const UInt32* gridRowBeginPtr = currGridRowPtr; )
	while (currViewRow != viewRowEnd)
	{
		dbg_assert(currViewRow - updateRect.top == currGridRowPtr -gridRowBeginPtr );

		UInt32 currGridRow = *currGridRowPtr;
		UInt32 nextViewRow = currViewRow;
		do {
			++currGridRowPtr;
			++nextViewRow;
		} while (nextViewRow != viewRowEnd && currGridRow == *currGridRowPtr);

		assert(IsDefined(currGridRow));
		if (IsDefined(currGridRow)) // REMOVE if Previous assert holds
		{
			assert(allCols.Empty());
			assert(currGridRow < UInt32(gridSize.Row()) );

			SizeT currGridRowBegin =  SizeT(currGridRow) * gridSize.Col();

			Int32 currViewCol = viewColBegin;
			const UInt32* currGridColPtr = gridColBeginPtr;
			while ( currViewCol != viewColEnd)
			{
				dbg_assert(currViewCol - viewColBegin == currGridColPtr - gridColBeginPtr );

				UInt32 currGridCol = *currGridColPtr;
				UInt32 nextViewCol = currViewCol;
				do {
					++currGridColPtr;
					++nextViewCol;
				} while (nextViewCol != viewColEnd && currGridCol == *currGridColPtr);

				dms_assert(IsDefined(currGridCol));
				if (IsDefined(currGridCol)) // REMOVE if Previous assert holds
				{
					dms_assert( currGridCol < UInt32( gridSize.Col() ) );
					SizeT currGridNr = currGridRowBegin+currGridCol;
					currGridNr = m_EntityIndexCollectorArray.GetEntityIndex(currGridNr);
					if (!IsDefined(currGridNr))
						goto afterSelData;

					if (Bool(selData[currGridNr]))
						allCols.Add( Region( GRect(currViewCol, currViewRow, nextViewCol, nextViewRow) ) );
				}
			afterSelData:
				currViewCol = nextViewCol;
			}

			allRows.Add(allCols.GetResult());
		}
		currViewRow = nextViewRow;
	}
	return allRows.GetResult();
}


void SelCaret::UpdateRgn(const Region& updateRgn)
{
	DBG_START("SelCaret", "UpdateSelCaret", MG_DEBUG_REGION);

	// =========== Get DataReadLocks
	PreparedDataReadLock readLock(m_SelAttr, "SelCaret::UpdateRgn");

	m_GridCoords->UpdateUnscaled();
	Region clippedUpdateRgn = Region( m_GridCoords->GetClippedRelDeviceRect() );
	clippedUpdateRgn &= updateRgn;
	if (clippedUpdateRgn.Empty())
		return;

	assert(IsMainThread()); // licensed to use statics ?

	static RectArray ra;
	clippedUpdateRgn.FillRectArray(ra);

	static RegionTower regionTower;
	RectArray::iterator
		rectPtr = ra.begin(),
		rectEnd = ra.end();

	for (; rectPtr != rectEnd; ++rectPtr)
		regionTower.Add(UpdateRectImpl(*rectPtr));

	Region selCaretRgn = regionTower.GetResult();
	dms_assert((selCaretRgn - clippedUpdateRgn).Empty());

	SetSelCaretRgn(selCaretRgn | (GetSelCaretRgn() - updateRgn)); // replace updateRgn with selCaretRgn, leave rest unaltered
	m_Ready = true;
}

