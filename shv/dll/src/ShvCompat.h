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
using WCHAR = char16_t;
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

#endif // !_WIN32

#endif // __SHV_COMPAT_H
