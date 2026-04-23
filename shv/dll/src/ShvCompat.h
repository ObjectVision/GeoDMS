// Copyright (C) 1998-2025 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////
//
// Portable replacements for Win32 constants and types used in shv.dll.
// On Windows, these come from <windows.h> and <commctrl.h>.
// On Linux, we provide matching definitions so the same code compiles everywhere.
//

#pragma once

#ifndef __SHV_COMPAT_H
#define __SHV_COMPAT_H

#ifdef _WIN32
#include <windows.h>
#include <commctrl.h>
#else

//----------------------------------------------------------------------
// Basic Win32 types
//----------------------------------------------------------------------

using UINT  = unsigned int;
using WCHAR = wchar_t;
using HCURSOR = void*;

//----------------------------------------------------------------------
// Menu flags (used by MenuItem::m_Flags)
//----------------------------------------------------------------------

#ifndef MF_CHECKED
#define MF_CHECKED      0x00000008
#define MF_UNCHECKED    0x00000000
#define MF_ENABLED      0x00000000
#define MF_GRAYED       0x00000001
#define MF_DISABLED     0x00000002
#endif

#ifndef MFS_CHECKED
#define MFS_CHECKED     MF_CHECKED
#define MFS_GRAYED      0x00000003
#define MFS_DISABLED    MFS_GRAYED
#endif

//----------------------------------------------------------------------
// Virtual key codes (used by OnKeyDown)
//----------------------------------------------------------------------

#ifndef VK_BACK
#define VK_BACK         0x08
#define VK_TAB          0x09
#define VK_RETURN       0x0D
#define VK_SHIFT        0x10
#define VK_CONTROL      0x11
#define VK_MENU         0x12  // Alt
#define VK_ESCAPE       0x1B
#define VK_PRIOR        0x21  // Page Up
#define VK_NEXT         0x22  // Page Down
#define VK_END          0x23
#define VK_HOME         0x24
#define VK_LEFT         0x25
#define VK_UP           0x26
#define VK_RIGHT        0x27
#define VK_DOWN         0x28
#define VK_DELETE       0x2E
#define VK_ADD          0x6B
#define VK_SUBTRACT     0x6D
#define VK_F2           0x71
#endif

//----------------------------------------------------------------------
// Mouse key flags (used by DispatchMouseEvent)
//----------------------------------------------------------------------

#ifndef MK_CONTROL
#define MK_CONTROL  0x0008
#define MK_SHIFT    0x0004
#define MK_LBUTTON  0x0001
#define MK_RBUTTON  0x0002
#define MK_MBUTTON  0x0010
#endif

//----------------------------------------------------------------------
// ShowWindow constants (used by UpdateChildViews)
//----------------------------------------------------------------------

#ifndef SW_HIDE
#define SW_HIDE            0
#define SW_SHOWNORMAL      1
#define SW_SHOWMINIMIZED   2
#define SW_SHOWMAXIMIZED   3
#endif

//----------------------------------------------------------------------
// Mouse wheel
//----------------------------------------------------------------------

#ifndef WHEEL_DELTA
#define WHEEL_DELTA     120
#endif

#ifndef GET_WHEEL_DELTA_WPARAM
#define GET_WHEEL_DELTA_WPARAM(wParam) ((short)((wParam) >> 16))
#endif

//----------------------------------------------------------------------
// Cursor IDs (portable: we store integer IDs, ViewHost maps to Qt cursors)
//----------------------------------------------------------------------

#ifndef IDC_ARROW
#define IDC_ARROW       ((HCURSOR)(uintptr_t)32512)
#define IDC_IBEAM       ((HCURSOR)(uintptr_t)32513)
#define IDC_WAIT        ((HCURSOR)(uintptr_t)32514)
#define IDC_SIZEWE      ((HCURSOR)(uintptr_t)32644)
#define IDC_HAND        ((HCURSOR)(uintptr_t)32649)
#endif

// App-specific cursor IDs (defined in shv resource.h for Win32)
#ifndef IDC_ZOOMIN
#define IDC_ZOOMIN      ((HCURSOR)(uintptr_t)40001)
#define IDC_ZOOMOUT     ((HCURSOR)(uintptr_t)40002)
#define IDC_PAN         ((HCURSOR)(uintptr_t)40003)
#define IDC_SELECTDIAMOND ((HCURSOR)(uintptr_t)40004)
#endif

//----------------------------------------------------------------------
// Stub functions
//----------------------------------------------------------------------

inline void  MessageBeep(unsigned int /*type*/) {}
inline short GetKeyState(int /*vk*/) { return 0; } // TODO: connect to Qt key state

inline HCURSOR LoadCursor(void* /*hInstance*/, HCURSOR id) { return id; }
inline HCURSOR SetCursor(HCURSOR /*cursor*/) { return nullptr; } // TODO: connect to ViewHost cursor

//----------------------------------------------------------------------
// Color constants
//----------------------------------------------------------------------

#ifndef CLR_INVALID
#define CLR_INVALID  0xFFFFFFFF
#endif

#ifndef COLOR_HIGHLIGHT
#define COLOR_HIGHLIGHT       13
#define COLOR_HIGHLIGHTTEXT   14
#endif

#ifndef COLOR_BTNFACE
#define COLOR_BTNFACE         15
#define COLOR_BTNSHADOW       16
#define COLOR_BTNTEXT         18
#endif

#ifndef COLOR_WINDOW
#define COLOR_WINDOW          5
#define COLOR_WINDOWTEXT      8
#define COLOR_3DSHADOW        16
#endif

// COLORREF format: 0x00BBGGRR  (R = byte0, G = byte1, B = byte2)
// Values match typical Windows 10/11 system colors.
inline UInt32 GetSysColor(int index) {
    switch (index) {
    case COLOR_WINDOW:        return 0x00FFFFFF; // white
    case COLOR_WINDOWTEXT:    return 0x00000000; // black
    case COLOR_HIGHLIGHT:     return 0x00D77800; // blue  (R=0,G=120,B=215)
    case COLOR_HIGHLIGHTTEXT: return 0x00FFFFFF; // white
    case COLOR_BTNFACE:       return 0x00F0F0F0; // light grey (R=240,G=240,B=240)
    case COLOR_BTNSHADOW:     return 0x00A0A0A0; // medium grey (R=160,G=160,B=160)
    case COLOR_BTNTEXT:       return 0x00000000; // black
    default:                  return 0x00F0F0F0; // default: grey
    }
}

//----------------------------------------------------------------------
// Message box constants
//----------------------------------------------------------------------

#ifndef MB_ICONEXCLAMATION
#define MB_ICONEXCLAMATION 0x00000030
#define MB_YESNO           0x00000004
#define IDYES              6
#endif

inline int MessageBoxA(void*, const char*, const char*, unsigned int) { return IDYES; } // TODO: Qt dialog

#endif // !_WIN32

#endif // __SHV_COMPAT_H
