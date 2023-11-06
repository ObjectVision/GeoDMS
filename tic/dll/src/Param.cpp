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

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

#include "Param.h"

#include "act/InterestRetainContext.h"
#include "dbg/DmsCatch.h"
#include "mci/ValueWrap.h"
#include "mci/ValueClass.h"
#include "geo/StringBounds.h"
#include "ser/SequenceArrayStream.h"

#include "AbstrDataItem.h"
#include "Unit.h"
#include "UnitClass.h"
#include "DataItemClass.h"
#include "TreeItemClass.h"
#include "TreeItemContextHandle.h"

//----------------------------------------------------------------------
// DMS interface functions
//----------------------------------------------------------------------

#include "TicInterface.h"

#define INSTANTIATE(T) \
TIC_CALL api_type<T>::type DMS_CONV DMS_##T##Param_GetValue(const AbstrDataItem* self) \
{ \
	DMS_CALL_BEGIN \
		TreeItemContextHandle checkPtr(self, DataArray<T>::GetStaticClass(), "DMS_" #T "Param_GetValue"); \
		return GetTheValue<T>(self); \
	DMS_CALL_END \
	return UNDEFINED_OR_ZERO(T); \
} \
TIC_CALL void DMS_CONV DMS_##T##Param_SetValue(AbstrDataItem* self, api_type<T>::type value) \
{ \
	DMS_CALL_BEGIN \
		TreeItemContextHandle checkPtr(self, DataArray<T>::GetStaticClass(), "DMS_" #T "Param_SetValue"); \
		SetTheValue<T>(self,value); \
		return; \
	DMS_CALL_END \
	return; \
} \

	INSTANTIATE_NUM_ELEM
	INSTANTIATE(Bool)

#undef INSTANTIATE

TIC_CALL Float64 DMS_CONV DMS_NumericParam_GetValueAsFloat64(const AbstrParam* self)
{
	DMS_CALL_BEGIN

		TreeItemContextHandle context(self, AbstrDataItem::GetStaticClass(), "DMS_NumericParam_GetValueAsFloat64");

		PreparedDataReadLock dlr(self);

		dms_assert(self->GetAbstrDomainUnit()->GetCount() == 1);
		return self->GetDataObj()->GetValueAsFloat64(0);

	DMS_CALL_END
	return UNDEFINED_VALUE(Float64);
}

TIC_CALL void DMS_CONV DMS_NumericParam_SetValueAsFloat64(AbstrParam* self, Float64 value)
{
	DMS_CALL_BEGIN

		TreeItemContextHandle context(self, AbstrDataItem::GetStaticClass(), "DMS_NumericParam_SetValueAsFloat64");

		DataWriteLock lock(self);

		dms_assert(self->GetAbstrDomainUnit()->GetCount() == 1);
		lock->SetValueAsFloat64(0, value);
		lock.Commit();

	DMS_CALL_END
}

Int32 NumericParam_GetValueAsInt32(const AbstrParam* self)
{
	FencedInterestRetainContext irc;
	InterestRetainContextBase::Add(self);
	PreparedDataReadLock dlr(self);

	dms_assert(self->GetAbstrDomainUnit()->GetCount() == 1);
	return self->GetRefObj()->GetValueAsInt32(0);
}

TIC_CALL void DMS_CONV DMS_StringParam_SetValue(AbstrParam* self, CharPtr value)
{
	DMS_CALL_BEGIN

		CheckPtr(self, DataArray<SharedStr>::GetStaticClass(), "DMS_StringParam_SetValue");
		self->SetTSF(TSF_HasConfigData);
		SetTheValue<SharedStr>(self, SharedStr(value));
		return;

	DMS_CALL_END
	return;
}
