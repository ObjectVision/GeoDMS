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
	if (m_Orientation == MC_Orientation::Rows)
	{
		auto n = NrEntries();
		auto resSize = shp2dms_order<CrdType>(m_MaxSize, m_SepSize);
		for (decltype(n) i = 0; i != n; ++i)
		{
			MovableObject* entry = GetEntry(i);

			auto extents = entry->GetBorderLogicalExtents();
			auto entryTop = resSize.Y();

			if (entry->IsVisible())
			{
				auto entrySize = entry->GetCurrClientSize() + Size(extents);
				resSize.Y() += entrySize.Y();
				resSize.Y() += m_SepSize;

				if (!m_MaxSize)
					MakeMax(resSize.X(), entrySize.X());
				SetClientSize(UpperBound(GetCurrClientSize(), resSize));
			}
			entry->MoveTo(shp2dms_order<CrdType>(0, entryTop) - TopLeft(extents));
		}

		SetClientSize(resSize);
	}
	else
	{
		gr_elem_index n = NrEntries();
		auto resSize = shp2dms_order<CrdType>(m_SepSize, m_MaxSize);
		for (gr_elem_index i = 0; i != n; ++i)
		{
			MovableObject* entry = GetEntry(i);

			auto extents = entry->GetBorderLogicalExtents();
			auto entryLeft = resSize.X();

			if (entry->IsVisible())
			{
				auto entrySize = entry->GetCurrClientSize() + Size(extents);

				resSize.X() += entrySize.X();
				resSize.X() += m_SepSize;

				if (!m_MaxSize)
					MakeMax(resSize.Y(), entrySize.Y());

				SetClientSize(UpperBound(GetCurrClientSize(), resSize));
			}
			entry->MoveTo(shp2dms_order<CrdType>(entryLeft, 0) - TopLeft(extents));
		}

		SetClientSize(resSize);
	}
	auto clientSize = Point<CrdType>(0, 0);

/* REMOVE
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
REMOVE */
}

void AutoSizeContainer::SetMaxSize(TType maxSize)
{
	if (m_MaxSize == maxSize)
		return;

	m_MaxSize = maxSize;
	ProcessCollectionChange();
}

void AutoSizeContainer::SetSepSize(UInt32 sepSize)
{
	if (m_SepSize == sepSize)
		return;

	m_SepSize = sepSize;
	ProcessCollectionChange();
}

void AutoSizeContainer::ToggleOrientation()
{
	if (m_Orientation == MC_Orientation::Rows)
		m_Orientation = MC_Orientation::Cols;
	else
		m_Orientation = MC_Orientation::Rows;

	ProcessCollectionChange();
}

//----------------------------------------------------------------------
// DrawBackgroun
//----------------------------------------------------------------------

void AutoSizeContainer::DrawBackground(const GraphDrawer& d) const
{
	base_type::DrawBackground(d);
	if (!m_SepSize)
		return;

	if (m_Orientation == MC_Orientation::Rows)
	{
		SizeT n = NrEntries();

		auto  absFullRect = GetClippedCurrFullAbsDeviceRect(d);
		GType clipStartRow = d.GetAbsClipDeviceRect().Top();
		TType clipEndRow = d.GetAbsClipDeviceRect().Bottom();
		SizeT recNo = 0;

		AddClientLogicalOffset localOffset(const_cast<GraphDrawer*>(&d), GetCurrClientRelPos());

		while (recNo < n && GetConstEntry(recNo)->GetCurrFullAbsDeviceRect(d).first.Y() <= clipStartRow)
			++recNo;

		GdiHandle<HBRUSH> br(CreateSolidBrush(DmsColor2COLORREF(0)));
		while (recNo < n)
		{
			absFullRect.second.Y() = GetConstEntry(recNo)->GetCurrFullAbsDeviceRect(d).first.Y();
			absFullRect.first.Y() = absFullRect.second.Y() - m_SepSize * d.GetSubPixelFactors().second;
			assert(absFullRect.second.Y() > clipStartRow); // follows from previous while condition, recNo < n, and the ascending order of Entries
			if (absFullRect.first.Y() >= clipEndRow)
				return;

			auto intFullRect = CrdRect2GRect(absFullRect);
			FillRect(d.GetDC(), &intFullRect, br);
			++recNo;
		}
		assert(recNo == n);
		absFullRect.first.Y() = (recNo)
			? GetConstEntry(--recNo)->GetCurrFullAbsDeviceRect(d).second.Y()
			: 0;
		if (absFullRect.first.Y() >= clipEndRow)
			return;
		absFullRect.second.Y() = absFullRect.first.Y() + m_SepSize * d.GetSubPixelFactors().second;
		if (absFullRect.second.Y() <= clipStartRow)
			return;
		auto  intFullRect = CrdRect2GRect(absFullRect);
		FillRect(d.GetDC(), &intFullRect, br);
	}
	else
	{
		SizeT n = NrEntries();

		auto   absFullRect = GetClippedCurrFullAbsDeviceRect(d);
		GType  clipStartCol = d.GetAbsClipDeviceRect().Left();
		GType  clipEndCol = d.GetAbsClipDeviceRect().Right();
		UInt32 recNo = 0;

		AddClientLogicalOffset localOffset(const_cast<GraphDrawer*>(&d), GetCurrClientRelPos());

		while (recNo < n && GetConstEntry(recNo)->GetCurrFullAbsDeviceRect(d).first.X() <= clipStartCol)
			++recNo;

		GdiHandle<HBRUSH> br(CreateSolidBrush(DmsColor2COLORREF(0)));
		while (recNo < n)
		{
			absFullRect.second.X() = GetConstEntry(recNo)->GetCurrFullAbsDeviceRect(d).first.X();
			absFullRect.first.X() = absFullRect.second.X() - m_SepSize * d.GetSubPixelFactors().first;
			assert(absFullRect.second.X() > clipStartCol); // follows from previous while condition, recNo < n, and the ascending order of Entries
			if (absFullRect.first.X() >= clipEndCol)
				return;

			auto intFullRect = CrdRect2GRect(absFullRect);
			FillRect(d.GetDC(), &intFullRect, br);
			++recNo;
		}
		dms_assert(recNo == n);
		absFullRect.first.X() = (recNo)
			? GetConstEntry(--recNo)->GetCurrFullAbsDeviceRect(d).second.X()
			: 0;
		if (absFullRect.first.X() >= clipEndCol)
			return;
		absFullRect.second.X() = absFullRect.first.X() + m_SepSize;
		if (absFullRect.second.X() <= clipStartCol)
			return;

		auto intFullRect = CrdRect2GRect(absFullRect);
		FillRect(d.GetDC(), &intFullRect, br);
	}
}


//----------------------------------------------------------------------
// section : Grow
//----------------------------------------------------------------------

void AutoSizeContainer::GrowHor(CrdType deltaX, CrdType relPosX, const MovableObject* sourceItem)
{
	if (m_Orientation == MC_Orientation::Rows)
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
			SizeT n = NrEntries();
			while (n)
				MakeMax(width, GetEntry(--n)->GetCurrFullSize().X());
			auto currWidth = m_ClientLogicalSize.X();
			assert(width <= currWidth);
			if (width < currWidth)
				base_type::GrowHor(width - currWidth, currWidth);
		}
	}
	else
	{
		assert(relPosX <= m_ClientLogicalSize.X());
		if (deltaX > 0)
			base_type::GrowHor(deltaX, relPosX);

		SizeT n = NrEntries();
		while (n)
		{
			MovableObject* subItem = GetEntry(--n);
			if (subItem->m_RelPos.X() < relPosX || subItem == sourceItem)
				break;
			subItem->m_RelPos.X() += deltaX;
		}

		if (deltaX < 0)
			base_type::GrowHor(deltaX, relPosX);

		MG_DEBUGCODE(CheckSubStates());

		m_cmdElemSetChanged();
	}
}

void AutoSizeContainer::GrowVer(CrdType deltaY, CrdType relPosY, const MovableObject* sourceItem)
{
	if (m_Orientation == MC_Orientation::Rows)
	{
		assert(relPosY <= m_ClientLogicalSize.Y());
		if (deltaY > 0)
			base_type::GrowVer(deltaY, relPosY);

		SizeT n = NrEntries();
		while (n)
		{
			MovableObject* subItem = GetEntry(--n);
			if (subItem->m_RelPos.Y() < relPosY || subItem == sourceItem)
				break;
			subItem->m_RelPos.Y() += deltaY;
		}

		if (deltaY < 0)
			base_type::GrowVer(deltaY, relPosY);

		MG_DEBUGCODE(CheckSubStates());

		m_cmdElemSetChanged();
	}
	else
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
			SizeT n = NrEntries();
			while (n)
				MakeMax(height, GetEntry(--n)->GetCurrFullSize().Y());
			auto currHeight = m_ClientLogicalSize.Y();
			assert(height <= currHeight);
			if (height < currHeight)
				base_type::GrowVer(height - currHeight, currHeight);
		}
	}
}
