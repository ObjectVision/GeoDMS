// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////
#include "StxPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

#include "DataBlockTask.h"

#include "mci/CompositeCast.h"
#include "mci/ValueClass.h"
#include "mci/ValueClassID.h"
#include "utl/StrFormat.h"

#include <algorithm>
#include <cctype>
#include <string_view>

#include "AbstrDataItem.h"
#include "DataArray.h"
#include "DataLocks.h"
#include "AbstrUnit.h"
#include "DataItemClass.h"

#include "ConfigProd.h"

// ============================= CLASS: DataBlockProd

DataBlockProd::DataBlockProd(AbstrDataItem* adi, SizeT elemCount)
	:	m_Lock(adi, dms_rw_mode::write_only_mustzero)
	,	m_ElemCount(elemCount)
	,	m_AbstrValue(adi->CreateAbstrValue())
{}

DataBlockProd::~DataBlockProd()
{
}

// *****************************************************************************
// Function/Procedure:DoArrayAssignment
// Description:       add record to attribute
// *****************************************************************************

// the text of an element that stands for null, in any case
static bool IsNullText(CharPtr first, CharPtr last)
{
	constexpr std::string_view nullText = "null";
	return SizeT(last - first) == nullText.size()
		&& std::equal(first, last, nullText.begin(), [](char c, char n) { return std::tolower(static_cast<unsigned char>(c)) == n; });
}

void DataBlockProd::DoArrayAssignment()
{
	AbstrDataItem* adi = CurrDI(); // stack-local borrow; kept alive by m_Lock
	const AbstrUnit* domain = adi->GetAbstrDomainUnit();

	SizeT i = m_nIndexValue++;
	if (i >= m_ElemCount)
	{
		auto errMsg = mySSPrintF("DoArrayAssignment: Index {} out of range of domain {}.\n"
			"Remove values from the array or consider adjusting the domain"
			, i, domain->GetNameID());
		adi->throwItemError(errMsg);
	}
	SizeT tileLocalIndex = i;
	tile_loc currTileLocation = checked_cast<const AbstrUnit*>(domain->GetCurrUltimateItem().get())->GetTiledRangeData()->GetTiledLocation(tileLocalIndex);
	tile_id currTileID = currTileLocation.first;
	if (currTileID == no_tile)
	{
		if (m_eValueType == ValueClassID::VT_Unknown)
			return; // OK to have undefined values for untiled area 
		adi->throwItemErrorF("DoArrayAssignment: Index {} is not part of any tile. Untiled area's cannot be assigned, this index should have a null value", i);
	}

	try {
		AssignElement(adi, i);
	}
	catch (DmsException& x)
	{
		// STX-A25: the errors of a conversion name the C++ type and not where the value is. The element goes
		// into the message itself: dms_guard_d passes on only m_Why, so an extra context would be lost.
		x.AsErrMsg()->m_Why = mySSPrintF("{}, in element {} of the data block", x.AsErrMsg()->m_Why, i);
		throw;
	}
}

void DataBlockProd::AssignElement(AbstrDataItem* adi, SizeT i)
{
	switch (m_eValueType)
	{
		case ValueClassID::VT_SharedStr:
		{
			m_AbstrValue->AssignFromCharPtrs(m_StringVal.begin(), m_StringVal.send());
			// STX-A25: a text that the values type cannot hold, such as '300' for a uint8, became null without
			// a word, where the same number written as a number is a range error
			if (m_AbstrValue->IsNull() && m_StringVal.begin() != m_StringVal.send() && !IsNullText(m_StringVal.begin(), m_StringVal.send()))
				throwErrorF("DataBlock", "the text '{}' is not a value of {}"
				,	std::string_view(m_StringVal.begin(), m_StringVal.send() - m_StringVal.begin()), adi->GetAbstrValuesUnit()->GetValueType()->GetName());
			m_Lock->SetAbstrValue(i, *m_AbstrValue); // OPTIMIZE: Avoid searching TileID(i) by GetLockedDataWrite(GetTileID(index)) in the called SetIndexedValue
			break;
		}
		case ValueClassID::VT_DPoint:
		{
			m_Lock->SetValueAsDPoint(i, m_DPointVal); // OPTIMIZE: Avoid searching TileID(i) by GetLockedDataWrite(GetTileID(index)) in the called SetIndexedValue
			// STX-A25: a coordinate outside the range of the point type became null without a word, where a
			// number outside the range of a numeric attribute is an error; read back, a defined coordinate
			// that came back null did not fit
			DPoint stored = m_Lock->GetValueAsDPoint(i);
			if ((IsDefined(m_DPointVal.first) && !IsDefined(stored.first)) || (IsDefined(m_DPointVal.second) && !IsDefined(stored.second)))
				throwErrorF("DataBlock", "the point ({}, {}) is outside the range of the coordinates of {}"
				,	m_DPointVal.first, m_DPointVal.second, adi->GetAbstrValuesUnit()->GetValueType()->GetName());
			break;
		}
		case ValueClassID::VT_Bool:
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
		case ValueClassID::VT_UInt32:
		case ValueClassID::VT_Int32:
		case ValueClassID::VT_Float64:
			m_Lock->SetValueAsFloat64(i, m_FloatVal ); // OPTIMIZE: Avoid searching TileID(i) by GetLockedDataWrite(GetTileID(index)) in the called SetIndexedValue
			break;
		case ValueClassID::VT_UInt64:
			m_Lock->SetValueAsSizeT(i, m_IntValAsUInt64); // OPTIMIZE: Avoid searching TileID(i) by GetLockedDataWrite(GetTileID(index)) in the called SetIndexedValue
			break;
		case ValueClassID::VT_Int64:
			m_Lock->SetValueAsDiffT(i, m_IntValAsInt64); // OPTIMIZE: Avoid searching TileID(i) by GetLockedDataWrite(GetTileID(index)) in the called SetIndexedValue
			break;
		case ValueClassID::VT_Unknown:
			m_Lock->SetNull(i); //  OPTIMIZE: Avoid GetLockedDataWrite(GetTileID(index))
			break;

		default:
			adi->throwItemErrorF("DoArrayAssignment: DataItem cannot contain values of type {}, or value type not supported in ArrayAssignment",
				ValueClass::FindByValueClassID(m_eValueType)->GetName() .c_str()
			);
	}
}

void DataBlockProd::Commit() 
{ 
	if (m_nIndexValue < m_ElemCount)
	{
		auto errMsg = mySSPrintF("DoArrayAssignment: Only {} values were provided, but domain {} has {} values. Since GeoDMS version 17.0.0, incomplete value arrays are diagnosed as errors.\n"
			"Provide {} values to fix this or consider adjusting the domain"
			, m_nIndexValue
			, CurrDI()->GetAbstrDomainUnit()->GetNameID()
			, m_ElemCount
			, m_ElemCount - m_nIndexValue);

		CurrDI()->throwItemError(errMsg);
	}

	m_Lock.Commit();
}

void DataBlockProd::throwSemanticError(CharPtr msg)
{
	throwItemErrorF(CurrDI(), "DataBlockAssignment error {}", msg);
}

// ============================= ConfigProd

#include "PropDefInterface.h"
#include "mci/AbstrValue.h"

void ConfigProd::DoArrayAssignment()
{
}

void ConfigProd::DataBlockCompleted(iterator_t first, iterator_t last)
{
	// STX-A25: a multi-name declaration gives every name the data block, as it gives them the calculation
	// rule; in `a, b: attribute<int32>(d): [1, 2, 3];` only b got it, and a was left without data
	auto assignDataBlock = [&](TreeItem* item)
	{
		if (!IsDataItem(item))
			item->throwItemError("DataBlockAssignment: assignee must be a DataItem");
		dms_assert(!item->GetInterestCount());

		item->GetOrCreateConfigProperties().mc_Calculator =
			new DataBlockTask(
				AsDataItem(item),
				&*first, &*last
			);
	};
	assignDataBlock(m_pCurrent.get());
	for (auto& sibling : m_LastDeclSiblings)
		assignDataBlock(sibling.get());
}

