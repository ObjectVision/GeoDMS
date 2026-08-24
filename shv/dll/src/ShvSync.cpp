// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////

#include "ShvDllPCH.h"
#include "act/UpdateMark.h" // UpdateMarker

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif

// Assorted shv helpers (see ShvUtils.h): values-unit and classification
// lookups, ViewData properties, status text and selection utilities.

#include <format>
#include "ShvUtils.h"

#include "dbg/debug.h"
#include "dbg/DebugContext.h"
#include "vt/BaseBounds.h"
#include "vt/Conversions.h"
#include "vt/Pair.h"
#include "mci/Class.h"
#include "mci/CompositeCast.h"
#include "mci/ValueClass.h"
#include "mci/ValueWrap.h"
#include "utl/Environment.h"
#include "utl/PlatformError.h"
#include "utl/IncrementalLock.h"
#include "utl/StrFormat.h"
#include "utl/splitPath.h"

#include "AbstrDataItem.h"
#include "AbstrDataObject.h"
#include "AbstrUnit.h"
#include "DataArray.h"
#include "DataArrayValue.h"
#include "DataItemClass.h"
#include "DataLocks.h"
#include "Projection.h"
#include "PropFuncs.h"
#include "SessionData.h"
#include "TicInterface.h"
#include "TreeItemClass.h"
#include "Unit.h"
#include "UnitClass.h"
#include "UnitProcessor.h"

#include "StgBase.h"

#include "CalcClassBreaks.h"
#include "ValuesTable.h"

#include "GeoTypes.h"

#include "Aspect.h"

#include "DataView.h"
#include "DcHandle.h"
#include "GraphicObject.h"
#include "LayerClass.h"
#include "Theme.h"

#ifdef _WIN32
#include "shellscalingapi.h"
#endif


// TreeItem DialogData/ViewData property access and the generic
// SaveValue/LoadValue/SyncValue/SyncState persistence templates.
// Split from ShvUtils.cpp (2026-08).


//----------------------------------------------------------------------
// section : TreeItem Properties
//----------------------------------------------------------------------

SharedStr GetViewData(const TreeItem* item)         // look  at DialogData of subItem "ViewData"
{
	dms_assert(item);
	item = item->GetConstSubTreeItemByID(GetTokenID_mt("ViewData")).get();
	if (item)
		return TreeItem_GetDialogData(item );
	return SharedStr();
}

void SetViewData(TreeItem* item, CharPtr data) // store at DialogData of subItem "ViewData"
{
	dms_assert(item);
	TreeItem_SetDialogData(item->CreateItem(GetTokenID_mt("ViewData")).get(), data);
}

const TreeItem* GetNextDialogDataRef(const TreeItem* item, CharPtr& i, CharPtr e)
{
	CharPtr orgI = i;
	CharPtr newI = std::find(i, e, ';');
	i = newI;
	if (newI != e) ++i;
	return item->ResolveItemPath( CharPtrRange(orgI, newI) ).get();
}

const TreeItem* GetDialogDataRef(const TreeItem* item)
{
	dms_assert(item);

	SharedStr dd = TreeItem_GetDialogData(item);
	if (dd.empty())
		return nullptr;
	CharPtr i = dd.begin();
	const TreeItem* result = GetNextDialogDataRef(item, i, dd.send() );
	if (!result)
		item->throwItemErrorF("Cannot find DialogData reference '{}'", dd.c_str());
	return result;
}

void SetDialogDataRef(TreeItem* item, const TreeItem* ref)
{
	if (!item)
		return;
	if (ref)
		TreeItem_SetDialogData(item, ref->GetFullName());
	else
		TreeItem_SetDialogData(item, "");
}

//----------------------------------------------------------------------
// section : Sync & Save
//----------------------------------------------------------------------


template <typename A, typename T>
void SyncRefImpl(T& ptr, TreeItem* context, TokenID id, ShvSyncMode sm)
{
	dms_assert(context);
	ObjectIdContextHandle contextHandle(context, id, (sm == SM_Load) ? "LoadRef" : "SaveRef");

	const TreeItem* subItem = FindTreeItemByID(context, id);
	if (sm == SM_Load)
	{
		if (subItem)
		{
			const TreeItem* refItem = GetDialogDataRef(subItem);
			if (refItem)
			{
				ptr = make_shared_tree(checked_valcast<const A*>(refItem), existing_obj{});
				return;
			}
		}
		ptr = nullptr;
		return;
	}

	if(sm == SM_Save && (ptr || subItem) ) 
	{
		if (!subItem)
			subItem = context->CreateItem(id).get();
		SetDialogDataRef(const_cast<TreeItem*>(subItem), ptr ? &*ptr : nullptr); // T is std::shared_ptr or InterestPtr; both deref
	}
}

void SyncRef(std::shared_ptr<const TreeItem>& ptr, TreeItem* context, TokenID id, ShvSyncMode sm) { SyncRefImpl<TreeItem>(ptr, context, id, sm); }
void SyncRef(SharedDataItemInterestPtr& ptr, TreeItem* context, TokenID id, ShvSyncMode sm) { SyncRefImpl<AbstrDataItem>(ptr, context, id, sm); }
void SyncRef(std::shared_ptr<const AbstrUnit>& ptr, TreeItem* context, TokenID id, ShvSyncMode sm) { SyncRefImpl<AbstrUnit>(ptr, context, id, sm); }

template <typename V>
void SaveValue(TreeItem* context, TokenID nameID, typename param_type<V>::type value)
{
	if (!context)
		return;

	SharedMutableDataItem adi = make_shared_tree(const_cast<AbstrDataItem*>(AsDataItem(FindTreeItemByID(context, nameID))), existing_obj{});
	if (!adi)
		adi = CreateDataItem(context, nameID, Unit<Void>::GetStaticClass()->CreateDefault(), Unit<V>::GetStaticClass()->CreateDefault());

	adi->DisableStorage();
	adi->SetKeepDataState(true);
	adi->UpdateMetaInfo();
	::SetTheValue<V>(adi.get(), value);
}

template <typename T>
T LoadValue(const TreeItem* context, TokenID nameID, typename param_type<T>::type defaultValue)
{
	if (context)
	{
		const TreeItem* refItem = FindTreeItemByID(context, nameID);
		if (IsDataItem(refItem))
		{
			refItem->UpdateMetaInfo();
			const AbstrDataItem* refAdi = AsDataItem(refItem);
			if (refAdi->GetDynamicObjClass()->GetValuesType() == ValueWrap<T>::GetStaticClass())
			{
				InterestRetainContextBase keepInterest;
				keepInterest.Add(refAdi);

				return refAdi->LockAndGetValue<T>(0);
			}
		}
	}
	return defaultValue;
}

template DPoint  LoadValue<DPoint >(const TreeItem* context, TokenID nameID, const DPoint& defaultValue);
template IPoint  LoadValue<IPoint >(const TreeItem* context, TokenID nameID, const IPoint  defaultValue);
template Float64 LoadValue<Float64>(const TreeItem* context, TokenID nameID, const Float64 defaultValue);

template <typename T>
void SyncValue(TreeItem* context, TokenID nameID, T& value, typename param_type<T>::type defaultValue, ShvSyncMode sm)
{
	dms_assert(context);
	if (!context) // REMOVE?
		return;

	if (sm == SM_Load)
		value = LoadValue<T>(context, nameID, defaultValue);
	else
	{	dms_assert(sm == SM_Save);
		if ((value  != defaultValue || FindTreeItemByID(context, nameID)) )
			SaveValue<T>(context, nameID, value);
	}
}
template void SyncValue<UInt32   >(TreeItem* context, TokenID nameID, UInt32& value, UInt32 defaultValue, ShvSyncMode sm);
template void SyncValue<SharedStr>(TreeItem* context, TokenID nameID, SharedStr& value, WeakStr defaultValue, ShvSyncMode sm);
template void SyncValue<SPoint   >(TreeItem* context, TokenID nameID, SPoint& value, SPoint defaultValue, ShvSyncMode sm);
template void SyncValue<Bool     >(TreeItem* context, TokenID nameID, Bool  & value, Bool   defaultValue, ShvSyncMode sm);


void SyncValue(TreeItem* context, TokenID nameID, SharedStr& value, ShvSyncMode sm)
{
	if (!context)
		return;

	const TreeItem* refItem = FindTreeItemByID(context, nameID);
	if (sm == SM_Load)
	{
		if (refItem)
		{
			InterestRetainContextBase keepInterest;
			keepInterest.Add(refItem);

			value = AsDataItem(refItem)->LockAndGetValue<SharedStr>(0).c_str();
		}
		else
			value = SharedStr();
	}
	else if (sm == SM_Save && (!value.empty() || refItem) )
		SaveValue<SharedStr>(context, nameID, value);
}


void SyncState(GraphicObject* obj, TreeItem* context, TokenID stateID, UInt32 state, bool defaultValue, ShvSyncMode sm)
{
	dms_assert(obj);
	dms_assert(context);
	const TreeItem* refItem = FindTreeItemByID(context, stateID);
	if (refItem && sm == SM_Load)
	{
		InterestRetainContextBase keepInterest;
		keepInterest.Add(refItem);

		obj->m_State.Set(state, AsDataItem(refItem)->LockAndGetValue<Bool>(0));
	}
	else if (sm == SM_Save && (obj->m_State.Get(state) != defaultValue || refItem) )
//		SaveState(obj, context, stateID, state);
		SaveValue<Bool>(context, stateID, obj->m_State.Get(state));
}

void ChangePoint(AbstrDataItem* pointItem, const CrdPoint& point, bool isNew)
{
	if (!pointItem)
		return;

	dms_assert(pointItem->GetAbstrDomainUnit()->GetCount() == 1);

	StaticMtIncrementalLock<TreeItem::s_NotifyChangeLockCount> dontNotify;

	pointItem->UpdateMetaInfo();

	if (!isNew && pointItem->HasDataObj())
	{
		auto ado = pointItem->GetDataObj();
		if (ado->GetValueAsDPoint(0) == point)
			return;
	}
	DataWriteLock dataHolder(pointItem);
	MG_CHECK(dataHolder->GetTiledRangeData()->GetRangeSize() == 1);
	dms_assert(pointItem->m_DataLockCount < 0);

	dataHolder->SetValueAsDPoint(0, point);
	dataHolder.Commit();
}
