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
#include "TicPCH.h"
#pragma hdrstop

#include "FreeDataManager.h"

#include "DbgInterface.h"
#include "utl/Environment.h"
#include "geo/Couple.h"
#include "mci/ValueClass.h"
#include "utl/IncrementalLock.h"

#include "AbstrDataItem.h"
#include "AbstrDataObject.h"
#include "DataStoreManagerCaller.h"
#include "DataItemClass.h"

// ====================== public Static functions

extern "C" TIC_CALL void DMS_CONV DMS_DataStoreManager_SetSwapfileMinSize(UInt32 sz)
{
	RTC_SetRegDWord(RegDWordEnum::SwapFileMinSize, sz);
}

extern "C" TIC_CALL UInt32 DMS_CONV GetSwapfileMinSize()
{
	return RTC_GetRegDWord(RegDWordEnum::SwapFileMinSize);
}

UInt32 ElemSize(const ValueClass* vc)
{
	Int32 result = vc->GetSize();
	if (result > 0)
		return result;
	return sizeof(Couple<UInt32>)*4;
}

UInt32 AbstrDataByteSize(const AbstrDataItem* adi)
{
	const ValueClass* vc = adi->GetDynamicObjClass()->GetValuesType();
	dms_assert(vc);
	return ElemSize(vc) * adi->GetAbstrDomainUnit()->GetCount();
}
/* REMOVE
bool IsFileableSize(const AbstrDataItem* adi, SizeT nrBytes)
{
	if (!nrBytes)
		return false;

	if (adi->GetStoreDataState())
		return true;

	if (!adi->GetFreeDataState())
	{
		if (!adi->IsPassor())
			return true;
		if (debug_cast<const TreeItem*>(adi->GetRoot())->GetTSF(TSF_HasPseudonym))
			return true;
	}
	auto swapFileMinSize = RTC_GetRegDWord(RegDWordEnum::SwapFileMinSize);
	return swapFileMinSize && (nrBytes >= swapFileMinSize);
}
*/

bool MustStorePersistent(const TreeItem* ti)
{
	MG_CHECK2(false, "NYI");
	/*
		if (ti->GetFreeDataState())
		return false;
	if (ti->IsPassor())
		return ti->IsDcKnown();
	else
		return !ti->HasConfigSource();
	*/
}


