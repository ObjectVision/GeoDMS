// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if !defined(__SHVDLL_INTERFACE_H)
#define __SHVDLL_INTERFACE_H

#include "ShvBase.h"
#include "act/ActorEnums.h"

class ViewHost;


SHV_CALL void  DMS_Shv_Load();

SHV_CALL auto SHV_LayerClass_GetFirst() -> const LayerClass*;
SHV_CALL auto SHV_LayerClass_GetNext(const LayerClass*) -> const LayerClass*;
SHV_CALL bool SHV_LayerClass_CanView(const LayerClass* self, const TreeItem* viewCandidate);

SHV_CALL auto SHV_DataView_Create(TreeItem* context, ViewStyle ct, ShvSyncMode sm)->std::weak_ptr<DataView>;
SHV_CALL void SHV_DataView_StoreDesktopData(DataView* dv);
SHV_CALL bool SHV_DataView_CanContain(DataView*, const TreeItem* viewCandidate);
SHV_CALL bool SHV_DataView_AddItem(DataView*, const TreeItem* viewItem, bool isDropped);
SHV_CALL void SHV_DataView_Destroy(DataView*);

#ifdef _WIN32
SHV_CALL MsgResult SHV_DataView_DispatchMessage(DataView*, HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
#endif

SHV_CALL void SHV_DataView_SetStatusTextFunc(DataView* self, ClientHandle clientHandle, StatusTextFunc stf);
SHV_CALL void SHV_DataView_SetViewHost(DataView* self, ViewHost* vh);
SHV_CALL void SHV_SetCreateViewActionFunc(CreateViewActionFunc cvaf);

SHV_CALL auto SHV_EditPaletteView_Create(TreeItem* desktopItem, TreeItem* classAttr, const TreeItem* themeAttr) -> const AbstrDataItem*;

SHV_CALL auto SHV_GetViewStyleFlags  (const TreeItem* item) -> ViewStyleFlags;
SHV_CALL auto SHV_GetDefaultViewStyle(const TreeItem* item) -> ViewStyle;
SHV_CALL bool SHV_CanUseViewStyle    (const TreeItem* item, ViewStyle vs);

//----------------------------------------------------------------------

void CreateEditPaletteMdiChild(GraphicLayer* layer, const AbstrDataItem* themeAttr);

#endif // !defined(__SHVDLL_INTERFACE_H)
