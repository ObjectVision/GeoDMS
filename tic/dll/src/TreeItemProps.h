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

#ifndef __TIC_TREEITEMPROPS_H
#define __TIC_TREEITEMPROPS_H

#include "geo/Pair.h"
#include "mci/PropDef.h"

using PropBool = Bool;

TIC_CALL extern PropDef<TreeItem, SharedStr>* exprPropDefPtr;
TIC_CALL extern PropDef<TreeItem, SharedStr>* descrPropDefPtr;
TIC_CALL extern PropDef<TreeItem, SharedStr>* integrityCheckPropDefPtr;
TIC_CALL extern PropDef<TreeItem, SharedStr>* explicitSupplPropDefPtr;

TIC_CALL extern PropDef<TreeItem, SharedStr>* storageNamePropDefPtr;
TIC_CALL extern PropDef<TreeItem, TokenID  >* storageTypePropDefPtr;
TIC_CALL extern PropDef<TreeItem, PropBool >* storageReadOnlyPropDefPtr;
TIC_CALL extern PropDef<TreeItem, TokenID  >* syncModePropDefPtr;

TIC_CALL extern PropDef<TreeItem, TokenID  >* dialogTypePropDefPtr;
TIC_CALL extern PropDef<TreeItem, SharedStr>* dialogDataPropDefPtr;
TIC_CALL extern PropDef<TreeItem, TokenID  >* paramTypePropDefPtr;
TIC_CALL extern PropDef<TreeItem, SharedStr>* paramDataPropDefPtr;
TIC_CALL extern PropDef<TreeItem, SharedStr>* labelPropDefPtr;
TIC_CALL extern PropDef<TreeItem, SharedStr>* viewActionPropDefPtr;
TIC_CALL extern PropDef<TreeItem, TokenID  >* configStorePropDefPtr;
TIC_CALL extern PropDef<TreeItem, SharedStr>* caseDirPropDefPtr;
TIC_CALL extern PropDef<TreeItem, SharedStr>* sourceDescrPropDefPtr;

TIC_CALL extern PropDef<TreeItem, SharedStr>* sqlStringPropDefPtr;
TIC_CALL extern PropDef<TreeItem, TokenID  >* tableTypeNamePropDefPtr;
TIC_CALL extern PropDef<TreeItem, SharedStr>* cdfPropDefPtr;
TIC_CALL extern PropDef<TreeItem, SharedStr>* urlPropDefPtr;

// Generic Properties

TIC_CALL SharedStr TreeItemPropertyValue       (const TreeItem* ti, const AbstrPropDef* pd);
TIC_CALL bool      TreeItemHasPropertyValue    (const TreeItem* ti, const AbstrPropDef* pd);

enum class SourceDescrMode {
	Configured = 0,
	ReadOnly = 1,
	WriteOnly = 2,
	All = 3
};

SharedStr TreeItem_GetSourceDescr(const TreeItem* studyObject, SourceDescrMode sdm, bool bShowHidden); // defined in SourceDescr.cpp

#endif // __TIC_TREEITEMPROPS_H
