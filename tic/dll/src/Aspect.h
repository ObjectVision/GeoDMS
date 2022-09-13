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

#if !defined(__TIC_ASPECT_H)
#define __TIC_ASPECT_H

#include "set/Token.h"

//----------------------------------------------------------------------
// enum   : AspectID
//----------------------------------------------------------------------

enum AspectNr             // Grid Point Arc Poly Netw
{
	AN_Feature,           // No   Yes   Yes  Yes Yes
	AN_OrderBy,           // No   YES   YES  YES YES

	AN_LabelTextColor,    // No   Yes   Yes  Yes Yes
	AN_LabelBackColor,    // No   Yes   Yes  Yes Yes
	AN_LabelText,         // No   Yes   Yes  Yes Yes
	AN_LabelSize,         // No   Yes   Yes  Yes Yes
	AN_LabelWorldSize,    // No   Yes   Yes  Yes Yes
	AN_LabelFont,         // No   Yes   Yes  Yes Yes
	AN_LabelAngle,        // No   Yes   Yes  Yes Yes
	AN_LabelTransparency, // No   Yes   Yes  Yes Yes

	AN_SymbolColor,       // No   Yes   Prop Prop Yes
	AN_SymbolIndex,       // No   Yes   Prop Prop Yes
	AN_SymbolSize,        // No   Yes   Prop Prop Yes
	AN_SymbolWorldSize,   // No   Yes   Prop Prop Yes
	AN_SymbolFont,        // No   Yes   Prop Prop Yes
	AN_SymbolAngle,       // No   Yes   Prop Prop Yes
	AN_SymbolTransparency,// No   Yes   Prop Prop Yes

	AN_PenColor,          // Prop No    Yes  Prop Yes
	AN_PenStyle,          // Prop No    Yes  Prop Yes
	AN_PenWidth,          // Prop No    Yes  Prop Yes
	AN_PenWorldWidth,     // Prop No    Yes  Prop Yes
	AN_PenTransparency,   // Prop No    Yes  Prop Yes

	AN_NetworkF1,         // No   No    No   No   Yes
	AN_NetworkF2,         // No   No    No   No   Yes

	AN_BrushColor,        // Yes  No    No   Yes  No
	AN_HatchStyle,        // Yes  No    No   Yes  No
	AN_BrushTransparency, // Yes  No    No   Yes  No

	AN_MinPixSize,        // No   Yes   Yes  Yes  Yes
	AN_MaxPixSize,        // No   Yes   Yes  Yes  Yes

	AN_Selections,        // No   Yes   Yes  Yes  Yes

	AN_AspectCount,
	AN_First = AN_Feature
};

enum AspectType    { AT_Numeric, AT_Ordinal, AT_Cardinal, AT_Color, AT_Text, AT_Feature };
enum AspectRelType { AR_Any, AR_AttrOnly, AR_ParamOnly };

enum AspectGroup
{
	AG_Symbol,
	AG_Pen,
	AG_Brush,
	AG_Label,
	AG_Network,

	AG_Other, // Includes AN_Feature, AN_OrderBy, AN_MinPixSize, AN_MaxPixSize, AN_Selections

	AG_Count,
	AG_None = AG_Count,
	AG_First= AG_Symbol,
};

enum AspectNrSet
{
	ASE_Feature           = (1 << AN_Feature           ),
	ASE_OrderBy           = (1 << AN_OrderBy           ), 

	ASE_LabelTextColor    = (1 << AN_LabelTextColor    ),
	ASE_LabelBackColor    = (1 << AN_LabelBackColor    ),
	ASE_LabelText         = (1 << AN_LabelText         ),
	ASE_LabelSize         = (1 << AN_LabelSize         ),
	ASE_LabelWorldSize    = (1 << AN_LabelWorldSize    ),
	ASE_LabelFont         = (1 << AN_LabelFont         ),
	ASE_LabelAngle        = (1 << AN_LabelAngle        ),
	ASE_LabelTransparency = (1 << AN_LabelTransparency ),

	ASE_SymbolColor       = (1 << AN_SymbolColor       ),
	ASE_SymbolIndex       = (1 << AN_SymbolIndex       ),
	ASE_SymbolSize        = (1 << AN_SymbolSize        ),
	ASE_SymbolWorldSize   = (1 << AN_SymbolWorldSize   ),
	ASE_SymbolFont        = (1 << AN_SymbolFont        ),
	ASE_SymbolAngle       = (1 << AN_SymbolAngle       ),
	ASE_SymbolTransparency= (1 << AN_SymbolTransparency),

	ASE_PenColor          = (1 << AN_PenColor          ),
	ASE_PenStyle          = (1 << AN_PenStyle          ),
	ASE_PenWidth          = (1 << AN_PenWidth          ),
	ASE_PenWorldWidth     = (1 << AN_PenWorldWidth     ),
	ASE_PenTransparency   = (1 << AN_PenTransparency   ),

	ASE_NetworkF1         = (1 << AN_NetworkF1         ),
	ASE_NetworkF2         = (1 << AN_NetworkF2         ),

	ASE_BrushColor        = (1 << AN_BrushColor        ),
	ASE_HatchStyle        = (1 << AN_HatchStyle        ),
	ASE_BrushTransparency = (1 << AN_BrushTransparency ),

	ASE_MinPixSize        = (1 << AN_MinPixSize        ),  
	ASE_MaxPixSize        = (1 << AN_MaxPixSize        ),  

	ASE_Selections        = (1 << AN_Selections        ),  

	ASE_Label   = ASE_LabelTextColor |ASE_LabelBackColor |ASE_LabelText  |ASE_LabelTransparency |ASE_LabelSize |ASE_LabelWorldSize |ASE_LabelFont|ASE_LabelAngle|ASE_LabelTransparency,
	ASE_Pen     = ASE_PenColor   |ASE_PenStyle   |ASE_PenTransparency   |ASE_PenWidth  |ASE_PenWorldWidth,
	ASE_Symbol  = ASE_SymbolColor|ASE_SymbolIndex|ASE_SymbolTransparency|ASE_SymbolSize|ASE_SymbolWorldSize|ASE_SymbolFont|ASE_SymbolAngle|ASE_SymbolTransparency,
	ASE_Network = ASE_Symbol     |ASE_Pen | ASE_NetworkF1 | ASE_NetworkF2,
	ASE_Brush   = ASE_BrushColor |ASE_HatchStyle |ASE_BrushTransparency,

	ASE_PixSizes = ASE_MinPixSize|ASE_MaxPixSize,
	ASE_Other = ASE_PixSizes | ASE_Feature | ASE_PixSizes,

	ASE_AnyColor= ASE_PenColor|ASE_SymbolColor|ASE_LabelTextColor |ASE_LabelBackColor |ASE_BrushColor,

	ASE_AllDrawAspects = ASE_Label| ASE_Symbol | ASE_Pen | ASE_Brush,

	ASE_None = 0,
};

struct AspectData
{
	CharPtr       name;
	AspectRelType relType;
	AspectType    aspectType;
	AspectGroup   aspectGroup;
	TokenID       oldDialogTypeID;
};

TIC_CALL extern AspectData* AspectArray;

inline CharPtr GetAspectName(AspectNr aNr)
{
	dms_assert(AspectArray);
	if (UInt32(aNr) < AN_AspectCount)
		return AspectArray[aNr].name;
	return AspectArray[AN_AspectCount].name;
}

TIC_CALL TokenID GetAspectNameID(AspectNr aNr);
TIC_CALL bool IsColorAspectNameID(TokenID id);


#endif // __TIC_ASPECT_H
