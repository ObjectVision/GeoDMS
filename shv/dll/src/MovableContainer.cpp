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

#include "MovableContainer.h"

#include "dbg/DebugCast.h"
#include "geo/PointOrder.h"
#include "mci/Class.h"
#include "utl/swap.h"

#include "TreeItem.h"
#include "TreeItemClass.h"

#include "GraphVisitor.h"
#include "Wrapper.h"
#include "ShvUtils.h"

//----------------------------------------------------------------------
// class  : MovableContainer
//----------------------------------------------------------------------

GraphVisitState MovableContainer::InviteGraphVistor(AbstrVisitor& v)
{
	return v.DoMovableContainer(this);
}


//----------------------------------------------------------------------
// class  : AutoSizeContainer
//----------------------------------------------------------------------

void AutoSizeContainer::ProcessCollectionChange()
{
	TPoint clientSize(0, 0);
	// calculate Size
	gr_elem_index n = NrEntries();
	while (n)
	{
		MovableObject* entry = GetEntry(--n);
		if (entry && entry->IsVisible())
		{
			TPoint entrySize = entry->GetCurrClientSize();
			dms_assert(entry->GetCurrClientRelPos().x() + entry->GetBorderPixelExtents().Left() >= 0);
			dms_assert(entry->GetCurrClientRelPos().y() + entry->GetBorderPixelExtents().Top () >= 0);

			MakeUpperBound( clientSize, 
				entrySize + entry->GetCurrClientRelPos() +  TPoint(entry->GetBorderPixelExtents().BottomRight())
			);
		}
	}
	SetClientSize(clientSize);
	base_type::ProcessCollectionChange();
}

//----------------------------------------------------------------------
// class  : GraphicVarRows
//----------------------------------------------------------------------

GraphicVarRows::GraphicVarRows(MovableObject* owner)
	:	base_type(owner)
	,	m_MaxColWidth(0)
	,	m_RowSepHeight(2)
{
}

void GraphicVarRows::ProcessCollectionChange()
{
	SizeT n = NrEntries();
	TPoint resSize(m_MaxColWidth, m_RowSepHeight);
	for (UInt32 i = 0; i!=n; ++i)
	{
		MovableObject* entry = GetEntry(i);

		GRect pixelExtents = entry->GetBorderPixelExtents();
		TType entryTop     = resSize.y();

		if (entry->IsVisible()) 
		{
			TPoint entrySize = entry->GetCurrClientSize() + TPoint(pixelExtents.Size());
			resSize.y() += entrySize.y();
			resSize.y() += m_RowSepHeight;

			if (!m_MaxColWidth)
				MakeMax(resSize.x(), entrySize.x());
			SetClientSize( UpperBound(GetCurrClientSize(), resSize) );
		}
		entry->MoveTo( TPoint(0, entryTop) - TPoint(pixelExtents.TopLeft()) );
	}

	SetClientSize( resSize );
	base_type::base_type::ProcessCollectionChange();
}

void GraphicVarRows::DrawBackground(const GraphDrawer& d) const
{
	base_type::DrawBackground(d);

	TType rowSep = RowSepHeight();
	if (!rowSep)
		return;

	SizeT n = NrEntries(); 

	GRect  absFullRect  = GetClippedCurrFullAbsRect(d);
	GType  clipStartRow = d.GetAbsClipRect().Top   ();
	TType  clipEndRow   = d.GetAbsClipRect().Bottom();
	SizeT  recNo        = 0;

	AddClientOffset localOffset(const_cast<GraphDrawer*>(&d), GetCurrClientRelPos());

	while (recNo < n && GetConstEntry(recNo)->GetCurrFullAbsRect(d).Top() <= clipStartRow)
		++recNo;

	GdiHandle<HBRUSH> br( CreateSolidBrush( DmsColor2COLORREF(0) ) );
	while (recNo < n)
	{
		absFullRect.Top   () = GetConstEntry(recNo)->GetCurrFullAbsRect(d).Top();
		absFullRect.Bottom() = absFullRect.Bottom() - rowSep;
		dms_assert(absFullRect.Bottom() > clipStartRow); // follows from previous while condition, recNo < n, and the ascending order of Entries
		if (absFullRect.Top() >= clipEndRow)
			return;

		FillRect(d.GetDC(), &absFullRect, br);
		++recNo;
	}
	dms_assert(recNo == n);
	absFullRect.Top() = (recNo)
		?	GetConstEntry(--recNo)->GetCurrFullAbsRect(d).Bottom()
		:	0;
	if (absFullRect.Top() >= clipEndRow)
		return;
	absFullRect.Bottom() = absFullRect.Top() + rowSep;
	if (absFullRect.Bottom() > clipStartRow)
		FillRect(d.GetDC(), &absFullRect, br);
}


GraphVisitState GraphicVarRows::InviteGraphVistor(class AbstrVisitor& gv)
{
	return gv.DoVarRows(this); 
}

void GraphicVarRows::SetMaxColWidth(TType maxColWidth)
{
	if (m_MaxColWidth == maxColWidth)
		return;

	m_MaxColWidth = maxColWidth;
	ProcessCollectionChange();
}

void GraphicVarRows::SetRowSepHeight(UInt32 rowSepHeight)
{
	if (m_RowSepHeight == rowSepHeight)
		return;

	m_RowSepHeight = rowSepHeight;
	ProcessCollectionChange();
}

//----------------------------------------------------------------------
// class  : GraphicVarCols
//----------------------------------------------------------------------

GraphicVarCols::GraphicVarCols(MovableObject* owner)
	:	base_type(owner)
	,	m_MaxRowHeight(0)
	,	m_ColSepWidth(1)
{
}

void GraphicVarCols::ProcessCollectionChange()
{
	gr_elem_index n = NrEntries();
	TPoint resSize(m_ColSepWidth, m_MaxRowHeight);
	for (gr_elem_index i = 0; i!=n; ++i)
	{
		MovableObject* entry = GetEntry(i);

		GRect pixelExtents = entry->GetBorderPixelExtents();
		TType entryLeft    = resSize.x();

		if (entry->IsVisible()) 
		{
			TPoint entrySize = entry->GetCurrClientSize() + TPoint(pixelExtents.Size());

			resSize.x() += entrySize.x();
			resSize.x() += m_ColSepWidth;

			if (!m_MaxRowHeight)
				MakeMax(resSize.y(), entrySize.y());

			SetClientSize(UpperBound(GetCurrClientSize(), resSize));
		}
		entry->MoveTo( TPoint(entryLeft, 0) - TPoint(pixelExtents.TopLeft()) );
	}

	SetClientSize( resSize );
	base_type::base_type::ProcessCollectionChange(); // skip the AutoSizeContainer::ProcessCollectionChange();
}

void GraphicVarCols::DrawBackground(const GraphDrawer& d) const
{
	base_type::DrawBackground(d);

	TType colSep = ColSepWidth();
	if (!colSep)
		return;

	SizeT n = NrEntries(); 

	GRect  absFullRect  = GetClippedCurrFullAbsRect(d);
	GType  clipStartCol = d.GetAbsClipRect().Left ();
	GType  clipEndCol   = d.GetAbsClipRect().Right();
	UInt32 recNo        = 0;

	AddClientOffset localOffset(const_cast<GraphDrawer*>(&d), GetCurrClientRelPos());

	while (recNo < n && GetConstEntry(recNo)->GetCurrFullAbsRect(d).Left() <= clipStartCol)
		++recNo;

	GdiHandle<HBRUSH> br( CreateSolidBrush( DmsColor2COLORREF(0) ) );
	while (recNo < n)
	{
		absFullRect.Right() = GetConstEntry(recNo)->GetCurrFullAbsRect(d).Left();
		absFullRect.Left () = absFullRect.Right() - colSep;
		dms_assert(absFullRect.Right() > clipStartCol); // follows from previous while condition, recNo < n, and the ascending order of Entries
		if (absFullRect.Left () >= clipEndCol)
			return;

		FillRect(d.GetDC(), &absFullRect, br);
		++recNo;
	}
	dms_assert(recNo == n);
	absFullRect.Left() = (recNo)
		?	GetConstEntry(--recNo)->GetCurrFullAbsRect(d).Right()
		:	0;
	if (absFullRect.Left() >= clipEndCol)
		return;
	absFullRect.Right() = absFullRect.Left () + colSep;
	if (absFullRect.Right() > clipStartCol)
		FillRect(d.GetDC(), &absFullRect, br);
}

GraphVisitState GraphicVarCols::InviteGraphVistor(class AbstrVisitor& gv)
{
	return gv.DoVarCols(this); 
}

void GraphicVarCols::SetMaxRowHeight(TType maxRowHeight)
{
	if (m_MaxRowHeight == maxRowHeight)
		return;

	m_MaxRowHeight = maxRowHeight;
	ProcessCollectionChange();
}

void GraphicVarCols::SetColSepWidth(UInt32 colSepWidth)
{
	if (m_ColSepWidth == colSepWidth)
		return;

	m_ColSepWidth = colSepWidth;
	ProcessCollectionChange();
}

//----------------------------------------------------------------------
// section : Grow
//----------------------------------------------------------------------

void GraphicVarCols::GrowHor(TType deltaX, TType relPosX, const MovableObject* sourceItem)
{
	dms_assert(relPosX <= m_ClientSize.x());
	if (deltaX > 0)
		base_type::GrowHor(deltaX, relPosX, 0);

	SizeT n = NrEntries(); 
	while(n)
	{
		MovableObject* subItem = GetEntry(--n);
		if (subItem->m_RelPos.x() < relPosX || subItem == sourceItem)
			break;
		subItem->m_RelPos.x() += deltaX;
	}

	if (deltaX < 0)
		base_type::GrowHor(deltaX, relPosX, 0);

	MG_DEBUGCODE( CheckSubStates() );

	m_cmdElemSetChanged();
}

void GraphicVarRows::GrowVer(TType deltaY, TType relPosY, const MovableObject* sourceItem)
{
	dms_assert(relPosY <= m_ClientSize.y());
	if (deltaY > 0)
		base_type::GrowVer(deltaY, relPosY, 0);

	SizeT n = NrEntries(); 
	while(n)
	{
		MovableObject* subItem = GetEntry(--n);
		if (subItem->m_RelPos.y() < relPosY || subItem == sourceItem)
			break;
		subItem->m_RelPos.y() += deltaY;
	}

	if (deltaY < 0)
		base_type::GrowVer(deltaY, relPosY, 0);

	MG_DEBUGCODE( CheckSubStates() );

	m_cmdElemSetChanged();
}

void GraphicVarCols::GrowVer(TType deltaY, TType relPosY, const MovableObject* sourceItem)
{
	if (deltaY > 0)
	{
		TType excess = relPosY + deltaY - m_ClientSize.y();
		if (excess > 0)
			base_type::GrowVer(excess, m_ClientSize.y(), 0);
	}
	if (deltaY < 0)
	{
		// recalculate max elem width
		TType height = 0;
		SizeT n=NrEntries();
		while (n) 
			MakeMax(height, GetEntry(--n)->GetCurrFullSize().y());
		dms_assert(height <= m_ClientSize.y());
		if (height < m_ClientSize.y())
			base_type::GrowVer(height - m_ClientSize.y(), m_ClientSize.y(), 0);
	}
}

void GraphicVarRows::GrowHor(TType deltaX, TType relPosX, const MovableObject* sourceItem)
{
	dms_assert(relPosX <= m_ClientSize.x());
	if (deltaX > 0)
	{
		TType excess = relPosX + deltaX - m_ClientSize.x();
		if (excess > 0)
			base_type::GrowHor(excess, m_ClientSize.x(), 0);
	}
	if (deltaX < 0)
	{
		// recalculate max elem width
		TType width = 0;
		SizeT n=NrEntries();
		while (n) 
			MakeMax(width, GetEntry(--n)->GetCurrFullSize().x());
		dms_assert(width <= m_ClientSize.x());
		if (width < m_ClientSize.x())
			base_type::GrowHor(width - m_ClientSize.x(), m_ClientSize.x(), 0);
	}
}

