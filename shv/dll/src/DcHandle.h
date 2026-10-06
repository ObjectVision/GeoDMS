// Copyright (C) 1998-2025 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#pragma once

#if !defined(__SHV_DCHANDLE_H)
#define __SHV_DCHANDLE_H

#ifdef _WIN32

//----------------------------------------------------------------------
// used modules and forward class references
//----------------------------------------------------------------------

#include "ShvUtils.h"

#include "utl/Environment.h"
#include "utl/PlatformError.h"
#include "utl/swapper.h"

//----------------------------------------------------------------------
// DcHandleBase
//----------------------------------------------------------------------

struct DcHandleBase : private geodms::rtc::noncopyable
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

struct CompatibleDcHandle : private geodms::rtc::noncopyable
{
	explicit CompatibleDcHandle(HDC hdc, HFONT defaultFont);
	~CompatibleDcHandle();

	HDC GetHDC()   { return m_hDC; }
	operator HDC() { return GetHDC(); }


private:
	HDC  m_hDC;
};

//----------------------------------------------------------------------
// PaintDcHandle
//----------------------------------------------------------------------

struct PaintDcHandle : private geodms::rtc::noncopyable
{
	explicit PaintDcHandle(HWND hWnd, HFONT defaultFont);
	~PaintDcHandle();

	HDC GetHDC()   { return m_PaintInfo.hdc; }
	operator HDC() { return m_PaintInfo.hdc; }
	GRect GetClipRect() const { return RECTToGRect(m_PaintInfo.rcPaint); }
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
// GdiHandle<HandleType>
//----------------------------------------------------------------------

#include "TypeInfoOrdering.h"

template <typename HandleType>
struct GdiHandle : private geodms::rtc::noncopyable
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
struct GdiObjectSelector : private geodms::rtc::noncopyable
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
// Portable visitor helpers moved to GraphVisitor.h:
//   AddTransformation, AddClientLogicalOffset,
//   ClipDeviceRectSelector, VisitorDeviceRectSelector
//----------------------------------------------------------------------

#endif // _WIN32

#endif // !defined(__SHV_DCHANDLE_H)
