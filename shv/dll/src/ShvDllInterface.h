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
// stdafx.h : include file for standard system include files,
//  or project specific include files that are used frequently, but
//      are changed infrequently
//

#if !defined(__SHVDLL_INTERFACE_H)
#define __SHVDLL_INTERFACE_H

#include "ShvBase.h"
#include "act/ActorEnums.h"


extern "C" {

	SHV_CALL void DMS_CONV DMS_Shv_Load();

	SHV_CALL const LayerClass* DMS_CONV SHV_LayerClass_GetFirst();
	SHV_CALL const LayerClass* DMS_CONV SHV_LayerClass_GetNext(const LayerClass*);
	SHV_CALL bool              DMS_CONV SHV_LayerClass_CanView(const LayerClass* self, const TreeItem* viewCandidate);

	SHV_CALL DataView*  DMS_CONV SHV_DataView_Create (TreeItem* context, ViewStyle ct, ShvSyncMode sm);
	SHV_CALL void       DMS_CONV SHV_DataView_StoreDesktopData(DataView* dv);
	SHV_CALL bool       DMS_CONV SHV_DataView_CanContain(DataView*, const TreeItem* viewCandidate);
	SHV_CALL bool       DMS_CONV SHV_DataView_AddItem(DataView*, const TreeItem* viewItem, bool isDropped);
	SHV_CALL void       DMS_CONV SHV_DataView_Destroy(DataView*);
	SHV_CALL ActorVisitState DMS_CONV SHV_DataView_Update(DataView*);

	SHV_CALL bool      DMS_CONV SHV_DataView_DispatchMessage(DataView*, HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam, LRESULT* resultPtr);

	SHV_CALL void      DMS_CONV SHV_DataView_SetStatusTextFunc(DataView* self, ClientHandle clientHandle, StatusTextFunc stf);
	SHV_CALL void      DMS_CONV SHV_SetCreateViewActionFunc(CreateViewActionFunc cvaf);

	SHV_CALL const AbstrDataItem* DMS_CONV SHV_EditPaletteView_Create(TreeItem* desktopItem, TreeItem* classAttr, const TreeItem* themeAttr);

	SHV_CALL ViewStyleFlags DMS_CONV SHV_GetViewStyleFlags  (const TreeItem* item);
	SHV_CALL ViewStyle      DMS_CONV SHV_GetDefaultViewStyle(const TreeItem* item);
	SHV_CALL bool           DMS_CONV SHV_CanUseViewStyle    (const TreeItem* item, ViewStyle vs);
}

//----------------------------------------------------------------------

void CreateEditPaletteMdiChild(GraphicLayer* layer, const AbstrDataItem* themeAttr);

#endif // !defined(__SHVDLL_INTERFACE_H)
