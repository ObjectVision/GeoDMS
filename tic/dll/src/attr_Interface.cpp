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

#include "TicInterface.h"

#include "dbg/debug.h"
#include "dbg/DmsCatch.h"

#include "AbstrDataItem.h"
#include "DataLocks.h"
#include "DataArray.h"
#include "DataItemClass.h"
#include "TreeItemClass.h"
#include "TreeItemContextHandle.h"
#include "CheckedDomain.h"

TIC_CALL bool DMS_CONV 
DMS_AnyDataItem_GetValueAsCharArray(
	const AbstrDataItem* self, 
	SizeT index, char* clientBuffer, UInt32 clientBufferLen)
{
	DMS_CALL_BEGIN

		TreeItemContextHandle checkPtr(self, AbstrDataItem::GetStaticClass(), "DMS_AnyDataItem_GetValueAsCharArray");

		PreparedDataReadLock drl(self);
		GuiReadLock lockHolder;
		return self->GetRefObj()->AsCharArray(index, clientBuffer, clientBufferLen, lockHolder, FormattingFlags::None);

	DMS_CALL_END
	_snprintf(clientBuffer, clientBufferLen, "<err>");
	return false;
}

TIC_CALL UInt32 DMS_CONV 
DMS_AnyDataItem_GetValueAsCharArraySize(
	const AbstrDataItem* self, SizeT index)
{
	DMS_CALL_BEGIN

		TreeItemContextHandle checkPtr(self, AbstrDataItem::GetStaticClass(), "DMS_AnyDataItem_GetValueAsCharArraySize");
		PreparedDataReadLock drl(self);
		GuiReadLock lockHolder;

		return self->GetRefObj()->AsCharArraySize(index, -1, lockHolder, FormattingFlags::None);

	DMS_CALL_END
	return UNDEFINED_VALUE(UInt32);
}

TIC_CALL void DMS_CONV 
DMS_AnyDataItem_SetValueAsCharArray(
	AbstrDataItem* self, 
	SizeT index, CharPtr clientBuffer)
{
	DMS_CALL_BEGIN

		TreeItemContextHandle checkPtr(self, AbstrDataItem::GetStaticClass(), "DMS_AnyDataItem_SetValueAsCharArray");

		DataWriteLock lock(self, dms_rw_mode::read_write);

		OwningPtr<AbstrValue> value = self->CreateAbstrValue();
		value->AssignFromCharPtr(clientBuffer);
		lock->SetAbstrValue(index, *value);

		lock.Commit();

	DMS_CALL_END
}

//----------------------------------------------------------------------
// DMS interface functions
//----------------------------------------------------------------------

TIC_CALL UInt32   DMS_CONV DMS_DataItem_GetNrFeatures(AbstrDataItem* self)
{
	DMS_CALL_BEGIN

		TreeItemContextHandle checkPtr(self, AbstrDataItem::GetStaticClass(), "DMS_DataItem_GetNrFeatures");
		return self->GetAbstrDomainUnit()->GetCount();

	DMS_CALL_END
	return UNDEFINED_VALUE(UInt32);
}

TIC_CALL bool   DMS_CONV DMS_DataItem_IsInMem(AbstrDataItem* self)
{
	DMS_CALL_BEGIN

		TreeItemContextHandle checkPtr(self, AbstrDataItem::GetStaticClass(), "DMS_DataItem_IsInMem");
		return AsDataItem(self->GetUltimateItem())->m_DataObject;

	DMS_CALL_END
	return false;
}

TIC_CALL Int32  DMS_CONV DMS_DataItem_GetLockCount(AbstrDataItem* self)
{
	DMS_CALL_BEGIN

		TreeItemContextHandle checkPtr(self, AbstrDataItem::GetStaticClass(), "DMS_DataItem_GetLockCount");
		return self->GetDataRefLockCount();

	DMS_CALL_END
	return 0;
}

TIC_CALL bool DMS_AbstrDataItem_HasVoidDomainGuarantee(const AbstrDataItem* self)
{
	DMS_CALL_BEGIN

		TreeItemContextHandle checkPtr(self, AbstrDataItem::GetStaticClass(), "DMS_AbstrDataItem_HasVoidDomainGuarantee");

		return self->HasVoidDomainGuarantee();

	DMS_CALL_END
	return false;
}


/*********************************** GetValueArrays *************************************/

TIC_CALL void DMS_CONV DMS_NumericAttr_GetValuesAsFloat64Array(const AbstrDataItem* self, SizeT index, SizeT len, Float64* data)
{
	DMS_CALL_BEGIN
		TreeItemContextHandle checkPtr(self, AbstrDataItem::GetStaticClass(), "DMS_NumericAttr_GetValuesAsFloat64Array");

		PreparedDataReadLock dlr(self);
		while (len)
		{
			auto tl = self->GetAbstrDomainUnit()->GetTiledRangeData()->GetTiledLocation(index);
//			ReadableTileLock tileLck(self->GetRefObj(), tl.first);
			auto nrRead = self->GetRefObj()->GetValuesAsFloat64Array(tl, len, data);
			MG_CHECK(nrRead);

			len  -= nrRead;
			data += nrRead;
		}
	DMS_CALL_END
}

TIC_CALL void DMS_CONV DMS_NumericAttr_SetValuesAsFloat64Array(AbstrDataItem* self, SizeT index, SizeT len, const Float64* data)
{
	DMS_CALL_BEGIN
		TreeItemContextHandle checkPtr(self, AbstrDataItem::GetStaticClass(), "DMS_NumericAttr_SetValuesAsFloat64Array");

		DataWriteLock lock(self, dms_rw_mode::read_write);

		PreparedDataReadLock dlr(self);
		auto tl = self->GetAbstrDomainUnit()->GetTiledRangeData()->GetTiledLocation(index);
//		ReadableTileLock tileLck(self->GetRefObj(), tl.first);
		lock->SetValuesAsFloat64Array(tl, len, data);

		lock.Commit();

	DMS_CALL_END
}

TIC_CALL void DMS_CONV DMS_NumericAttr_GetValuesAsInt32Array  (const AbstrDataItem* self, SizeT index, SizeT len, Int32* data)
{
	DMS_CALL_BEGIN
		TreeItemContextHandle checkPtr(self, AbstrDataItem::GetStaticClass(), "DMS_NumericAttr_GetValuesAsInt32Array");

		PreparedDataReadLock dlr(self);
		while (len)
		{
			auto tl = self->GetAbstrDomainUnit()->GetTiledRangeData()->GetTiledLocation(index);

//			ReadableTileLock tileLck(self->GetRefObj(), tl.first);
			SizeT nrRead = self->GetRefObj()->GetValuesAsInt32Array(tl, len, data);
			MG_CHECK(nrRead);

			len  -= nrRead;
			data += nrRead;
		}

	DMS_CALL_END
}

/*********************************** SetValueArrays *************************************/

TIC_CALL void    DMS_CONV DMS_NumericAttr_SetValuesAsInt32Array  (AbstrDataItem* self, SizeT index, SizeT len, const Int32* data)
{
	DMS_CALL_BEGIN
		TreeItemContextHandle checkPtr(self, AbstrDataItem::GetStaticClass(), "DMS_NumericAttr_SetValuesAsInt32Array");

		DataWriteLock lock(self, dms_rw_mode::read_write);
		tile_loc location = self->GetAbstrDomainUnit()->GetTiledRangeData()->GetTiledLocation(index);
	
		lock->SetValuesAsInt32Array(location, len, data);

		lock.Commit();

	DMS_CALL_END
}


/*********************************** SetRangeValue *************************************/

TIC_CALL Float64 DMS_CONV DMS_NumericAttr_GetValueAsFloat64 (const AbstrDataItem* self, SizeT index)
{
	DMS_CALL_BEGIN

		TreeItemContextHandle checkPtr(self, (AbstrDataItem::GetStaticClass()), "DMS_NumericAttr_GetValueAsFloat64");

		PreparedDataReadLock dlr(self);
		return self->GetRefObj()->GetValueAsFloat64(index);

	DMS_CALL_END
	return UNDEFINED_VALUE(Float64);
}

TIC_CALL void DMS_CONV DMS_NumericAttr_SetValueAsFloat64 (AbstrDataItem* self, SizeT index, Float64 value)
{
	DMS_CALL_BEGIN

		TreeItemContextHandle checkPtr(self, (AbstrDataItem::GetStaticClass()), "DMS_NumericAttr_SetValueAsFloat64");

		DataWriteLock lock(self, dms_rw_mode::read_write);

		lock->SetValueAsFloat64(index, value);

		lock.Commit();

	DMS_CALL_END
}

TIC_CALL Int32 DMS_CONV DMS_NumericAttr_GetValueAsInt32 (const AbstrDataItem* self, SizeT index)
{
	DMS_CALL_BEGIN

		TreeItemContextHandle checkPtr(self, (AbstrDataItem::GetStaticClass()), "DMS_NumericAttr_GetValueAsInt32");

		PreparedDataReadLock dlr(self);
		return self->GetRefObj()->GetValueAsInt32(index);

	DMS_CALL_END
	return UNDEFINED_VALUE(Int32);
}

TIC_CALL void DMS_CONV DMS_NumericAttr_SetValueAsInt32 (AbstrDataItem* self, SizeT index, SizeT value)
{
	DMS_CALL_BEGIN

		TreeItemContextHandle checkPtr(self, (AbstrDataItem::GetStaticClass()), "DMS_NumericAttr_SetValueAsInt32");

		DataWriteLock lock(self, dms_rw_mode::read_write);

		lock->SetValueAsInt32(index, value);

		lock.Commit();

	DMS_CALL_END
}

