// Copyright (C) 2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

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
