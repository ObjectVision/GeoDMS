// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////

#include "ShvDllPCH.h"
#include "act/UpdateMark.h" // UpdateMarker

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif

// Assorted shv helpers (see ShvUtils.h): values-unit and classification
// lookups, ViewData properties, status text and selection utilities.

#include <format>
#include "ShvUtils.h"

#include "dbg/debug.h"
#include "dbg/DebugContext.h"
#include "vt/BaseBounds.h"
#include "vt/Conversions.h"
#include "vt/Pair.h"
#include "mci/Class.h"
#include "mci/CompositeCast.h"
#include "mci/ValueClass.h"
#include "mci/ValueWrap.h"
#include "utl/Environment.h"
#include "utl/PlatformError.h"
#include "utl/IncrementalLock.h"
#include "utl/StrFormat.h"
#include "utl/splitPath.h"

#include "AbstrDataItem.h"
#include "AbstrDataObject.h"
#include "AbstrUnit.h"
#include "DataArray.h"
#include "DataArrayValue.h"
#include "DataItemClass.h"
#include "DataLocks.h"
#include "Projection.h"
#include "PropFuncs.h"
#include "SessionData.h"
#include "TicInterface.h"
#include "TreeItemClass.h"
#include "Unit.h"
#include "UnitClass.h"
#include "UnitProcessor.h"

#include "StgBase.h"

#include "CalcClassBreaks.h"
#include "ValuesTable.h"

#include "GeoTypes.h"

#include "Aspect.h"

#include "DataView.h"
#include "DcHandle.h"
#include "GraphicObject.h"
#include "LayerClass.h"
#include "Theme.h"

#ifdef _WIN32
#include "shellscalingapi.h"
#endif


// GDI drawing utilities: CheckedGdiCall, color getters, FillRectWithBrush /
// ShadowRect / border drawing, FontSizeCategory and the DIP<->pixel factor
// helpers, and the portable color functions.
// Split from ShvUtils.cpp (2026-08).


//----------------------------------------------------------------------
// section : CheckedGdiCall
//----------------------------------------------------------------------

#ifdef _WIN32

void CheckedGdiCall(bool result, CharPtr context)
{
	if (result)
		return;
	DWORD lastErr = GetLastError();
	if (lastErr)
		throwSystemError(lastErr, context);
}

//----------------------------------------------------------------------
// section : Colors
//----------------------------------------------------------------------

// Fixed magenta focus highlight (issue #1039): the OS COLOR_HIGHLIGHT is blue and
// collides with blue layer colors; magenta is distinct from both data palettes and
// the yellow selection color. Kept fixed (not OS-following) for cross-platform parity.
COLORREF GetFocusClr() { return DmsColor2COLORREF(CombineRGB(255, 0, 255)); }
COLORREF GetFocusTextClr() { return ::GetSysColor(COLOR_HIGHLIGHTTEXT); }
COLORREF GetDefaultClr(UInt32 i) { return DmsColor2COLORREF(STG_Bmp_GetDefaultColor(i)); }
COLORREF GetSelectedClr() { return DmsColor2COLORREF(CombineRGB(255, 255,0)); }

//----------------------------------------------------------------------
// section : DrawBorder
//----------------------------------------------------------------------

void FillRectWithBrush(HDC dc, const GRect& rect, HBRUSH br)
{
	GdiHandle<HPEN>           invisiblePen(CreatePen(PS_NULL, 0, RGB(0, 0, 0)));
	GdiObjectSelector<HPEN>   penSelector(dc, invisiblePen);

	GdiObjectSelector<HBRUSH> brushSelector(dc, br);
	Rectangle(dc, rect.left, rect.top, rect.right+1, rect.bottom+1);
}

void ShadowRect(HDC dc, GRect rect, HBRUSH lightBrush, HBRUSH darkBrush)
{
	if (rect.top >= rect.bottom || rect.left >= rect.right)
		return;

	Int32 nextLeft   = rect.left+1;
	Int32 nextTop    = rect.top+1;
	Int32 prevRight  = rect.right - 1;
	Int32 prevBottom = rect.bottom - 1;

	FillRectWithBrush(dc, GRect(prevRight, rect.top,   rect.right, rect.bottom), darkBrush );  // right vertical line
	FillRectWithBrush(dc, GRect(rect.left, prevBottom, prevRight,  rect.bottom), darkBrush );  // bottom horizontal line

	if (rect.top >= prevBottom || rect.left >= prevRight)
		return;

	FillRectWithBrush(dc, GRect(rect.left, rect.top, prevRight,  nextTop ), lightBrush );  // top horizontal line
	FillRectWithBrush(dc, GRect(rect.left, nextTop,  nextLeft, prevBottom), lightBrush );  // left vertical line
}

void DrawButtonBorder(HDC dc, GRect& clientDeviceRect)
{
	HBRUSH lightBrush = GetSysColorBrush(COLOR_3DLIGHT);
	HBRUSH blackBrush = GetSysColorBrush(COLOR_3DDKSHADOW);

	ShadowRect(dc, clientDeviceRect, lightBrush, blackBrush);
	clientDeviceRect.Shrink(1);

	HBRUSH whiteBrush = GetSysColorBrush(COLOR_3DHIGHLIGHT);
	HBRUSH shadowBrush= GetSysColorBrush(COLOR_3DSHADOW);

	ShadowRect(dc, clientDeviceRect, whiteBrush, shadowBrush);
	clientDeviceRect.Shrink(1);
}

void DrawReversedBorder(HDC dc, GRect& clientDeviceRect)
{
	HBRUSH lightBrush = GetSysColorBrush(COLOR_3DLIGHT);
	HBRUSH blackBrush = GetSysColorBrush(COLOR_3DDKSHADOW);

	ShadowRect(dc, clientDeviceRect, blackBrush, lightBrush);
	clientDeviceRect.Shrink(1);

	HBRUSH whiteBrush = GetSysColorBrush(COLOR_3DHIGHLIGHT);
	HBRUSH shadowBrush= GetSysColorBrush(COLOR_3DSHADOW);

	ShadowRect(dc, clientDeviceRect, shadowBrush, whiteBrush);
	clientDeviceRect.Shrink(1);
}

void DrawRectDmsColor(HDC dc, const GRect& rect, DmsColor color)
{
	GdiHandle<HBRUSH> brush(
		CreateSolidBrush( DmsColor2COLORREF( color ) ),
		"DrawRectDmsColor::CreateSolidBrush"
	);

	ShadowRect(dc, rect, brush, brush);
}

void FillRectDmsColor(HDC dc, const GRect& rect, DmsColor color)
{
	GdiHandle<HBRUSH> brushHandle(
		CreateSolidBrush( DmsColor2COLORREF( color ) ),
		"FillRectDmsColor::CreateSolidBrush"
	);

	FillRect(dc, &AsRECT(rect), brushHandle );
}

#endif // _WIN32

//----------------------------------------------------------------------
// enum class FontSizeCategory
//----------------------------------------------------------------------

static const UInt32 g_DefaultFontHDIP[static_cast<int>(FontSizeCategory::COUNT)] = { 12, 16, 20 };
static CharPtr      g_DefaultFontName[static_cast<int>(FontSizeCategory::COUNT)] = { "Small", "Medium", "Large" };

CharPtr GetDefaultFontName(FontSizeCategory fid)
{
	assert(fid >= FontSizeCategory::SMALL && fid <= FontSizeCategory::COUNT);
	if (fid < FontSizeCategory::SMALL || fid > FontSizeCategory::COUNT)
		fid = FontSizeCategory::SMALL;

	return g_DefaultFontName[static_cast<int>(fid)];
}

UInt32 GetDefaultFontHeightDIP(FontSizeCategory fid)
{
	assert(fid >= FontSizeCategory::SMALL && fid <= FontSizeCategory::COUNT);
	if (fid < FontSizeCategory::SMALL || fid > FontSizeCategory::COUNT)
		fid = FontSizeCategory::SMALL;

	return g_DefaultFontHDIP[static_cast<int>(fid)];
}

UInt32 GetDefaultRowHeightDIP(FontSizeCategory fid)
{
	// GetDefaultFontHeightDIP returns POINTS (legacy name); * (96/72) gives DIPs.
	// + 4 DIPs of padding so top-aligned text leaves breathing room above the next
	// row separator (~5 device px at 125% scale).
	return UInt32(GetDefaultFontHeightDIP(fid) * (96.0 / 72.0) + 4);
}

#ifdef _WIN32

Point<UINT> GetWindowEffectiveDPI(HWND hWnd)
{
	assert(hWnd);
	HWND hTopWnd = GetAncestor(hWnd, GA_ROOT);
	assert(hTopWnd);
	HMONITOR hMonitor = MonitorFromWindow(hTopWnd, MONITOR_DEFAULTTONEAREST);
	assert(hMonitor);
	UINT dpiX, dpiY;

	auto result = GetDpiForMonitor(hMonitor, MDT_EFFECTIVE_DPI, &dpiX, &dpiY);
	assert(result == S_OK);
	return shp2dms_order<UINT>( dpiX, dpiY );
}

Float64 GetWindowDip2PixFactorX(HWND hWnd)
{
	auto dpi = GetWindowEffectiveDPI(hWnd);
	return dpi.X() / 96.0;
}

Float64 GetWindowDip2PixFactorY(HWND hWnd)
{
	auto dpi = GetWindowEffectiveDPI(hWnd);
	return dpi.Y() / 96.0;
}

Point<Float64> GetWindowDip2PixFactors(HWND hWnd)
{
	auto dpi = GetWindowEffectiveDPI(hWnd);
	return { dpi.first / 96.0, dpi.second / 96.0 };
}

Float64 GetWindowDip2PixFactor(HWND hWnd)
{
	auto dpi = GetWindowEffectiveDPI(hWnd);
	return (dpi.first + dpi.second) / (2.0*96.0);
}

Point<Float64> GetWindowPix2DipFactors(HWND hWnd)
{
	auto dpi = GetWindowEffectiveDPI(hWnd);
	return shp2dms_order<Float64>(96.0 / dpi.first, 96.0 / dpi.second);
}

#endif // _WIN32

#ifndef _WIN32
//----------------------------------------------------------------------
// Portable color functions
//----------------------------------------------------------------------

COLORREF GetFocusClr() { return CombineRGB(255, 0, 255); } // Magenta focus highlight (issue #1039), matches Windows path
COLORREF GetFocusTextClr() { return CombineRGB(255, 255, 255); } // white text on focus background
COLORREF GetSelectedClr() { return CombineRGB(255, 255, 0); }

#endif // !_WIN32

