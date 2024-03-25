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
	auto clientSize = Point<CrdType>(0, 0);
	// calculate Size
	auto n = NrEntries();
	while (n)
	{
		MovableObject* entry = GetEntry(--n);
		if (entry && entry->IsVisible())
		{
			auto entrySize = entry->GetCurrClientSize();
			assert(entry->GetCurrClientRelPos().X() + Left(entry->GetBorderLogicalExtents()) >= 0);
			assert(entry->GetCurrClientRelPos().Y() + Top (entry->GetBorderLogicalExtents()) >= 0);

			entrySize += entry->GetCurrClientRelPos() + BottomRight(entry->GetBorderLogicalExtents());
			MakeUpperBound( clientSize,	entrySize);
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
{
}

void GraphicVarRows::ProcessCollectionChange()
{
	auto n = NrEntries();
	auto resSize = shp2dms_order<CrdType>(m_MaxColWidth, m_RowSepHeight);
	for (decltype(n) i = 0; i!=n; ++i)
	{
		MovableObject* entry = GetEntry(i);

		auto extents  = entry->GetBorderLogicalExtents();
		auto entryTop = resSize.Y();

		if (entry->IsVisible()) 
		{
			auto entrySize = entry->GetCurrClientSize() + Size(extents);
			resSize.Y() += entrySize.Y();
			resSize.Y() += m_RowSepHeight;

			if (!m_MaxColWidth)
				MakeMax(resSize.X(), entrySize.X());
			SetClientSize( UpperBound(GetCurrClientSize(), resSize) );
		}
		entry->MoveTo(shp2dms_order<CrdType>(0, entryTop) - TopLeft(extents) );
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

	auto  absFullRect  = GetClippedCurrFullAbsDeviceRect(d);
	GType clipStartRow = d.GetAbsClipDeviceRect().Top   ();
	TType clipEndRow   = d.GetAbsClipDeviceRect().Bottom();
	SizeT recNo        = 0;

	AddClientLogicalOffset localOffset(const_cast<GraphDrawer*>(&d), GetCurrClientRelPos());

	while (recNo < n && GetConstEntry(recNo)->GetCurrFullAbsDeviceRect(d).first.Y() <= clipStartRow)
		++recNo;

	GdiHandle<HBRUSH> br( CreateSolidBrush( DmsColor2COLORREF(0) ) );
	while (recNo < n)
	{
		absFullRect.second .Y() = GetConstEntry(recNo)->GetCurrFullAbsDeviceRect(d).first.Y();
		absFullRect.first.Y() = absFullRect.second.Y() - rowSep * d.GetSubPixelFactors().second;
		assert(absFullRect.second.Y() > clipStartRow); // follows from previous while condition, recNo < n, and the ascending order of Entries
		if (absFullRect.first.Y() >= clipEndRow)
			return;

		auto intFullRect = CrdRect2GRect(absFullRect);
		FillRect(d.GetDC(), &intFullRect, br);
		++recNo;
	}
	assert(recNo == n);
	absFullRect.first.Y() = (recNo)
		?	GetConstEntry(--recNo)->GetCurrFullAbsDeviceRect(d).second.Y()
		:	0;
	if (absFullRect.first.Y() >= clipEndRow)
		return;
	absFullRect.second.Y() = absFullRect.first.Y() + rowSep * d.GetSubPixelFactors().second;
	if (absFullRect.second.Y() <= clipStartRow)
		return;
	auto  intFullRect = CrdRect2GRect(absFullRect);
	FillRect(d.GetDC(), &intFullRect, br);
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
{
}

void GraphicVarCols::ProcessCollectionChange()
{
	gr_elem_index n = NrEntries();
	auto resSize = shp2dms_order<CrdType>(m_ColSepWidth, m_MaxRowHeight);
	for (gr_elem_index i = 0; i!=n; ++i)
	{
		MovableObject* entry = GetEntry(i);

		auto extents   = entry->GetBorderLogicalExtents();
		auto entryLeft = resSize.X();

		if (entry->IsVisible()) 
		{
			auto entrySize = entry->GetCurrClientSize() + Size(extents);

			resSize.X() += entrySize.X();
			resSize.X() += m_ColSepWidth;

			if (!m_MaxRowHeight)
				MakeMax(resSize.Y(), entrySize.Y());

			SetClientSize(UpperBound(GetCurrClientSize(), resSize));
		}
		entry->MoveTo(shp2dms_order<CrdType>(entryLeft, 0) - TopLeft(extents) );
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

	auto   absFullRect  = GetClippedCurrFullAbsDeviceRect(d);
	GType  clipStartCol = d.GetAbsClipDeviceRect().Left ();
	GType  clipEndCol   = d.GetAbsClipDeviceRect().Right();
	UInt32 recNo        = 0;

	AddClientLogicalOffset localOffset(const_cast<GraphDrawer*>(&d), GetCurrClientRelPos());

	while (recNo < n && GetConstEntry(recNo)->GetCurrFullAbsDeviceRect(d).first.X() <= clipStartCol)
		++recNo;

	GdiHandle<HBRUSH> br( CreateSolidBrush( DmsColor2COLORREF(0) ) );
	while (recNo < n)
	{
		absFullRect.second.X() = GetConstEntry(recNo)->GetCurrFullAbsDeviceRect(d).first.X();
		absFullRect.first.X() = absFullRect.second.X() - colSep * d.GetSubPixelFactors().first;
		assert(absFullRect.second.X() > clipStartCol); // follows from previous while condition, recNo < n, and the ascending order of Entries
		if (absFullRect.first.X() >= clipEndCol)
			return;

		auto intFullRect = CrdRect2GRect(absFullRect);
		FillRect(d.GetDC(), &intFullRect, br);
		++recNo;
	}
	dms_assert(recNo == n);
	absFullRect.first.X() = (recNo)
		?	GetConstEntry(--recNo)->GetCurrFullAbsDeviceRect(d).second.X()
		:	0;
	if (absFullRect.first.X() >= clipEndCol)
		return;
	absFullRect.second.X() = absFullRect.first.X() + colSep;
	if (absFullRect.second.X() <= clipStartCol)
		return;

	auto intFullRect = CrdRect2GRect(absFullRect);
	FillRect(d.GetDC(), &intFullRect, br);
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

void GraphicVarCols::GrowHor(CrdType deltaX, CrdType relPosX, const MovableObject* sourceItem)
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

void GraphicVarRows::GrowVer(CrdType deltaY, CrdType relPosY, const MovableObject* sourceItem)
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

void GraphicVarCols::GrowVer(CrdType deltaY, CrdType relPosY, const MovableObject* sourceItem)
{
	if (deltaY > 0)
	{
		auto excess = relPosY + deltaY - m_ClientLogicalSize.Y();
		if (excess > 0)
			base_type::GrowVer(excess, m_ClientLogicalSize.Y());
	}
	if (deltaY < 0)
	{
		// recalculate max elem width
		auto height = 0;
		SizeT n=NrEntries();
		while (n)
			MakeMax(height, GetEntry(--n)->GetCurrFullSize().Y());
		auto currHeight = m_ClientLogicalSize.Y();
		assert(height <= currHeight);
		if (height < currHeight)
			base_type::GrowVer(height - currHeight, currHeight);
	}
}

void GraphicVarRows::GrowHor(CrdType deltaX, CrdType relPosX, const MovableObject* sourceItem)
{
	assert(relPosX <= m_ClientLogicalSize.X());
	if (deltaX > 0)
	{
		auto excess = relPosX + deltaX - m_ClientLogicalSize.X();
		if (excess > 0)
			base_type::GrowHor(excess, m_ClientLogicalSize.X());
	}
	if (deltaX < 0)
	{
		// recalculate max elem width
		auto width = 0;
		SizeT n=NrEntries();
		while (n) 
			MakeMax(width, GetEntry(--n)->GetCurrFullSize().X());
		auto currWidth = m_ClientLogicalSize.X();
		assert(width <= currWidth);
		if (width < currWidth)
			base_type::GrowHor(width - currWidth, currWidth);
	}
}
