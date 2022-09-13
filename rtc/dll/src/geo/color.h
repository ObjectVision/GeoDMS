//<HEADER> 
/*
Data & Model Server (DMS) is a server written in C++ for DSS applications. 
Version: see srv/dms/rtc/dll/src/RtcVersion.h for version info.

Copyright (C) 1998-2004  YUSE GSO Object Vision BV. 

Documentation on using the Data & Model Server software can be found at:
http://www.ObjectVision.nl/DMS/

See additional guidelines and notes in srv/dms/Readme-srv.txt 

This library is free software; you can use, redistribute, and/or
modify it under the terms of the GNU General Public License version 2 
(the License) as published by the Free Software Foundation,
provided that this entire header notice and readme-srv.txt is preserved.

See LICENSE.TXT for terms of distribution or look at our web site:
http://www.objectvision.nl/DMS/License.txt
or alternatively at: http://www.gnu.org/copyleft/gpl.html

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
General Public License for more details. However, specific warranties might be
granted by an additional written contract for support, assistance and/or development
*/
//</HEADER>
#pragma once

#if !defined(__RTC_COLOR_H)
#define __RTC_COLOR_H

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

#endif __RTC_COLOR_H
