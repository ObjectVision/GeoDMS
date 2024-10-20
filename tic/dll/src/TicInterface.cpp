// Copyright (C) 1998-2024 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "TicPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

#include "SymInterface.h"

//----------------------------------------------------------------------
// used modules and forward class references
//----------------------------------------------------------------------
#include "TicInterface.h"

#include "RtcInterface.h"
#include "act/actorVisitor.h"
#include "act/SupplierVisitFlag.h"
#include "act/TriggerOperator.h"
#include "dbg/debug.h"
#include "dbg/DebugCast.h"
#include "dbg/DmsCatch.h"
#include "geo/Conversions.h"
#include "utl/Encodes.h"

#include "stg/AbstrStorageManager.h"
#include "AbstrCalculator.h"
#include "CopyTreeContext.h"
#include "DataController.h"
#include "DataItemClass.h"
#include "DataStoreManagerCaller.h"
#include "ItemUpdate.h"
#include "Param.h"
#include "PropFuncs.h"
#include "StateChangeNotification.h"
#include "TreeItemClass.h"
#include "TreeItemContextHandle.h"

//----------------------------------------------------------------------
// C style Interface functions for construction
//----------------------------------------------------------------------

const TreeItem* GetCaseItem(const TreeItem* context)
{
	return context
		? DMS_TreeItem_GetItem(context, "Case", 0)
		: nullptr;
}

AbstrDataItem* GetCaseNameItem(TreeItem* context)
{
	return checked_cast<AbstrDataItem*>(
		context->GetItem("Case/Name")->CheckCls(DataArray<SharedStr>::GetStaticClass()));
}

SharedStr GetCaseName(TreeItem* context)
{
	AbstrDataItem* caseNameItem= GetCaseNameItem(context);
	dms_assert(caseNameItem);
	return caseNameItem->GetValue<SharedStr>(0);
}

TIC_CALL TreeItem* DMS_CONV DMS_CreateTree(CharPtr name)
{
	DMS_CALL_BEGIN

		DBG_START("DMS", "CreateTree", true);
		DBG_TRACE(("name = %s", name));

		return TreeItem::CreateConfigRoot(GetTokenID_mt(name));

	DMS_CALL_END
	return nullptr;
}

TIC_CALL TreeItem* DMS_CONV DMS_CreateTreeItem(TreeItem* context, CharPtr name)
{
	DMS_CALL_BEGIN

		CheckPtr(context, TreeItem::GetStaticClass(), "DMS_CreateTreeItem");
		return context->CreateItemFromPath(name);

	DMS_CALL_END
	return nullptr;
}

//----------------------------------------------------------------------
// C style Interface functions for class id retrieval
//----------------------------------------------------------------------

TIC_CALL const Class* DMS_CONV DMS_TreeItem_GetStaticClass() 
{ 
	DMS_CALL_BEGIN

		return TreeItem::GetStaticClass(); 

	DMS_CALL_END
	return nullptr;
}

//----------------------------------------------------------------------
// C style Interface functions for Runtime Type Info (RTTI) and Dynamic Casting
//----------------------------------------------------------------------

TIC_CALL const Object* DMS_CONV DMS_TreeItem_QueryInterface(const Object* self, const Class* requiredClass)
{
	DMS_CALL_BEGIN

		ObjectContextHandle checkPtr(self, 0, "DMS_TreeItem_QueryInterface");

		return self->IsKindOf(requiredClass) ? self : nullptr; // DYNAMIC CAST
	
	DMS_CALL_END
	return nullptr;
}

TIC_CALL Object* DMS_CONV DMS_TreeItemRW_QueryInterface(Object* self, const Class* requiredClass)
{
	DMS_CALL_BEGIN

		ObjectContextHandle checkPtr(self, 0, "DMS_TreeItemRW_QueryInterface");

		return self->IsKindOf(requiredClass) ? self : nullptr; // DYNAMIC CAST
	
	DMS_CALL_END
	return nullptr;
}

TIC_CALL const Class* DMS_CONV DMS_TreeItem_GetDynamicClass(const TreeItem* self)
{
	DMS_CALL_BEGIN

		TreeItemContextHandle checkPtr(self, TreeItem::GetStaticClass(), "DMS_TreeItem_GetDynamicClass");

		return self->GetDynamicClass(); // DYNAMIC CAST

	DMS_CALL_END
	return nullptr;
}


//----------------------------------------------------------------------
// C style Interface functions for destruction
//----------------------------------------------------------------------

TIC_CALL void DMS_CONV DMS_TreeItem_SetAutoDelete(TreeItem* self)
{
	DMS_CALL_BEGIN

		TreeItemContextHandle checkPtr(self, TreeItem::GetStaticClass(), "DMS_TreeItem_SetAutoDelete");
		FixedContextHandle contextReporter("DMS_TreeItem_SetAutoDelete");

		if (self)
			self->EnableAutoDelete();

	DMS_CALL_END
}

TIC_CALL bool DMS_CONV DMS_TreeItem_GetAutoDeleteState(TreeItem* self)
{
	DMS_CALL_BEGIN

		TreeItemContextHandle checkPtr(self, TreeItem::GetStaticClass(), "DMS_TreeItem_GetAutoDeleteState");
		return !self->IsAutoDeleteDisabled();

	DMS_CALL_END
	return true;
}

TIC_CALL bool DMS_CONV DMS_TreeItem_IsCacheItem(TreeItem* self)
{
	DMS_CALL_BEGIN

		TreeItemContextHandle checkPtr(self, TreeItem::GetStaticClass(), "DMS_TreeItem_IsCacheItem");
		return self->IsCacheItem();

	DMS_CALL_END
	return true;
}

TIC_CALL bool DMS_CONV DMS_TreeItem_IsHidden(TreeItem* self)
{
	DMS_CALL_BEGIN

		CheckPtr(self, TreeItem::GetStaticClass(), "DMS_TreeItem_IsHidden");
		return self->GetTSF(TSF_IsHidden);

	DMS_CALL_END
	return true;
}

TIC_CALL void   DMS_CONV DMS_TreeItem_SetIsHidden(TreeItem* self, bool isHidden)
{
	DMS_CALL_BEGIN

		CheckPtr(self, TreeItem::GetStaticClass(), "DMS_TreeItem_IsHidden");
		self->SetIsHidden(isHidden);

	DMS_CALL_END
}


TIC_CALL bool DMS_CONV DMS_TreeItem_InHidden(TreeItem* self)
{
	DMS_CALL_BEGIN

		CheckPtr(self, TreeItem::GetStaticClass(), "DMS_TreeItem_InHidden");
		return self->GetTSF(TSF_InHidden);

	DMS_CALL_END
	return true;
}

TIC_CALL bool   DMS_CONV DMS_TreeItem_IsTemplate(TreeItem* self)
{
	DMS_CALL_BEGIN

		CheckPtr(self, TreeItem::GetStaticClass(), "DMS_TreeItem_IsTemplate");
		return self->GetTSF(TSF_IsTemplate);

	DMS_CALL_END
	return true;
}

TIC_CALL bool   DMS_CONV DMS_TreeItem_InTemplate(TreeItem* self)
{
	DMS_CALL_BEGIN

		CheckPtr(self, TreeItem::GetStaticClass(), "DMS_TreeItem_InTemplate");
		return self->InTemplate();

	DMS_CALL_END
	return true;
}

//----------------------------------------------------------------------
// C style Interface functions for TreeItem retrieval
//----------------------------------------------------------------------

TIC_CALL const TreeItem* DMS_CONV DMS_TreeItem_GetItem(const TreeItem* context, CharPtr path, const Class* requiredClass)
{
	DMS_CALL_BEGIN

		dms_assert(!SuspendTrigger::DidSuspend());
		SuspendTrigger::Resume();

		TreeItemContextHandle tich(context, TreeItem::GetStaticClass(), "DMS_TreeItem_GetItem");
		const TreeItem* result = context->FindItem(path);
		if	(	!requiredClass 
			||	result && result->GetDynamicObjClass()->IsDerivedFrom(requiredClass)
			)
			return result;

	DMS_CALL_END
	return nullptr;
}

TIC_CALL auto TreeItem_GetBestItemAndUnfoundPart(const TreeItem* context, CharPtr path) ->BestItemRef
{
	assert(context);

	while (*path && *path != '/' && !itemNameFirstChar_test(*path))
		++path;

	CharPtrRange pathRange = { path, ParseTreeItemPath(path) }; // skip trailing trash

	return context->FindBestItem(pathRange);
}

TIC_CALL const TreeItem* DMS_CONV DMS_TreeItem_GetBestItemAndUnfoundPart(const TreeItem* context, CharPtr path, IStringHandle* unfoundPart)
{
	DMS_CALL_BEGIN

		TreeItemContextHandle tich(context, TreeItem::GetStaticClass(), "DMS_TreeItem_GetItem");

		assert(unfoundPart);
		assert(!SuspendTrigger::DidSuspend());
		SuspendTrigger::Resume();

		auto itemRef = TreeItem_GetBestItemAndUnfoundPart(context, path);
		*unfoundPart = IString::Create(itemRef.second);
		return itemRef.first;

	DMS_CALL_END
	return nullptr;
}

TIC_CALL TreeItem* DMS_CONV DMS_TreeItem_CreateItem(TreeItem* context, CharPtr path, const Class* requiredClass)
{
	DMS_CALL_BEGIN

		TreeItemContextHandle checkPtr(context, TreeItem::GetStaticClass(), "DMS_TreeItem_CreateItem");
		return context->CreateItemFromPath(path, requiredClass);

	DMS_CALL_END
	return nullptr;
}

TIC_CALL const TreeItem* DMS_CONV DMS_TreeItem_GetRoot(const TreeItem* self)
{
	DMS_CALL_BEGIN

		TreeItemContextHandle checkPtr(self, TreeItem::GetStaticClass(), "DMS_TreeItem_GetRoot");
		return debug_cast<const TreeItem*>(self->GetRoot());

	DMS_CALL_END
	return 0;
}

TIC_CALL const TreeItem* DMS_CONV DMS_TreeItem_GetParent(const TreeItem* self)
{
	DMS_CALL_BEGIN

		TreeItemContextHandle checkPtr(self, TreeItem::GetStaticClass(), "DMS_TreeItem_GetParent");
		return self->GetTreeParent();

	DMS_CALL_END
	return nullptr;
}


bool Actor_CallbackSuppliers_Impl (const Actor* self, ClientHandle clientHandle, ProgressState ps, TSupplCallbackFunc func)
{
	DMS_CALL_BEGIN

		return
			VisitSupplBoolImpl(self, SupplierVisitFlag::Inspect, 
				[=] (const Actor* supplier) -> bool
				{
					const TreeItem* ti = dynamic_cast<const TreeItem*>(supplier);
					if (ti)
						return func(clientHandle, ti);
					if (dynamic_cast<const AbstrCalculator*>(supplier))
						return Actor_CallbackSuppliers_Impl(supplier, clientHandle, ps, func);
					return true;
				}
			);

	DMS_CALL_END
	return false;
}

TIC_CALL bool DMS_CONV DMS_TreeItem_VisitSuppliers (const TreeItem* self, ClientHandle clientHandle, ProgressState ps, TSupplCallbackFunc func)
{
	DMS_CALL_BEGIN

		TreeItemContextHandle checkPtr(self, TreeItem::GetStaticClass(), "DMS_TreeItem_VisitSuppliers");

		Actor_CallbackSuppliers_Impl(self, clientHandle, ps, func);

	DMS_CALL_END
	return false;
}

//----------------------------------------------------------------------
// C style Interface functions for Metadata retrieval
//----------------------------------------------------------------------

bool TreeItem_HasSubItems_Impl(const TreeItem* self)
{
	DMS_CALL_BEGIN

		CheckPtr(self, TreeItem::GetStaticClass(), "DMS_TreeItem_HasSubItems");
		return self->HasSubItems();

	DMS_CALL_END
	return false;
}

TIC_CALL bool DMS_CONV DMS_TreeItem_HasSubItems(const TreeItem* self)
{
	DMS_SE_CALL_BEGIN

		return TreeItem_HasSubItems_Impl(self);

	DMS_SE_CALL_END
	return false;
}

TIC_CALL const TreeItem* DMS_CONV DMS_TreeItem_GetFirstSubItem(const TreeItem* self)
{
	DMS_CALL_BEGIN

		CheckPtr(self, TreeItem::GetStaticClass(), "DMS_TreeItem_GetFirstSubItem");

		return self->GetFirstSubItem();

	DMS_CALL_END
	return nullptr;
}

TIC_CALL const TreeItem* DMS_CONV DMS_TreeItem_GetNextItem(const TreeItem* self)
{
	DMS_CALL_BEGIN

		CheckPtr(self, TreeItem::GetStaticClass(), "DMS_TreeItem_GetNextItem");

		return self->GetNextItem();

	DMS_CALL_END
	return nullptr;
}

TIC_CALL const TreeItem*  DMS_CONV DMS_TreeItem_GetFirstVisibleSubItem(const TreeItem* self)
{
	DMS_CALL_BEGIN

		CheckPtr(self, TreeItem::GetStaticClass(), "DMS_TreeItem_GetFirstVisibleSubItem");

		return self->GetFirstVisibleSubItem();

	DMS_CALL_END
	return nullptr;
}

TIC_CALL const TreeItem* DMS_CONV DMS_TreeItem_GetNextVisibleItem(const TreeItem* self)
{
	DMS_CALL_BEGIN

		CheckPtr(self, TreeItem::GetStaticClass(), "DMS_TreeItem_GetVisibleNextItem");

		return self->GetNextVisibleItem();

	DMS_CALL_END
	return nullptr;
}


TIC_CALL CharPtr DMS_CONV DMS_TreeItem_GetName(const Object* self)
{
	DMS_CALL_BEGIN

		ObjectContextHandle checkPtr(self, 0, "DMS_TreeItem_GetName");
		return self->GetName().c_str();

	DMS_CALL_END
	return "";
}

TIC_CALL IStringHandle DMS_CONV DMS_TreeItem_GetFullNameAsIString(const TreeItem* self)
{
	DMS_CALL_BEGIN

		ObjectContextHandle checkPtr(self, TreeItem::GetStaticClass(), "DMS_TreeItem_GetFullNameAsIString");
		return IString::Create(self->GetFullName());

	DMS_CALL_END
	return nullptr;
}

TIC_CALL void DMS_CONV DMS_TreeItem_SetDescr(TreeItem* self, CharPtr description)
{
	DMS_CALL_BEGIN

		TreeItemContextHandle checkPtr(self, TreeItem::GetStaticClass(), "DMS_TreeItem_SetDescr");
		self->SetDescr(SharedStr(description));

	DMS_CALL_END
}

TIC_CALL bool DMS_CONV DMS_TreeItem_HasStorage(TreeItem* self)
{
	DMS_CALL_BEGIN

		TreeItemContextHandle checkPtr(self, TreeItem::GetStaticClass(), "DMS_TreeItem_hasStorage");
		return self->HasStorageManager();

	DMS_CALL_END
	return false;
}

TIC_CALL void DMS_CONV DMS_TreeItem_DisableStorage(TreeItem* self)
{
	DMS_CALL_BEGIN

		DBG_START("DMS_TreeItem", "DisableStorage", true);
		TreeItemContextHandle checkPtr(self, TreeItem::GetStaticClass(), "DMS_TreeItem_DisableStorage");
		DBG_TRACE(("self = %s", self->GetName().c_str()));

		self->DisableStorage();

	DMS_CALL_END
}

TIC_CALL bool        DMS_CONV DMS_TreeItem_HasCalculator(const TreeItem* self)
{
	DMS_CALL_BEGIN

		TreeItemContextHandle checkPtr(self, TreeItem::GetStaticClass(), "DMS_TreeItem_HasCalculator");

		return self->HasCalculator();

	DMS_CALL_END
	return false;
}

TIC_CALL CharPtr DMS_CONV DMS_TreeItem_GetExpr(const TreeItem* self)
{
	DMS_CALL_BEGIN

		TreeItemContextHandle checkPtr(self, TreeItem::GetStaticClass(), "DMS_TreeItem_GetExpr");
		SuspendTrigger::Resume();

		return self->GetExpr().c_str();

	DMS_CALL_END
	return "";
}

TIC_CALL void        DMS_CONV DMS_TreeItem_SetExpr(TreeItem* self, CharPtr expression)
{
	DMS_CALL_BEGIN

		DBG_START("DMS_TreeItem", "SetExpr", false);
		TreeItemContextHandle checkPtr(self, TreeItem::GetStaticClass(), "DMS_TreeItem_SetExpr");
		DBG_TRACE(("self = %s", self->GetName().c_str()));
		DBG_TRACE(("expr = %s", expression));

		self->SetExpr(SharedStr(expression));

	DMS_CALL_END
}


TIC_CALL IStringHandle DMS_CONV DMS_TreeItem_GetStoredNameAsIString(const TreeItem* self)
{
	DMS_CALL_BEGIN

		DBG_START("DMS_TreeItem", "GetStoredNameAsIString", true);
		TreeItemContextHandle checkPtr(self, TreeItem::GetStaticClass(), "DMS_TreeItem_GetStoredNameAsIString");
		DBG_TRACE(("self = %s", self->GetName().c_str()));
		
		const TreeItem* parent = self->GetStorageParent(false);
		return IString::Create(parent ? self->GetRelativeName(parent).c_str() : "");

	DMS_CALL_END
	return nullptr;
}

TIC_CALL IStringHandle DMS_CONV DMS_TreeItem_GetSourceNameAsIString(const TreeItem* self)
{
	DMS_CALL_BEGIN

		DBG_START("DMS_TreeItem", "GetSourceNameAsIString", true);
		TreeItemContextHandle checkPtr(self, TreeItem::GetStaticClass(), "DMS_TreeItem_GetSourceNameAsIString");
		DBG_TRACE(("self = %s", self->GetName().c_str()));
		
		return IString::Create(self->GetSourceName());

	DMS_CALL_END
	return nullptr;
}

TIC_CALL CharPtr DMS_CONV DMS_TreeItem_GetAssociatedFilename(const TreeItem* self)
{
	DMS_CALL_BEGIN

		DBG_START("DMS_TreeItem", "GetAssociatedFilename", false);
		TreeItemContextHandle checkPtr(self, TreeItem::GetStaticClass(), "DMS_TreeItem_GetAssociatedFilename");
		DBG_TRACE(("self = %s", self->GetName().c_str()));

		const TreeItem* storageParent = self->GetStorageParent(false);
		if (storageParent)
		{
			const AbstrStorageManager* sm = storageParent->GetStorageManager();
			dms_assert(sm); // because of POSTCONDITION of GetStorageParant
			static SharedStr nameBuffer; // TODO QT: Return string.
			nameBuffer = sm->GetNameStr();
			return nameBuffer.c_str();
		}

	DMS_CALL_END
	return "";
}

TIC_CALL CharPtr DMS_CONV DMS_TreeItem_GetAssociatedFiletype(const TreeItem* self)
{
	DMS_CALL_BEGIN

		DBG_START("DMS_TreeItem", "GetAssociatedFileType", false);
		TreeItemContextHandle checkPtr(self, TreeItem::GetStaticClass(), "DMS_TreeItem_GetAssociatedFiletype");
		DBG_TRACE(("self = %s", self->GetName().c_str()));

		const TreeItem* storageParent = self->GetStorageParent(false);
		if (storageParent && storageParent->GetStorageManager())
			return storageParent->GetStorageManager()->GetClsName().c_str();

	DMS_CALL_END
	return "";
}

TIC_CALL AbstrStorageManager* DMS_CONV DMS_TreeItem_GetStorageManager(const TreeItem* self)
{
	DMS_CALL_BEGIN

		DBG_START("DMS_TreeItem", "GetStorageManager", false);
		TreeItemContextHandle checkPtr(self, TreeItem::GetStaticClass(), "DMS_TreeItem_GetStorageManager");
		DBG_TRACE(("self = %s", self->GetName().c_str()));

		return self->GetStorageManager();

	DMS_CALL_END
	return nullptr;
}

TIC_CALL const TreeItem* DMS_CONV DMS_TreeItem_GetStorageParent (const TreeItem* self)
{
	DMS_CALL_BEGIN
		DBG_START("DMS_TreeItem", "GetStorageParent", false);
		TreeItemContextHandle checkPtr(self, TreeItem::GetStaticClass(), "DMS_TreeItem_GetStorageParent");
		DBG_TRACE(("self = %s", self->GetName().c_str()));

		return self->GetStorageParent(false);

	DMS_CALL_END
	return nullptr;
}

TIC_CALL void DMS_CONV DMS_TreeItem_Invalidate(TreeItem* self)
{
	DMS_CALL_BEGIN

		DBG_START("DMS_TreeItem", "Invalidate", false);
		TreeItemContextHandle checkPtr(self, TreeItem::GetStaticClass(), "DMS_TreeItem_Invalidate");
		DBG_TRACE(("self = %s", self->GetFullName().c_str()));
		
		self->Invalidate();

	DMS_CALL_END
}

bool ItemUpdateImpl(const TreeItem* self, CharPtr context, SharedTreeItemInterestPtr& holder )
{
	CheckPtr(self, TreeItem::GetStaticClass(), context);

	if (self->InTemplate() || self->Is(PS_Committed) || self->IsFailed())
		return true;

	holder = self;
	if (!self->Update(PS_Committed, false, context) && SuspendTrigger::DidSuspend())
		return false;

	return true;
}

void BlockedItemUpdate(const TreeItem* self)
{
	SuspendTrigger::FencedBlocker lockSuspend("DMS_TreeItem_Update");
	SharedTreeItemInterestPtr holder;
	ItemUpdateImpl(self, "DMS_TreeItem_Update", holder);
}

TIC_CALL void DMS_CONV DMS_TreeItem_Update(const TreeItem* self)
{
	DMS_CALL_BEGIN

		BlockedItemUpdate(self);
		if (IsMetaThread())
			ProcessMainThreadOpers();

	DMS_CALL_END
}

bool TreeUpdateImpl(const TreeItem* self, CharPtr context, SharedTreeItemInterestPtr& holder )
{
	for (const TreeItem* walker = self; walker; walker = self->WalkConstSubTree(walker))
		if (!ItemUpdateImpl(walker, context, holder))
			return false;

	return true;
}

TIC_CALL void Tree_Update(const TreeItem* self, CharPtr context)
{
	SuspendTrigger::FencedBlocker lockSuspend(context);
	SharedTreeItemInterestPtr holder;
	TreeUpdateImpl(self, context, holder);
}

#include "time.h"

// TreeItem status management
UInt32 TreeItem_GetProgressState(const TreeItem* self)
{
	DMS_CALL_BEGIN

		SuspendTrigger::FencedBlocker lockSuspend("TreeItem_GetProgressState");
		dms_assert( !SuspendTrigger::DidSuspend() );
		self->UpdateMetaInfo();

		if (self->InTemplate() || self->IsPassor())
			return NC2_Committed;

		auto treeitem_progress_state = self->m_State.GetProgress();
		if (IsDataCurrReady(self->GetCurrRangeItem())) // treeitem_progress_state < PS_Committed && 
			return NC2_DataReady;
		
//		self->DetermineState();
		switch (treeitem_progress_state)//self->m_State.GetProgress())
		{
			case PS_Validated: return NC2_Validated;
			case PS_Committed: return NC2_Committed;
			case PS_MetaInfo:  return NC2_MetaReady;
		}

	DMS_CALL_END
	return NC2_Invalidated;
}

extern "C" TIC_CALL UInt32 DMS_CONV DMS_TreeItem_GetProgressState(const TreeItem* self)
{
	DMS_SE_CALL_BEGIN

		if (dms_is_debuglocked)
			return 0;

		return TreeItem_GetProgressState(self);

	DMS_SE_CALL_END
	return 0;
}

bool TreeItem_IsFailed(const TreeItem* self)
{
	DMS_CALL_BEGIN

		TreeItemContextHandle checkPtr(self, TreeItem::GetStaticClass(), "DMS_TreeItem_IsFailed");

		SuspendTrigger::Resume();

		return !self || self->IsFailed();

	DMS_CALL_END
	return true;
}

extern "C" TIC_CALL bool DMS_CONV DMS_TreeItem_IsFailed(const TreeItem* self)
{
	DMS_SE_CALL_BEGIN

		if (dms_is_debuglocked)
			return true;

		return TreeItem_IsFailed(self);

	DMS_SE_CALL_END
	return true;
}

extern "C" TIC_CALL bool DMS_CONV DMS_TreeItem_IsDataFailed(const TreeItem* self)
{
	DMS_CALL_BEGIN

		if (dms_is_debuglocked)
			return true;

		TreeItemContextHandle checkPtr(self, TreeItem::GetStaticClass(), "DMS_TreeItem_IsDataFailed");

		SuspendTrigger::Resume();

		return !self || self->IsDataFailed();

	DMS_CALL_END
	return true;
}

extern "C" TIC_CALL bool DMS_CONV DMS_TreeItem_IsMetaFailed(const TreeItem* self)
{
	DMS_CALL_BEGIN

		TreeItemContextHandle checkPtr(self, TreeItem::GetStaticClass(), "DMS_TreeItem_IsDataFailed");

		SuspendTrigger::Resume();

		return !self || self->IsFailed(FR_MetaInfo);

	DMS_CALL_END
	return true;
}

TIC_CALL TimeStamp DMS_CONV DMS_TreeItem_GetLastChangeTS(const TreeItem* self)
{
	DMS_CALL_BEGIN

		dms_assert(self);
		return self->GetLastChangeTS();

	DMS_CALL_END
	return 0;
}

TIC_CALL IStringHandle DMS_CONV DMS_TreeItem_GetFailReasonAsIString(const TreeItem* self)
{
	DMS_CALL_BEGIN

		DBG_START("DMS_TreeItem", "GetFailedReason", false);
		TreeItemContextHandle checkPtr(self, TreeItem::GetStaticClass(), "DMS_TreeItem_GetFailReason");
		DBG_TRACE(("self = %s", self->GetName().c_str()));
		
		return IString::Create(self->GetFailReason()->Why());

	DMS_CALL_END
	return nullptr;
}

TIC_CALL void DMS_CONV DMS_TreeItem_ThrowFailReason(const TreeItem* self)
{
	DMS_CALL_BEGIN

		DBG_START("DMS_TreeItem", "ThrowFailedReason", false);

		TreeItemContextHandle checkPtr(self, TreeItem::GetStaticClass(), "DMS_TreeItem_ThrowFailReason");
		DBG_TRACE(("self = %s", self->GetName().c_str()));
		
		if (self->WasFailed())
			self->ThrowFail();

	DMS_CALL_END
}


//----------------------------------------------------------------------
// C style Interface function for treeitem query and traversal 
//----------------------------------------------------------------------

TIC_CALL const TreeItem* DMS_CONV DMS_TreeItem_GetSourceObject(const TreeItem* ti)
{
	DMS_CALL_BEGIN

#if defined(MG_DEBUG_DATA)
		SuspendTrigger::ApplyLock lock;
#endif

		TreeItemContextHandle tich(ti, TreeItem::GetStaticClass(), "DMS_TreeItem_GetSourceObject");
		if (!ti->IsFailed())
		{
			const TreeItem* si = ti->GetSourceItem();
			dms_assert(si != ti); 
			if (si != ti) // REMOVE
				return si;
		}

	DMS_CALL_END

	return nullptr;
}

TIC_CALL TreeItem* DMS_CONV DMS_TreeItem_Copy(TreeItem* dest, const TreeItem* src, CharPtr name)
{
	DMS_CALL_BEGIN
		TreeItemContextHandle checkPtr(src, TreeItem::GetStaticClass(), "DMS_TreeItem_Copy");
		if (dest)
		{
			CheckPtr(dest, TreeItem::GetStaticClass(), "DMS_TreeItem_Copy");
		}

		CopyTreeContext copyContext(dest, src, name, DataCopyMode::CopyExpr);
		return copyContext.Apply();

	DMS_CALL_END
	return dest;
}

TIC_CALL const AbstrDataItem* DMS_CONV DMS_TreeItem_AsAbstrDataItem(const TreeItem* x)
{
	return AsDynamicDataItem(x);
}

TIC_CALL const AbstrUnit*  DMS_CONV DMS_TreeItem_AsAbstrUnit (const TreeItem* x)
{
	return AsDynamicUnit(x);
}

TIC_CALL void DMS_CONV DMS_TreeItem_DoViewAction(const TreeItem* x)
{
	CDebugContextHandle dch("DMS_TreeItem_DoViewAction", "", true);
	DMS_CALL_BEGIN
		TreeItemContextHandle checkPtr(x, TreeItem::GetStaticClass(), "DMS_TreeItem_DoViewAction");
		SuspendTrigger::FencedBlocker lockSuspend("DMS_TreeItem_DoViewAction");

		SharedStr viewAction = TreeItem_GetViewAction(x);
		if (viewAction.empty()) return;
		AbstrCalculatorRef apr = AbstrCalculator::ConstructFromStr(x, viewAction, CalcRole::Other);
		if (!apr)
			return;
		auto res = CalledCalcHandle(apr.get_ptr(), nullptr);

	DMS_CALL_END
}

TIC_CALL bool DMS_CONV DMS_TreeItem_IsEndogenous(const TreeItem* x)
{
	DMS_CALL_BEGIN

		TreeItemContextHandle checkPtr(x, TreeItem::GetStaticClass(), "DMS_TreeItem_IsExogenous");
		return x->IsEndogenous();

	DMS_CALL_END
	return false;
}

TIC_CALL const TreeItem* DMS_CONV DMS_TreeItem_GetCreator(const TreeItem* x)
{
	DMS_CALL_BEGIN

		TreeItemContextHandle checkPtr(x, TreeItem::GetStaticClass(), "DMS_TreeItem_GetCreator");
		if (!x->IsEndogenous())
			return nullptr;
		do {
			x = x->GetTreeParent();
		}
		while (x && ! x->HasCalculator());
		return x;

	DMS_CALL_END
	return nullptr;
}

TIC_CALL const TreeItem* DMS_CONV DMS_TreeItem_GetTemplInstantiator(const TreeItem* x)
{
	DMS_CALL_BEGIN

		TreeItemContextHandle checkPtr(x, TreeItem::GetStaticClass(), "DMS_TreeItem_GetCreator");
		dms_assert(!x->IsCacheItem());
		do {
			if (x->HasCalculator() && x->GetCalculator() && x->GetCalculator()->HasTemplSource())
				return x;

			if (!x->IsEndogenous())
				return nullptr;

			const TreeItem* p = x->GetTreeParent();
			dms_assert(p); // endogenous x cannot be root of configuration
			if (p->HasCalculator() && p->GetCalculator() && p->GetCalculator()->IsForEachTemplHolder())
				return x;

			x = p;
		}
		while (x);

	DMS_CALL_END
	return nullptr;
}

TIC_CALL const TreeItem* DMS_CONV DMS_TreeItem_GetTemplSource(const TreeItem* x)
{
	DMS_CALL_BEGIN

		MGD_PRECONDITION(DMS_TreeItem_GetTemplInstantiator(x) == x);

		TreeItemContextHandle checkPtr(x, TreeItem::GetStaticClass(), "DMS_TreeItem_GetTemplSource");

		if (x->HasCalculator() && x->GetCalculator() && x->GetCalculator()->HasTemplSource())
			return x->GetCalculator()->GetTemplSource();


		const TreeItem* p = x->GetTreeParent();
//		dms_assert(!p || !p->HasCalculator() || !p->GetCalculator() && p->GetCalculator()->IsForEachTemplHolder());

		if (p)
			DMS_TreeItem_GetTemplSource(p);

	DMS_CALL_END
	return nullptr;
}

TIC_CALL const TreeItem* DMS_CONV DMS_TreeItem_GetTemplSourceItem(const TreeItem* templI, const TreeItem* target)
{
	DMS_CALL_BEGIN

		TreeItemContextHandle checkPtr1(templI, TreeItem::GetStaticClass(), "DMS_TreeItem_GetTemplSourceItem");
		TreeItemContextHandle checkPtr2(target, TreeItem::GetStaticClass(), "DMS_TreeItem_GetTemplSourceItem");

		const TreeItem* templS = DMS_TreeItem_GetTemplSource(templI);
		dms_assert(templS);

		return const_cast<TreeItem*>(templS)->GetItem(target->GetRelativeName(templI));

	DMS_CALL_END
	return nullptr;
}

//----------------------------------------------------------------------
// C style Interface function for treeitem error traversal 
//----------------------------------------------------------------------

#include "UsingCache.h"
#include "SupplCache.h"
#include "TreeItemProps.h"

TIC_CALL BestItemRef TreeItem_GetErrorSource(const TreeItem* src, bool tryCalcSuppliers)
{
	TreeItemContextHandle checkPtr1(src, TreeItem::GetStaticClass(), "TreeItem_GetErrorSource");
	if (!src)
		return { nullptr, {} };

	if (!src->WasFailed())
	{
		if (WasInFailed(src))
			return { src->GetTreeParent(), {} };
		return { nullptr, {} };
	}
	// parent ?
	auto context = src->GetTreeParent();
	if (context)
	{
		if (WasInFailed(context))
			return { context, {} };

		// using refs ?
		if (src->CurrHasUsingCache())
		{
			auto usingCache = src->GetUsingCache();
			assert(usingCache);
			for (auto usingUrl : usingCache->UsingUrls())
			{
				auto usingUrlStr = SharedStr(usingUrl);
				auto ur = context->FindBestItem(usingUrlStr);
				if (ur.first && WasInFailed(ur.first))
					return ur;
			}
		}

		// explicit Suppliers ?
		SharedStr strConfigured = explicitSupplPropDefPtr->GetValueAsSharedStr(src);
		if (AbstrCalculator::MustEvaluate(strConfigured.c_str()))
		{
			auto result = AbstrCalculator::GetErrorSource(src, strConfigured);
			if (result.first)
				return result;
			strConfigured = AbstrCalculator::EvaluatePossibleStringExpr(src, strConfigured, CalcRole::Other);
		}


		CharPtr
			iBegin = strConfigured.begin(),
			iEnd = strConfigured.send();
		while (iBegin != iEnd)
		{
			CharPtr iFirstEnd = std::find(iBegin, iEnd, ';');
			CharPtrRange explicitSupplierName(iBegin, iFirstEnd);
			Trim(explicitSupplierName);
			if (!explicitSupplierName.empty())
			{
				auto ur = context->FindBestItem(explicitSupplierName);
				if (ur.first && WasInFailed(ur.first))
					return ur;
			}
			if (iFirstEnd == iEnd)
				break;
			iBegin = iFirstEnd + 1;
		}
	}

	// domain and values units ?
	if (IsDataItem(src))
	{
		BestItemRef result = { AsDataItem(src)->GetAbstrDomainUnit(), {} };
		if (!result.first)
			result = src->FindBestItem(AsDataItem(src)->m_tDomainUnit.AsStrRange());

		if (result.first && WasInFailed(result.first))
			return result;

		result = { AsDataItem(src)->GetAbstrValuesUnit(), {} };
		if (!result.first)
			result = src->FindBestItem(AsDataItem(src)->m_tValuesUnit.AsStrRange());
		if (result.first && WasInFailed(result.first))
			return result;
	}

	// CalcRule ?
	if (src->HasCalculator())
	{
		if (AbstrCalculator::MustEvaluate(src->mc_Expr.c_str()))
		{
			auto result = AbstrCalculator::GetErrorSource(src, src->mc_Expr);
			if (result.first)
				return result;
		}

		auto sc = src->GetCalculator();
		if (sc)
		{
			BestItemRef si = sc->FindErrorneousItem();
			if (si.first && WasInFailed(si.first))
				return si;
		}
	}

	// SourceItem
	auto sourceItem = src->GetCurrSourceItem();
	if (sourceItem)
	{
		assert(!sourceItem->IsCacheItem());
		assert(sourceItem != src);
		if (WasInFailed(sourceItem))
			return { sourceItem, {} };
	}

	// try all suppliers
	const TreeItem* errorneousItem = nullptr;
	auto errorChecker = [&errorneousItem](const Actor* a)
		{
			auto ti = dynamic_cast<const TreeItem*>(a);
			if (ti && !ti->IsCacheItem() && WasInFailed(ti))
			{
				errorneousItem = ti;
				return  AVS_SuspendedOrFailed;
			}
			return AVS_Ready;
		};
	auto visitor = MakeDerivedBoolVisitor(std::move(errorChecker));

	src->VisitSuppliers(SupplierVisitFlag::CalcErrorSearch, std::move(visitor));
	if (errorneousItem)
		return { errorneousItem , {} };

	// if FailReason was > FR_Data, try finding a supplier that fails too when pressed.
	if (tryCalcSuppliers && src->WasFailed(FR_Data) && !src->WasFailed(FR_MetaInfo))
	{
		if (src->HasCalculator())
		{
			auto sc = src->GetCalculator();
			return sc->FindPrimaryDataFailedItem();
		}
	}
	return { nullptr, {} };
}

TIC_CALL BestItemRef TreeItem_GetErrorSourceCaller(const TreeItem* src)
{
	try {
		return TreeItem_GetErrorSource(src, true);
	}
	catch (...)
	{
		catchAndReportException();
	}
	return {};
}

extern "C" TIC_CALL const TreeItem* DMS_CONV DMS_TreeItem_GetErrorSource(const TreeItem* src, IStringHandle* unfoundPart)
{
	DMS_CALL_BEGIN

		BestItemRef result = TreeItem_GetErrorSourceCaller(src);
		if (unfoundPart)
			*unfoundPart = IString::Create(result.second);
		return result.first;

	DMS_CALL_END

	if (unfoundPart)
		*unfoundPart = nullptr;
	return nullptr;
}


TIC_CALL void DMS_CONV DMS_Tic_Load()
{
	DMS_Rtc_Load();
}

