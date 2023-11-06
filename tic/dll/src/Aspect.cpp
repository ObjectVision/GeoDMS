// Copyright (C) 2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "TicPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

#include "Aspect.h"

//----------------------------------------------------------------------
// enum   : AspectID
//----------------------------------------------------------------------

//NO LONGER SUPPORTED: 'RotationPalette', VT_FLOAT64


AspectData AspectArrayData[] = 
{

	{ "Feature",            AR_AttrOnly, AT_Feature,  AG_Other},

	{ "OrderBy",            AR_AttrOnly, AT_Numeric,  AG_Other,  UNDEFINED_VALUE(TokenID)      },

	{ "LabelColor",         AR_Any,      AT_Color,    AG_Label,  UNDEFINED_VALUE(TokenID)      },
	{ "LabelBackColor",     AR_Any,      AT_Color,    AG_Label,  UNDEFINED_VALUE(TokenID)      },
	{ "LabelText",          AR_AttrOnly, AT_Text,     AG_Label,  GetTokenID_st("Labels")          },
	{ "LabelSize",          AR_Any,      AT_Numeric,  AG_Label,  GetTokenID_st("SizePalette" )    },
	{ "LabelWorldSize",     AR_Any,      AT_Numeric,  AG_Label,  UNDEFINED_VALUE(TokenID)      },
	{ "LabelFont",          AR_Any,      AT_Text,     AG_Label,  GetTokenID_st("FontPalette" )    }, // oldName not for Symbols
	{ "LabelAngle",         AR_Any,      AT_Numeric,  AG_Label,  UNDEFINED_VALUE(TokenID)      },
	{ "LabelTransparency",  AR_Any,      AT_Numeric,  AG_Label,  UNDEFINED_VALUE(TokenID)      },

	{ "SymbolColor",        AR_Any,      AT_Color,    AG_Symbol,  GetTokenID_st("Palette")         }, // oldName only for Points
	{ "SymbolIndex",        AR_Any,      AT_Ordinal,  AG_Symbol,  GetTokenID_st("CharacterIndexPalette") },
	{ "SymbolSize",         AR_Any,      AT_Numeric,  AG_Symbol,  GetTokenID_st("SizePalette" )    },
	{ "SymbolWorldSize",    AR_Any,      AT_Numeric,  AG_Symbol,  UNDEFINED_VALUE(TokenID)      },
	{ "SymbolFont",         AR_Any,      AT_Text,     AG_Symbol,  GetTokenID_st("FontPalette" )    }, // oldName only for Points
	{ "SymbolAngle",        AR_Any,      AT_Numeric,  AG_Symbol,  UNDEFINED_VALUE(TokenID)      },
	{ "SymbolTransparency", AR_Any,      AT_Numeric,  AG_Symbol,  UNDEFINED_VALUE(TokenID)      },

	{ "PenColor",           AR_Any,      AT_Color,    AG_Pen,     GetTokenID_st("Palette")         }, // oldName only for Arcs
	{ "PenStyle",           AR_Any,      AT_Ordinal,  AG_Pen,     UNDEFINED_VALUE(TokenID)      },
	{ "PenWidth",           AR_Any,      AT_Numeric,  AG_Pen,     UNDEFINED_VALUE(TokenID)      },
	{ "PenWorldWidth",      AR_Any,      AT_Numeric,  AG_Pen,     UNDEFINED_VALUE(TokenID)      },
	{ "PenTransparency",    AR_Any,      AT_Numeric,  AG_Pen,     UNDEFINED_VALUE(TokenID)      },

	{ "NetworkF1",          AR_AttrOnly, AT_Cardinal, AG_Network, UNDEFINED_VALUE(TokenID)      },
	{ "NetworkF2",          AR_AttrOnly, AT_Cardinal, AG_Network, UNDEFINED_VALUE(TokenID)      },

	{ "BrushColor",         AR_Any,      AT_Color,    AG_Brush,   GetTokenID_st("Palette")         },  // oldName only for polygons & grids
	{ "HatchStyle",         AR_Any,      AT_Ordinal,  AG_Brush,   UNDEFINED_VALUE(TokenID)      },
	{ "BrushTransparency",  AR_Any,      AT_Numeric,  AG_Pen,     UNDEFINED_VALUE(TokenID)      },

	{ "MinPixSize",     AR_ParamOnly, AT_Numeric,  AG_Other,  UNDEFINED_VALUE(TokenID)          },
	{ "MaxPixSize",     AR_ParamOnly, AT_Numeric,  AG_Other,  UNDEFINED_VALUE(TokenID)          },

	{ "Selected",       AR_AttrOnly,  AT_Cardinal, AG_None,   UNDEFINED_VALUE(TokenID)          },

	{ "Unknown" } // AspectThemeNr[AN_AspectCount]     
};
AspectData* AspectArray = AspectArrayData;

//----------------------------------------------------------------------
// Support func
//----------------------------------------------------------------------

TokenID g_AspectIds[AN_AspectCount];

TokenID GetAspectNameID(AspectNr aNr)
{
	static_assert(AN_AspectCount <= 32);
	dms_assert(AspectArray);
	dms_assert( UInt32(aNr) < AN_AspectCount );
	if (! g_AspectIds[aNr] )
		g_AspectIds[aNr] = GetTokenID_mt(AspectArray[aNr].name);
	return g_AspectIds[aNr];
}

static TokenID stPalette("Palette", (st_tag*)nullptr);

bool IsColorAspectNameID(TokenID id)
{
	if (id == TokenID())
		return false;
	if (id == GetAspectNameID(AN_BrushColor) 
	||  id == GetAspectNameID(AN_SymbolColor)
	||	id == GetAspectNameID(AN_PenColor  )
	||	id == GetAspectNameID(AN_LabelTextColor)
	||	id == GetAspectNameID(AN_LabelBackColor))
		return true;

	// support for Obsolete stuff
	return id == stPalette;
}

