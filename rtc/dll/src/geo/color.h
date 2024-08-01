// Copyright (C) 1998-2024 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

#if !defined(__RTC_GEO_COLOR_H)
#define __RTC_GEO_COLOR_H

#include "RtcBase.h"
#include "dbg/Check.h"

// ------------------------------------------------------------------------
//
// Common definition of RGB color
//
// ------------------------------------------------------------------------

typedef UInt32 DmsColor;
typedef UInt16 PALETTE_SIZE;

// (r,g,b) to DmsColor
inline constexpr DmsColor CombineRGB( UInt8 r, UInt8 g, UInt8 b, UInt8 t=0)
{
	// DMSConvention: red least significant, then green, then blue, then overlay,
	// which is in line with GetRValue, GetGValue and GetBValue, Delphi's TColor,
	// and the inverse of the GetRed, GetGreen etc funcs
	// but conflicting with definition of struct RGBQUAD
	return 
			(((unsigned long) t) << 24)
		|	(((unsigned long) b) << 16)
		|	(((unsigned long) g) <<  8) 
		|	(((unsigned long) r) <<  0);
}

inline UInt8 GetRed  (DmsColor combinedClr) { return  combinedClr        & 0xFF;}
inline UInt8 GetGreen(DmsColor combinedClr) { return (combinedClr >>  8) & 0xFF;}
inline UInt8 GetBlue (DmsColor combinedClr) { return (combinedClr >> 16) & 0xFF;}
inline UInt8 GetTrans(DmsColor combinedClr) { return (combinedClr >> 24) & 0xFF;}

const DmsColor DmsRed   = CombineRGB(0xFF, 0x00, 0x00);
const DmsColor DmsGreen = CombineRGB(0x00, 0xFF, 0x00);
const DmsColor DmsBlue  = CombineRGB(0x00, 0x00, 0xFF);
const DmsColor DmsYellow= CombineRGB(0xFF, 0xFF, 0x00);
const DmsColor DmsOrange= CombineRGB(0xFF, 0x80, 0x00);
const DmsColor DmsWhite = DmsRed + DmsGreen + DmsBlue;
const DmsColor DmsBlack = 0;
const DmsColor DmsTransparent = 0xFFFFFFFF;

const UInt32 MAX_COLOR = 0xFFFFFF;

inline void CheckColor(DmsColor clr)
{
	MG_CHECK(clr <= MAX_COLOR);
}


#endif __RTC_GEO_COLOR_H
