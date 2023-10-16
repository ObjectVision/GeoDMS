// Copyright (C) 2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#pragma once

#if !defined(__SHV_DCHANDLE_H)
#define __SHV_DCHANDLE_H

//----------------------------------------------------------------------
// used modules and forward class references
//----------------------------------------------------------------------

#include "ShvUtils.h"

#include "utl/Environment.h"
#include "utl/swapper.h"

//----------------------------------------------------------------------
// DcHandleBase
//----------------------------------------------------------------------

struct DcHandleBase : private boost::noncopyable
{
	explicit DcHandleBase(HWND hWnd);
	~DcHandleBase();

	HDC GetHDC()   { return m_hDC; }
	operator HDC() { return GetHDC(); }

private:
	HDC  m_hDC;
	HWND m_hWnd;
};

//----------------------------------------------------------------------
// DcHandle
//----------------------------------------------------------------------

struct DcHandle : DcHandleBase
{
	explicit DcHandle(HWND hWnd, HFONT defaultFont);
};

//----------------------------------------------------------------------
// CompatibleDcHandle
//----------------------------------------------------------------------

struct CompatibleDcHandle : private boost::noncopyable
{
	explicit CompatibleDcHandle(HDC hdc, HFONT defaultFont);
	~CompatibleDcHandle();

	HDC GetHDC()   { return m_hDC; }
	operator HDC() { return GetHDC(); }


private:
	HDC  m_hDC;
};

//----------------------------------------------------------------------
// CaretDcHandle
//----------------------------------------------------------------------

struct CaretDcHandle : DcHandle
{
	explicit CaretDcHandle(HWND hWnd, HFONT defaultFont);
};

//----------------------------------------------------------------------
// PaintDcHandle
//----------------------------------------------------------------------

struct PaintDcHandle : private boost::noncopyable
{
	explicit PaintDcHandle(HWND hWnd, HFONT defaultFont);
	~PaintDcHandle();

	HDC GetHDC()   { return m_PaintInfo.hdc; }
	operator HDC() { return m_PaintInfo.hdc; }
	GRect GetClipRect() const { return m_PaintInfo.rcPaint; }
	bool  MustEraseBkgnd() const { return m_PaintInfo.fErase; }

private:
	PAINTSTRUCT m_PaintInfo;
	HWND        m_hWnd;
};

//----------------------------------------------------------------------
// CaretHider
//----------------------------------------------------------------------

struct CaretHider {
	CaretHider(DataView* view, HDC hdc);
	~CaretHider();
private: 
	DataView*   m_View;
	HDC         m_hDC; 
	bool        m_WasVisible;
};

//----------------------------------------------------------------------
//	ClippedDC
//----------------------------------------------------------------------

struct ClippedDC : DcHandle
{
	explicit ClippedDC(DataView* dv, const Region& rgn);
	bool IsEmpty() const { return m_Empty; }
private:
	bool m_Empty;
};

//----------------------------------------------------------------------
//	DirectDC
//----------------------------------------------------------------------

struct DirectDC : ClippedDC
{
	explicit DirectDC(DataView* dv, const Region& rgn);

private:
	CaretHider m_CaretHider;
};

//----------------------------------------------------------------------
// GdiHandle<HandleType>
//----------------------------------------------------------------------

#include "utl/TypeInfoOrdering.h"

template <typename HandleType>
struct GdiHandle : private boost::noncopyable
{
	GdiHandle() // default ctor does not obtain a resource
		:	m_hGdiObj(NULL) 
	{} 

	explicit GdiHandle(HandleType hGdiObj)
		:	m_hGdiObj(hGdiObj) 
	{
		CheckedGdiCall(hGdiObj != nullptr, GetName(typeid(GdiHandle<HandleType>) ) );
	}

	GdiHandle(HandleType hGdiObj, const void*) // special ctor that doesn't throw for use in stack unwinding
		:	m_hGdiObj(hGdiObj) 
	{}

	GdiHandle(GdiHandle&& rhs) noexcept
		:	m_hGdiObj(rhs.release())
	{}

	void operator = (GdiHandle&& rhs) noexcept { std::swap(m_hGdiObj, rhs.m_hGdiObj); }
	void operator = (HandleType hGdiObj) noexcept { operator = (GdiHandle(hGdiObj)); }

	~GdiHandle() 
	{
		if (m_hGdiObj)
			DeleteObject(m_hGdiObj);
	}
	operator HandleType() const { return m_hGdiObj; }

	void swap(GdiHandle& oth) noexcept { std::swap(m_hGdiObj, oth.m_hGdiObj); }
	HandleType release() { HandleType result = m_hGdiObj; m_hGdiObj = NULL; return result; }

private:
	HandleType m_hGdiObj;
};

//----------------------------------------------------------------------
// GdiObjectSelector<HandleType>
//----------------------------------------------------------------------

template <typename HandleType>
struct GdiObjectSelector : private boost::noncopyable
{
	GdiObjectSelector(HDC hdc, HandleType gdiObj)
		:	m_hDC(hdc)
		,	m_hOldGdiObj(hdc ? reinterpret_cast<HandleType>(SelectObject(hdc, gdiObj)): 0)
#if defined(MG_DEBUG_DATA)
		,	m_hSelGdiObj(gdiObj)
#endif
	{
		if (hdc)
		{
			dms_assert(gdiObj != NULL);
			if (m_hOldGdiObj == NULL)
				throwLastSystemError(
					GetName(typeid(GdiObjectSelector<HandleType>))
				);
		}
	}

	~GdiObjectSelector()
	{
		if (m_hDC)
		{
			HandleType hCurrGdiObj = reinterpret_cast<HandleType>( SelectObject(m_hDC, m_hOldGdiObj) );
			MGD_CHECKDATA(hCurrGdiObj == m_hSelGdiObj);
		}
	}

private:
	HDC        m_hDC;
	HandleType m_hOldGdiObj;
#if defined(MG_DEBUG_DATA)
	HandleType m_hSelGdiObj;
#endif
};

//----------------------------------------------------------------------
// AddTransformation
//----------------------------------------------------------------------

class GraphVisitor;

struct AddTransformation : private tmp_swapper<CrdTransformation>
{
	AddTransformation(GraphVisitor* v, const CrdTransformation& w2v);
};

//----------------------------------------------------------------------
// AddClientLogicalOffset
//----------------------------------------------------------------------

struct AddClientLogicalOffset
{
	AddClientLogicalOffset(GraphVisitor* v, CrdPoint c2p);

private:
	tmp_swapper<CrdPoint> clientSwapper;
};

//----------------------------------------------------------------------
// ClipRectSelector
//----------------------------------------------------------------------

struct ClipDeviceRectSelector : private tmp_swapper<GRect>
{
	ClipDeviceRectSelector(GRect& clipRect, const GRect& newClip)
		:	tmp_swapper<GRect>(clipRect, newClip)
	{
		m_ResourceHandleRef &= m_ResourceHandleCopy; // newClip &= clipRect
	}
	bool empty() const 
	{
		return m_ResourceHandleRef.empty();
	}
};


//----------------------------------------------------------------------
// VistorRectSelector
//----------------------------------------------------------------------

struct VisitorDeviceRectSelector : ClipDeviceRectSelector
{
	VisitorDeviceRectSelector(GraphVisitor* v, GRect objRect);
};


//----------------------------------------------------------------------
// DcMixModeSelector
//----------------------------------------------------------------------

struct DcMixModeSelector : private boost::noncopyable
{
	explicit DcMixModeSelector(HDC hdc, int fnDrawMode = R2_NOTXORPEN);
	~DcMixModeSelector();

private:
	HDC m_hDC;
	int m_oldDrawMode;
#if defined(MG_DEBUG_DATA)
	int m_selDrawMode;
#endif
};

//----------------------------------------------------------------------
// DcTextAlignSelector
//----------------------------------------------------------------------

struct DcTextAlignSelector : private boost::noncopyable
{
	explicit DcTextAlignSelector(HDC hdc, UINT fTextAlignMode = TA_LEFT|TA_TOP|TA_NOUPDATECP);
	~DcTextAlignSelector();

private:
	HDC m_hDC;
	UINT m_oldTextAlignMode;
#if defined(MG_DEBUG)
	UINT m_selTextAlignMode;
#endif
};

//----------------------------------------------------------------------
// DcTextColorSelector
//----------------------------------------------------------------------

struct DcTextColorSelector : private boost::noncopyable
{
	explicit DcTextColorSelector(HDC hdc, DmsColor crColor);
	~DcTextColorSelector();

private:
	HDC      m_hDC;
	COLORREF m_oldTextColor;
};

//----------------------------------------------------------------------
// DcBackColorSelector
//----------------------------------------------------------------------

struct DcBackColorSelector : private boost::noncopyable
{
	explicit DcBackColorSelector(HDC hdc, DmsColor crColor);
	~DcBackColorSelector();

private:
	HDC      m_hDC;
	COLORREF m_oldBkColor;
};

//----------------------------------------------------------------------
// DcBkModeSelector
//----------------------------------------------------------------------

struct DcBkModeSelector : private boost::noncopyable
{
	explicit DcBkModeSelector(HDC hdc, int iBkMode = OPAQUE);
	~DcBkModeSelector();

private:
	HDC m_hDC;
	int m_oldBkMode;
#if defined(MG_DEBUG)
	int m_selBkMode;
#endif
};

//----------------------------------------------------------------------
// DcPolyFillModeSelector
//----------------------------------------------------------------------

struct DcPolyFillModeSelector : private boost::noncopyable
{
	explicit DcPolyFillModeSelector(HDC hdc, int polyFillMode = ALTERNATE);
	~DcPolyFillModeSelector();

private:
	HDC m_hDC;
	int m_oldPolyFillMode;
#if defined(MG_DEBUG)
	int m_selPolyFillMode;
#endif
};

//----------------------------------------------------------------------
// DcPolyFillModeSelector
//----------------------------------------------------------------------

struct DcBrushOrgSelector : private boost::noncopyable
{
	explicit DcBrushOrgSelector(HDC hdc, GPoint brushOrg);
	~DcBrushOrgSelector();

private:
	HDC m_hDC;
	GPoint m_oldBrushOrg;
#if defined(MG_DEBUG)
	GPoint m_selBrushOrg;
#endif
};



#endif // !defined(__SHV_DCHANDLE_H)
