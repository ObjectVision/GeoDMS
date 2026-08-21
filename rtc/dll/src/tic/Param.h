// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////
#pragma once

#if !defined(__TIC_PARAM_H)
#define __TIC_PARAM_H

#include "vt/Conversions.h"
#include "CheckedDomain.h"

#include "DataArray.h"
#include "Unit.h"
#include "UnitClass.h"

template <typename V>
SharedMutableDataItem CreateCacheParam()
{
	return
		CreateCacheDataItem(
			Unit<Void             >::GetStaticClass()->CreateDefault(),
			Unit<typename field_of<V>::type>::GetStaticClass()->CreateDefault(),
			COMPOSITION(V)
		);
}

/********** CreateConstParam            **********/

template <typename T>
SharedMutableDataItem
CreateConstParam(const T& value)
{
	typedef DataArray<T> DataObjectType;

	StaticStIncrementalLock<TreeItem::s_MakeEndoLockCount> makeEndoLock;
	SharedMutableDataItem dataItem = CreateCacheParam<T>();

	UpdateMarker::ChangeSourceLock changeStamp( dataItem.get(), "CreateConstParam");
	SuspendTrigger::FencedBlocker progressLock("CreateConstParam");

	DataWriteLock lock(dataItem.get());
	DataObjectType* dataObj = mutable_array_cast<T>(lock);
	auto data = dataObj->GetDataWrite(no_tile, dms_rw_mode::write_only_all);
	dms_assert(data.size() == 1);
	Assign(data[0], value);

	dataItem->DisableStorage();
	dataItem->SetKeepDataState(true);
	dataItem->SetFreeDataState(true);
	lock.Commit();
	return dataItem;
}

Int32 NumericParam_GetValueAsInt32(const AbstrParam* self); 

#endif //__TIC_PARAM_H
