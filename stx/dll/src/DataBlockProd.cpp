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
#include "StxPch.h"
#pragma hdrstop

#include "DataBlockTask.h"

#include "mci/CompositeCast.h"
#include "mci/ValueClass.h"
#include "mci/ValueClassID.h"
#include "utl/mySPrintF.h"

#include "AbstrDataItem.h"
#include "DataArray.h"
#include "DataLocks.h"
#include "AbstrUnit.h"
#include "DataItemClass.h"
#include "DataStoreManagerCaller.h"

#include "ConfigProd.h"

//REMOVE #include <boost/thread/tss.hpp>
//REMOVE #include <boost/thread/detail/tss_hooks.hpp>

// ============================= CLASS: DataBlockProd

DataBlockProd::DataBlockProd(AbstrDataItem* adi, SizeT elemCount)
	:	m_Lock(adi, dms_rw_mode::write_only_mustzero)
	,	m_ElemCount(elemCount)
	,	m_AbstrValue(adi->CreateAbstrValue())
{}

DataBlockProd::~DataBlockProd()
{
	//REMOVE boost::on_thread_exit();
}

// *****************************************************************************
// Function/Procedure:DoArrayAssignment
// Description:       add record to attribute
// *****************************************************************************

template <typename T, typename A>
inline void SafeSetValue(std::vector<T, A>& vec, typename std::vector<T, A>::size_type i, const T& value)
{
	if (vec.size() <= i)
		throwErrorF("DataBlockAssignment", "Index %d out of range", i);
	vec[i] = value;
}

void DataBlockProd::DoArrayAssignment()
{
	WeakPtr<AbstrDataItem> adi = CurrDI();
	WeakPtr<const AbstrUnit> domain = adi->GetAbstrDomainUnit();

	SizeT i = m_nIndexValue++;
	if (i >= m_ElemCount)
	{
		auto errMsg = mySSPrintF("DoArrayAssignment: Index %d out of range of domain %s", i, domain->GetName().c_str());
		adi->throwItemError(errMsg);
	}
	SizeT tileLocalIndex = i;
	tile_loc currTileLocation = checked_cast<const AbstrUnit*>(domain->GetCurrUltimateItem())->GetTiledRangeData()->GetTiledLocation(tileLocalIndex);
	tile_id currTileID = currTileLocation.first;
	if (currTileID == no_tile)
	{
		if (m_eValueType == VT_Unknown)
			return; // OK to have undefined values for untiled area 
		adi->throwItemErrorF("DoArrayAssignment: Index %d is not part of any tile. Untiled area's cannot be assigned, this index should have a null value", m_nIndexValue);
	}

	switch (m_eValueType) 
	{
		case VT_SharedStr:
		{
//			DMS_AnyDataItem_SetValueAsCharArray(adi, i, m_StringVal.c_str());
			m_AbstrValue->AssignFromCharPtrs(m_StringVal.begin(), m_StringVal.send());
			m_Lock->SetAbstrValue(i, *m_AbstrValue); // OPTIMIZE: Avoid searching TileID(i) by GetLockedDataWrite(GetTileID(index)) in the called SetIndexedValue
			break;
		}
		case VT_DPoint:
			m_Lock->SetValueAsDPoint(i, m_DPointVal); // OPTIMIZE: Avoid searching TileID(i) by GetLockedDataWrite(GetTileID(index)) in the called SetIndexedValue
			break;
		case VT_Bool:
		{
			DataArray<Bool>* di = mutable_array_dynacast<Bool>(m_Lock);
			if (di) 
			{
				di->SetIndexedValue(i, m_BoolVal); // OPTIMIZE: Avoid GetLockedDataWrite(GetTileID(index))
				
				break;
			}
			m_FloatVal = m_BoolVal;
		}
		[[fallthrough]];
		case VT_UInt32:
		case VT_Int32:
		case VT_Float64:
			m_Lock->SetValueAsFloat64(i, m_FloatVal ); // OPTIMIZE: Avoid searching TileID(i) by GetLockedDataWrite(GetTileID(index)) in the called SetIndexedValue
			break;
		case VT_Unknown:
			m_Lock->SetNull(i); //  OPTIMIZE: Avoid GetLockedDataWrite(GetTileID(index))
			break;

		default:
			adi->throwItemErrorF("DoArrayAssignment: DataItem cannot contain values of type %s, or value type not supported in ArrayAssignment",
				ValueClass::FindByValueClassID(m_eValueType)->GetName() .c_str()
			);
	}
}
void DataBlockProd::Commit() 
{ 
	while (m_nIndexValue < m_ElemCount)
		m_Lock->SetNull(m_nIndexValue++); // padding with zero values

	m_Lock.Commit();
}

void DataBlockProd::throwSemanticError(CharPtr msg)
{
	Object::throwItemErrorF(CurrDI(), "DataBlockAssignment error %s", msg);
}

// ============================= ConfigProd

#include "PropDefInterface.h"
#include "mci/AbstrValue.h"

void ConfigProd::DoArrayAssignment()
{
	m_nIndexValue++;
}


void ConfigProd::DataBlockCompleted(iterator_t first, iterator_t last)
{
	if (!IsDataItem(m_pCurrent.get_ptr()) )
		m_pCurrent->throwItemError("DataBlockAssignment: assignee must be a DataItem");
	dms_assert(!m_pCurrent->GetInterestCount());

	m_pCurrent->mc_Calculator =
		new DataBlockTask(
			AsDataItem(m_pCurrent.get_ptr()), 
			&*first, &*last, 
			m_nIndexValue
		);
}

