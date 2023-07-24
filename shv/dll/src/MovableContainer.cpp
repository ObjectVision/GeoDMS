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
	TPoint clientSize = Point<TType>(0, 0);
	// calculate Size
	auto n = NrEntries();
	while (n)
	{
		MovableObject* entry = GetEntry(--n);
		if (entry && entry->IsVisible())
		{
			TPoint entrySize = entry->GetCurrClientSize();
			assert(entry->GetCurrClientRelPos().X() + entry->GetBorderLogicalExtents().Left() >= 0);
			assert(entry->GetCurrClientRelPos().Y() + entry->GetBorderLogicalExtents().Top () >= 0);

			MakeUpperBound( clientSize
			,	entrySize + entry->GetCurrClientRelPos() + TPoint(entry->GetBorderLogicalExtents().BottomRight())
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
	auto n = NrEntries();
	TPoint resSize = shp2dms_order<TType>(m_MaxColWidth, m_RowSepHeight);
	for (decltype(n) i = 0; i!=n; ++i)
	{
		MovableObject* entry = GetEntry(i);

		TRect extents  = entry->GetBorderLogicalExtents();
		TType entryTop = resSize.Y();

		if (entry->IsVisible()) 
		{
			TPoint entrySize = entry->GetCurrClientSize() + extents.Size();
			resSize.Y() += entrySize.Y();
			resSize.Y() += m_RowSepHeight;

			if (!m_MaxColWidth)
				MakeMax(resSize.X(), entrySize.X());
			SetClientSize( UpperBound(GetCurrClientSize(), resSize) );
		}
		entry->MoveTo(shp2dms_order<TType>(0, entryTop) - extents.TopLeft() );
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

	GRect  absFullRect  = GetClippedCurrFullAbsDeviceRect(d);
	GType  clipStartRow = d.GetAbsClipDeviceRect().Top   ();
	TType  clipEndRow   = d.GetAbsClipDeviceRect().Bottom();
	SizeT  recNo        = 0;

	AddClientLogicalOffset localOffset(const_cast<GraphDrawer*>(&d), GetCurrClientRelPos());

	while (recNo < n && GetConstEntry(recNo)->GetCurrFullAbsDeviceRect(d).Top() <= clipStartRow)
		++recNo;

	GdiHandle<HBRUSH> br( CreateSolidBrush( DmsColor2COLORREF(0) ) );
	while (recNo < n)
	{
		absFullRect.Top   () = GetConstEntry(recNo)->GetCurrFullAbsDeviceRect(d).Top();
		absFullRect.Bottom() = absFullRect.Bottom() - rowSep;
		dms_assert(absFullRect.Bottom() > clipStartRow); // follows from previous while condition, recNo < n, and the ascending order of Entries
		if (absFullRect.Top() >= clipEndRow)
			return;

		FillRect(d.GetDC(), &absFullRect, br);
		++recNo;
	}
	dms_assert(recNo == n);
	absFullRect.Top() = (recNo)
		?	GetConstEntry(--recNo)->GetCurrFullAbsDeviceRect(d).Bottom()
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
	TPoint resSize = shp2dms_order<TType>(m_ColSepWidth, m_MaxRowHeight);
	for (gr_elem_index i = 0; i!=n; ++i)
	{
		MovableObject* entry = GetEntry(i);

		TRect extents = entry->GetBorderLogicalExtents();
		TType entryLeft    = resSize.X();

		if (entry->IsVisible()) 
		{
			TPoint entrySize = entry->GetCurrClientSize() + extents.Size();

			resSize.X() += entrySize.X();
			resSize.X() += m_ColSepWidth;

			if (!m_MaxRowHeight)
				MakeMax(resSize.Y(), entrySize.Y());

			SetClientSize(UpperBound(GetCurrClientSize(), resSize));
		}
		entry->MoveTo(shp2dms_order<TType>(entryLeft, 0) - extents.TopLeft() );
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

	GRect  absFullRect  = GetClippedCurrFullAbsDeviceRect(d);
	GType  clipStartCol = d.GetAbsClipDeviceRect().Left ();
	GType  clipEndCol   = d.GetAbsClipDeviceRect().Right();
	UInt32 recNo        = 0;

	AddClientLogicalOffset localOffset(const_cast<GraphDrawer*>(&d), GetCurrClientRelPos());

	while (recNo < n && GetConstEntry(recNo)->GetCurrFullAbsDeviceRect(d).Left() <= clipStartCol)
		++recNo;

	GdiHandle<HBRUSH> br( CreateSolidBrush( DmsColor2COLORREF(0) ) );
	while (recNo < n)
	{
		absFullRect.Right() = GetConstEntry(recNo)->GetCurrFullAbsDeviceRect(d).Left();
		absFullRect.Left () = absFullRect.Right() - colSep;
		dms_assert(absFullRect.Right() > clipStartCol); // follows from previous while condition, recNo < n, and the ascending order of Entries
		if (absFullRect.Left () >= clipEndCol)
			return;

		FillRect(d.GetDC(), &absFullRect, br);
		++recNo;
	}
	dms_assert(recNo == n);
	absFullRect.Left() = (recNo)
		?	GetConstEntry(--recNo)->GetCurrFullAbsDeviceRect(d).Right()
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
	assert(relPosX <= m_ClientLogicalSize.X());
	if (deltaX > 0)
		base_type::GrowHor(deltaX, relPosX);

	SizeT n = NrEntries(); 
	while(n)
	{
		MovableObject* subItem = GetEntry(--n);
		if (subItem->m_RelPos.X() < relPosX || subItem == sourceItem)
			break;
		subItem->m_RelPos.X() += deltaX;
	}

	if (deltaX < 0)
		base_type::GrowHor(deltaX, relPosX);

	MG_DEBUGCODE( CheckSubStates() );

	m_cmdElemSetChanged();
}

void GraphicVarRows::GrowVer(TType deltaY, TType relPosY, const MovableObject* sourceItem)
{
	assert(relPosY <= m_ClientLogicalSize.Y());
	if (deltaY > 0)
		base_type::GrowVer(deltaY, relPosY);

	SizeT n = NrEntries(); 
	while(n)
	{
		MovableObject* subItem = GetEntry(--n);
		if (subItem->m_RelPos.Y() < relPosY || subItem == sourceItem)
			break;
		subItem->m_RelPos.Y() += deltaY;
	}

	if (deltaY < 0)
		base_type::GrowVer(deltaY, relPosY);

	MG_DEBUGCODE( CheckSubStates() );

	m_cmdElemSetChanged();
}

void GraphicVarCols::GrowVer(TType deltaY, TType relPosY, const MovableObject* sourceItem)
{
	if (deltaY > 0)
	{
		TType excess = relPosY + deltaY - m_ClientLogicalSize.Y();
		if (excess > 0)
			base_type::GrowVer(excess, m_ClientLogicalSize.Y());
	}
	if (deltaY < 0)
	{
		// recalculate max elem width
		TType height = 0;
		SizeT n=NrEntries();
		while (n)
			MakeMax(height, GetEntry(--n)->GetCurrFullSize().Y());
		auto currHeight = m_ClientLogicalSize.Y();
		assert(height <= currHeight);
		if (height < currHeight)
			base_type::GrowVer(height - currHeight, currHeight);
	}
}

void GraphicVarRows::GrowHor(TType deltaX, TType relPosX, const MovableObject* sourceItem)
{
	assert(relPosX <= m_ClientLogicalSize.X());
	if (deltaX > 0)
	{
		TType excess = relPosX + deltaX - m_ClientLogicalSize.X();
		if (excess > 0)
			base_type::GrowHor(excess, m_ClientLogicalSize.X());
	}
	if (deltaX < 0)
	{
		// recalculate max elem width
		TType width = 0;
		SizeT n=NrEntries();
		while (n) 
			MakeMax(width, GetEntry(--n)->GetCurrFullSize().X());
		auto currWidth = m_ClientLogicalSize.X();
		assert(width <= currWidth);
		if (width < currWidth)
			base_type::GrowHor(width - currWidth, currWidth);
	}
}
