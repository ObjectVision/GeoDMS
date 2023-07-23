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

#include "dbg/Debug.h"
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
	,	m_NrTPointsPerGPoint(1, 1)
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
	ScrollLogicalTo (Point<TType>(0, 0) );
}

void ScrollPort::ScrollEnd()
{
	// down = negative
	TPoint topLeft = shp2dms_order<TType>( 0, GetCurrClientSize().Y() - GetContents()->CalcClientSize().Y() );

	ScrollLogicalTo( topLeft );
}

// TODO: MOVE TO RANGES
template <typename T>
T CalcDelta(Range<T> obj, Range<T> window, T borderSize)
{
	T d1 = window.first  - obj.first;
	T d2 = window.second - obj.second;
	if (d1>0 && d2>0) return Min<T>(d1 + borderSize, d2);
	if (d1<0 && d2<0) return Max<T>(d1, d2 - borderSize);
	return 0;
}


void ScrollPort::MakeLogicalRectVisible(TRect rect, const TPoint& border)
{
#if defined(MG_DEBUG)
	TRect contentsClientRect = GetContents()->GetCurrClientRelLogicalRect();
	dms_assert( IsIncluding(contentsClientRect, rect));
#endif

	TRect scrollRect = GetCurrNettRelLogicalRect();
	rect += scrollRect.TopLeft();

	TPoint delta = shp2dms_order<TType>(
		CalcDelta(rect.HorRange(), scrollRect.HorRange(), border.X()),
		CalcDelta(rect.VerRange(), scrollRect.VerRange(), border.Y())
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
	TPoint contentSize = GetContents()->CalcClientSize();

	TPoint newNettSize = GetCurrClientSize();
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
	MakeUpperBound(newNettSize, Point<TType>(0, 0));

	auto sf = GetScaleFactors();
	TPoint smallestSize = LowerBound(m_NettSize, newNettSize);
	if (GetContents()->IsDrawn())
		GetContents()->ResizeDrawnRect(
			GetDrawnNettAbsDeviceRect(),
			TPoint2GPoint(smallestSize - m_NettSize, sf),
			TPoint2GPoint(GetCurrClientAbsLogicalPos() + m_NettSize, sf)
		);

	omni::swap(m_NettSize, newNettSize);

	bool hasScroll = m_HorScroll && m_VerScroll;
	SetScrollX(horScroll);
	SetScrollY(verScroll);
	
	if (GetContents()->IsDrawn())
		GetContents()->ResizeDrawnRect(
			GetDrawnNettAbsDeviceRect(),
			TPoint2GPoint(m_NettSize - smallestSize, sf),
			TPoint2GPoint(GetCurrClientAbsLogicalPos() + smallestSize, sf) // Resize from curr size, which bas set by previous IsDrawn m_NettSize
		);

	if (!(newNettSize == m_NettSize))
	{
		if (horScroll && verScroll && !hasScroll)
			InvalidateClientRect(TRect(m_NettSize, GetCurrClientSize()));

		m_cmdOnScrolled();
	}
}

void ScrollPort::GrowHor(TType deltaX, TType relPosX, const MovableObject* sourceItem)
{
	if (!sourceItem)
	{
		assert(relPosX <= GetCurrClientSize().X());
		MakeMin(relPosX, m_NettSize.X());

		if (deltaX > 0)
			base_type::GrowHor(deltaX, relPosX, 0);                  // maakt ruimte buiten groter

		m_NettSize.X() += deltaX;                                      // maakt DrawnNettRect kleiner
		MakeMax(m_NettSize.X(), TType());
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

void ScrollPort::GrowVer(TType deltaY, TType relPosY, const MovableObject* sourceItem)
{
	if (!sourceItem)
	{
		dms_assert(relPosY <= GetCurrClientSize().Y());
		MakeMin(relPosY, m_NettSize.Y());

		if (deltaY > 0)
			base_type::GrowVer(deltaY, relPosY);  // maakt ruimte buiten groter

		m_NettSize.Y() += deltaY;                               // maakt DrawnNettRect groter
		MakeMax(m_NettSize.Y(), TType());
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

void RePosScrollBar(HWND hScroll, const TPoint& absPos, TType nettWidth, TType nettHeight, CrdPoint sf)
{
	SetWindowPos(
		hScroll,
		HWND_TOP, 
		absPos.X() * sf.first,      // horizontal position 
		absPos.Y() * sf.second,     // vertical position 
		nettWidth * sf.first,    // width of the scroll bar 
		nettHeight* sf.second,   // default height 
		SWP_NOACTIVATE|SWP_NOREPOSITION|SWP_SHOWWINDOW|SWP_NOSENDCHANGING
	);
}

void ScrollPort::SetScrollX(bool horScroll)
{
	TPoint absBase = GetCurrClientAbsLogicalPos();
//	absBase *= GetScaleFactors();

	auto sf = GetScaleFactors();
	if (horScroll)
	{
		absBase.Y() += m_NettSize.Y();
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
				absBase.X() * sf.first,                              // horizontal position 
				absBase.Y() * sf.second,                              // vertical position 
				m_NettSize.X() * sf.first,                           // width of the scroll bar 
				GType(GetCurrClientSize().Y() * sf.second) - GType(m_NettSize.Y() * sf.second) , // default height 
				hWnd,                                                   // handle to main window 
				(HMENU) NULL,                                           // no menu for a scroll bar 
				GetInstance(hWnd),                                      // instance owning this window 
				(LPVOID) NULL                                           // pointer not needed 
			);
		}
		RePosScrollBar(m_HorScroll, absBase
		,	m_NettSize.X()                             // width of the scroll bar 
		,	GetCurrClientSize().Y() - m_NettSize.Y()  // default height 
		,	GetScaleFactors()
		);
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
	TPoint absBase = GetCurrClientAbsLogicalPos();

	if (verScroll)
	{
		absBase.X() += m_NettSize.X();
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
				absBase.X(),                        // horizontal position 
				absBase.Y(),                        // vertical position 
				GetCurrClientSize().X() - m_NettSize.X(),   // default width
				m_NettSize.Y(),                     // height of the scroll bar
				hWnd,                               // handle to main window 
				(HMENU) NULL,                       // no menu for a scroll bar 
				GetInstance(hWnd),                  // instance owning this window 
				(LPVOID) NULL                       // pointer not needed 
			); 
		}
		RePosScrollBar(m_VerScroll, absBase
		,	GetCurrClientSize().X() - m_NettSize.X() // default width
		,	m_NettSize.Y()                          // height of the scroll bar
		,	GetScaleFactors()
		);
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
	return CheckedMul<TType>(newPos, nrTPerG);
}

void ScrollPort::OnHScroll(UInt16 scollCmd)
{
	if (!m_HorScroll)
		return;

	TType newPos = CalcNewPos(m_HorScroll, scollCmd, m_NrTPointsPerGPoint.x);
	if (newPos >= 0)
		ScrollLogical(shp2dms_order<TType>(-(newPos + GetContents()->GetCurrClientRelPos().X()), 0) );
}

void ScrollPort::OnVScroll(UInt16 scollCmd)
{
	if (!m_VerScroll)
		return;

	TType newPos = CalcNewPos(m_VerScroll, scollCmd, m_NrTPointsPerGPoint.y);
	if (newPos >= 0)
		ScrollLogical(shp2dms_order<TType>(0, -(newPos + GetContents()->GetCurrClientRelPos().Y())) );
}

void ScrollPort::ScrollLogicalTo(TPoint newClientPos)
{
	ScrollLogical(newClientPos - GetContents()->GetCurrClientRelPos() );
}

void ScrollPort::ScrollLogical(TPoint delta)
{
	DBG_START("ScrollPort", "Scroll", MG_DEBUG_SCROLL);
	DBG_TRACE(("org delta = %s", AsString(delta).c_str()));

	if (!GetContents())
		return;

	dbg_assert(md_ScrollRecursionCount == 0);
	{
		MG_DEBUGCODE( DynamicIncrementalLock<> xxx(md_ScrollRecursionCount); )

		TPoint viewBase = GetCurrClientAbsLogicalPos();
		TPoint viewSize = m_NettSize;
		TRect viewExtents = TRect(viewBase, viewBase + viewSize );
		DBG_TRACE(("viewExtents = %s", AsString(viewExtents).c_str()));

		TRect contentExtents = GetContents()->GetCurrClientRelLogicalRect();
		DBG_TRACE(("contentExtents = %s", AsString(contentExtents).c_str()));

		// positive delta increases contentBase, thus scrols towards left/up.
		// positive delta can cause empty space at end, which is to be avoided; limit delta to not creating such space
		MakeMax(delta.X(), viewExtents.Width() - contentExtents.Right());
		MakeMin(delta.X(), 0                   - contentExtents.Left ());

		// positive delta can cause empty space at the bottom, which is to be avoided: limit highness of delta 
		// to not creating such space, and then too low delta that causes empty space at the top is limited
		MakeMax(delta.Y(), viewExtents.Height() - contentExtents.Bottom());
		MakeMin(delta.Y(), 0                    - contentExtents.Top   ());

		DBG_TRACE(("new delta = %s", AsString(delta).c_str()));


		if (!(delta == Point<TType>(0,0)))
			GetContents()->MoveTo(GetContents()->GetCurrClientRelPos() + delta);

		m_NrTPointsPerGPoint.x = 1 + contentExtents.Width () / MaxValue<GType>();
		m_NrTPointsPerGPoint.y = 1 + contentExtents.Height() / MaxValue<GType>();

		if (m_HorScroll)
		{
			SCROLLINFO scrollInfo;
			scrollInfo.cbSize = sizeof(SCROLLINFO);
			scrollInfo.fMask  = SIF_PAGE|SIF_POS|SIF_RANGE;
			scrollInfo.nMin   = 0;
			scrollInfo.nMax   = (contentExtents.Width() - 1) / m_NrTPointsPerGPoint.x;
			scrollInfo.nPage  = viewSize.X();
			scrollInfo.nPos   = - GetContents()->GetCurrClientRelPos().X() / m_NrTPointsPerGPoint.x;
	//		scrollInfo.nTrackPos; 
			SetScrollInfo(m_HorScroll, SB_CTL, &scrollInfo, true); // may Send msg's to SHV_DataView_DispatchMessage
		}
		if (m_VerScroll)
		{
			SCROLLINFO scrollInfo;
			scrollInfo.cbSize = sizeof(SCROLLINFO);
			scrollInfo.fMask  = SIF_PAGE|SIF_POS|SIF_RANGE;
			scrollInfo.nMin   = 0;
			scrollInfo.nMax   = (contentExtents.Height() - 1) / m_NrTPointsPerGPoint.y;
			scrollInfo.nPage  = viewSize.Y();
			scrollInfo.nPos   = -GetContents()->GetCurrClientRelPos().Y() / m_NrTPointsPerGPoint.y;
	//		scrollInfo.nTrackPos; 
			SetScrollInfo(m_VerScroll, SB_CTL, &scrollInfo, true); // may Send msg's to SHV_DataView_DispatchMessage
		}
	}
	if (!(delta == Point<TType>(0,0)))
		m_cmdOnScrolled();
}

bool ScrollPort::MouseEvent(MouseEventDispatcher& med)
{
	if (med.GetEventInfo().m_EventID & EID_MOUSEWHEEL )
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
void ScrollPort::MoveTo(TPoint newRelPos)
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

TPoint ScrollPort::CalcMaxSize() const
{
	return GetContents()->CalcMaxSize() + GetBorderLogicalExtents().Size();
}


IMPL_RTTI_CLASS(ScrollPort)


