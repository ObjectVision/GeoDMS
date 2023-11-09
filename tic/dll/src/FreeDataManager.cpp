// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "TicPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

#include "FreeDataManager.h"

#include "DbgInterface.h"
#include "utl/Environment.h"
#include "geo/Couple.h"
#include "mci/ValueClass.h"
#include "utl/IncrementalLock.h"

#include "AbstrDataItem.h"
#include "AbstrDataObject.h"
#include "AbstrUnit.h"
#include "DataStoreManagerCaller.h"
#include "DataItemClass.h"

// ====================== public Static functions

extern "C" TIC_CALL void DMS_CONV DMS_DataStoreManager_SetSwapfileMinSize(UInt32 sz)
{
	RTC_SetCachedDWord(RegDWordEnum::SwapFileMinSize, sz);
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


