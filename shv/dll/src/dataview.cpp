// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "ShvDllPch.h"

#include "DataView.h"

#include "act/ActorVisitor.h"
#include "act/Updatemark.h"
#include "act/TriggerOperator.h"
#include "dbg/debug.h"
#include "dbg/DmsCatch.h"
#include "geo/Conversions.h"
#include "geo/PointOrder.h"
#include "mci/Class.h"
#include "mci/DoubleLinkedTree.inc"
#include "set/VectorFunc.h"
#include "xct/DmsException.h"

#include "LockLevels.h"

#include "AbstrUnit.h"
#include "StateChangeNotification.h"

#include "ShvUtils.h"

#include "ActivationInfo.h"
#include "Carets.h"
#include "CaretOperators.h"
#include "FocusElemProvider.h"
#include "GraphVisitor.h"
#include "IdleTimer.h"
#include "KeyFlags.h"
#include "MouseEventDispatcher.h"
#include "ScrollPort.h"

#include "windowsx.h"

//    NOTOOLBAR    Customizable bitmap-button toolbar control.
//    NOUPDOWN     Up and Down arrow increment/decrement control.
//    NOSTATUSBAR  Status bar control.
//    NOMENUHELP   APIs to help manage menus, especially with a status bar.
//    NOTRACKBAR   Customizable column-width tracking control.
//    NODRAGLIST   APIs to make a listbox source and sink drag&drop actions.
//    NOPROGRESS   Progress gas gauge.
//    NOHOTKEY     HotKey control
//    NOHEADER     Header bar control.
//    NOIMAGEAPIS  ImageList apis.
//    NOLISTVIEW   ListView control.
//    NOTREEVIEW   TreeView control.
//    NOTABCONTROL Tab control.
//    NOANIMATE    Animate control.

#include "CommCtrl.h"
ActorVisitState UpdateChildViews(DataViewTree* dvl);

////////////////////////////////////////////////////////////////////////////
// const

const int CN_BASE = 0x0000BC00;
const int UM_COMMAND_STATUS = WM_APP;
const int UPDATE_TIMER_ID = 3;

GPoint LParam2Point(LPARAM lParam)
{
	return GPoint(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
}

//----------------------------------------------------------------------
// ViewStyle
//----------------------------------------------------------------------

SHV_CALL CharPtr GetViewStyleName(ViewStyle ct)
{
	switch (ct) {
		case tvsMapView: return "MapView";
		case tvsTableView: return "TableView";
		case tvsExprEdit: return "ExprEdit";
		case tvsClassificationEdit: return "ClassificationEdit";
		case tvsPaletteEdit: return "PaletteEdit";
		case tvsDefault: return "Default";
		case tvsContainer: return "Container";
		case tvsTableContainer: return "TableContainer";
		case tvsHistogram: return "Histogram";
		case tvsUpdateItem: return "UpdateItem";
		case tvsUpdateTree: return "UpdateTree";
		case tvsSubItemSchema: return "SubItemSchema";
		case tvsSupplierSchema: return "SupplierSchema";
		case tvsExprSchema: return "ExprSchema";
		case tvsUndefined: return "Undefined";
		case tvsStatistics: return "Statistics";
		case tvsCalculationTimes: return "CalculationTimes";
		case tvsCurrentConfigFileList: return "CurrentConfigFileList";
		default: return "Unknown";
	}
}

//----------------------------------------------------------------------
// struct : MsgStruct
//----------------------------------------------------------------------

bool MsgStruct::Send() const
{
	#if defined(MG_DEBUG_DATAVIEWSET)
	MG_CHECK(m_DataView->IsInActiveDataViewSet());
	#endif
	return m_DataView->DispatchMsg(*this);
}

//----------------------------------------------------------------------
// class  : DataViewTree
//----------------------------------------------------------------------

DataViewTree g_DataViewRoots;

void DataViewTree::BringChildToTop(DataView* dv)
{
	DelSub(dv);
	AddSub(dv); // top of Z-order
}

void DataViewTree::AddChildView(DataView* childView)
{
	dms_assert(childView);
	dms_assert(childView->m_ParentView == nullptr);
	AddSub(childView);
	childView->m_ParentView = this;
}

void DataViewTree::DelChildView(DataView* childView)
{
	dms_assert(childView);
	dms_assert(childView->m_ParentView == this);
	DelSub(childView); 
	childView->m_ParentView = nullptr;
}

void DataViewTree::BroadcastCmd(ToolButtonID id)
{
	for (auto cv = _GetFirstSubItem(); cv; cv = cv->GetNextItem())
	{
		if (auto contents = cv->GetContents())
			contents->OnCommand(id);
		cv->BroadcastCmd(id);
	}
}

void BroadcastCommandToAllDataViews(ToolButtonID id)
{
	g_DataViewRoots.BroadcastCmd(id);
}


//----------------------------------------------------------------------
// section: DEBUG TOOLS
//----------------------------------------------------------------------

#if defined(MG_DEBUG)

DbgInvalidateDrawLock::DbgInvalidateDrawLock(DataView* dv)
	:	m_DataView(dv)
{ 
	dms_assert(dv);
	if (dv)
		++dv->md_InvalidateDrawLock;
}

DbgInvalidateDrawLock::~DbgInvalidateDrawLock()
{
	if (m_DataView.has_ptr())
		--m_DataView->md_InvalidateDrawLock;
}

#endif


#if defined(MG_DEBUG_DATAVIEWSET)

std::mutex g_csActiveDataViewSet;
std::set<DataView*> g_ActiveDataViews;

bool DataView::IsInActiveDataViewSet()
{
	auto lock = std::unique_lock(g_csActiveDataViewSet);
	return g_ActiveDataViews.find(this) != g_ActiveDataViews.end();
}

#endif


//----------------------------------------------------------------------
// class  : DataView
//----------------------------------------------------------------------

/////////////////////////////////////////////////////////////////////////////
// DataView construction/destruction

DataView::DataView(TreeItem* viewContext)
	:	m_hWnd(0)
	,	m_ViewContext(viewContext)
	,	m_CheckedTS(0)
	,	m_ViewDeviceSize(0, 0)
	,	m_FocusCaret(new FocusCaret)
	,	m_ParentView(0)
	,	m_ScrollEventsReceiver(0)
#if defined(MG_DEBUG)
	,	md_InvalidateDrawLock(0)
	,	md_IsDrawingCount(0)
#endif
	,	m_ColorPalette {
			CombineRGB(253,  98,  94), // rood
			CombineRGB(53, 116, 201), // fel blauw
			CombineRGB(41, 184, 102), // fel groen
			CombineRGB(254, 150, 102), // oranje
			CombineRGB(166, 105, 153), // paars
			CombineRGB(121,  55,  10), // bruin
			CombineRGB(112,  17,  23), // donker rood
			CombineRGB(138, 212, 235), // licht blauw
			CombineRGB(71, 155,   0), // groen
			CombineRGB(57,  60,  61), // donker grijs
			CombineRGB(9,  92,  43), // donker groen
			CombineRGB(20,  44,  76), // donker blauw
			CombineRGB(255,   9, 127), // roze
			CombineRGB(242, 200,  15), // donker geel
			CombineRGB(0,   0,   0), // zwart
			CombineRGB(118,  76,  26)  // lichter bruin
		}
{
//	for (int i = 0; i != nrPaletteColors; ++i)
//		m_ColorPalette[i] = DmsColor2COLORREF(i + 1);

	assert(viewContext);

	InsertCaret(m_FocusCaret);
	m_State.Set(DVF_CaretsVisible); // Paint will draw them.if true; we assume application and view has focus.

#if defined(MG_DEBUG_DATAVIEWSET)
	auto lock = std::unique_lock(g_csActiveDataViewSet);
	g_ActiveDataViews.insert(this);
#endif //defined(MG_DEBUG_DATAVIEWSET)
}

DataView::~DataView()
{
	dbg_assert(md_IsDrawingCount == 0);
	// avoid destructor of m_ControllerVector that triggers RemoveCaret to Reverse any remaining carets
	m_State.Clear(DVF_CaretsVisible); 

	DataView* subView = _GetFirstSubItem();
	while (subView)
	{
		DataView* thisSubView = subView;
		subView = subView->GetNextItem();
		thisSubView->DestroyWindow();
	}

	if (m_ParentView)
		m_ParentView->DelChildView(this);

	SelThemeCreator::UnregisterDataView(this);
	dbg_assert(md_InvalidateDrawLock == 0);

	OnDestroyDataView(this); // remove remaining messages for this DataView from the queue.

#if defined(MG_DEBUG_DATAVIEWSET)
	auto lock = std::unique_lock(g_csActiveDataViewSet);
	g_ActiveDataViews.erase(this);
#endif //defined(MG_DEBUG_DATAVIEWSET)
}

void DataView::SetContents(std::shared_ptr<MovableObject> contents, ShvSyncMode sm)
{
	ObjectContextHandle contextHandle1(contents.get(), "DataView::SetContents");

	dms_assert(contents);
	dms_assert(contents->GetDataView().lock().get() == this);
	dms_assert(!m_Contents);
	m_Contents = contents->shared_from_this();
	TreeItem* ti = GetViewContext();

	ObjectContextHandle contextHandle2(ti, sm ==SM_Load ? "Load" : "Save");
	m_Contents->Sync(ti, sm);
}

void DataView::DestroyWindow()
{
	SendMessage(GetHWnd(), WM_CLOSE, 0, 0);
}

/////////////////////////////////////////////////////////////////////////////
// DataView Interface funcs


HFONT DataView::GetDefaultFont(FontSizeCategory fid, Float64 dip2pixFactor) const
{
//	dip2pixFactor *= GetWindowDIP2pixFactor(m_hWnd);
	assert(fid >= FontSizeCategory::SMALL && fid <= FontSizeCategory::COUNT);
	if (fid < FontSizeCategory::SMALL || fid >= FontSizeCategory::COUNT)
		return {};

	auto font_scaling = dip2pixFactor * (96.0 / 72.0);
	if (! m_DefaultFonts[static_cast<int>(fid)][dip2pixFactor])
	{
		m_DefaultFonts[static_cast<int>(fid)][dip2pixFactor] =
			GdiHandle<HFONT>(
				CreateFont(
	//				-10, // nHeight, -MulDiv(8, GetDeviceCaps(hDC, LOGPIXELSY), 72) // height, we assume LOGPIXELSY == 96*dip2pixFactor
					GetDefaultFontHeightDIP(fid)*font_scaling,
					  0, // nWidth,  match against the digitization aspect ratio of the available fonts 
					  0, // nEscapement
					  0, // nOrientaion
					400, // fnWeight
					false,	                   // DWORD fdwItalic,         // italic attribute flag
					false,                     // DWORD fdwUnderline,      // underline attribute flag
					false,                     // DWORD fdwStrikeOut,      // strikeout attribute flag
					ANSI_CHARSET,              // DWORD fdwCharSet,        // character set identifier
					OUT_TT_PRECIS,             // DWORD fdwOutputPrecision,  // output precision
					CLIP_DEFAULT_PRECIS,       // DWORD fdwClipPrecision,  // clipping precision
					PROOF_QUALITY,             // DWORD fdwQuality,        // output quality
					DEFAULT_PITCH|FF_DONTCARE, // DWORD fdwPitchAndFamily,  // pitch and family
					"Noto Sans Medium"         // pointer to typeface name string
				)
			);
	}
	return m_DefaultFonts[static_cast<int>(fid)][dip2pixFactor];
}

void DataView::InsertCaret(AbstrCaret* c)
{ 
	DBG_START("DataView", "InsertCaret", MG_DEBUG_CARET);

	m_CaretVector.push_back(c);
	if (m_State.Get(DVF_CaretsVisible) && m_hWnd && c->IsVisible())
	{
		dms_assert(m_hWnd);
		CaretDcHandle dc(m_hWnd, GetDefaultFont(FontSizeCategory::CARET));
		c->Reverse(dc, true);
	}
}

void DataView::RemoveCaret(AbstrCaret* c)
{
	DBG_START("DataView", "RemoveCaret", MG_DEBUG_CARET);

	if (m_State.Get(DVF_CaretsVisible) && m_hWnd && c->IsVisible())
	{
		CaretDcHandle dc(m_hWnd, GetDefaultFont(FontSizeCategory::CARET));
		c->Reverse(dc, false);
	}
	vector_erase(m_CaretVector, c);
}

void DataView::InsertController(AbstrController* c)
{
	m_ControllerVector.push_back(c);
}

void DataView::RemoveController(AbstrController* c)
{
	vector_erase(m_ControllerVector, c);
}

void DataView::MoveCaret(AbstrCaret* caret, const AbstrCaretOperator& caretOperator)
{
	DBG_START("DataView", "MoveCaret", MG_DEBUG_CARET);

	assert(caret);

	if (m_State.Get(DVF_CaretsVisible))
		caret->Move(
			caretOperator, 
			CaretDcHandle(m_hWnd, GetDefaultFont(FontSizeCategory::CARET))
		);
	else
		caretOperator(caret);
}

void DataView::SetTextCaret(const GPoint& caretPos)
{
	m_State.Set(DVF_HasTextCaret);
	m_TextCaretPos = caretPos;
	UpdateTextCaret();
}

void DataView::ClearTextCaret()
{
	m_State.Clear(DVF_HasTextCaret);
	UpdateTextCaret();
}

auto DataView::OnCommandEnable(ToolButtonID id) const->CommandStatus
{
	auto result = GetContents()->OnCommandEnable(id);
	if (result == CommandStatus::ENABLED)
	{
		for (AbstrController* ctrlPtr: m_ControllerVector)
		{
			switch (ctrlPtr->GetPressStatus(id))
			{
				case PressStatus::DontCare: continue;
				case PressStatus::Dn: return CommandStatus::DOWN;
				case PressStatus::Up: return CommandStatus::UP;
			}
		}
	}
	return result;
}

void DataView::UpdateTextCaret()
{
	if (m_State.GetBits(DVF_HasFocus|DVF_HasTextCaret) == (DVF_HasFocus|DVF_HasTextCaret))
	{
		if (!m_State.Get(DVF_TextCaretCreated))
			::CreateCaret(m_hWnd, NULL, 2, 16);

		::SetCaretPos(m_TextCaretPos.x, m_TextCaretPos.y);

		if (!m_State.Get(DVF_TextCaretCreated))
		{
			if (m_State.Get(DVF_CaretsVisible))
				::ShowCaret(m_hWnd);
			m_State.Set(DVF_TextCaretCreated);
		}
	}
	else
	{
		if (m_State.Get(DVF_TextCaretCreated))
		{
			if (m_State.Get(DVF_CaretsVisible))
				::HideCaret(m_hWnd);
			::DestroyCaret();
			m_State.Clear(DVF_TextCaretCreated);
		}
	}
}

void DataView::SetCaretsVisible(bool visibility, HDC dc)
{
	DBG_START("DataView", "SetCaretsVisible", MG_DEBUG_CARET);
	DBG_TRACE(("Curr %d", m_State.Get(DVF_CaretsVisible)));
	DBG_TRACE(("Req  %d", visibility));
	DBG_TRACE(("HDC  %x", dc));
	DBG_TRACE(("HWND %x", m_hWnd));

	if (m_State.Get(DVF_CaretsVisible) != visibility)
	{
		if (m_hWnd)
			ReverseCarets(dc, visibility);
		m_State.Set(DVF_CaretsVisible, visibility);
		if (m_State.Get(DVF_TextCaretCreated))
			if (visibility)
				::ShowCaret(m_hWnd);
			else
				::HideCaret(m_hWnd);

	}
}

void DataView::ReverseCarets(HDC hdc, bool newVisibleState)
{
	DBG_START("DataView", "ReverseCarets", MG_DEBUG_CARET);

	dms_assert(m_hWnd);

	if (hdc)
	{
		DcBkModeSelector bkMode(hdc, TRANSPARENT);
		GdiObjectSelector smallFont(hdc, GetDefaultFont(FontSizeCategory::CARET));
		DcMixModeSelector xorMode(hdc);
		ReverseCaretsImpl(hdc, newVisibleState);
	}
	else
	{
		CaretDcHandle dc(m_hWnd, GetDefaultFont(FontSizeCategory::CARET)); // activates xorMode in its constructor
		ReverseCaretsImpl(dc, newVisibleState);
	}
}

void DataView::ReverseCaretsImpl(HDC hdc, bool newVisibleState)
{
	DBG_START("DataView", "ReverseCaretsImpl", MG_DEBUG_CARET);

	dms_assert(hdc);

	caret_iterator i = m_CaretVector.begin();
	caret_iterator e = m_CaretVector.end();
	for(; i != e; ++i)
		if ((*i)->IsVisible())
			(*i)->Reverse(hdc, newVisibleState);

// SELCARET
//	if (!m_SelCaret.Empty())
//		ReverseSelCaretImpl(hdc, m_SelCaret);
}



void DataView::ReverseSelCaretImpl(HDC hdc, const Region& selCaretRgn)
{
	if (!m_SelBrush)
	{
		struct {
			BITMAPINFOHEADER bmiHeader;
			RGBQUAD          colorData[2];
			UINT32           bitData[8];
		} packedDIB =
		{
			{ sizeof(BITMAPINFOHEADER), 8, 8, 1, 1, BI_RGB, 0 },
			{ {255,255,255, 0}, {0,0,0,0}, },
//			{ 0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55, }
//			{ 0xAA, 0x00, 0xAA, 0x00, 0xAA, 0x00, 0xAA, 0x00, }
//			{ 0xCC, 0xCC, 0x33, 0x33, 0xCC, 0xCC, 0x33, 0x33, }
			{ 0xCC, 0xCC, 0x00, 0x00, 0xCC, 0xCC, 0x00, 0x00, }
//			{ 0xF0, 0xF0, 0xF0, 0xF0, 0x00, 0x00, 0x00, 0x00  }
		};

		m_SelBrush = GdiHandle<HBRUSH>(CreateDIBPatternBrushPt(&packedDIB, DIB_RGB_COLORS));

		UINT32 bitData2[8] = { 0xAA, 0x00, 0xAA, 0x00, 0xAA, 0x00, 0xAA, 0x00, };

		for (UInt32 i = 0; i !=8; ++i)
			packedDIB.bitData[i] ^= bitData2[i];

		m_BrdBrush = GdiHandle<HBRUSH>(CreateDIBPatternBrushPt(&packedDIB, DIB_RGB_COLORS));
	}
	FillRgn (hdc, selCaretRgn.GetHandle(), m_SelBrush);

//	SELCARET
//	FrameRgn(hdc, selCaretRgn.GetHandle(), m_BrdBrush, 4, 4);
}

void DataView::SetSelCaret(Region& newSelCaret)
{
	m_SelCaret.swap( newSelCaret );
}

void DataView::XOrSelCaret(const Region& newSelCaret)
{
	m_SelCaret ^= newSelCaret;
}


/////////////////////////////////////////////////////////////////////////////
// DataView event handlers

#define WM_QT_ACTIVATENOTIFIERS WM_USER+2
bool DataView::DispatchMsg(const MsgStruct& msg)
{
	DBG_START("DataView", "DispatchMsg", MG_DEBUG_WNDPROC);
		DBG_TRACE(("msg: %x(%x, %x)", msg.m_Msg, msg.m_wParam, msg.m_lParam));

	switch (msg.m_Msg)
	{
		case WM_PAINT:
//			dbg_assert(!md_IsDrawingCount); can be Sent when Peeking msg in Update.
			OnPaint();
			return true;
		case WM_TIMER:
		{
			if (msg.m_wParam == UPDATE_TIMER_ID)
			{
				KillTimer(m_hWnd, UPDATE_TIMER_ID);
				if (IdleTimer::IsInIdleMode())
				{
					IdleTimer::Subscribe(shared_from_this());
					m_Waiter.end();
				}
				else
				{
					auto status = UpdateView();
					if (status == GraphVisitState::GVS_Break)
						SetUpdateTimer();
					else
						m_Waiter.end();
				}
				goto completed;
			}
			goto defaultProcessing;
		}
		case WM_ERASEBKGND:
			assert(msg.m_ResultPtr);
			goto completed;

		case WM_SETFOCUS:
			m_State.Set(DVF_HasFocus);
			UpdateTextCaret();
			goto defaultProcessing;

		case WM_KILLFOCUS:
			m_State.Clear(DVF_HasFocus);
			UpdateTextCaret();
			goto defaultProcessing;

		case WM_MOUSEMOVE:
			OnMouseMove (msg.m_wParam, LParam2Point(msg.m_lParam));
			goto completed;

		case WM_MOUSEWHEEL:
			DispatchMouseEvent(EventID::MOUSEWHEEL, msg.m_wParam, LParam2Point(msg.m_lParam).ScreenToClient(m_hWnd));
			goto completed;

		case WM_SETCURSOR:
			//	We assume to not arrive here reentrant: DMS doesn't employ message pumps and we assume that WM_SETCURSOR isn't sent from PeekMessage as WM_SIZE and WM_WINDOWPOSCHANGED and WM_NCIHITTEST adn WM_COPYDATA sometimes seems to be.
			//	REMOVE dms_assert(GetContextLevel() == 0); but user32 sends it when we pump WM_MOUSEMOUVE; let's handle it anyway
			if (LOWORD(msg.m_lParam) != HTCLIENT)
				goto defaultProcessing;
			if (IsBusy())
				SetCursor( LoadCursor(NULL, IDC_WAIT) );
			else
			{
				GPoint cursorPos;
				CheckedGdiCall(
					GetCursorPos(&cursorPos),
					"GetCursorPos"
				);
				if (!DispatchMouseEvent(EventID::SETCURSOR, 0, cursorPos.ScreenToClient(m_hWnd)) )
					SetCursor( LoadCursor(NULL, IDC_ARROW) );
			}
			return true;

		case WM_MOUSELEAVE:
			OnMouseMove(0, UNDEFINED_VALUE(GPoint) );
			goto completed;

		case WM_LBUTTONDOWN:
		{
			SetCapture(m_hWnd);
			// notify Qt parent that we are active
			auto parent = GetAncestor(m_hWnd, GA_PARENT);
			SendMessage(parent, WM_USER+17, 0, 0);
			DispatchMouseEvent(EventID::LBUTTONDOWN, msg.m_wParam, LParam2Point(msg.m_lParam));
			goto completed;
		}
		case WM_RBUTTONDOWN:
			DispatchMouseEvent(EventID::RBUTTONDOWN,   msg.m_wParam, LParam2Point(msg.m_lParam) );
			*msg.m_ResultPtr = 0;
			return true;

		case WM_LBUTTONUP:
		 	DispatchMouseEvent(EventID::LBUTTONUP,     msg.m_wParam, LParam2Point(msg.m_lParam) );
			ReleaseCapture();
			goto completed;

		case WM_RBUTTONUP:
			DispatchMouseEvent(EventID::RBUTTONUP,     msg.m_wParam, LParam2Point(msg.m_lParam) );
			goto completed;

		case WM_LBUTTONDBLCLK:
			DispatchMouseEvent(EventID::LBUTTONDBLCLK, msg.m_wParam, LParam2Point(msg.m_lParam) );
			goto completed;

		case WM_RBUTTONDBLCLK:
			DispatchMouseEvent(EventID::RBUTTONDBLCLK, msg.m_wParam, LParam2Point(msg.m_lParam) );
			goto completed;


#if defined(MG_SKIP_WINDOWPOSCHANGED)
		case WM_SIZE:
			{
				OnSize(msg.m_wParam, LParam2Point(msg.m_lParam) );
				goto completed;
			}
#else
		case WM_WINDOWPOSCHANGED:
			{
				DBG_START(GetClsName().c_str(), "DispatchMsg(WM_WINDOWPOSCHANGED)", MG_DEBUG_SIZE);
				WINDOWPOS* wp = reinterpret_cast<WINDOWPOS*>(msg.m_lParam);
				OnSize(0, GPoint(wp->cx, wp->cy));
//				IdleTimer::OnStartLoop();
				goto completed;
			}
#endif
		case WM_HSCROLL:
			if (!msg.m_lParam)
				goto defaultProcessing;
			dms_assert(m_ScrollEventsReceiver);
			if (m_ScrollEventsReceiver)
				m_ScrollEventsReceiver->OnHScroll(LOWORD(msg.m_wParam));
			goto completed;

		case WM_VSCROLL:
			if (!msg.m_lParam)
				goto defaultProcessing;
			dms_assert(m_ScrollEventsReceiver);
			if (m_ScrollEventsReceiver)
				m_ScrollEventsReceiver->OnVScroll(LOWORD(msg.m_wParam));
			goto completed;


		case WM_CAPTURECHANGED:
			OnCaptureChanged(HWND(msg.m_lParam));
			goto completed;

		case WM_MOUSEACTIVATE:
//			if (GetActiveWindow() != GetWindowParent(m_hWnd) )
//			{
//				SetFocus(m_hWnd);
//				SetActiveWindow(m_hWnd);
//				MessageBeep(-1);
//				*msg.m_ResultPtr = MA_ACTIVATEANDEAT;
//			}
			SetFocus(m_hWnd);
			*msg.m_ResultPtr = MA_ACTIVATE;
			return true;

		case WM_ACTIVATE:
			OnActivate(LOWORD(msg.m_wParam) != WA_INACTIVE); // bool minimized = (HIWORD(msg.m_wParam) != 0);
			goto completed;

		case WM_COMMAND:
		{
			SuspendTrigger::FencedBlocker suspendLock("DataView::OnCommand()");
			if (HIWORD(msg.m_wParam) == 0 && GetContents()->OnCommand(ToolButtonID(LOWORD(msg.m_wParam))))
				goto completed;
			goto defaultProcessing;
		}
		case UM_COMMAND_STATUS:
			*msg.m_ResultPtr = static_cast<LRESULT>(GetContents()->OnCommandEnable(ToolButtonID(LOWORD(msg.m_wParam))));
			return true;

		case WM_KEYDOWN:
			if (OnKeyDown(msg.m_wParam))
				goto completed;
//			if (TranslateMessage(msg))
//				goto completed;
			goto defaultProcessing;

		case WM_SYSKEYDOWN:
			if (OnKeyDown(msg.m_wParam | (msg.m_lParam & KeyInfo::Flag::Menu) | KeyInfo::Flag::Syst))
				goto completed;
//			if (TranslateMessage(msg))
//				goto completed;
			goto defaultProcessing;
		case WM_CHAR:
			if (OnKeyDown(msg.m_wParam | KeyInfo::Flag::Char))
				goto completed;
			goto defaultProcessing;
			
		case WM_SYSCHAR:
			if (OnKeyDown(msg.m_wParam  | (msg.m_lParam & KeyInfo::Flag::Menu) | KeyInfo::Flag::Syst | KeyInfo::Flag::Char ))
				goto completed;
			goto defaultProcessing;

		case WM_PROCESS_QUEUE:
			ProcessGuiOpers();
			goto completed;

		case WM_APP + 4:
		case WM_COPYDATA:
			PCOPYDATASTRUCT pCopyData = PCOPYDATASTRUCT(msg.m_lParam);
			const UInt32* dataBegin = PUINT32(pCopyData->lpData);
			OnCopyData(UINT(pCopyData->dwData), dataBegin, dataBegin + (pCopyData->cbData / 4));
			goto completed;
	}
defaultProcessing:
	return false; // send to default processing

completed:
	*msg.m_ResultPtr = 0; // don't dispatch any further if processed
	return true;
}

bool DataView::OnKeyDown(UInt32 nVirtKey)
{
	if (GetKeyState(VK_CONTROL) & 0x8000)
		nVirtKey |= KeyInfo::Flag::Ctrl;
	if (GetKeyState(VK_MENU) & 0x8000)
		nVirtKey |= KeyInfo::Flag::Menu;
	if (GetKeyState(VK_SHIFT) & 0x8000)
		nVirtKey |= KeyInfo::Flag::Shift;
	if (m_ActivationInfo)
		return m_ActivationInfo.OnKeyDown(nVirtKey);
	return GetContents()->OnKeyDown(nVirtKey);
}

ActorVisitState DataView::VisitSuppliers(SupplierVisitFlag svf, const ActorVisitor& visitor) const
{
	if (GetContents()->VisitSuppliers(svf, visitor) == AVS_SuspendedOrFailed)
		return AVS_SuspendedOrFailed;
	return base_type::VisitSuppliers(svf, visitor);
}

void DataView::DoInvalidate() const
{
	DBG_START("DataView", "DoInvalidate", MG_DEBUG_REGION);

	dbg_assert( md_IsDrawingCount == 0);
	dbg_assert( md_InvalidateDrawLock == 0);

	if (m_ViewDeviceSize.x > 0 && m_ViewDeviceSize.y > 0)
		m_DoneGraphics.Reset( Region(m_ViewDeviceSize) );

	dms_assert(DoesHaveSupplInterest() || !GetInterestCount());
}

void DataView::InvalidateChangedGraphics()
{
	dbg_assert(!md_IsDrawingCount);
	dms_assert(!SuspendTrigger::DidSuspend());

	auto go = GetContents();
	dms_assert(go);

	MG_DEBUG_DATA_CODE( SuspendTrigger::ApplyLock protectSuspend; )

	assert( !go->IsDrawn() || IsIncluding(GRect2CrdRect(ViewDeviceRect()), go->GetDrawnFullAbsDeviceRect()));

	if (m_CheckedTS != UpdateMarker::LastTS())
	{
		m_CheckedTS = UpdateMarker::GetLastTS();
		GraphInvalidator().Visit( go.get() );
	}

	assert(!SuspendTrigger::DidSuspend());

	// make sure the m_DoneGraphics is updated according to invalidated rgn before scrolling; can result in UpdateView
	UpdateWindow(GetHWnd()); // may Send WM_PAINT or WM_ERASEBKGND to SHV_DataView_DispatchMessage
	assert(!SuspendTrigger::DidSuspend());
}

#include "act/TriggerOperator.h"

//	SELCARET
#include "MapControl.h"
#include "ViewPort.h"
//	END SELCARET


GraphVisitState DataView::UpdateView()
{
	if (m_State.Get(DVF_InUpdateView))
		return GVS_Continue;
	if (SuspendTrigger::MustSuspend())
		return GVS_Break;

	SuspendibleUpdate(PS_Committed);
	if (SuspendTrigger::DidSuspend())
		return GVS_Break;

	auto_flag_recursion_lock<DVF_InUpdateView> recursionLock(m_State);

	ObjectContextHandle context(this, "UpdateView");

	dms_assert(!SuspendTrigger::DidSuspend());

	InvalidateChangedGraphics();

	auto scaleFactors = GetScaleFactors();

	if (GraphUpdater( ViewDeviceRect(), scaleFactors).Visit( GetContents().get() ) == GVS_Break)
		return GVS_Break;

	assert(!SuspendTrigger::DidSuspend());
	if (SuspendTrigger::MustSuspend())
		return GVS_Break;


	if (!m_DoneGraphics.Empty())
	{
		dbg_assert(md_IsDrawingCount == 0);
		MG_DEBUGCODE( DynamicIncrementalLock<decltype(md_IsDrawingCount)> lock(md_IsDrawingCount); )

		DcHandle dc(m_hWnd, GetDefaultFont(FontSizeCategory::MEDIUM));

		MG_DEBUGCODE( DbgInvalidateDrawLock protectFromViewChanges(this); )

		dms_assert(!SuspendTrigger::DidSuspend());
		while (! m_DoneGraphics.Empty() )
		{
			DBG_START("DataView::UpdateView", "Region", MG_DEBUG_REGION);

			const Region& drawRegion = m_DoneGraphics.CurrRegion();
#if defined(MG_DEBUG)
			if (MG_DEBUG_REGION)
			{
				Region dcRegion(dc, m_hWnd);
				Region combRegion = dcRegion & drawRegion;

				DBG_TRACE(("drawRegion = %s", drawRegion.AsString().c_str()));
				DBG_TRACE(("dc  Region = %s", dcRegion.AsString().c_str()));
				DBG_TRACE(("combRegion = %s", combRegion.AsString().c_str()));
				ProcessMainThreadOpers();
			}
#endif
			if	( ExtSelectClipRgn(dc, drawRegion.GetHandle(), RGN_AND) == NULLREGION )
			{
#if defined(MG_DEBUG)
				if (MG_DEBUG_REGION)
				{
					Region dcRegion2(dc, m_hWnd);
					if (!dcRegion2.Empty())
					{
						DBG_TRACE(("dc2 Region = %s", dcRegion2.AsString().c_str()));
						ProcessMainThreadOpers();
					}
				}
#endif
				m_DoneGraphics.PopBack();
				continue;
			}

			GraphDrawer drawer(dc, m_DoneGraphics, this, GdMode( GD_StoreRect|GD_Suspendible|GD_UpdateData|GD_DrawData), scaleFactors);
			CaretHider caretHider(this, dc); // Only area as clipped by m_DoneGraphics.Curr().Region() is hidden

			dms_assert(!SuspendTrigger::DidSuspend());

			GraphVisitState suspended = drawer.Visit( GetContents().get() );
			bool stopped   = m_DoneGraphics.DidBreak();

//			dms_assert((suspended == GVS_Break) == (SuspendTrigger::DidSuspend() || stopped)); // stopped implies suspended
			dms_assert(!stopped  || !SuspendTrigger::DidSuspend()); // maximum one cause of suspension

			if (stopped || suspended == GVS_Continue)
			{
				// SELCARET
 				if (!m_DoneGraphics.HasMultipleStacks() && !m_SelCaret.Empty())
				{
					DcMixModeSelector xorMode(dc);
					DcBrushOrgSelector brushOrgSelector(dc, debug_pointer_cast<MapControl>( GetContents() )->GetViewPort()->GetBrushOrg() );

					ReverseSelCaretImpl(dc, m_SelCaret); // SelectClipRgn still actve?
				}
				dms_assert( m_DoneGraphics.NoSuspendedCounters() );
				m_DoneGraphics.PopBack();
				SuspendTrigger::MarkProgress();
			}
			if (SuspendTrigger::DidSuspend())
				return GVS_Break;
		}
	}
	dms_assert(m_DoneGraphics.Empty()); // post condition of while loop

	// now try to update all remaining (invisible) Contents, thus preparing data for later view

	MG_DEBUGCODE( DbgInvalidateDrawLock protectFromViewChanges(this); )

	CounterStacks updateAllStack;
//	GPoint viewSize = m_ViewDeviceSize; viewSize *= GetWindowDIP2pixFactorXY(GetHWnd());
	updateAllStack.AddDrawRegion(Region(m_ViewDeviceSize));

	dms_assert(!SuspendTrigger::DidSuspend()); // should have been acted upon, DEBUG, REMOVE

	GraphVisitState suspended = GraphDrawer(NULL, updateAllStack, this, GdMode(GD_Suspendible | GD_UpdateData), scaleFactors)
		.Visit(GetContents().get());

//	auto allDrawer = GraphDrawer(NULL, updateAllStack, this, GdMode(GD_Suspendible | GD_UpdateData));
//	GraphVisitState suspended = allDrawer.Visit( GetContents().get() );
	dms_assert((suspended == GVS_Break) == SuspendTrigger::DidSuspend());

	dms_assert(m_DoneGraphics.Empty()); // it was empty and the OnPaint is only processed in sync
	if (!m_DoneGraphics.Empty()) // DEBUG, REMOVE when above assert is proven to hold
		return GVS_Break;

	if (SuspendTrigger::DidSuspend())
		return GVS_Break;

	if (UpdateChildViews(this) == AVS_SuspendedOrFailed)
		return GVS_Break;
	return GVS_Ready;
}

#include "utl/Environment.h"

UINT GetShowCmd(HWND hWnd)
{
	WINDOWPLACEMENT wpl;
	wpl.length = sizeof(WINDOWPLACEMENT);
	CheckedGdiCall(
		GetWindowPlacement(hWnd, &wpl)
	,	"GetWindowPlacement"
	);
	if (wpl.showCmd == SW_SHOWNORMAL)
		CheckedGdiCall(
			GetWindowPlacement(GetParent(hWnd), &wpl)
		,	"GetWindowPlacement"
		);

	dms_assert(
			wpl.showCmd == SW_HIDE
		||	wpl.showCmd == SW_SHOWMAXIMIZED
		||	wpl.showCmd == SW_SHOWMINIMIZED
		||	wpl.showCmd == SW_SHOWNORMAL
	);
	return wpl.showCmd;
}

ActorVisitState UpdateChildViews(DataViewTree* dvl)
{
	DataView* dv = dvl->_GetFirstSubItem();
	while (dv)
	{
		if (UpdateChildViews(dv) == AVS_SuspendedOrFailed)
			return AVS_SuspendedOrFailed;

		HWND hWnd = dv->GetHWnd();
		if (IsWindowVisible(hWnd))
		{
			UINT showCmd = GetShowCmd(hWnd);
			if (showCmd != SW_HIDE && showCmd != SW_SHOWMINIMIZED)
			{
				if ((dv->UpdateView() != GVS_Continue))
					return AVS_SuspendedOrFailed;  // come back if suspended and not failed
				if (showCmd == SW_SHOWMAXIMIZED)
					return AVS_Ready; // don't update sibblings that are hidden under a maximized View
			}
		}
		dv = dv->GetNextItem();
	}
	return AVS_Ready;
}

/* REMOVE
ActorVisitState DataView::UpdateViews()
{
	if (g_DispatchLockCount > 1) // any other locks than the one from SHV_DataView_Update?
		return AVS_SuspendedOrFailed;
	return UpdateChildViews(&g_DataViewRoots);
}
*/

void DataView::ScrollDevice(GPoint delta, GRect rcScroll, GRect rcClip, const MovableObject* src)
{
	DBG_START("DataView", "Scroll", MG_DEBUG_SCROLL);
	DBG_TRACE(("dx = %d, dy=%d", delta.x, delta.y));
	DBG_TRACE(("rect = %s", AsString(rcScroll).c_str()));
	DBG_TRACE(("clip = %s", AsString(rcClip  ).c_str()));

	dbg_assert( md_InvalidateDrawLock == 0);
	assert(src);
	{
		DcHandle dc(m_hWnd, GetDefaultFont(FontSizeCategory::MEDIUM)); // we could clip on the rcScroll|rcClip region
		Region   rgnClip(rcClip);
		SelectClipRgn(dc, rgnClip.GetHandle());

		CaretHider caretHider(this, dc);

		GRect rcClippedScroll = rcClip; 
		rcClippedScroll &= rcScroll; // prepare for making rcScrolll TRect

		Region drawRgn(GPoint(0, 0));
		Region invalidRgn( GetHWnd() );

		DBG_TRACE(("invr = %s", invalidRgn.AsString().c_str()));

		GRect validRect = (rcClippedScroll + delta) & rcClip; // dest region of scroll
		if (!validRect.empty())
			ValidateRect(validRect);

		ScrollWindowEx(GetHWnd(),
			delta.x , delta.y,
			&rcClippedScroll,      // prcScrill
			&rcClip,               // prcClip
			drawRgn.GetHandle(),   // HRGN hrgnUpdate,  // handle to update region
			NULL,                  // LPRECT prcUpdate, // address of structure for update rectangle
			0                      // SW_SCROLLCHILDREN //SW_ERASE|SW_INVALIDATE // |SW_SMOOTHSCROLL + 0x00010000
		);
/*
		Region drawRgn(rcClippedScroll);                         // source region of scroll
		if (rcScroll != rcClippedScroll)
			drawRgn |= Region((rcScroll  + delta) & rcClip);     // invalidate dest region that was outside the clipping region
		drawRgn -= Region(validRect);
*/
		// scroll invalidations within clippedScroll rect
		if (! invalidRgn.Empty())
		{
			invalidRgn &= rcClippedScroll;
			if (! invalidRgn.Empty())
			{
				invalidRgn += delta;
				invalidRgn &= rcClip;
				drawRgn  |= invalidRgn;
			}
		}

		// then invalidate rgn that became waste
		if (!drawRgn.Empty())
			InvalidateRgn(drawRgn);

		m_DoneGraphics.ScrollDevice( delta, rcScroll, rcClip );

		// Update positions of carets before reappearing
		dms_assert(!m_State.Get(DVF_CaretsVisible)); // guaranteed by CaretHider

		if (m_SelCaret.IsIntersecting(rcClip))
			m_SelCaret.ScrollDevice( delta, rcScroll, rgnClip);

		if (IsIncluding(rcClippedScroll, m_TextCaretPos))
			m_TextCaretPos += delta;
		if (m_FocusCaret)
		{
			GRect focusRect = m_FocusCaret->GetRect();
			if (IsIncluding(rcClippedScroll, focusRect))
			{
				focusRect += delta;
				focusRect &= rcClip;
				SetFocusRect(focusRect);
			}
		}
	}
	// Remove active carets after they reappeared to erase them in full.
	// Removing them before reappearande would avoid flicker, but we have only partially removed active carets)
	DispatchMouseEvent(EventID::SCROLLED, 0, UNDEFINED_VALUE(GPoint) ); 
}

typedef GdiHandle<HMENU>             SafeMenuHandle;
typedef std::vector<SafeMenuHandle>  MenuHandleCollection;

void InsertMenuItems(
	MenuHandleCollection& hMenus,
	MenuData::const_iterator& i,
	MenuData::const_iterator e,
	UInt32* menuItemIdPtr,
	UInt32 level)
{
	dms_assert(i!=e && i->m_Level == level); // only get here when at least one item to insert
	

	hMenus.push_back( SafeMenuHandle( CreatePopupMenu() ) );
	HMENU hMenu = hMenus.back();

	while (i!=e && i->m_Level == level)
	{
		MENUITEMINFO itemInfo;
		itemInfo.cbSize = sizeof(MENUITEMINFO);
		itemInfo.fMask  = MIIM_TYPE|MIIM_ID|MIIM_STATE|MIIM_SUBMENU|MIIM_DATA;
		if (! i->m_Caption.empty())
		{
			itemInfo.fType      = MFT_STRING;
			itemInfo.dwTypeData = const_cast<LPSTR>(i->m_Caption.c_str());
			itemInfo.cch        = i->m_Caption.ssize();
		}
		else
			itemInfo.fType = MFT_SEPARATOR;
		itemInfo.fState     = i->m_Flags;
		itemInfo.wID        = *menuItemIdPtr;
		itemInfo.hSubMenu   = 0;
		itemInfo.dwItemData = 0;

		++i; ++*menuItemIdPtr;

		if (i!=e && i->m_Level > level)
		{
			dms_assert(i->m_Level == level + 1);

			SizeT menuCount = hMenus.size();
			InsertMenuItems(hMenus, i, e, menuItemIdPtr, level+1);
			dms_assert(menuCount < hMenus.size());
			itemInfo.hSubMenu = hMenus[menuCount];
		}

		CheckedGdiCall(
			InsertMenuItem(hMenu,
				itemInfo.wID, true,
				&itemInfo),
			"ShowPopupMenu"
		);
	}
	dms_assert(i==e || i->m_Level < level);

}

void DataView::ShowPopupMenu(GPoint point, const MenuData& menuData) const
{
	dms_assert(menuData.size());

	MenuHandleCollection hMenus;

	UInt32 menuItemID = 1;
	auto menuDataIter = menuData.begin();
	InsertMenuItems(hMenus, menuDataIter, menuData.end(), &menuItemID, 0);
	dms_assert(hMenus.size() > 0);

	GPoint screenPoint = point;
	CheckedGdiCall(
		ClientToScreen(m_hWnd, &screenPoint),
		"ShowPopupMenu"
	);

	UInt32 result;
	{
		IdleTimer idleProcessingProvider;
		result = TrackPopupMenuEx(
			hMenus[0], 
			TPM_NONOTIFY|TPM_RETURNCMD|TPM_RIGHTBUTTON , 
			screenPoint.x, screenPoint.y, 
			m_hWnd, 
			NULL //Pointer to a TPMPARAMS structure that specifies an area of the screen the menu should not overlap
		);
	}
	if (result)
	{
		--result;
		assert(result < menuData.size());
		menuData[result].Execute();
	}
}

bool IsAnchestor(MovableObject* candidateParent, MovableObject* childObj)
{
	dms_assert(candidateParent);
	std::shared_ptr<GraphicObject> co = childObj->shared_from_this();
	while (co)
	{
		if (co.get() == candidateParent)
			return true;
		co = co->GetOwner().lock();

	}
	return false;
}

void Activate(MovableObject* newAct, MovableObject* base)
{
	if (newAct && newAct != base)
	{
		Activate(newAct->GetOwner().lock().get(), base);
		newAct->SetActive(true);
	}
}

void ChangeActivation(MovableObject*  oldAct, MovableObject* newAct)
{
	dms_assert(newAct);
	std::shared_ptr<GraphicObject> oa;
	if (oldAct) oa = oldAct->shared_from_this();
	while (oa && !IsAnchestor(debug_cast<MovableObject*>(oa.get()), newAct))
	{
		auto pa = oa->GetOwner().lock();
		assert(pa);
		if (oa->IsActive())
			oa->SetActive(false);
		oa = pa;
	}
	Activate(newAct, debug_cast<MovableObject*>(oa.get()));
}

void DataView::Activate(MovableObject* src)
{
	dms_assert(src);
	ChangeActivation(m_ActivationInfo.get(), ActivationInfo(src).get());
	m_ActivationInfo = ActivationInfo(src);
}

void DataView::SetCursorPos(GPoint clientPoint)
{
	ClientToScreen(m_hWnd, &clientPoint);

	GPoint currPos;
	if (!::GetCursorPos(&currPos) || !(currPos == clientPoint))
	{
		::SetCursorPos(clientPoint.x, clientPoint.y);
	}
}

/////////////////////////////////////////////////////////////////////////////
// DataView message handlers

// MK_CONTROL	Set if the CTRL key is down.
// MK_LBUTTON	Set if the left mouse button is down.
// MK_MBUTTON	Set if the middle mouse button is down.
// MK_RBUTTON	Set if the right mouse button is down.
// MK_SHIFT	  Set if the SHIFT key is down.

bool DataView::DispatchMouseEvent(EventID event, WPARAM nFlags, GPoint devicePoint)
{
	if (nFlags & MK_CONTROL) event |= EventID::CTRLKEY;
	if (nFlags & MK_SHIFT  ) event |= EventID::SHIFTKEY;

	EventInfo eventInfo(event, nFlags, devicePoint);

	m_ControllerVector(eventInfo);

	if (eventInfo.m_EventID & EventID::HANDLED)
		return true;

	if (!IsDefined(devicePoint))
		return false;
	if (eventInfo.m_EventID & EventID::RBUTTONUP)
		m_TextEditController.CloseCurr();

	bool result = false;
	MouseEventDispatcher med(this, eventInfo);
	{
		SuspendTrigger::FencedBlocker blockSuspension("DataView::DispatchMouseEvent");
		result = med.Visit(GetContents().get());
	}

	if ( ! med.GetMenuData().empty() )
	{
		ShowPopupMenu(eventInfo.m_Point, med.GetMenuData() );
		return true;
	}
	if (med.IsActivating() && med.m_ActivatedObject)
	{
		Activate(med.m_ActivatedObject.get());
		return true;
	}
	return result;
}

void DataView::SetFocusRect(GRect focusRect)
{
	if (m_FocusCaret)
	{
		if (focusRect == m_FocusCaret->GetRect()) 
			return;

		MoveCaret(
			m_FocusCaret,
			DualPointCaretOperator(focusRect.LeftTop(), focusRect.RightBottom(), 0)
		);	
	}
}

SharedStr DataView::GetCaption() const
{
	if (!m_Contents)
		return {};
	return m_Contents->GetCaption();
}


void DataView::OnCaptionChanged() const
{
	SendStatusText(SeverityTypeID::ST_MajorTrace, GetCaption().c_str());

}

// ============   Painting

void DataView::OnEraseBkgnd(HDC dc) 
{
	DBG_START("DataView", "OnEraseBkgnd", MG_DEBUG_CARET || MG_DEBUG_REGION);

	GRect rect;
	int clipStatus = GetClipBox(dc, &rect);
	if (clipStatus == NULLREGION)
		return;
	if (clipStatus == ERROR)
		throwLastSystemError("DataView::OnEraseBkgnd");

	DBG_TRACE(("FillRect [(%d,%d)-(%d,%d)]", rect.left, rect.top, rect.right, rect.bottom));

	FillRect(dc, &rect, (HBRUSH) (COLOR_WINDOW+1) );
}

std::atomic<UInt32> s_DataViewOnPaintRecursionCount = 0;

void DataView::OnPaint() 
{
	DBG_START("DataView", "OnPaint", MG_DEBUG_CARET || MG_DEBUG_REGION);

	if (s_DataViewOnPaintRecursionCount)
		return; // already processing a message; do after completion.
	StaticMtIncrementalLock<s_DataViewOnPaintRecursionCount> recursionLock;

//	======================

	PaintDcHandle paintDC(m_hWnd, GetDefaultFont(FontSizeCategory::MEDIUM));
	if (!paintDC.GetHDC())
		throwLastSystemError("DataView::OnPaint");

//	SuspendTrigger::Resume();
//	dms_assert(! SuspendTrigger::DidSuspend() );
	Region rgn(paintDC, m_hWnd);
	if (rgn.Empty())
		return;

	dbg_assert( md_InvalidateDrawLock == 0);
	MG_DEBUGCODE( DbgInvalidateDrawLock protectFromViewChanges(this); )

#if defined(MG_DEBUG)
	if (MG_DEBUG_INVALIDATE || true)
	{
		GRect rect(GPoint(0, 0), m_ViewDeviceSize);
		GdiHandle<HBRUSH> br1( CreateSolidBrush( DmsColor2COLORREF(DmsYellow) ) );
		::FillRect(paintDC, &rect, br1 );
	}
#endif
	GraphDrawer( paintDC, rgn, this, GdMode(GD_StoreRect|GD_OnPaint|GD_DrawBackground), GetScaleFactors())
		.Visit( GetContents().get() );

	m_DoneGraphics.AddDrawRegion( std::move(rgn) );

	DBG_TRACE(("PaintDc must erase %d", paintDC.MustEraseBkgnd()));

	if (m_State.Get(DVF_CaretsVisible))
		ReverseCarets(paintDC, true); // draw carets also in new areas; PaintDcHandle validates updateRect
	SetUpdateTimer();
}

void DataView::SetUpdateTimer()
{
	m_Waiter.start(this);
	SetTimer(m_hWnd, UPDATE_TIMER_ID, 100, nullptr);
}
// ============   Mouse Handling

void DataView::OnMouseMove(WPARAM nFlags, GPoint devicePoint) 
{
	if (IsDefined(devicePoint))
	{
		TRACKMOUSEEVENT tme;
		tme.cbSize      = sizeof(TRACKMOUSEEVENT);
		tme.dwFlags     = TME_LEAVE; // Request a WM_MOUSELEAVE message for this window
		tme.hwndTrack   = m_hWnd;
//		tme.dwHoverTime = HOVER_DEFAULT;
		if (!_TrackMouseEvent(&tme))
			throwLastSystemError("TrackMouseEvent");
	}
	if (nFlags & MK_LBUTTON) // && (::GetCapture == HWindow)
		DispatchMouseEvent(EventID::MOUSEDRAG, nFlags, devicePoint);
	else if ((nFlags & (MK_LBUTTON|MK_MBUTTON|MK_RBUTTON)) == 0)
		DispatchMouseEvent(EventID::MOUSEMOVE, nFlags, devicePoint);
}

void DataView::OnCaptureChanged(HWND hWnd) 
{
//	SetCaretsVisible(false, 0);
	if (hWnd && hWnd != m_hWnd)
		DispatchMouseEvent(EventID::CAPTURECHANGED, 0, UNDEFINED_VALUE(GPoint) );
}

void DataView::OnActivate(bool becomeActive) 
{
	DBG_START("DataView", "OnActivate", MG_DEBUG_CARET);
	SetCaretsVisible(becomeActive, 0);
	if (becomeActive)
	{
		dms_assert(m_ParentView);
		m_ParentView->BringChildToTop(this);
		UpdateMarker::GetFreshTS(MG_DEBUG_TS_SOURCE_CODE("DataView::OnActivate")); // trigger DetermineStateChange to look for authentic source changes
	}
}

void DataView::OnSize(WPARAM nType, GPoint deviceSize) 
{
	DBG_START(GetClsName().c_str(), "OnSize", MG_DEBUG_CARET || MG_DEBUG_REGION || MG_DEBUG_SIZE);
	DBG_TRACE(("NewSize=(%d,%d)", deviceSize.x, deviceSize.y));
	auto hWnd = GetHWnd();
	assert(hWnd);
	auto currScaleFactors = GetWindowDip2PixFactors(hWnd);
	if (m_CurrScaleFactors != currScaleFactors)
	{
		m_CurrScaleFactors = currScaleFactors;
		GetContents()->InvalidateDraw();
	}
		 
	auto logicalSize = Reverse(GPoint2CrdPoint(deviceSize));
	if (m_ViewDeviceSize == deviceSize)
	{
		if (GetContents()->GetCurrFullSize() == logicalSize)
			return;
	}

	if (deviceSize.x <= 0 || deviceSize.y <= 0)
	{
		GetContents()->InvalidateDraw();
		return;
	}

#if defined(MG_DEBUG)
	if (MG_DEBUG_INVALIDATE || true)
	{
		DcHandle dc(GetHWnd(), 0);

		GRect rect1(GPoint(m_ViewDeviceSize.x, 0), UpperBound(m_ViewDeviceSize, deviceSize));
		GRect rect2(GPoint(0, m_ViewDeviceSize.y), UpperBound(m_ViewDeviceSize, deviceSize));

		::FillRect(dc, &rect1, GdiHandle<HBRUSH>(CreateSolidBrush(DmsColor2COLORREF(DmsBlue))));
		::FillRect(dc, &rect2, GdiHandle<HBRUSH>(CreateSolidBrush(DmsColor2COLORREF(DmsGreen))));
	}
#endif

	MakeUpperBound(m_ViewDeviceSize, deviceSize);


	if (m_Contents) {
		auto deviceRect = GRect(GPoint(0, 0), deviceSize);
		auto logicalRect = CrdRect(Point<CrdType>(0, 0), logicalSize);
		if (GetContents()->IsDrawn())
			GetContents()->ClipDrawnRect(GRect2CrdRect(deviceRect));
		GetContents()->SetFullRelRect(logicalRect);
	}
	m_ViewDeviceSize = deviceSize;

	dbg_assert( md_InvalidateDrawLock == 0);
	m_DoneGraphics.LimitDrawRegions(deviceSize); // limit rect if window becomes smaller
}

TreeItem* DataView::GetDesktopContext() const
{
	TreeItem* desktopItem = const_cast<TreeItem*>( GetViewContext()->GetTreeParent().get() ); 
	assert(desktopItem && !desktopItem->IsCacheItem());
	return desktopItem;
}

void DataView::ResetHWnd(HWND hWnd)
{
	dms_assert(hWnd);
	m_hWnd = hWnd;
	if (!m_ParentView)
		g_DataViewRoots.AddChildView(this);
}

void DataView::SetScrollEventsReceiver(ScrollPort* sp)
{
	dms_assert(m_ScrollEventsReceiver == 0 || m_ScrollEventsReceiver == sp);
	m_ScrollEventsReceiver = sp;
}

ExportInfo DataView::GetExportInfo()
{
	return ExportInfo();
}

void DataView::SendStatusText(SeverityTypeID st, CharPtr msg) const
{
	m_StatusTextCaller(st, msg); 
}

void DataView::SetStatusTextFunc(ClientHandle clientHandle, StatusTextFunc stf)
{
	m_StatusTextCaller.m_ClientHandle = clientHandle; 
	m_StatusTextCaller.m_Func   = stf;
}

void DataView::InvalidateDeviceRect(GRect rect)
{
#if defined(MG_DEBUG)
	CheckRgnLimits(rect);
#endif
	::InvalidateRect(m_hWnd, &rect, true);
#if defined(MG_DEBUG)
	if (MG_DEBUG_INVALIDATE || true)
	{
		DcHandle dc(GetHWnd(), 0);
		GdiHandle<HBRUSH> br( CreateSolidBrush( DmsColor2COLORREF(DmsRed) ) );
		::FillRect(dc, &rect, br );
	}
#endif
}

void DataView::InvalidateRgn (const Region& rgn)
{
	::InvalidateRgn(m_hWnd, rgn.GetHandle(), true);
#if defined(MG_DEBUG)
	if (MG_DEBUG_INVALIDATE || true)
	{
		DcHandle dc(GetHWnd(), 0);
		GdiHandle<HBRUSH> br( CreateSolidBrush( DmsColor2COLORREF(DmsOrange) ) );
		::FillRgn(dc, rgn.GetHandle(), br );
	}
#endif
}


void DataView::ValidateRect(const GRect& pixRect)
{
	::ValidateRect(m_hWnd, &pixRect);
#if defined(MG_DEBUG)
	if (MG_DEBUG_INVALIDATE && false)
	{
		DcHandle dc(GetHWnd(), 0);
		GdiHandle<HBRUSH> br( CreateSolidBrush( DmsColor2COLORREF(DmsBlue) ) );
		::FillRect(dc, &pixRect, br );
	}
#endif
}

DmsColor DataView::GetNextDmsColor() const
{
	auto currIndex = m_PaletteIndex++;
	dms_assert(currIndex < nrPaletteColors);
	if (m_PaletteIndex == nrPaletteColors)
		m_PaletteIndex = 0;
	return m_ColorPalette[currIndex];
}


IMPL_RTTI_CLASS(DataView);

/////////////////////////////////////////////////////////////////////////////
// DataView Creation funcs

#include "ShvDllInterface.h"

bool DataView::CreateMdiChild(ViewStyle ct, CharPtr caption)
{
	assert(m_ParentView == 0);

	MdiCreateStruct createStruct{
		.ct = ct,
		.dataView = this,
		.contextItem = GetViewContext(),
		.caption = caption,
	};
//	if (m_Contents) 
//		MakeLowerBound(createStruct.maxSize, TPoint2GPoint(GetContents()->CalcMaxSize(), GetScaleFactors()));

	NotifyStateChange(reinterpret_cast<const TreeItem*>(&createStruct), CC_CreateMdiChild);
	if (createStruct.hWnd)
	{
		m_hWnd = createStruct.hWnd;
		if (m_ParentView)
			m_ParentView->DelChildView(this);
		g_DataViewRoots.AddChildView(this);
		return true;
	}
	return false;
}

leveled_critical_section s_QueueSection(item_level_type(0), ord_level_type::DataViewQueue, "DataViewQueue");

void DataView::AddGuiOper(std::function<void()>&& func)
{
	leveled_critical_section::scoped_lock lock(s_QueueSection);

	bool wasEmpty = m_GuiOperQueue.empty();
	m_GuiOperQueue.emplace_back(std::move(func));
	if (wasEmpty && m_hWnd)
		PostMessage(m_hWnd, WM_PROCESS_QUEUE, 0, 0);
}


void DataView::ProcessGuiOpers()
{
	assert(IsMainThread());
	while (true) {
		std::function<void()> nextOperation;
		{
			leveled_critical_section::scoped_lock lock(s_QueueSection);
			if (m_GuiOperQueue.empty())
				return;
			if (SuspendTrigger::MustSuspend())
			{
				PostMessage(m_hWnd, WM_PROCESS_QUEUE, 0, 0);
				return;
			}
			nextOperation = std::move(m_GuiOperQueue.front());
			m_GuiOperQueue.pop_front();
		}
		StaticMtDecrementalLock<decltype(g_DispatchLockCount), g_DispatchLockCount> dispatchLock;
		try {
			nextOperation();
		}
		catch (...) {} // let it go, it's just GUI.
		SuspendTrigger::MarkProgress();
	}
}

void OnControlActivate(DataView* self, const UInt32* first, const UInt32* last)
{
	auto mo = self->GetContents();

	while (first != last)
	{
		UInt32 i = *first++;
		reportF(SeverityTypeID::ST_MajorTrace, "At %s %s go to item %s"
			, mo->GetDynamicClass()->GetName()
			, mo->GetFullName()
			, i
		);
		if (!i)
			break;
		auto n = mo->NrEntries();
		if (i > n)
		{
			reportF(SeverityTypeID::ST_MajorTrace, "exit because object has only %s sub-objects", n);
			return;
		}
		auto sgo = mo->GetEntry(--i);
		dms_assert(sgo);
		auto smo = dynamic_cast<MovableObject*>(sgo);
		if (!smo)
		{
			reportD(SeverityTypeID::ST_MajorTrace, "exit because object is not a movable");
			return;
		}
		mo = smo->shared_from_this();
	}
	reportF(SeverityTypeID::ST_MajorTrace, "Activate %s %s!"
		, mo->GetDynamicClass()->GetName()
		, mo->GetFullName()
	);
	self->Activate(mo.get());
}

void FillAllMenu(sharedPtrMovableObject mo, MouseEventDispatcher& med)
{
	dms_assert(mo);
	mo->FillMenu(med);
	mo = mo->GetOwner().lock();
	if (mo)
		FillAllMenu(mo, med);
}

void OnPopupMenuActivate(DataView* self, const UInt32* first, const UInt32* last)
{
	EventInfo eventInfo(EventID::RBUTTONDOWN, 0);
	MouseEventDispatcher med(self, eventInfo);
	FillAllMenu(self->m_ActivationInfo ? self->m_ActivationInfo : self->GetContents()->shared_from_this(), med);

	auto& menuData = med.GetMenuData();
	UInt32 result = 0;
	while (first != last)
	{
		UInt32 i = *first++;
		if (!i)
			break;

		// go to i-th item at current level.
		if (result >= menuData.size())
			return;


		UInt32 level = menuData[result].m_Level;

		reportF(SeverityTypeID::ST_MajorTrace, "At %s with level %s go to  the %s-th item", menuData[result].m_Caption, level, i);

		while (--i)
		{
			while (true) {
				if (++result >= menuData.size())
				{
					reportD(SeverityTypeID::ST_MajorTrace, "exit at end of menu");
					return;
				}
				UInt32 currLevel = menuData[result].m_Level;
				if (currLevel < level)
				{
					reportD(SeverityTypeID::ST_MajorTrace, "exit at end of sub-menu");
					return;
				}
				if (currLevel == level)
					break;
			};
			assert(menuData[result].m_Level == level);
		}
		assert(menuData[result].m_Level == level);

		// if more to come, go to next item, which should be first sub-item of the current menu-item.
		if (first != last && *first)
		{
			if (++result >= menuData.size())
			{
				reportD(SeverityTypeID::ST_MajorTrace, "exit at end of menu");
				return;
			}
			if (menuData[result].m_Level < level)
			{
				reportD(SeverityTypeID::ST_MajorTrace, "exit at end of sub-menu");
				return;
			}
		}
	}
	reportF(SeverityTypeID::ST_MajorTrace, "Execute %s", menuData[result].m_Caption);
	menuData[result].Execute();
}

#include "ptr/SharedArrayPtr.h"

void DataView::OnCopyData(UINT cmd, const UInt32* first, const UInt32* last)
{
	reportF(SeverityTypeID::ST_MajorTrace, "OnCopyDataPost with cmd %d", cmd);

	switch (cmd)
	{
	case 0: OnControlActivate(this, first, last); break;
	case 1: OnPopupMenuActivate(this, first, last); break;
	}

}

// ContextHandling
void DataView::GenerateDescription()
{
	SetText(GetCaption());
}

std::map<DataView*, std::shared_ptr<DataView>> g_DataViewMap;

void Keep(const std::shared_ptr<DataView>& self)
{
	g_DataViewMap[self.get()] = self;
}

void Unkeep(DataView* self)
{
	g_DataViewMap.erase(self);
}
