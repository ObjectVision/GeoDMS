// Copyright (C) 1998-2024 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

// Default BMP palette lookup - shared by BMP and TIFF storage managers.
// Split from BmpStorageManager.cpp so it can be compiled on all platforms.

#include "StoragePCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif

#include "StgBase.h"
#include "geo/color.h"

static DmsColor g_BmpDefaultPalette[259] =
{ 
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
	0xFFFFFFFF, // Palette Index 255 (NO_DATA):  transparent color
	0x00FFFFFF, // Palette Index 256 (VIEWPORT BACKGROUND); white color
	0x00FF0000, // Palette Index 257 (RAMP_START): clBlue
	0x000000FF  // Palette Index 258 (RAMP_END  ): clRed
};

extern "C" STGDLL_CALL DmsColor DMS_CONV STG_Bmp_GetDefaultColor(PALETTE_SIZE i)
{
	dms_assert(i <= CI_LAST);
	return g_BmpDefaultPalette[i];
}

extern "C" STGDLL_CALL void DMS_CONV STG_Bmp_SetDefaultColor(PALETTE_SIZE i, DmsColor color)
{
	dms_assert(i <= CI_LAST);
	g_BmpDefaultPalette[i] = color;
}
