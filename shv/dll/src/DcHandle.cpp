// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////

#include "ShvDllPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif

// GDI device-context handle wrappers used by the shv drawing code.

#include "DcHandle.h"

#include "dbg/debug.h"
#include "utl/Environment.h"
#include "utl/PlatformError.h"
#include "ser/AsString.h"

#include "DataView.h"
#include "DrawContext.h"
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
// PaintDcHandle
//----------------------------------------------------------------------

PaintDcHandle::PaintDcHandle(HWND hWnd, HFONT defaultFont)
	: m_hWnd(hWnd)
{
	assert(hWnd);
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
	DBG_TRACE(("WasVisible {}", m_WasVisible));
	DBG_TRACE(("HDC        {:x}", hdc));

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

DcClipRegionSelector::DcClipRegionSelector(DrawContext* dc, Region& currClipRegion, const GRect& newClipRect)
	:	m_DC(dc)
	,	m_OrgRegionPtr (&currClipRegion )
	,	m_OrgRegionCopy(currClipRegion.Clone() )
{
	DBG_START("DcClipRegionSelector", "ctor", MG_DEBUG_REGION);
	DBG_TRACE(("NewRect    {}", AsString(   newClipRect).c_str()));
	DBG_TRACE(("CurrRegion {}", currClipRegion.AsString().c_str()));

	dms_assert(! m_OrgRegionCopy.Empty() ); // else we shoudn't get here at all

	currClipRegion &= newClipRect;

	if (! currClipRegion.Empty() && m_DC)
		m_DC->SetClipRegion(currClipRegion);
}

DcClipRegionSelector::~DcClipRegionSelector()
{
	dms_assert(m_OrgRegionPtr);

	if (! m_OrgRegionPtr->Empty() && m_DC)
		m_DC->SetClipRegion(m_OrgRegionCopy);
	m_OrgRegionPtr->swap(m_OrgRegionCopy);

	assert(! m_OrgRegionPtr->Empty() ); // else we shoudn't get here at all
}
