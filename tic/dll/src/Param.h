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

#if !defined(__TIC_PARAM_H)
#define __TIC_PARAM_H

#include "geo/Conversions.h"
#include "CheckedDomain.h"

#include "DataArray.h"
#include "Unit.h"
#include "UnitClass.h"

template <typename V>
AbstrDataItem* CreateCacheParam()
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
SharedPtr<AbstrDataItem> 
CreateConstParam(const T& value)
{
	typedef DataArray<T> DataObjectType;

	StaticStIncrementalLock<TreeItem::s_MakeEndoLockCount> makeEndoLock;
	SharedPtr<AbstrDataItem> dataItem = CreateCacheParam<T>();

	UpdateMarker::ChangeSourceLock changeStamp( dataItem, "CreateConstParam");
	SuspendTrigger::FencedBlocker progressLock("CreateConstParam");

	DataWriteLock lock(dataItem);
	DataObjectType* dataObj = mutable_array_cast<T>(lock);
	auto data = dataObj->GetDataWrite();
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
