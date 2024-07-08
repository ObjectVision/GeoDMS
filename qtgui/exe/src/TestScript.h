// Copyright (C) 1998-2024 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

#if !defined(__TESTSCRIPT_H)
#define __TESTSCRIPT_H

#include "DmsMainWindow.h"

using stx_error = std::exception;

void reportErr(CharPtr errMsg);
UINT32 str2int(CharPtr str);

enum class CommandCode {
	// MSG, WPARAM, LPARAM, 
	SendApp = 0,
	SendMain = 1,
	SendFocus = 2,
	SendActiveDmsControl = 3,
	WmCopyActiveDmsControl = 4, // MSG=WM_COPYDATA, WPARAM=WindowHandle, LPARAM=cds(nrRemainingNumbers, {numbers})
	DefaultView = 5,
	ActivateItem = 6,
	miExportViewPorts = 7,
	Expand = 8, ExpandAll = 15, ExpandRecursive = 16,
	ShowDetailPage = 9, // followed by integer page number.
	SaveDetailPage = 10,
	miDatagridView = 11,
	miHistogramView = 12,
	CascadeSubWindows = 13,
	TileSubWindows = 14,
	SaveValueInfo = 17
};


int RunTestScript(SharedStr testScriptName, bool* mustTerminateToken);

#endif // __TESTSCRIPT_H