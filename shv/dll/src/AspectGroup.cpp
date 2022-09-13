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
#include "ShvDllPch.h"

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

	dms_assert( ag < AG_Count );
	if (! g_AspectGroupIds[ag] )
		g_AspectGroupIds[ag] = GetTokenID_mt(AspectGroupArray[ag].name);
	return g_AspectGroupIds[ag];
}

