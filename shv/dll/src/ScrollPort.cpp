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

#include "ScrollPort.h"

#include "dbg/debug.h"
#include "dbg/DebugCast.h"
#include "geo/Conversions.h"
#include "geo/PointOrder.h"
#include "mci/Class.h"
#include "ser/RangeStream.h"
#include "utl/IncrementalLock.h"
#include "utl/mySPrintF.h"

#include "ShvUtils.h"

#include "AbstrCmd.h"
#include "DataView.h"
#include "GraphVisitor.h"
#include "IdleTimer.h"
#include "KeyFlags.h"
#include "MouseEventDispatcher.h"

//----------------------------------------------------------------------
// class  : ScrollPort
//----------------------------------------------------------------------

ScrollPort::ScrollPort(MovableObject* owner, DataView* dv, CharPtr caption, bool disableScrollbars)
	:	Wrapper(owner, dv, caption)
	,	m_NrLogicalUnitsPerTumpnailTick(1, 1)
#if defined(MG_DEBUG)
	,	md_ScrollRecursionCount(0)
#endif

{
	if (disableScrollbars)
		m_State.Set(SPF_NoScrollBars);
}

void ScrollPort::ScrollHome()
{
	// up is positive, 
	ScrollLogicalTo (Point<CrdType>(0, 0) );
}

void ScrollPort::ScrollEnd()
{
	// down = negative
	auto topLeft = shp2dms_order<CrdType>( 0, GetCurrClientSize().Y() - GetContents()->CalcClientSize().Y() );

	ScrollLogicalTo( topLeft );
}

CrdType CalcDelta(Range<CrdType> obj, Range<CrdType> window, TType borderSize)
{
	CrdType d1 = window.first  - obj.first;
	CrdType d2 = window.second - obj.second;
	if (d1>0 && d2>0) return Min<CrdType>(d1 + borderSize, d2);
	if (d1<0 && d2<0) return Max<CrdType>(d1, d2 - borderSize);
	return 0;
}


void ScrollPort::MakeLogicalRectVisible(CrdRect rect, TPoint border)
{
#if defined(MG_DEBUG)
	auto contentsClientRect = GetContents()->GetCurrClientRelLogicalRect();
	assert( IsIncluding(contentsClientRect, rect));
#endif

	auto scrollRect = GetCurrNettRelLogicalRect();
	rect += scrollRect.first;

	auto delta = shp2dms_order<CrdType>(
		CalcDelta(Range<CrdType>(rect.first.X(), rect.second.X()), Range<CrdType>(scrollRect.first.X(), scrollRect.second.X()), border.X()),
		CalcDelta(Range<CrdType>(rect.first.Y(), rect.second.Y()), Range<CrdType>(scrollRect.first.Y(), scrollRect.second.Y()), border.Y())
	);

	ScrollLogical(delta);
}

void ScrollPort::Export()
{
	auto dv = GetDataView().lock(); if (!dv) return;
	dbg_assert( DataViewOK() );
	GetContents()->CopyToClipboard( dv.get() );
}

void ScrollPort::DoUpdateView()
{
	base_type::DoUpdateView();

	CalcNettSize();
	ScrollLogical(Point<TType>(0, 0));
}

const UInt32 SCROLLBAR_WIDTH = 16;

void ScrollPort::CalcNettSize()
{
	auto contentSize = GetContents()->CalcClientSize();

	auto newNettSize = GetCurrClientSize();
	bool horScroll = false, verScroll = false;
	if (!m_State.Get(SPF_NoScrollBars))
	{
		horScroll = newNettSize.X() < contentSize.X(); 
		if (horScroll)
			newNettSize.Y() -= SCROLLBAR_WIDTH;
		verScroll = newNettSize.Y() < contentSize.Y(); 
		if (verScroll)
		{
			newNettSize.X() -= SCROLLBAR_WIDTH;
			if (!horScroll && newNettSize.X() < contentSize.X())
			{
				horScroll = true;
				newNettSize.Y() -= SCROLLBAR_WIDTH;
			}
		}
	}
	MakeUpperBound(newNettSize, Point<CrdType>(0, 0));

	auto sf = GetScaleFactors();
	auto smallestSize = LowerBound(m_NettSize, newNettSize);
	if (GetContents()->IsDrawn())
		GetContents()->ResizeDrawnRect(
			GetDrawnNettAbsDeviceRect(),
			CrdPoint2GPoint(ScaleCrdPoint(smallestSize - m_NettSize, sf)),
			CrdPoint2GPoint(ScaleCrdPoint(GetCurrClientAbsLogicalPos() + m_NettSize, sf))
		);

	omni::swap(m_NettSize, newNettSize);

	bool hasScroll = m_HorScroll && m_VerScroll;
	SetScrollX(horScroll);
	SetScrollY(verScroll);
	
	// Resize from curr size, which bas set by previous IsDrawn m_NettSize
	if (GetContents()->IsDrawn())
		GetContents()->ResizeDrawnRect(
			GetDrawnNettAbsDeviceRect(),
			CrdPoint2GPoint(ScaleCrdPoint(m_NettSize - smallestSize, sf)),
			CrdPoint2GPoint(ScaleCrdPoint(GetCurrClientAbsLogicalPos() + smallestSize, sf))
		);

	if (!(newNettSize == m_NettSize))
	{
		if (horScroll && verScroll && !hasScroll)
			InvalidateClientRect(CrdRect(m_NettSize, GetCurrClientSize()));

		m_cmdOnScrolled();
	}
}

void ScrollPort::GrowHor(CrdType deltaX, CrdType relPosX, const MovableObject* sourceItem)
{
	if (!sourceItem)
	{
		assert(relPosX <= GetCurrClientSize().X());
		MakeMin(relPosX, m_NettSize.X());

		if (deltaX > 0)
			base_type::GrowHor(deltaX, relPosX, 0);                  // maakt ruimte buiten groter

		m_NettSize.X() += deltaX;                                      // maakt DrawnNettRect kleiner
		MakeMax(m_NettSize.X(), CrdType());
		SetScrollBars();

		if (deltaX < 0)
			base_type::GrowHor(deltaX, relPosX, 0);                  // maakt ruimte buiten kleiner

		if ((m_HorScroll)
			?	deltaX <= 0
			:	deltaX >= 0
			)
			return; // no invalidation required
	}
	InvalidateView();
}

void ScrollPort::GrowVer(CrdType deltaY, CrdType relPosY, const MovableObject* sourceItem)
{
	if (!sourceItem)
	{
		assert(relPosY <= GetCurrClientSize().Y());
		MakeMin(relPosY, m_NettSize.Y());

		if (deltaY > 0)
			base_type::GrowVer(deltaY, relPosY);  // maakt ruimte buiten groter

		m_NettSize.Y() += deltaY;                               // maakt DrawnNettRect groter
		MakeMax(m_NettSize.Y(), CrdType());
		SetScrollBars();

		if (deltaY < 0)
			base_type::GrowVer(deltaY, relPosY);  // maakt ruimte buiten kleiner en clipt DrawnFullSize

		if ((m_VerScroll)
			?	deltaY <= 0
			:	deltaY >= 0
			)
			return; // no invalidation required
	}
	InvalidateView();
}

void RePosScrollBar(HWND hScroll, GPoint devPos, GType nettWidth, GType nettHeight)
{
	SetWindowPos(hScroll, HWND_TOP
	,	devPos.x, devPos.y  // vertical position 
	,	nettWidth, nettHeight    // size of the scroll bar 
	,	SWP_NOACTIVATE|SWP_NOREPOSITION|SWP_SHOWWINDOW|SWP_NOSENDCHANGING
	);
}

void ScrollPort::SetScrollX(bool horScroll)
{
	auto absBase = GetCurrClientAbsLogicalPos();
//	absBase *= GetScaleFactors();

	if (horScroll)
	{
		auto sf = GetScaleFactors();
		absBase.Y() += m_NettSize.Y();
		auto absDeviceBase = ScaleCrdPoint(absBase, sf);
		auto nettDeviceSize = ScaleCrdPoint(m_NettSize, sf);
		auto deviceHeight = GetCurrClientSize().Y() * sf.second - nettDeviceSize.Y();
		if (!m_HorScroll)
		{
			auto dv = GetDataView().lock(); if (!dv) return;
			dv->SetScrollEventsReceiver(this);
			HWND hWnd = dv->GetHWnd();
			
			m_HorScroll = CreateWindowEx( // may Send WM_PARENTNOTIFY to SHV_DataView_DispatchMessage
				0L,                                       // no extended styles 
				"SCROLLBAR",                              // scroll bar control class 
				(LPSTR) NULL,                             // text for window title bar 
				WS_CHILD | SBS_HORZ,                      // scroll bar styles 
				absDeviceBase.X(),                        // horizontal position 
				absDeviceBase.Y(),                        // vertical position 
				nettDeviceSize.X(),                       // width of the scroll bar 
				deviceHeight,                             // default height 
				hWnd,                                     // handle to main window 
				(HMENU) NULL,                             // no menu for a scroll bar 
				GetInstance(hWnd),                        // instance owning this window 
				(LPVOID) NULL                             // pointer not needed 
			);
		}
		RePosScrollBar(m_HorScroll, CrdPoint2GPoint(absDeviceBase), nettDeviceSize.X(), deviceHeight);
	}
	else
	{
		if (m_HorScroll)
		{
			DestroyWindow(m_HorScroll);
			m_HorScroll = 0;
		}
	}
}

void ScrollPort::SetScrollY(bool verScroll)
{
	auto absBase = GetCurrClientAbsLogicalPos();

	if (verScroll)
	{
		auto sf = GetScaleFactors();
		absBase.X() += m_NettSize.X();
		auto absDeviceBase = ScaleCrdPoint(absBase, sf);
		auto nettDeviceSize = ScaleCrdPoint(m_NettSize, sf);
		auto deviceWidth = GetCurrClientSize().X() * sf.first - nettDeviceSize.X();
		if (!m_VerScroll)
		{
			auto dv = GetDataView().lock();
			dv->SetScrollEventsReceiver(this);
			HWND hWnd = dv->GetHWnd();
			m_VerScroll = CreateWindowEx(
				0L,                                 // no extended styles 
				"SCROLLBAR",                        // scroll bar control class 
				(LPSTR) NULL,                       // text for window title bar 
				WS_CHILD | SBS_VERT,                // scroll bar styles 
				absDeviceBase.X(),                  // horizontal position 
				absDeviceBase.Y(),                  // vertical position 
				deviceWidth, nettDeviceSize.Y(),    // size of the scroll bar
				hWnd,                               // handle to main window 
				(HMENU) NULL,                       // no menu for a scroll bar 
				GetInstance(hWnd),                  // instance owning this window 
				(LPVOID) NULL                       // pointer not needed 
			); 
		}
		RePosScrollBar(m_VerScroll, CrdPoint2GPoint(absDeviceBase), deviceWidth, nettDeviceSize.Y());
	}
	else
	{
		if (m_VerScroll)
		{
			DestroyWindow(m_VerScroll);
			m_VerScroll = 0;
		}
	}
}

void ScrollPort::SetScrollBars()
{
	SetScrollX(m_HorScroll);                   // horizontal bar up
	SetScrollY(m_VerScroll);                   // shorten vertical bar
}

void ScrollPort::OnChildSizeChanged()
{
	InvalidateView();
}

GType CalcNewPosBase(HWND scrollBarCtl, UInt16 scrollCmd)
{
	SCROLLINFO si;
	si.cbSize = sizeof(SCROLLINFO);
	si.fMask = SIF_TRACKPOS|SIF_POS|SIF_PAGE|SIF_RANGE;
	GetScrollInfo(scrollBarCtl, SB_CTL, &si);

//	IdleTimer::OnStartLoop();

	switch (scrollCmd) {
		case SB_LEFT:      return 0;
		case SB_RIGHT:     return si.nMax;
		case SB_LINELEFT:  return Max<Int32>(si.nPos - 10, si.nMin);
		case SB_LINERIGHT: return Min<Int32>(si.nPos + 10, si.nMax);
		case SB_PAGELEFT:  return Max<Int32>(Int32(si.nPos - si.nPage), si.nMin);
		case SB_PAGERIGHT: return Min<Int32>(Int32(si.nPos + si.nPage), si.nMax);
		case SB_THUMBPOSITION: 
		case SB_THUMBTRACK: 
//		case SB_ENDSCROLL:
			return si.nTrackPos + si.nMin;
	}

	return -1;
}

TType CalcNewPos(HWND scrollBarCtl, UInt16 scrollCmd, GType nrTPerG)
{
	GType newPos = CalcNewPosBase(scrollBarCtl, scrollCmd);
	if (newPos < 0)
		return -1;
	return CheckedMul<TType>(newPos, nrTPerG, false);
}

void ScrollPort::OnHScroll(UInt16 scollCmd)
{
	if (!m_HorScroll)
		return;

	TType newPos = CalcNewPos(m_HorScroll, scollCmd, m_NrLogicalUnitsPerTumpnailTick.x);
	if (newPos >= 0)
		ScrollLogical(shp2dms_order<TType>(-(newPos + GetContents()->GetCurrClientRelPos().X()), 0) );
}

void ScrollPort::OnVScroll(UInt16 scollCmd)
{
	if (!m_VerScroll)
		return;

	TType newPos = CalcNewPos(m_VerScroll, scollCmd, m_NrLogicalUnitsPerTumpnailTick.y);
	if (newPos >= 0)
		ScrollLogical(shp2dms_order<TType>(0, -(newPos + GetContents()->GetCurrClientRelPos().Y())) );
}

void ScrollPort::ScrollLogicalTo(CrdPoint newClientPos)
{
	ScrollLogical(newClientPos - Convert<CrdPoint>(GetContents()->GetCurrClientRelPos()) );
}

void ScrollPort::ScrollLogical(CrdPoint delta)
{
	DBG_START("ScrollPort", "Scroll", MG_DEBUG_SCROLL);
	DBG_TRACE(("org delta = %s", AsString(delta).c_str()));

	if (!GetContents())
		return;

	dbg_assert(md_ScrollRecursionCount == 0);
	{
		MG_DEBUGCODE( DynamicIncrementalLock<> xxx(md_ScrollRecursionCount); )

		auto viewBase = GetCurrClientAbsLogicalPos();
		auto viewExtents = CrdRect(viewBase, viewBase + m_NettSize);
		DBG_TRACE(("viewExtents = %s", AsString(viewExtents).c_str()));

		auto contentExtents = GetContents()->GetCurrClientRelLogicalRect();
		DBG_TRACE(("contentExtents = %s", AsString(contentExtents).c_str()));

		// positive delta increases contentBase, thus scrols towards left/up.
		// positive delta can cause empty space at end, which is to be avoided; limit delta to not creating such space
		MakeMax(delta.X(), Width(viewExtents) - contentExtents.second.X());
		MakeMin(delta.X(), 0                   - contentExtents.first .X());

		// positive delta can cause empty space at the bottom, which is to be avoided: limit highness of delta 
		// to not creating such space, and then too low delta that causes empty space at the top is limited
		MakeMax(delta.Y(), Height(viewExtents) - contentExtents.second.Y());
		MakeMin(delta.Y(), 0                    - contentExtents.first .Y());

		DBG_TRACE(("new delta = %s", AsString(delta).c_str()));


		if (!(delta == Point<CrdType>(0,0)))
			GetContents()->MoveTo(GetContents()->GetCurrClientRelPos() + delta);

		m_NrLogicalUnitsPerTumpnailTick.x = 1 + Width (contentExtents) / MaxValue<GType>();
		m_NrLogicalUnitsPerTumpnailTick.y = 1 + Height(contentExtents) / MaxValue<GType>();

		if (m_HorScroll)
		{
			SCROLLINFO scrollInfo;
			scrollInfo.cbSize = sizeof(SCROLLINFO);
			scrollInfo.fMask  = SIF_PAGE|SIF_POS|SIF_RANGE;
			scrollInfo.nMin   = 0;
			scrollInfo.nMax   = (Width(contentExtents) - 1) / m_NrLogicalUnitsPerTumpnailTick.x;
			scrollInfo.nPage  = m_NettSize.X();
			scrollInfo.nPos   = - GetContents()->GetCurrClientRelPos().X() / m_NrLogicalUnitsPerTumpnailTick.x;
	//		scrollInfo.nTrackPos; 
			SetScrollInfo(m_HorScroll, SB_CTL, &scrollInfo, true); // may Send msg's to SHV_DataView_DispatchMessage
		}
		if (m_VerScroll)
		{
			SCROLLINFO scrollInfo;
			scrollInfo.cbSize = sizeof(SCROLLINFO);
			scrollInfo.fMask  = SIF_PAGE|SIF_POS|SIF_RANGE;
			scrollInfo.nMin   = 0;
			scrollInfo.nMax   = (Height(contentExtents) - 1) / m_NrLogicalUnitsPerTumpnailTick.y;
			scrollInfo.nPage  = m_NettSize.Y();
			scrollInfo.nPos   = -GetContents()->GetCurrClientRelPos().Y() / m_NrLogicalUnitsPerTumpnailTick.y;
	//		scrollInfo.nTrackPos; 
			SetScrollInfo(m_VerScroll, SB_CTL, &scrollInfo, true); // may Send msg's to SHV_DataView_DispatchMessage
		}
	}
	if (!(delta == Point<CrdType>(0,0)))
		m_cmdOnScrolled();
}

bool ScrollPort::MouseEvent(MouseEventDispatcher& med)
{
	if (med.GetEventInfo().m_EventID & EventID::MOUSEWHEEL)
	{
		bool shift = GetKeyState(VK_SHIFT) & 0x8000;
		int wheelDelta = GET_WHEEL_DELTA_WPARAM(med.r_EventInfo.m_wParam);

		ScrollLogical(shp2dms_order<TType>(0, (wheelDelta * 37) / WHEEL_DELTA));
		return true;
	}
	return Wrapper::MouseEvent(med); // returns false
}

MovableObject* ScrollPort::GetContents()
{
	return debug_cast<MovableObject*>(base_type::GetContents()); 
}

const MovableObject* ScrollPort::GetContents() const
{
	return debug_cast<const MovableObject*>(base_type::GetContents()); 
}
void ScrollPort::MoveTo(CrdPoint newRelPos)
{
	base_type::MoveTo(newRelPos);

	SetScrollBars();
}

GraphVisitState ScrollPort::InviteGraphVistor(AbstrVisitor& v)
{
	return v.DoScrollPort(this);
}

bool ScrollPort::OnKeyDown(UInt32 virtKey)
{
	if (KeyInfo::IsCtrl(virtKey))
		switch (KeyInfo::CharOf(virtKey)) {
			case VK_HOME:     ScrollHome();                         return true;
			case VK_END:      ScrollEnd ();                         return true;
			case VK_RIGHT:    ScrollLogical(shp2dms_order<TType>(-PAGE_SCROLL_STEP, 0)); return true;
			case VK_LEFT:     ScrollLogical(shp2dms_order<TType>( PAGE_SCROLL_STEP, 0)); return true;
			case VK_UP:       ScrollLogical(shp2dms_order<TType>(0,  PAGE_SCROLL_STEP)); return true;
			case VK_DOWN:     ScrollLogical(shp2dms_order<TType>(0, -PAGE_SCROLL_STEP)); return true;
		}
	return base_type::OnKeyDown(virtKey);
}

CrdPoint ScrollPort::CalcMaxSize() const
{
	return GetContents()->CalcMaxSize() + Size(GetBorderLogicalExtents());
}


IMPL_RTTI_CLASS(ScrollPort)


