// Copyright (C) 1998-2024 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "StxPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

#include "DataBlockProd.h"

#include "vt/color.h"
#include "vt/Conversions.h"
#include "geom/PointOrder.h"
#include "mci/ValueClassID.h"

// *****************************************************************************
// Function/Procedure:DoRgbValue
// Description:       Retrieve the RGB values from stream and convert it to 
//					  member attribute m_nRgbValue with the RGB macro
// Parameters:        
//     UInt32 p_nRed: color red
//     UInt32 p_nGreen: color green
//     UInt32 p_nBlue: color blue
// *****************************************************************************

void AbstrDataBlockProd::DoRgbValue1(UInt32 p_nRed)
{
	// first check if the rgb values are within the range of 0..255
	if (p_nRed>255)
		throwSemanticError("Value Red out of range");
	m_IntValAsUInt64 = CombineRGB(p_nRed, 0, 0);
}
void AbstrDataBlockProd::DoRgbValue2(UInt32 p_nGreen)
{
	// first check if the rgb values are within the range of 0..255
	if (p_nGreen>255)
		throwSemanticError("Value Green out of range");
	m_IntValAsUInt64 += CombineRGB(0, p_nGreen, 0);
}
void AbstrDataBlockProd::DoRgbValue3(UInt32 p_nBlue)
{
	// first check if the rgb values are within the range of 0..255
	if (p_nBlue>255)
		throwSemanticError("Value Blue out of range");
	m_IntValAsUInt64 += CombineRGB(0, 0, p_nBlue);

	m_eValueType = ValueClassID::VT_UInt32;
	m_FloatVal   = m_IntValAsUInt64;
}

// *****************************************************************************
// Function/Procedure:DoIntegerValue
// Description:       retrieve the number from stream and initialise member 
//					  attribute m_IntValAsInt32 according the sign
// All three versions (m_IntValAsInt32, m_IntValAsUInt32, m_IntValAsF64)
// Therefore, also the CharPtr is relevant in case of inappropiateness of UInt32
//
// Parameters:        
//     Int32 p_nIntVal: the retrieved integer value
// *****************************************************************************

void AbstrDataBlockProd::DoUInt64(UInt64 nIntVal)
{
	// m_IntValAsUInt32
	m_IntValAsUInt64 = (m_bSignIsPlus)
		? nIntVal
		: UNDEFINED_VALUE(UInt64);

	m_FloatVal = Convert<Float64>(nIntVal);

	if (!m_bSignIsPlus)
		m_FloatVal = -m_FloatVal;
	
	// m_IntValAsInt32
	if (nIntVal > UInt64(MAX_VALUE(Int64)) || nIntVal == UNDEFINED_VALUE(UInt64))
		m_IntValAsInt64 = UNDEFINED_VALUE(Int64);
	else
		m_IntValAsInt64 = (m_bSignIsPlus)
			? +Int64(nIntVal)
			: -Int64(nIntVal);
	m_eValueType = (m_bSignIsPlus) ? ValueClassID::VT_UInt64 : ValueClassID::VT_Int64;

	// STX-A25: a negative integer below -2^63 has no Int64, and became null, even in a float64 block. It
	// goes the float way instead: a float attribute gets its value, an integer attribute the range error
	// that every number outside its range gets.
	if (!m_bSignIsPlus && nIntVal > UInt64(MAX_VALUE(Int64)))
		m_eValueType = ValueClassID::VT_Float64;
}

// *****************************************************************************
// Function/Procedure:DoFloatValue
// Description:       Retrieve and calculate the float value and store it in 
//	                   the member attribute m_FloatVal
// Parameters:        
//     Int32 nDecFrac: decimal fraction of the float; with precision determined
//                     by the number of leading zeroes found in sDecFrac
// *****************************************************************************

void AbstrDataBlockProd::DoFloatValue(const Float64& v)
{
	m_FloatVal = m_bSignIsPlus ? v : -v;
	m_eValueType = ValueClassID::VT_Float64;
}

///////////////////////////////////////////////////////////////////////////////
//
//  tmp funcs that need to become msSpiritProduct member funcs
//
///////////////////////////////////////////////////////////////////////////////

void AbstrDataBlockProd::DoBoolValue(bool v)
{
	m_BoolVal      = v;
	m_FloatVal     = v;
	m_eValueType = ValueClassID::VT_Bool;
}

void AbstrDataBlockProd::DoFirstPointValue()
{
	dmsPoint_SetFirstCfgValue( m_DPointVal, m_FloatVal);
}

void AbstrDataBlockProd::DoSecondPointValue()
{
	dmsPoint_SetSecondCfgValue( m_DPointVal, m_FloatVal);
}

void AbstrDataBlockProd::DoPointXValue()
{
	m_DPointVal.X() = m_FloatVal;
}

void AbstrDataBlockProd::DoPointYValue()
{
	m_DPointVal.Y() = m_FloatVal;
}

void AbstrDataBlockProd::DoNullValue()
{
	MakeUndefined(m_IntValAsUInt64);
	MakeUndefined(m_IntValAsInt64 );
	MakeUndefined(	m_FloatVal    );
	MakeUndefined(m_StringVal.m_StringValue);
	MakeUndefined(m_DPointVal);
	m_BoolVal    = 0;
	m_eValueType = ValueClassID::VT_Unknown;
}

