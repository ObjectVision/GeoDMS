// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////

#if !defined(__SHV_FONTROLE_H)
#define __SHV_FONTROLE_H

#include "ShvBase.h"
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
const CharPtr  defFontNames    [FR_Count] = { "Noto Sans Medium", "DMS Font" };
const WCHAR    defSymbol = 0x0023;

#endif // __SHV_FONTROLE_H
