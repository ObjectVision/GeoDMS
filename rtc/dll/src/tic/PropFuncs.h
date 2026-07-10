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

#ifndef __TIC_PROPFUNCS_H
#define __TIC_PROPFUNCS_H

// Specific Properties

TIC_CALL TokenID   TreeItem_GetDialogType(const TreeItem* self);
TIC_CALL void      TreeItem_SetDialogType(TreeItem* self, TokenID dialogType);
TIC_CALL SharedStr TreeItem_GetDialogData(const TreeItem* item);
TIC_CALL void      TreeItem_SetDialogData(TreeItem* item, CharPtrRange dialogData);
TIC_CALL SharedStr TreeItem_GetViewAction(const TreeItem* self);

// Specific DialogTypes

TIC_CALL bool IsClassBreakAttr(const TreeItem* adi);
TIC_CALL void MakeClassBreakAttr(AbstrDataItem* adi);
TIC_CALL bool HasMapType(const TreeItem* ti);

// cdf support

TIC_CALL bool                 HasCdfProp(const TreeItem* item);
TIC_CALL SharedStr            GetCdfProp(const TreeItem* item);
TIC_CALL const AbstrDataItem* GetCdfAttr(const TreeItem* item);

#endif // __TIC_PROPFUNCS_H
