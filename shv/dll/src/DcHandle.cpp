// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "ShvDllPch.h"

#include "DcHandle.h"

#include "dbg/debug.h"
#include "utl/Environment.h"
#include "ser/AsString.h"
#include "utl/Environment.h"

#include "DataView.h"
#include "GraphVisitor.h"
#include "GraphicObject.h"

void CustomizeDC(HDC hdc, HFONT defaultFont)
{
	SetBkMode(hdc, TRANSPARENT);
	if (defaultFont)
		SelectObject(hdc, defaultFont);
}


//----------------------------------------------------------------------
// DcHandleBase
//----------------------------------------------------------------------

DcHandleBase::DcHandleBase(HWND hWnd)
		:	m_hWnd(hWnd)
		,	m_hDC(GetDC(hWnd))
{
	if (!m_hDC)
		throwLastSystemError("DcHandle");
}

DcHandleBase::~DcHandleBase()
{
	ReleaseDC(m_hWnd, m_hDC);
}


//----------------------------------------------------------------------
// DcHandle
//----------------------------------------------------------------------

DcHandle::DcHandle(HWND hWnd, HFONT defaultFont)
		:	DcHandleBase(hWnd)
{
	CustomizeDC(GetHDC(), defaultFont);
}

//----------------------------------------------------------------------
// CompatibleDcHandle
//----------------------------------------------------------------------

CompatibleDcHandle::CompatibleDcHandle(HDC hdc, HFONT defaultFont)
	:	m_hDC(CreateCompatibleDC(hdc))
{
	CustomizeDC(m_hDC, defaultFont);
}

CompatibleDcHandle::~CompatibleDcHandle()
{
	DeleteDC(m_hDC);
}


//----------------------------------------------------------------------
// CaretDcHandle
//----------------------------------------------------------------------

CaretDcHandle::CaretDcHandle(HWND hWnd, HFONT defaultFont)
	:	DcHandle(hWnd, defaultFont)
{
	SetROP2(GetHDC(), R2_NOTXORPEN);
}


//----------------------------------------------------------------------
// PaintDcHandle
//----------------------------------------------------------------------

PaintDcHandle::PaintDcHandle(HWND hWnd, HFONT defaultFont)
	: m_hWnd(hWnd)
{
	dms_assert(hWnd);
	BeginPaint(hWnd, &m_PaintInfo);
	CustomizeDC(GetHDC(), defaultFont);
}

PaintDcHandle::~PaintDcHandle()
{
	EndPaint(m_hWnd, &m_PaintInfo);
}


//----------------------------------------------------------------------
// CaretHider
//----------------------------------------------------------------------

CaretHider::CaretHider(DataView* view, HDC hdc) 
	: m_View(view)
	, m_hDC(hdc)
	, m_WasVisible(view->m_State.Get(DVF_CaretsVisible)) 
{ 
	DBG_START("CaretHider", "CONSTRUCTOR", MG_DEBUG_CARET);
	DBG_TRACE(("WasVisible %d", m_WasVisible));
	DBG_TRACE(("HDC        %x", hdc));

	view->SetCaretsVisible(false, hdc); 
}

CaretHider::~CaretHider()
{ 
	DBG_START("CaretHider", "DESTRUCTOR", MG_DEBUG_CARET);
	try {
		m_View->SetCaretsVisible(m_WasVisible, m_hDC);
	}
	catch (...) {}
}

//----------------------------------------------------------------------
//	ClippedDC
//----------------------------------------------------------------------

ClippedDC::ClippedDC(DataView* dv, const Region& rgn)
	:	DcHandle(dv->GetHWnd(), dv->GetDefaultFont(FontSizeCategory::MEDIUM) )
{
	m_Empty = ( SelectClipRgn(GetHDC(), rgn.GetHandle() ) == NULLREGION );
}

//----------------------------------------------------------------------
//	DirectDC
//----------------------------------------------------------------------

DirectDC::DirectDC(DataView* dv, const Region& rgn)
	:	ClippedDC(dv, rgn)
	,	m_CaretHider(dv, GetHDC())
{}


//----------------------------------------------------------------------
// AddTransformation
//----------------------------------------------------------------------

AddTransformation::AddTransformation(GraphVisitor* v, const CrdTransformation& w2v)
	:	tmp_swapper<CrdTransformation>(v->m_Transformation, w2v * v->m_Transformation)
{
}

//----------------------------------------------------------------------
// AddClientLogicalOffset
//----------------------------------------------------------------------

AddClientLogicalOffset::AddClientLogicalOffset(GraphVisitor* v, CrdPoint c2p)
	: clientSwapper(v->m_ClientLogicalAbsPos, v->m_ClientLogicalAbsPos + c2p)
{}

//----------------------------------------------------------------------
// VistorRectSelector
//----------------------------------------------------------------------

VisitorDeviceRectSelector::VisitorDeviceRectSelector(GraphVisitor* v, GRect objRect)
	:	ClipDeviceRectSelector(v->m_ClipDeviceRect, objRect)
{
}

//----------------------------------------------------------------------
// DcClipRegionSelector
//----------------------------------------------------------------------

DcClipRegionSelector::DcClipRegionSelector(HDC hdc, Region& currClipRegion, const GRect& newClipRect)
	:	m_hDC(hdc)
	,	m_OrgRegionPtr (&currClipRegion )
	,	m_OrgRegionCopy(currClipRegion.Clone() )
{
	DBG_START("DcClipRegionSelector", "ctor", MG_DEBUG_REGION);
	DBG_TRACE(("NewRect    %s", AsString(   newClipRect).c_str()));
	DBG_TRACE(("CurrRegion %s", currClipRegion.AsString().c_str()));

	dms_assert(! m_OrgRegionCopy.Empty() ); // else we shoudn't get here at all

	currClipRegion &= newClipRect;

	if (! currClipRegion.Empty() )
	{
		int result = SelectClipRgn(hdc, currClipRegion.GetHandle() );

		if (result == ERROR && GetLastError() )
			throwLastSystemError("DcClipRegionSelector");
	}
}

DcClipRegionSelector::~DcClipRegionSelector()
{
	dms_assert(m_OrgRegionPtr);

	if (! m_OrgRegionPtr->Empty() )
	{
		SelectClipRgn(
			m_hDC, 
			m_OrgRegionCopy.GetHandle()
		);
	}
	m_OrgRegionPtr->swap(m_OrgRegionCopy);

	assert(! m_OrgRegionPtr->Empty() ); // else we shoudn't get here at all
}


//----------------------------------------------------------------------
// DcMixModeSelector
//----------------------------------------------------------------------

// s = source bit, t = target bit, x = s^t; r = ~x = R2_NOTXOR(s,t)
//	s t x r
//	0 0 0 1
//	0 1 1 0
//	1 0 1 0
//	1 1 0 1

DcMixModeSelector::DcMixModeSelector(HDC hdc, int fnDrawMode)
	:	m_hDC(hdc)
	,	m_oldDrawMode(SetROP2(hdc, fnDrawMode) )
#if defined(MG_DEBUG_DATA)
	,	m_selDrawMode(fnDrawMode)
#endif
{
	dms_assert(hdc);
	dms_assert(fnDrawMode != 0);
	if (m_oldDrawMode == 0)
		throwLastSystemError("DcMixModeSelector::ctor");
}

DcMixModeSelector::~DcMixModeSelector()
{
	int result = SetROP2(m_hDC, m_oldDrawMode);
	MGD_CHECKDATA(result == m_selDrawMode);
}


//----------------------------------------------------------------------
// DcTextAlignSelector
//----------------------------------------------------------------------

DcTextAlignSelector::DcTextAlignSelector(HDC hdc, UINT fTextAlignMode)
	:	m_hDC(hdc)
	,	m_oldTextAlignMode(SetTextAlign(hdc, fTextAlignMode) )
#if defined(MG_DEBUG)
	,	m_selTextAlignMode(fTextAlignMode)
#endif
{
	dms_assert(hdc);
	dms_assert(fTextAlignMode != GDI_ERROR);
	if (m_oldTextAlignMode == GDI_ERROR)
		throwLastSystemError("DcTextAlignSelector::ctor");
}

DcTextAlignSelector::~DcTextAlignSelector()
{
	UINT result = SetTextAlign(m_hDC, m_oldTextAlignMode);
	dbg_assert(result == m_selTextAlignMode);
}


//----------------------------------------------------------------------
// DcTextColorSelector
//----------------------------------------------------------------------

DcTextColorSelector::DcTextColorSelector(HDC hdc, DmsColor selColor)
	:	m_hDC(hdc)
	,	m_oldTextColor(hdc ? SetTextColor(hdc, DmsColor2COLORREF(selColor)) : CLR_INVALID)
{
//	dms_assert(hdc);
	dms_assert(selColor != CLR_INVALID || !hdc);
	if (hdc && m_oldTextColor == CLR_INVALID)
		throwLastSystemError("DcTextColorSelector::ctor");
}

DcTextColorSelector::~DcTextColorSelector()
{
	if (m_hDC)
		SetTextColor(m_hDC, m_oldTextColor);
}


//----------------------------------------------------------------------
// DcBackColorSelector
//----------------------------------------------------------------------

DcBackColorSelector::DcBackColorSelector(HDC hdc, DmsColor crColor)
	:	m_hDC(crColor == CLR_INVALID ? nullptr : hdc)
	,	m_oldBkColor(m_hDC ? SetBkColor(m_hDC, DmsColor2COLORREF(crColor)) : CLR_INVALID )
{
	dms_assert(crColor != CLR_INVALID || !m_hDC);
	if (m_hDC && m_oldBkColor == CLR_INVALID)
		throwLastSystemError("DcBackColorSelector::ctor");
}

DcBackColorSelector::~DcBackColorSelector()
{
	if (m_hDC)
		SetBkColor(m_hDC, m_oldBkColor);
}


//----------------------------------------------------------------------
// DcBkModeSelector
//----------------------------------------------------------------------

DcBkModeSelector::DcBkModeSelector(HDC hdc, int iBkMode)
	:	m_hDC(hdc)
	,	m_oldBkMode(hdc ? SetBkMode(hdc, iBkMode) : 0 )
#if defined(MG_DEBUG)
	,	m_selBkMode(iBkMode)
#endif
{
	dms_assert(iBkMode != 0 || !hdc);
	if (hdc && m_oldBkMode == 0)
		throwLastSystemError("DcBkModeSelector::ctor");
}

DcBkModeSelector::~DcBkModeSelector()
{
	if (m_hDC)
	{
		UINT result = SetBkMode(m_hDC, m_oldBkMode);
		dbg_assert(result == m_selBkMode);
	}
}


//----------------------------------------------------------------------
// DcPolyFillModeSelector
//----------------------------------------------------------------------

DcPolyFillModeSelector::DcPolyFillModeSelector(HDC hdc, int polyFillMode)
	:	m_hDC(hdc)
	,	m_oldPolyFillMode(SetPolyFillMode(hdc, polyFillMode) )
#if defined(MG_DEBUG)
	,	m_selPolyFillMode(polyFillMode)
#endif
{
	dms_assert(hdc);
	dms_assert(polyFillMode != 0);
	if (m_oldPolyFillMode == 0)
		throwLastSystemError("DcPolyFillSelector::ctor");
}

DcPolyFillModeSelector::~DcPolyFillModeSelector()
{
	int result = SetPolyFillMode(m_hDC, m_oldPolyFillMode);
	dbg_assert(result == m_selPolyFillMode);
}

//----------------------------------------------------------------------
// DcBrushOrgSelector
//----------------------------------------------------------------------

DcBrushOrgSelector::DcBrushOrgSelector(HDC hdc, GPoint brushOrg)
	:	m_hDC(hdc)
	,	m_oldBrushOrg(0, 0)
#if defined(MG_DEBUG)
	,	m_selBrushOrg(brushOrg)
#endif
{
	dms_assert(hdc);
	
	CheckedGdiCall(GetBrushOrgEx(hdc, &m_oldBrushOrg), "DcBrushOrgSelector");
	brushOrg += m_oldBrushOrg;

	SetBrushOrgEx(hdc, brushOrg.x, brushOrg.y, &m_oldBrushOrg);
}

DcBrushOrgSelector::~DcBrushOrgSelector()
{
#if defined(MG_DEBUG)
	GPoint currBrushOrg;
	GetBrushOrgEx(m_hDC, &currBrushOrg);
	dms_assert(currBrushOrg == m_selBrushOrg + m_oldBrushOrg);
#endif
	int result = SetBrushOrgEx(m_hDC, m_oldBrushOrg.x, m_oldBrushOrg.y, NULL);
}
