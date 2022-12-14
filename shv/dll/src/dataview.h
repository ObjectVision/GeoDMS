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
// SheetVisualTestView.h : interface of the DataView class
//
/////////////////////////////////////////////////////////////////////////////

#if !defined(__SHV_DATAVIEW_H)
#define __SHV_DATAVIEW_H

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

//----------------------------------------------------------------------
// used modules and forward class references
//----------------------------------------------------------------------

#include <vector>

#include "act/Actor.h"
#include "geo/Geometry.h"
#include "geo/Range.h"
#include "mci/DoubleLinkedList.h"
#include "ptr/OwningPtr.h"
#include "ptr/SharedPtr.h"


#include "AbstrController.h"
#include "AbstrCaret.h"
#include "ActivationInfo.h"
#include "CounterStacks.h"
#include "DcHandle.h"
#include "ExportInfo.h"
#include "GraphicLayer.h"
#include "GraphVisitor.h"
#include "LayerInfo.h"
#include "MovableObject.h"
#include "TextEditController.h"

#include "ShvUtils.h"

class AbstrCaretOperator;
class AbstrCmd;
class FocusCaret;

//----------------------------------------------------------------------
// struct : MsgStruct
//----------------------------------------------------------------------

// ViewStyle must be in sync with ilTreeview in unit fMain.dfm and TTreeItemViewStyle in uDmsInterface.pas
enum ViewStyle {
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
};
	
enum ViewStyleFlags {
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
// struct : MsgStruct
//----------------------------------------------------------------------

struct MsgStruct
{ 
	MsgStruct(DataView* dv, UINT msg, WPARAM wParam, LPARAM lParam, LRESULT* resultPtr)
		:	m_DataView(dv)
		,	m_Msg(msg)
		,	m_wParam(wParam)
		,	m_lParam(lParam)
		,	m_ResultPtr(resultPtr)
	{}

	DataView* m_DataView;
	UINT      m_Msg;
	WPARAM    m_wParam;
	LPARAM    m_lParam;
	LRESULT*  m_ResultPtr;

	bool Send() const;  // calls DataView::DispachMsg, and returns true if processed; else caller must call DefWindowProc
};

const UInt32 DVF_InUpdateView     = actor_flag_set::AF_Next * 0x0001;
const UInt32 DVF_HasFocus         = actor_flag_set::AF_Next * 0x0002;
const UInt32 DVF_HasTextCaret     = actor_flag_set::AF_Next * 0x0004;
const UInt32 DVF_CaretsVisible    = actor_flag_set::AF_Next * 0x0008;
const UInt32 DVF_TextCaretCreated = actor_flag_set::AF_Next * 0x0010;

const int nrPaletteColors = 16;

//----------------------------------------------------------------------
// class  : DataViewList
//----------------------------------------------------------------------

struct DataViewList : double_linked_list<DataView>
{
	void BringChildToTop(DataView* dv);
	void AddChildView(DataView* childView);
	void DelChildView(DataView* childView);
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

//----------------------------------------------------------------------
// class  : DataView
//----------------------------------------------------------------------

class DataView : public Actor, public DataViewList, public enable_shared_from_this_base<DataView>
{
	typedef Actor base_type;
public:
	typedef std::vector<SharedPtr<AbstrCaret> >      caret_vector;
	typedef caret_vector::iterator                   caret_iterator;

	DataView(TreeItem* viewContext);
	virtual ~DataView();

//	void StopOwning() { m_SelfOwned.reset(); }
//	std::shared_ptr<DataView> shared_from_this() { return m_SelfOwned; }

	virtual void SetContents(std::shared_ptr<MovableObject> contents, ShvSyncMode sm);

//	composition
	void BringToTop();
	void DestroyWindow();
	void CreateViewWindow(DataView* parent, CharPtr caption);
	bool CreateMdiChild  (ViewStyle ct,     CharPtr caption);

//	Operations
	bool DispatchMsg(const MsgStruct& msg);
	bool OnKeyDown(UInt32 nVirtKey);

	// Attributes
	std::shared_ptr<MovableObject> GetContents()             { dms_assert(m_Contents); return m_Contents; }
	std::shared_ptr<const MovableObject> GetContents() const { dms_assert(m_Contents); return m_Contents; }
	TreeItem*      GetViewContext   () const { dms_assert(m_ViewContext); return m_ViewContext; }
	TreeItem*      GetDesktopContext() const;

	SHV_CALL void ResetHWnd(HWND hWnd);
	HWND  GetHWnd()        const { return m_hWnd; }

	HFONT   GetDefaultFont(FontSizeCategory fid, Float64 subPixelFactor) const;

	void SendStatusText(SeverityTypeID st, CharPtr msg) const;
	void SetStatusTextFunc(ClientHandle clientHandle, StatusTextFunc stf);

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

	void InvalidateRect(const GRect&  rect);
	void InvalidateRgn (const Region& rgn );
	void ValidateRect  (const GRect&  rect);
	void ValidateRgn   (const Region& rgn );

	virtual GraphVisitState UpdateView();
	ActorVisitState UpdateViews();
	void Scroll(GPoint delta, const GRect& rcScroll, const GRect& rcClip, const MovableObject* src);

	void Activate(MovableObject* src);
	void SetCursorPos(GPoint clientPoint);

	GRect ViewRect() const { return GRect(GPoint(0, 0), m_ViewSize); }

	void ShowPopupMenu(const GPoint& point, const MenuData& menuData) const;
	void SetFocusRect(const GRect& focusRect);
	void SetScrollEventsReceiver(ScrollPort* sp);

	virtual ExportInfo GetExportInfo(); // overruled by TableView and MapView, but not EditPaletteView
	virtual SharedStr GetCaption() const;

#if defined(MG_DEBUG_DATAVIEWSET)
	bool IsInActiveDataViewSet();
#endif
	void AddGuiOper(std::function<void()>&& func);

protected: // override virtuals of Actor 
	ActorVisitState VisitSuppliers(SupplierVisitFlag svf, const ActorVisitor& visitor) const override;
	void DoInvalidate() const override;

// Implementation

private:
	void InvalidateChangedGraphics();

	void SetCaretsVisible(bool visibility, HDC dc); friend struct CaretHider;
	void UpdateTextCaret();
	void ReverseCarets(HDC dc, bool newVisibleState); // creates a new tmp dc if pdc==0
	void ReverseCaretsImpl(HDC  dc, bool newVisibleState);
	void ReverseSelCaretImpl(HDC hdc, const Region& selCaretRgn);
	bool DispatchMouseEvent(UInt32 event, WPARAM modKeys, const GPoint& point);

	// message handlers
	void OnEraseBkgnd(HDC dc);
	void OnPaint();

	void OnMouseMove(WPARAM nFlags, const GPoint& point);
	void OnSize     (WPARAM nType,  const GPoint& point);
	void OnCaptureChanged(HWND hWnd);
	void OnActivate(bool becomeActive);
	void ProcessGuiOpers();
	void OnCopyData(UINT cmd, const UInt32* first, const UInt32* last);

	MG_DEBUGCODE( bool IsShvObj() const override { return true; } )
public:
	void OnCaptionChanged() const;

protected:
	caret_vector                   m_CaretVector;
	std::shared_ptr<MovableObject> m_Contents;
	SharedPtr<TreeItem>            m_ViewContext;

	controller_vector             m_ControllerVector;

	GPoint                        m_ViewSize;
	TimeStamp                     m_CheckedTS;
	mutable CounterStacks         m_DoneGraphics;
	HWND                          m_hWnd;
	GPoint                        m_TextCaretPos;
	Region                        m_SelCaret;
	GdiHandle<HBRUSH>             m_SelBrush, m_BrdBrush;

	mutable std::map<Float64, GdiHandle<HFONT> > m_DefaultFonts[static_cast<int>(FontSizeCategory::COUNT)];

	StatusTextCaller              m_StatusTextCaller;

private:
	DataViewList*                 m_ParentView;  friend DataViewList;
	ScrollPort*                   m_ScrollEventsReceiver;

	std::list < std::function<void()>>  m_GuiOperQueue;

public:
	ToolButtonID                  m_ControllerID = TB_Neutral;
	ActivationInfo                m_ActivationInfo;
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
};

//================================= ownership

void Keep(const std::shared_ptr<DataView>& self);
void Unkeep(DataView*);

void OnDestroyDataView(DataView* self);

SHV_CALL LRESULT CALLBACK DataViewWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

#endif // !defined(__SHV_DATAVIEW_H)
