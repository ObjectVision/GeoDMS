// Copyright (C) 1998-2025 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "ShvDllPch.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

#include "AspectGroup.h"

#include "GraphicLayer.h"
#include "MenuData.h"
#include "Theme.h"
#include "ThemeCmd.h"

//----------------------------------------------------------------------
// AspectGroupData
//----------------------------------------------------------------------

//NO LONGER SUPPORTED: 'RotationPalette', VT_FLOAT64

void AspectGroupMenuFunc(GraphicLayer* layer, AspectGroup ag, MenuData& menuData)
{
	dms_assert(AspectArray);

	SubMenu subMenu( menuData, SharedStr(AspectGroupArray[ag].name) ); // SUBMENU
	if (ag != AG_Other)
	{
		bool isDisabled = layer->IsDisabledAspectGroup(ag);
		menuData.push_back( MenuItem(SharedStr("Visible"), new SubLayerCmd(ag, isDisabled), layer, isDisabled ? 0 : MF_CHECKED) );
	}

	for (AspectNr a = AN_First; a != AN_AspectCount; ++reinterpret_cast<UInt32&>(a))
		if (AspectArray[a].aspectGroup == ag)
		{
			auto theme = layer->GetTheme(a);

			AbstrCmd* themeCmd = 0;
			UInt32    flags = 0;

			if (theme)
			{
				themeCmd = new ThemeCmd(
					theme->IsDisabled() 
					?	&Theme::Enable
					:	&Theme::Disable,
					a
				);
				if (!theme->IsDisabled())
					flags = MFS_CHECKED;
			}
			else 
				flags = MFS_GRAYED;
			menuData.push_back( MenuItem(SharedStr(AspectArray[a].name), themeCmd, layer, flags) );
		}
}

extern AspectGroupData AspectGroupArray[] =
{
	{ "Symbol",  ASE_Symbol,   AspectGroupMenuFunc },
	{ "Pen",     ASE_Pen,      AspectGroupMenuFunc },
	{ "Brush",   ASE_Brush,    AspectGroupMenuFunc },
	{ "Label",   ASE_Label,    AspectGroupMenuFunc },

	{ "Other",   ASE_Other,    AspectGroupMenuFunc },

	{ "None",    ASE_None    , NULL },
};

//----------------------------------------------------------------------
// Support func
//----------------------------------------------------------------------

TokenID g_AspectGroupIds[AG_Count];


TokenID GetAspectGroupNameID(AspectGroup ag)
{
	static_assert(AG_Count <= 32);

	assert( ag >= 0 && ag < AG_Count );
	if (! g_AspectGroupIds[ag] )
		g_AspectGroupIds[ag] = GetTokenID_mt(AspectGroupArray[ag].name);
	return g_AspectGroupIds[ag];
}

