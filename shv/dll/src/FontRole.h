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

#if !defined(__SHV_FONTROLE_H)
#define __SHV_FONTROLE_H

#include "Aspect.h"

enum FontRole { FR_Label, FR_Symbol, FR_Count = 2 };

const Float64 DEFAULT_FONT_PIXEL_SIZE = 10.0;
const Float64 DEFAULT_FONT_WORLD_SIZE =  4.0;

const Float64 DEFAULT_SYMB_PIXEL_SIZE = 12.0;
const Float64 DEFAULT_SYMB_WORLD_SIZE =  2.0;

const AspectNr fontSizeAspect [FR_Count] = { AN_LabelSize,      AN_SymbolSize      };
const AspectNr worldSizeAspect[FR_Count] = { AN_LabelWorldSize, AN_SymbolWorldSize };
const AspectNr fontNameAspect [FR_Count] = { AN_LabelFont,      AN_SymbolFont      };
const AspectNr fontAngleAspect[FR_Count] = { AN_LabelAngle,     AN_SymbolAngle     };

const Float64  defFontPixelSize[FR_Count] = { DEFAULT_FONT_PIXEL_SIZE, DEFAULT_SYMB_PIXEL_SIZE };
const Float64  defFontWorldSize[FR_Count] = { DEFAULT_FONT_WORLD_SIZE, DEFAULT_SYMB_WORLD_SIZE };
const CharPtr  defFontNames    [FR_Count] = { "", "DMS Font" };


#endif // __SHV_FONTROLE_H
