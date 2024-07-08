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
// SheetVisualTestView.cpp : implementation of the DataView class
//

#include "ShvDllPch.h"

#include "RtcInterface.h"

#include "act/TriggerOperator.h"
#include "dbg/CheckPtr.h"
#include "dbg/DmsCatch.h"
#include "ser/AsString.h"
#include "utl/IncrementalLock.h"

#include "TreeItem.h"
#include "TreeItemClass.h"
#include "TreeItemContextHandle.h"
#include "TreeItemProps.h"
#include "TreeItemUtils.h"

#include "DataView.h"

std::vector<MsgStruct> g_MsgQueue;

//----------------------------------------------------------------------
// C style Interface functions for class id retrieval
//----------------------------------------------------------------------

#include "ShvDllInterface.h"
#include "ClcInterface.h"
#include "StgBase.h"

#include "Stg2Base.h"

#include "dbg/DebugContext.h"

#include "GeoInterface.h"

#include "TableDataView.h"
#include "GraphDataView.h"
#include "ItemSchemaView.h"
#include "EditPalette.h"

//----------------------------------------------------------------------
// section : Dll load
//----------------------------------------------------------------------

SHV_CALL void DMS_CONV DMS_Shv_Load() 
{
	DMS_Clc_Load();
	DMS_Stg_Load();
	DMS_Geo_Load();
}

// ============   "C" interface funcs
#include "MapControl.h"

DataView* DMS_CONV SHV_DataView_Create(TreeItem* context, ViewStyle ct, ShvSyncMode sm)
{
	DMS_CALL_BEGIN

		ObjectContextHandle contexHandle(context, TreeItem::GetStaticClass(), "SHV_DataView_Create");
		InterestRetainContextBase interestContext;

		if (context->IsTemplate())
			context->throwItemError("Cannot process ViewDefinition that is a template");
		std::shared_ptr<DataView> result;
		switch (ct) {
			case tvsMapView:
				result = std::make_shared<GraphDataView>(context, sm);
				result->SetContents(std::static_pointer_cast<MovableObject>(make_shared_gr<MapControl>(result.get())(result.get())), sm);
				break;

			case tvsPaletteEdit:
			case tvsClassificationEdit:
				if (auto adi = AsDynamicDataItem(context)) {
					result = std::make_shared<EditPaletteView>(context, sm);
					result->SetContents(std::make_shared<EditPaletteControl>(result.get(), adi), sm);
				}
				break;

			case tvsTableView:
			case tvsTableContainer: {
				auto tableRes = std::make_shared<TableDataView>(context, sm);
				result = tableRes;
				result->SetContents(std::static_pointer_cast<MovableObject>(make_shared_gr<DefaultTableViewControl>(result.get())(result.get())), sm);
				if (!tableRes->GetTableControl()->IsActive())
					result->Activate(tableRes->GetTableControl());

				break;
			}
			case tvsUpdateItem:
			case tvsUpdateTree:
			case tvsSubItemSchema:
			case tvsSupplierSchema:
			case tvsExprSchema:
				result = std::make_shared <ItemSchemaView>(context, ct, sm);
				result->SetContents(std::static_pointer_cast<MovableObject>(make_shared_gr<MapControl>(result.get())(result.get())), sm);
				break;
		}
		if (result) {
			Keep(result);
			return result.get();
		}

	DMS_CALL_END
	return nullptr;
}

void DMS_CONV SHV_DataView_StoreDesktopData(DataView* dv)
{
	DMS_CALL_BEGIN

		dms_assert(dv);
		TreeItem* vc = dv->GetViewContext();
		dms_assert(vc); // INVARIANT of DataView
		if (!vc->IsEndogenous())
			dv->GetContents()->Sync(vc, SM_Save);

	DMS_CALL_END
}

bool DMS_CONV SHV_DataView_CanContain(DataView* dv, const TreeItem* viewCandidate)
{
	DMS_CALL_BEGIN

		StaticMtIncrementalLock<g_DispatchLockCount> dispatchLock;

		CheckPtr(dv,            DataView::GetStaticClass(), "SHV_DataView_CanContain");
		CheckPtr(dv,            DataView::GetStaticClass(), "SHV_DataView_CanContain");
		CheckPtr(viewCandidate, TreeItem::GetStaticClass(), "SHV_DataView_CanContain");

		return dv->CanContain(viewCandidate);

	DMS_CALL_END
	return false;
}

bool  DMS_CONV SHV_DataView_AddItem(DataView* dv, const TreeItem* viewItem, bool isDropped)
{
	DMS_CALL_BEGIN

		StaticMtIncrementalLock<g_DispatchLockCount> dispatchLock;

		assert(!SuspendTrigger::DidSuspend());
		SuspendTrigger::Resume();  // REMOVE

		CheckPtr(dv,            DataView::GetStaticClass(), "SHV_DataView_AddItem");
		CheckPtr(viewItem,      TreeItem::GetStaticClass(), "SHV_DataView_AddItem");

		ObjectContextHandle context(viewItem, "DataView_AddItem");

		UpdateMarker::ChangeSourceLock changeStamp(UpdateMarker::GetActiveTS(MG_DEBUG_TS_SOURCE_CODE("SHV_DataView_AddItem")), "SHV_DataView_AddItem");

		if (viewItem->IsFailed(FR_MetaInfo))
			viewItem->ThrowFail();

		dv->AddLayer(viewItem, isDropped);
		return true;

	DMS_CALL_END
	return false;
}

static LRESULT dummyResultLoc;
#include "dbg/debug.h"

bool DMS_CONV SHV_DataView_DispatchMessage(DataView* dv, HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam, LRESULT* resultPtr)
{
	DMS_CALL_BEGIN

		DBG_START("SHV_DataView","DispatchMessage", false);

		DBG_TRACE(("msg = %x",  msg));
		DBG_TRACE(("WP  = %x",  wParam));
		DBG_TRACE(("LP  = %x",  lParam));
		DBG_TRACE(("LR  = %x", *resultPtr));
		DBG_TRACE(("dv  = %x",  dv));
		DBG_TRACE(("hWnd= %x",  hWnd));

		dms_assert(resultPtr);
		dms_assert(hWnd != 0);

		if (dv)
		{
#if defined(MG_DEBUG_DATAVIEWSET)
			MG_CHECK( dv->IsInActiveDataViewSet());
#endif
			if (g_DispatchLockCount)
			{
				if (msg == MG_WM_SIZE // WM_WINDOWPOSCHANGED or WM_SIZE  // can be sent to window from GUI Callback on NotifyStateChange
					|| msg == WM_VSCROLL
					|| msg == WM_HSCROLL
					|| msg == WM_KILLFOCUS
					|| msg == UM_PROCESS_QUEUE
					)
				{
					bool incSuspendIfPushBackSucceeds = g_MsgQueue.empty();
					g_MsgQueue.push_back(MsgStruct(dv, msg, wParam, lParam, &dummyResultLoc));
					if (incSuspendIfPushBackSucceeds)
						SuspendTrigger::IncSuspendLevel();
					DBG_TRACE(("Queue the msg and returns true without changing LR"));
					return true; // consider message handled.
				}
				if (msg == WM_NCHITTEST
					|| msg == WM_NCCALCSIZE
					|| msg == WM_NCPAINT
					|| msg == WM_NCACTIVATE
				)
					return false; // lets try defaultWindowProc
			}
			CheckPtr(dv, DataView::GetStaticClass(), "SHV_DataView_DispatchMessage");

//			TreeItemContextHandle checkPtr(dv->GetViewContext(), TreeItem::GetStaticClass(), "SHV_DataView_DispatchMessage");
//			Waiter handleMessage(&checkPtr);

			StaticMtIncrementalLock<g_DispatchLockCount> dispatchLock;
			dv->ResetHWnd(hWnd);

			SuspendTrigger::Resume();
			{
//					MG_DEBUG_DATA_CODE(SuspendTrigger::ApplyLock dontCallHere; )

					if (g_DispatchLockCount == 1) // process queued messages first
					{

						while (!g_MsgQueue.empty())
						{
							MsgStruct first = g_MsgQueue.front();
							g_MsgQueue.erase(g_MsgQueue.begin());
							if (g_MsgQueue.empty())
								SuspendTrigger::DecSuspendLevel();

							first.Send();
						}
					}
				SuspendTrigger::Resume();
			}
			bool result = MsgStruct(dv, msg, wParam, lParam, resultPtr).Send();

			DBG_TRACE(("LR = %x", *resultPtr));
			DBG_TRACE(("res= %d", result));
			return result;
		}

		DBG_TRACE(("no dv, return false"));

	DMS_CALL_END

	return false; // lets try defaultWindowProc after exception
}

void OnDestroyDataView(DataView* self)
{
	g_MsgQueue.erase(
		std::remove_if(
			g_MsgQueue.begin(), 
			g_MsgQueue.end(), 
			[self](const MsgStruct& candidate) { return candidate.m_DataView == self; }
		),
		g_MsgQueue.end()
	);
}

ActorVisitState DataView_Update(DataView* self)
{
	DMS_CALL_BEGIN

		if (self->UpdateView() != GVS_Continue)
			return AVS_SuspendedOrFailed;  // come back if suspended and not failed

		return AVS_Ready;

	DMS_CALL_END

	return AVS_Ready;
}

ActorVisitState DMS_CONV SHV_DataView_Update(DataView* self)
{
	DMS_SE_CALL_BEGIN

		return DataView_Update(self);

	DMS_SE_CALL_END

	return AVS_Ready;
}

void DMS_CONV SHV_DataView_Destroy(DataView* self)
{
	DMS_CALL_BEGIN

		CheckPtr(self, DataView::GetStaticClass(), "SHV_DataView_Destroy");

		Unkeep(self);

	DMS_CALL_END
}

void DMS_CONV SHV_DataView_SetStatusTextFunc(DataView* self, ClientHandle clientHandle, StatusTextFunc stf)
{
	DMS_CALL_BEGIN

		assert(self); // Precondition

		CheckPtr(self, DataView::GetStaticClass(), "SHV_DataView_SetStatusTextFunc");

		self->SetStatusTextFunc(clientHandle, stf);

	DMS_CALL_END
}

void DMS_CONV SHV_SetCreateViewActionFunc(CreateViewActionFunc cvaf)
{
	g_ViewActionFunc = cvaf;

}

/******************************************************************************/

//#include <oscalls.h>
//#define _DECL_DLLMAIN   /* include prototype of _pRawDllMain */
//#include <process.h>

/***
*DllMain - dummy version DLLs linked with all 3 C Run-Time Library models
*
*Purpose:
*       The routine DllMain is always called by _DllMainCrtStartup.  If
*       the user does not provide a routine named DllMain, this one will
*       get linked in so that _DllMainCRTStartup has something to call.
*
*       For the LIBC.LIB and MSVCRT.LIB models, the CRTL does not need
*       per-thread notifications so if the user is ignoring them (default
*       DllMain and _pRawDllMain == NULL), just turn them off.  (WIN32-only)
******************************************************************************/

HINSTANCE g_ShvDllInstance = 0;

extern "C" BOOL WINAPI DllMain(
        HANDLE  hDllHandle,
        DWORD   dwReason,
        LPVOID  lpreserved
        )
{
	g_ShvDllInstance = (HINSTANCE) hDllHandle;
#if !defined (_MT) || defined (CRTDLL)
        if ( dwReason == DLL_PROCESS_ATTACH && ! _pRawDllMain )
                DisableThreadLibraryCalls(hDllHandle);
#endif  /* !defined (_MT) || defined (CRTDLL) */
	return TRUE ;
}

static const TreeItem* g_LastQueriedItem    = 0;
static bool            g_LastAdminMode      = false;
static UInt32          g_LastViewStyleFlags;

#include "AbstrUnit.h"
#include "mci/ValueClass.h"

#include "PropFuncs.h"

bool IsMapViewable(const AbstrDataItem* adi)
{
	dms_assert(adi);
	dms_assert(!SuspendTrigger::DidSuspend());
	if (adi->WasFailed(FR_MetaInfo))
		return false;

	const AbstrUnit* adu = adi->GetAbstrDomainUnit();
	const AbstrUnit* avu = adi->GetAbstrValuesUnit();

	if (!adu || !avu)
		return false;

	if (adu->GetValueType()->GetNrDims() == 2)
		return avu->GetValueType()->IsNumericOrBool();
	if (avu->GetValueType()->GetNrDims() == 2)
	{
		dms_assert(adu->GetValueType()->GetNrDims() != 2);
		return true;
	}
	if (GetMappingItem(adi) || GetMappingItem(adu))
		return true;
	do
	{
		if (HasMapType(adu))
			return true;
		adu = AsUnit( adu->GetCurrSourceItem() );
	}	while (adu);

	return false;
}

SHV_CALL ViewStyleFlags DMS_CONV SHV_GetViewStyleFlags(const TreeItem* item)
{
	DMS_CALL_BEGIN

		SuspendTrigger::FencedBlocker blockSuspension("SHV_GetViewStyleFlags");

		dms_assert(item);
		if (g_LastQueriedItem != item || g_LastAdminMode != HasAdminMode())
		{
			g_LastViewStyleFlags = (vsfUpdateItem|vsfUpdateTree|vsfSubItemSchema|vsfSupplierSchema|vsfExprSchema);

			if (!item->InTemplate())
			{
				TokenID tid = TreeItem_GetDialogType(item);
				if (IsClassBreakAttr(item) )       g_LastViewStyleFlags |= vsfClassificationEdit; else
				if (IsColorAspectNameID(tid)) g_LastViewStyleFlags |= vsfPaletteEdit;

				if (IsDataItem(item))
				{
					const AbstrDataItem* adi = AsDataItem(item);
					g_LastViewStyleFlags |= vsfTableView;
					if (IsMapViewable(adi)) 
						g_LastViewStyleFlags |= vsfMapView;
					const AbstrUnit* avu = adi->GetAbstrValuesUnit();
					if (avu->CanBeDomain())
						g_LastViewStyleFlags |= vsfHistogram;	
				}
				else
					if (SHV_DataContainer_GetDomain(item,1, HasAdminMode()))  
						g_LastViewStyleFlags |= vsfTableContainer;
			}
			if (item->HasSubItems  ()) g_LastViewStyleFlags |= vsfContainer;
			if (Waiter::IsWaiting())
			{
				if (item->mc_Calculator) g_LastViewStyleFlags |= vsfExprEdit;
			}
			else
			{
				if (item->HasCalculator()) g_LastViewStyleFlags |= vsfExprEdit;
			}

			g_LastQueriedItem = item;
			g_LastAdminMode   = HasAdminMode();
		}
		return ViewStyleFlags( g_LastViewStyleFlags );

	DMS_CALL_END
	return vsfNone;
}

SHV_CALL ViewStyle DMS_CONV SHV_GetDefaultViewStyle (const TreeItem* item)
{
	DMS_CALL_BEGIN

		ObjectContextHandle hnd(item, "SHV_GetDefaultViewStyle");

		SuspendTrigger::FencedBlocker lock("SHV_GetDefaultViewStyle");

		ViewStyleFlags vsf = SHV_GetViewStyleFlags(item);
		if (vsf & vsfMapView           ) return tvsMapView;
		if (vsf & vsfPaletteEdit       ) return tvsPaletteEdit;
		if (vsf & vsfClassificationEdit) return tvsClassificationEdit;
		if (vsf & vsfTableView         ) return tvsTableView;
		if (vsf & vsfTableContainer    ) return tvsTableContainer;
		if (vsf & vsfContainer         ) return tvsContainer;
		if (vsf & vsfExprEdit          ) return tvsExprEdit;

	DMS_CALL_END_NOTHROW
	return tvsDefault;
}

SHV_CALL bool DMS_CONV SHV_CanUseViewStyle  (const TreeItem* item, ViewStyle vs)
{
	DMS_CALL_BEGIN

		SuspendTrigger::Resume();
		ViewStyleFlags vsf = SHV_GetViewStyleFlags(item);
		return (vsf & (1 << vs)); // || ((vs == vsfTableView) && (vsf & vsfTableContainer));

	DMS_CALL_END_NOTHROW
	return false;
}

extern "C" SHV_CALL bool SHV_DataView_GetExportInfo(DataView* view, UInt32* nrTileRows, UInt32* nrTileCols, UInt32* nrSubDotRows, UInt32* nrSubDotCols, IStringHandle* fullFileNameBaseStr)
{
	DMS_CALL_BEGIN

		FencedInterestRetainContext resultHolder("SHV_DataView_GetExportInfo");

		dms_assert(view);
		ExportInfo info = view->GetExportInfo();
		if (info.empty())
			return false;

		if (nrTileRows) *nrTileRows = info.m_NrTiles.Row();
		if (nrTileCols) *nrTileCols = info.m_NrTiles.Col();
		if (nrSubDotRows) *nrSubDotRows = info.GetNrSubDotsPerTile().Row();
		if (nrSubDotCols) *nrSubDotCols = info.GetNrSubDotsPerTile().Col();

		if (fullFileNameBaseStr) 
			*fullFileNameBaseStr = IString::Create(info.m_FullFileNameBase);

		return true;

	DMS_CALL_END
	return false;
}
