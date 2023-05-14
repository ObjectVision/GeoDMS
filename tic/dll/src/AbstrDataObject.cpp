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

#include "AbstrDataObject.h"
#include "TiledRangeData.h"

#include "geo/Range.h"
#include "mci/ValueWrap.h"
#include "mci/PropDef.h"
#include "mci/ValueClass.h"
#include "mem/tiledata.h"

#include "AbstrDataItem.h"
#include "DataArray.h"
#include "AbstrUnit.h"
#include "CopyTreeContext.h"
#include "DataItemClass.h"
#include "DataLocks.h"
#include "TileChannel.h"
#include "TreeItemClass.h"

//  -----------------------------------------------------------------------
//  Class  : AbstrDataObject
//  -----------------------------------------------------------------------
/* REMOVE
void AbstrDataObject::InitDataObj(const AbstrDataItem* owner,
	const AbstrUnit* domainUnit,
	const AbstrUnit* valuesUnit)
{
	if (m_DomainUnitCopy)
	{
		if (m_DomainUnitCopy != domainUnit)
		{
			dms_assert(m_DomainUnitCopy->GetUltimateItem() == domainUnit->GetUltimateItem());
			m_DomainUnitCopy->UnifyDomain(domainUnit, UM_Throw);
			// TODO: NOG BETER: ValidateRange subtieler maken (IsContaining in goede richting)
		}
	}
	else
	{
		m_DomainUnitCopy = domainUnit;
		if (owner && !owner->IsEndogenous() && domainUnit)
			m_DomainUnitCopy->AddDataItemOut(owner);
	}

	if (m_ValuesUnitCopy)
	{
		if (m_ValuesUnitCopy != valuesUnit)
			m_ValuesUnitCopy->UnifyValues(valuesUnit, UnifyMode(UM_Throw));
	}
	else
	{
		if (!CheckValuesUnit(valuesUnit))
		{
			PersistentSharedObj::throwItemErrorF(owner,
				"InitDataObj: unexpected type %s of values unit %s ",
				valuesUnit->GetValueType()->GetName(),
				valuesUnit->GetFullName().c_str()
			);
		}
		m_ValuesUnitCopy = valuesUnit;
	}

	dms_assert(m_DomainUnitCopy);
	dms_assert(m_ValuesUnitCopy);

}
*/

SharedPtr<const AbstrTileRangeData> AbstrDataObject::GetTiledRangeData() const
{ 
	return m_TileRangeData;
}

const DataItemClass* AbstrDataObject::GetDataItemClass() const
{
	const DataItemClass* rtc = debug_cast<const DataItemClass*>(GetDynamicClass());
	dms_assert(rtc);
	return rtc;
}

const ValueClass* AbstrDataObject::GetValuesType() const
{
	return GetDataItemClass()->GetValuesType();
}

//----------------------------------------------------------------------
// CopyData
//----------------------------------------------------------------------

// TODO G8: use info->changePos.

void CopyData(const AbstrDataObject* oldDataO, AbstrDataObject* newDataO, const DomainChangeInfo* info)
{
	auto oldDataSize = oldDataO->GetTiledRangeData()->GetDataSize();
	auto newDataSize = newDataO->GetTiledRangeData()->GetDataSize();
	auto copySize = std::min(oldDataSize, newDataSize);
	visit<typelists::value_elements>(oldDataO->GetValueClass(),
		[oldDataO, newDataO, info, newDataSize, copySize]<typename V>(const V*)
		{
			auto restSize = copySize;
			auto oldData = const_array_cast<V>(oldDataO);
			auto writeChannel = tile_write_channel<V>(mutable_array_checkedcast<V>(newDataO));
			auto tn = oldDataO->GetTiledRangeData()->GetNrTiles();
			for (tile_id t = 0; t != tn; ++t)
			{
				auto tileData = oldData->GetTile(t);
				auto currWriteSize = std::min(restSize, tileData.size());
				writeChannel.Write(tileData.begin(), tileData.begin()+currWriteSize);
				restSize -= currWriteSize;
			}
			writeChannel.WriteConst(V(), newDataSize - copySize);
			dms_assert(writeChannel.IsEndOfChannel());
		}
	);
}


//----------------------------------------------------------------------
// Tile support
//----------------------------------------------------------------------


tile_loc AbstrDataObject::GetTiledLocation(row_id idx) const
{
	return GetTiledRangeData()->GetTiledLocation(idx);
}

//----------------------------------------------------------------------
// Illegal Abstracts
//----------------------------------------------------------------------

void AbstrDataObject::illegalNumericOperation() const
{
	throwErrorF("DataObject", "illegal numeric operation called on DataItem<%s>", GetValuesType()->GetName().c_str());
}

void AbstrDataObject::illegalGeometricOperation() const
{
	throwErrorF("DataObject", "illegal geometric operation called on DataItem<%s>", GetValuesType()->GetName().c_str());
}

// Support for numerics (optional)
Float64 AbstrDataObject::GetValueAsFloat64(SizeT  index) const                { illegalNumericOperation(); }
void    AbstrDataObject::SetValueAsFloat64(SizeT  index, Float64 val)         { illegalNumericOperation(); }
SizeT   AbstrDataObject::FindPosOfFloat64 (Float64 val, SizeT startPos) const { illegalNumericOperation(); }
Int32   AbstrDataObject::GetValueAsInt32(SizeT index) const                   { illegalNumericOperation(); }
void    AbstrDataObject::SetValueAsInt32(SizeT index, Int32 val)              { illegalNumericOperation(); }
UInt32  AbstrDataObject::GetValueAsUInt32(SizeT index) const                  { illegalNumericOperation(); }
SizeT   AbstrDataObject::GetValueAsSizeT(SizeT index) const                   { illegalNumericOperation(); }
void    AbstrDataObject::SetValueAsSizeT(SizeT index, SizeT val)              { illegalNumericOperation(); }
void    AbstrDataObject::SetValueAsSizeT(SizeT index, SizeT val, tile_id t)   { illegalNumericOperation(); }
UInt8   AbstrDataObject::GetValueAsUInt8 (SizeT index) const                  { illegalNumericOperation(); }
void    AbstrDataObject::SetValueAsUInt32(SizeT index, UInt32 val)            { illegalNumericOperation(); }
SizeT  AbstrDataObject::FindPosOfSizeT(SizeT val, SizeT startPos) const       { illegalNumericOperation(); }
Float64 AbstrDataObject::GetSumAsFloat64() const                              { illegalNumericOperation(); }

// Support for numeric arrays (optional)
SizeT AbstrDataObject::GetValuesAsFloat64Array(tile_loc tl, SizeT len, Float64* data) const { illegalNumericOperation(); }
SizeT AbstrDataObject::GetValuesAsUInt32Array (tile_loc tl, SizeT len, UInt32* data) const { illegalNumericOperation(); }
SizeT AbstrDataObject::GetValuesAsInt32Array  (tile_loc tl, SizeT len, Int32* data) const { illegalNumericOperation(); }
SizeT AbstrDataObject::GetValuesAsUInt8Array  (tile_loc tl, SizeT len, UInt8* data) const { illegalNumericOperation(); }

void AbstrDataObject::SetValuesAsFloat64Array(tile_loc tl, SizeT len, const Float64* data) { illegalNumericOperation(); }
void AbstrDataObject::SetValuesAsInt32Array  (tile_loc tl, SizeT len, const Int32*   data) { illegalNumericOperation(); }
void AbstrDataObject::SetValuesAsUInt8Array  (tile_loc tl, SizeT len, const UInt8*   data) { illegalNumericOperation(); }

void AbstrDataObject::FillWithFloat64Values  (tile_loc tl, SizeT len, Float64 fillValue) { illegalNumericOperation(); }
void AbstrDataObject::FillWithUInt32Values   (tile_loc tl, SizeT len, UInt32  fillValue) { illegalNumericOperation(); }
void AbstrDataObject::FillWithInt32Values    (tile_loc tl, SizeT len, Int32   fillValue) { illegalNumericOperation(); }
void AbstrDataObject::FillWithUInt8Values    (tile_loc tl, SizeT len, UInt8   fillValue) { illegalNumericOperation(); }


// Support for Geometrics (optional)
DRect AbstrDataObject::GetActualRangeAsDRect(bool checkForNulls) const    { illegalGeometricOperation(); }

// Support for GeometricPoints (optional)
DPoint  AbstrDataObject::GetValueAsDPoint(SizeT index) const              { illegalGeometricOperation(); }
void    AbstrDataObject::SetValueAsDPoint(SizeT index, const DPoint& val) { illegalGeometricOperation(); }

// Support for GeometricSequences (optional)
void AbstrDataObject::GetValueAsDPoints(SizeT index, std::vector<DPoint>& dpoints) const { illegalGeometricOperation(); }

//----------------------------------------------------------------------
// Serialization and rtti
//----------------------------------------------------------------------


IMPL_CLASS(AbstrDataObject, 0)


//----------------------------------------------------------------------
// FutureTileArray
//----------------------------------------------------------------------

#include "FutureTileArray.h"

TIC_CALL auto GetAbstrFutureTileArray(const AbstrDataObject* ado) -> abstr_future_tile_array
{
	using abstr_future_tile_ptr = SharedPtr<abstr_future_tile>;
	using abstr_future_tile_array = OwningPtrSizedArray< abstr_future_tile_ptr >;
	assert(ado); // PRECONDITION
	auto tn = ado->GetTiledRangeData()->GetNrTiles();
	auto result = abstr_future_tile_array(tn, ValueConstruct_tag() MG_DEBUG_ALLOCATOR_SRC("GetAbstrFutureTileArray"));
	for (tile_id t = 0; t != tn; ++t)
		result[t] = ado->GetFutureAbstrTile(t);
	return result;
}
