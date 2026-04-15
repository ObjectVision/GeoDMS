// Copyright (C) 1998-2025 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

#if !defined(__SHV_DATAVIEW_H)
#define __SHV_DATAVIEW_H

//----------------------------------------------------------------------
// used modules and forward class references
//----------------------------------------------------------------------

#include <vector>

#include "act/Actor.h"
#include "geo/Geometry.h"
#include "geo/Range.h"
#include "mci/DoubleLinkedTree.h"
#include "ptr/OwningPtr.h"
#include "ptr/SharedPtr.h"


#include "AbstrController.h"
#include "AbstrCaret.h"
#include "ActivationInfo.h"
#include "CounterStacks.h"
#ifdef _WIN32
#include "DcHandle.h"
#endif
#include "ExportInfo.h"
#include "GraphicLayer.h"
#include "GraphVisitor.h"
#include "LayerInfo.h"
#include "MovableObject.h"
#include "TextEditController.h"
#include "ViewHost.h"
#include "act/Waiter.h"

#include "ShvUtils.h"

class AbstrCaretOperator;
class AbstrCmd;
class FocusCaret;
class DataView;

////////////////////////////////////////////////////////////////////////////
// const WM_TIMER IDs

const int UPDATE_TIMER_ID = 3;
const int HOVER_TIMER_ID = 4;
const int TIP_WATCH_TIMER_ID = 5;

//----------------------------------------------------------------------
// ViewStyle
//----------------------------------------------------------------------

// ViewStyle must be in sync with ilTreeview in unit fMain.dfm and TTreeItemViewStyle in uDmsInterface.pas
enum ViewStyle : int {
//	Possible Default views
	tvsMapView,
	tvsTableView,
	tvsExprEdit,
	tvsClassificationEdit,
	tvsPaletteEdit,
	tvsDefault,
	tvsContainer,
	tvsTableContainer,
//	other viewtypes
	tvsHistogram
,	tvsUpdateItem
,	tvsUpdateTree
,	tvsSubItemSchema
,	tvsSupplierSchema
,	tvsExprSchema
,	tvsUndefined
,   tvsStatistics
//  Non treeitem related styles
,   tvsCalculationTimes
,	tvsCurrentConfigFileList
};

SHV_CALL CharPtr GetViewStyleName(ViewStyle ct);
	
enum ViewStyleFlags : int {
	vsfNone               = 0,
	vsfMapView            = (1 << tvsMapView),
	vsfTableView          = (1 << tvsTableView),
	vsfExprEdit           = (1 << tvsExprEdit),
	vsfClassificationEdit = (1 << tvsClassificationEdit),
	vsfPaletteEdit        = (1 << tvsPaletteEdit),
	vsfDefault            = (1 << tvsDefault),

	vsfContainer          = (1 << tvsContainer),
	vsfTableContainer     = (1 << tvsTableContainer),

	vsfHistogram          = (1 << tvsHistogram),
	vsfUpdateItem         = (1 << tvsUpdateItem),
	vsfUpdateTree         = (1 << tvsUpdateTree),
	vsfSubItemSchema      = (1 << tvsSubItemSchema),
	vsfSupplierSchema     = (1 << tvsSupplierSchema),
	vsfExprSchema         = (1 << tvsExprSchema),
};

//----------------------------------------------------------------------
// struct : MsgStruct (Win32 message dispatch)
//----------------------------------------------------------------------

#ifdef _WIN32
struct MsgStruct
{ 
	MsgStruct(DataView* dv, UINT msg, WPARAM wParam, LPARAM lParam)
		:	m_DataView(dv)
		,	m_Msg(msg)
		,	m_wParam(wParam)
		,	m_lParam(lParam)
	{}

	DataView* m_DataView;
	UINT      m_Msg;
	WPARAM    m_wParam;
	LPARAM    m_lParam;

	MsgResult Send() const;  // calls DataView::DispachMsg, and returns true if processed; else caller must call DefWindowProc
};
#endif // _WIN32

const UInt32 DVF_InUpdateView     = actor_flag_set::AF_Next * 0x0001;
const UInt32 DVF_HasFocus         = actor_flag_set::AF_Next * 0x0002;
const UInt32 DVF_HasTextCaret     = actor_flag_set::AF_Next * 0x0004;
const UInt32 DVF_CaretsVisible    = actor_flag_set::AF_Next * 0x0008;
const UInt32 DVF_TextCaretCreated = actor_flag_set::AF_Next * 0x0010;

const int nrPaletteColors = 16;

//----------------------------------------------------------------------
// class  : DataViewTree
//----------------------------------------------------------------------

struct DataViewTree : double_linked_tree<DataView>
{
	void BringChildToTop(DataView* dv);
	void AddChildView(DataView* childView);
	void DelChildView(DataView* childView);

	void BroadcastCmd(ToolButtonID id);
	void BroadcastUpdateRequest();
};

//----------------------------------------------------------------------
// section: DEBUG TOOLS
//----------------------------------------------------------------------

#if defined(MG_DEBUG)

struct DbgInvalidateDrawLock
{
	DbgInvalidateDrawLock(DataView* dv);
	~DbgInvalidateDrawLock();

	WeakPtr<DataView> m_DataView;
};

#endif


#if defined(MG_DEBUG)
#define MG_DEBUG_DATAVIEWSET
#endif

struct MdiCreateStruct
{
	ViewStyle ct = tvsUndefined;
	DataView* dataView = nullptr;
	TreeItem* contextItem = nullptr;
	CharPtr   caption = nullptr;
	bool      makeOverlapped = true;
	GPoint    maxSize = GPoint(600, 400);
#ifdef _WIN32
	HWND      hWnd = 0;
#endif
};

//----------------------------------------------------------------------
// class  : DataView
//----------------------------------------------------------------------

class DataView : public Actor, public DataViewTree, public enable_shared_from_this_base<DataView>, private MsgGenerator
{
	using base_type = Object;
public:
	typedef std::vector<SharedPtr<AbstrCaret> >      caret_vector;
	typedef caret_vector::iterator                   caret_iterator;

	DataView(TreeItem* viewContext);
	virtual ~DataView();

	virtual auto GetViewType() const->ViewStyle = 0;

	virtual void SetContents(std::shared_ptr<MovableObject> contents, ShvSyncMode sm);

//	composition
	void BringToTop();
#ifdef _WIN32
	void DestroyWindow();
	bool CreateMdiChild  (ViewStyle ct,     CharPtr caption);

//	Operations
	MsgResult DispatchMsg(const MsgStruct& msg);
#endif
	SHV_CALL bool OnKeyDown(UInt32 nVirtKey);
	SHV_CALL void OnResize(GPoint deviceSize, CrdPoint scaleFactors);

//	Attributes
	std::shared_ptr<MovableObject> GetContents()             { assert(m_Contents); return m_Contents; }
	std::shared_ptr<const MovableObject> GetContents() const { assert(m_Contents); return m_Contents; }
	TreeItem*      GetViewContext   () const { assert(m_ViewContext); return m_ViewContext.get(); }
	TreeItem*      GetDesktopContext() const;

	#ifdef _WIN32
	SHV_CALL void ResetHWnd(HWND hWnd);
	HWND     GetHWnd()        const { return m_ViewHost ? m_ViewHost->VH_GetHWnd() : m_hWnd; }
#endif
	SHV_CALL void SetViewHost(ViewHost* vh);
	ViewHost* GetViewHost() const { return m_ViewHost; }
	CrdPoint GetScaleFactors() const { assert(m_CurrScaleFactors.X() > 0.0 && m_CurrScaleFactors.Y() > 0.0);  return m_CurrScaleFactors; }
	CrdPoint GetReverseFactors() const { auto sf = GetScaleFactors(); return { 1.0 / sf.first, 1.0 / sf.second }; }
	CrdPoint Reverse(CrdPoint pnt) const { auto rf = GetReverseFactors();  pnt.X() *= rf.first; pnt.Y() *= rf.second; return pnt; }
	CrdRect  Reverse(CrdRect rect) const { return CrdRect(Reverse(rect.first), Reverse(rect.second)); }
	#ifdef _WIN32
	HFONT   GetDefaultFont(FontSizeCategory fid, Float64 scaleFactor) const;
	HFONT   GetDefaultFont(FontSizeCategory fid) const { return GetDefaultFont(fid, GetWindowDip2PixFactorY(GetHWnd())); }
#endif

	void SendStatusText(SeverityTypeID st, CharPtr msg) const;
	SHV_CALL void SetStatusTextFunc(ClientHandle clientHandle, StatusTextFunc stf);

	//	Contents
	virtual void AddLayer(const TreeItem*, bool isDropped) = 0;
	virtual bool CanContain(const TreeItem* viewCandidate) const=0;

	// Controller management
	void InsertController(AbstrController*);
	void RemoveController(AbstrController*);
	void RemoveAllControllers();

	// Caret management
	void InsertCaret(AbstrCaret*);
	void RemoveCaret(AbstrCaret*);
	void RemoveAllCarets();
	void MoveCaret  (AbstrCaret*, const AbstrCaretOperator&);
	void SetSelCaret (      Region& newSelCaret);
	void XOrSelCaret (const Region& newSelCaret);
	void SetTextCaret(const GPoint& caretPos);
	void ClearTextCaret();

	SHV_CALL auto OnCommandEnable(ToolButtonID id) const->CommandStatus;

	SHV_CALL void InvalidateDeviceRect(GRect  rect);
	void InvalidateRgn (const Region& rgn );
	void ValidateRect  (const GRect& pixRect);

	virtual GraphVisitState UpdateView();
	void ScrollDevice(GPoint delta, GRect rcScroll, GRect rcClip, const MovableObject* src);

	void Activate(MovableObject* src);
	void ActivatePrev();
	void SetCursorPos(GPoint clientPoint);

	GRect ViewDeviceRect() const { return GRect(GPoint(0, 0), m_ViewDeviceSize); }
	TRect ViewLogicalRect() const { return Convert<TRect>(Reverse(GRect2CrdRect(ViewDeviceRect()))); }

	void ShowPopupMenu(GPoint point, const MenuData& menuData) const;
	void SetFocusRect(GRect focusRect);
	void SetScrollEventsReceiver(ScrollPort* sp);

	virtual ExportInfo GetExportInfo(); // overruled by TableView and MapView, but not EditPaletteView
	virtual SharedStr GetCaption() const;

	bool IsInActiveDataViewSet();
	void PostGuiOper(operation_type&& func);
	SHV_CALL void OnTimer(UInt32 timerId); // Called by ViewHost timer callback

protected: // override virtuals of Actor
	ActorVisitState VisitSuppliers(SupplierVisitFlag svf, const ActorVisitor& visitor) const override;
	void DoInvalidate() const override;

// Implementation

private:
	void InvalidateChangedGraphics();

#ifdef _WIN32
	void SetCaretsVisible(bool visibility, HDC dc); friend struct CaretHider;
#endif
	void UpdateTextCaret();
#ifdef _WIN32
	void ReverseCarets(HDC dc, bool newVisibleState);
#endif
	void ReverseCaretsImpl(DrawContext& dc, bool newVisibleState);
	bool DispatchMouseEvent(EventID event, WPARAM modKeys, GPoint point);
#ifdef _WIN32
	void ReverseSelCaretImpl(HDC hdc, const Region& selCaretRgn);

	// message handlers
	void OnEraseBkgnd(HDC dc);
#endif
	void OnPaint();
	void SetUpdateTimer(); friend struct IdleTimer;
public:
	void RequestUpdate() { SetUpdateTimer(); } // portable: called by Qt ViewHost on invalidation

#ifdef _WIN32
	void OnMouseMove(WPARAM nFlags, GPoint devicePoint);
	void OnSize     (WPARAM nType,  GPoint deviceSize);
	void OnCaptureChanged(HWND hWnd);
#endif
	void OnActivate(bool becomeActive);
	void ProcessGuiOpers();
	void OnCopyData(UINT cmd, const UInt32* first, const UInt32* last);

	// ContextHandling
	void GenerateDescription() override;

	// rtti
public:
	void OnCaptionChanged() const;

protected:
	caret_vector                   m_CaretVector;
	std::shared_ptr<MovableObject> m_Contents;
	SharedPtr<TreeItem>            m_ViewContext;

	controller_vector             m_ControllerVector;

	CrdPoint                      m_CurrScaleFactors = CrdPoint(-1.0, -1.0);
	GPoint                        m_ViewDeviceSize = GPoint(0, 0);
	TimeStamp                     m_CheckedTS;
	mutable CounterStacks         m_DoneGraphics;
#ifdef _WIN32
	HWND                          m_hWnd;
#endif
	ViewHost*                     m_ViewHost = nullptr;
	GPoint                        m_TextCaretPos;
	Region                        m_SelCaret;
#ifdef _WIN32
	GdiHandle<HBRUSH>             m_SelBrush, m_BrdBrush;

	mutable std::map<Float64, GdiHandle<HFONT> > m_DefaultFonts[static_cast<int>(FontSizeCategory::COUNT)];
#endif

	StatusTextCaller              m_StatusTextCaller;

private:
	DataViewTree*                 m_ParentView;  friend DataViewTree;
	ScrollPort*                   m_ScrollEventsReceiver;

	operation_queue               m_GuiOperQueue;
	Waiter                        m_Waiter;

public:
	ToolButtonID                  m_ControllerID = TB_Neutral;
	ActivationInfo                m_ActivationInfo;
	std::weak_ptr<MovableObject>  m_PrevActivated;
	SharedPtr<FocusCaret>         m_FocusCaret;
	TextEditController            m_TextEditController;

#if defined(MG_DEBUG)
	std::atomic<UInt32>           md_IsDrawingCount;
	std::atomic<UInt32>           md_InvalidateDrawLock;
#endif //defined(MG_DEBUG)

	mutable UInt8 m_PaletteIndex = 0;
	DmsColor m_ColorPalette[nrPaletteColors];
	DmsColor GetNextDmsColor() const;

	DECL_RTTI(SHV_CALL, Class);

	// =============================================== ToolTip

public:
	// Called by GraphicObject when it shows/hides a tooltip
	void SetActiveTooltipObject(const GraphicObject* obj) noexcept;
	void ClearActiveTooltipObject(const GraphicObject* obj) noexcept;

	// Called by GraphicObject (lazy init) to access the tooltip HWND
#ifdef _WIN32
	HWND EnsureTooltipWindow();
#endif

	// Helpers used by watchdog
	bool IsCursorInsideObject(const GraphicObject& obj) const noexcept;

	GPoint m_hoverStartLocation{};

	// Watchdog state
	std::weak_ptr<const GraphicObject> m_activeTooltipObj;

	static constexpr UInt32 kTipWatchPeriodMs = 50;

	void StartTipWatchdog();
	void StopTipWatchdog();

	void HideActiveTooltip();

#ifdef _WIN32
	// Tooltip window (TOOLTIPS_CLASS)
	HWND m_hwndTooltip = nullptr;
	std::unique_ptr<wchar_t[]> m_ToolTipText;
#endif
};


//================================= ownership

void Keep(const std::shared_ptr<DataView>& self);
void Unkeep(DataView*);

void OnDestroyDataView(DataView* self);
void BroadcastCommandToAllDataViews(ToolButtonID id);
void BroadcastUpdateRequest();

#endif // !defined(__SHV_DATAVIEW_H)
