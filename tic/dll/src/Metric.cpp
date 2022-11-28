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

#include "Metric.h"
#include "AbstrUnit.h"

#include "mci/Class.h"

#include "ser/PointStream.h"
#include "ser/FormattedStream.h"
#include "ser/MoreStreamBuff.h"
#include "ser/StringStream.h"


// *****************************************************************************
// UnitMetric, used to express a unit as term of base unit power factors and one numeric factor
// *****************************************************************************

void  WriteQuantity(FormattedOutStream&  str, Float64 remainingFactor, CharPtr prefix)
{
	if (remainingFactor != 1)
		str << remainingFactor;
	str << prefix;
}

void WriteFactor(FormattedOutStream& str, Float64 factor)
{
	const Float64 kilo = 1000;
	const Float64 mega = kilo * kilo;
	const Float64 giga = kilo * mega; // 10^09
	const Float64 tera = mega * mega; // 10^12
	const Float64 peta = giga * mega; // 10^15
	const Float64 exa  = tera * mega; // 10^15
	const Float64 zetta = mega * peta; // 10^21
	const Float64 yotta = giga * peta; // 10^24
	const Float64 ronna  = tera * peta; // 10^27
	const Float64 quetta = peta * peta; // 10^30

	if (factor >= kilo)
	{
		if (factor >= mega)
		{
			if (factor >= peta)
			{
				if (factor >= quetta)
					WriteQuantity(str, factor / quetta, " quetta ");
				else if (factor >= ronna)
					WriteQuantity(str, factor / ronna, " ronna ");
				else if (factor >= yotta)
					WriteQuantity(str, factor / yotta, " yotta ");
				else if (factor >= zetta)
					WriteQuantity(str, factor / zetta, " zetta ");
				else if (factor >= exa)
					WriteQuantity(str, factor / exa, " exa ");
				else
					WriteQuantity(str, factor / peta, " peta ");
			}
			if (factor >= tera)
				WriteQuantity(str, factor / tera, " tera ");
			else if (factor >= giga)
				WriteQuantity(str, factor / giga, " giga ");
			else
				WriteQuantity(str, factor / mega, " mega ");
		}
		else
			WriteQuantity(str, factor / kilo, " kilo ");
	}
	else if (factor <= 0.01)
	{
		if (factor >= 0.001)
			WriteQuantity(str, factor * kilo, " milli ");
		else if (factor >= 0.000001)
			WriteQuantity(str, factor * mega, " micro ");
		else if (factor >= 0.000000001)
			WriteQuantity(str, factor * giga, " nano ");
		else if (factor >= 0.000000000001)
			WriteQuantity(str, factor * tera, " pico ");
		else if (factor >= 0.000000000000001)
			WriteQuantity(str, factor * peta, " femto ");
		else if (factor >= 0.000000000000000001)
			WriteQuantity(str, factor * exa, " atto ");
		else if (factor >= 0.000000000000000000001)
			WriteQuantity(str, factor * zetta, " zepto ");
		else if (factor >= 0.000000000000000000000001)
			WriteQuantity(str, factor * yotta, " yocto ");
		else if (factor >= 0.000000000000000000000000001)
			WriteQuantity(str, factor * ronna, " ronto ");
		else if (factor >= 0.000000000000000000000000000001)
			WriteQuantity(str, factor * quetta, " quecto ");
	}
	else
		WriteQuantity(str, factor, "");
}

bool HasNegativeUnits(const UnitMetric& repr)
{
	for (const auto& u: repr.m_BaseUnits)
		if (u.second < 0)
			return true;
	return false;
}

FormattedOutStream& operator <<(FormattedOutStream& str, const UnitMetric& repr)
{
	Float64 factor = repr.m_Factor;
	dms_assert(factor > 0);

	if (factor == 0.01 && repr.m_BaseUnits.empty()) // special case
	{
		str << '%';
		return str;
	}
	if (factor >= 1.0 || !HasNegativeUnits(repr))
	{
		WriteFactor(str, factor);
		factor = 1.0;
	}

	bool unitsPrinted = false;
	for (const auto& u : repr.m_BaseUnits)
	{
		dms_assert(u.second != 0);
		if (u.second > 0)
		{
			if (unitsPrinted)
				str << "*";
			str << (u.first);
			if (u.second != 1)
				str << "^" << u.second;
			unitsPrinted = true;
		}
	}
	unitsPrinted = false;
	for (const auto& u : repr.m_BaseUnits)
	{
		if (u.second < 0)
		{
			if (unitsPrinted)
				str << "*";
			else
			{
				str << " per ";
				if (factor < 1.0)
					WriteFactor(str, 1.0 / factor);
			}
			str << (u.first);
			if (u.second != -1)
				str << "^" << - u.second;
			unitsPrinted = true;
		}
	}

	return str;
}

SharedStr UnitMetric::AsString(FormattingFlags ff) const
{
	VectorOutStreamBuff resultBuff;
	FormattedOutStream resultStr(&resultBuff, ff);
	resultStr << (*this);
	CharPtr cstr = resultBuff.GetData(); 
	return SharedStr(cstr, cstr + resultBuff.CurrPos());
}

typedef const UnitMetric::BaseUnitsMapType::value_type* BaseUnitCPtr;

BaseUnitCPtr BeginPtr(const UnitMetric* argSI)
{
	return argSI
		?	begin_ptr(argSI->m_BaseUnits)
		:	BaseUnitCPtr();
}

BaseUnitCPtr EndPtr(const UnitMetric* argSI)
{
	return argSI
		?	end_ptr(argSI->m_BaseUnits)
		:	BaseUnitCPtr();
}

UInt32 MergeCount(BaseUnitCPtr b1, BaseUnitCPtr e1, BaseUnitCPtr b2, BaseUnitCPtr e2, Int32 sign)
{
	UInt32 result = 0;
	while (b1!=e1 || b2!=e2)
	{
		if (b1!=e1 && (b2==e2 || b1->first<b2->first))  // b1->first < b2->first
			++result, ++b1;
		else if (b2!=e2 && (b1==e1 || b2->first < b1->first)) // b1->first > b2->first
			++result, ++b2;
		else // b1->first == b2->first
		{
			lfs_assert(b1->first == b2->first);
			if (b1->second + sign * b2->second)
				++result;
			++b1, ++b2;
		}
	}
	return result;
}

void UnitMetric::SetProduct(const UnitMetric* arg1SI, const UnitMetric* arg2SI)
{
	if (arg1SI) m_Factor *= arg1SI->m_Factor;
	if (arg2SI) m_Factor *= arg2SI->m_Factor;

	lfs_assert(m_BaseUnits.empty()); // PRECONDITION

	BaseUnitCPtr
		b1 = BeginPtr(arg1SI), 
		b2 = BeginPtr(arg2SI), 
		e1 = EndPtr  (arg1SI), 
		e2 = EndPtr  (arg2SI);
	m_BaseUnits.reserve(MergeCount(b1, e1, b2, e2, +1));

	while (b1!=e1 || b2!=e2)
	{
		if (b1!=e1 && (b2==e2 || b1->first<b2->first))  // b1->first < b2->first
			m_BaseUnits.push_back(*b1++);
		else if (b2!=e2 && (b1==e1 || b2->first < b1->first)) // b1->first > b2->first
			m_BaseUnits.push_back(*b2++);
		else // b1->first == b2->first
		{
			lfs_assert(b1->first == b2->first);
			Int32 combinedPower = b1->second + b2->second;
			if (combinedPower)
				m_BaseUnits.push_back(UnitMetric::BaseUnitsMapType::value_type(b1->first, combinedPower));
			++b1; ++b2;
		}
	}
	dms_assert(m_BaseUnits.size() == m_BaseUnits.capacity());
}

void UnitMetric::SetQuotient(const UnitMetric* arg1SI, const UnitMetric* arg2SI)
{
	if (arg1SI) m_Factor *= arg1SI->m_Factor;
	if (arg2SI) m_Factor /= arg2SI->m_Factor;

	lfs_assert(m_BaseUnits.empty()); // PRECONDITION

	BaseUnitCPtr
		b1 = BeginPtr(arg1SI), 
		b2 = BeginPtr(arg2SI), 
		e1 = EndPtr  (arg1SI), 
		e2 = EndPtr  (arg2SI);
	m_BaseUnits.reserve(MergeCount(b1, e1, b2, e2, -1));

	while (b1!=e1 || b2!=e2)
	{
		if (b1!=e1 && (b2==e2 || b1->first < b2->first)) // b1->first < b2->first
		{
			m_BaseUnits.push_back(*b1);
			++b1;
		}
		else if (b2!=e2 && (b1==e1 || b2->first < b1->first)) // b2->first < b1->first
		{
			m_BaseUnits.push_back(BaseUnitsMapType::value_type(b2->first, -b2->second));
			++b2;
		}
		else // b1->first == b2->first
		{
			lfs_assert(b1->first == b2->first);
			Int32 combinedPower = b1->second - b2->second;
			if (combinedPower)
				m_BaseUnits.push_back(BaseUnitsMapType::value_type(b1->first, combinedPower));
			++b1; ++b2;
		}
	}
	dms_assert(m_BaseUnits.size() == m_BaseUnits.capacity());
}

bool UnitMetric::IsNumeric() const
{
	BaseUnitsMapType::const_iterator 
		i = m_BaseUnits.begin(), 
		e = m_BaseUnits.end();

	for (; i != e; ++i)
		if (i->second)
			return false;
	return true;
}

bool IsNear(Float64 factor, Float64 rhsFactor)
{
	if (factor == 0 && rhsFactor == 0)
		return true;
	Float64 margin = 0.999999999;
	return (factor * margin < rhsFactor) && (rhsFactor * margin < factor);
}

bool UnitMetric::operator ==(const UnitMetric& rhs) const
{
	return this==&rhs
		||	(this && &rhs && IsNear(m_Factor, rhs.m_Factor) && m_BaseUnits == rhs.m_BaseUnits)
		||	(!this && rhs.Empty())
		||	(!&rhs && Empty());
}

// *****************************************************************************
// UnitProjection, used to express coordinate units as (x,y) scale + (x,y) offset 
// of a base coordinate system
// *****************************************************************************
#include "Projection.h"

class AbstrUnit;

UnitProjection::UnitProjection(const AbstrUnit* unit)
	:	m_BaseUnit(unit)
{
	dms_assert(unit); 
	dms_assert(m_BaseUnit); 
	dbg_assert(m_BaseUnit->m_State.GetProgress() >= PS_MetaInfo || m_BaseUnit->WasFailed(FR_MetaInfo) || m_BaseUnit->IsPassor()); // PRECONDITION / INVARIANT OF UnitProjection
	dbg_assert(m_BaseUnit->GetCurrUltimateItem() == m_BaseUnit);
}

UnitProjection::UnitProjection(const AbstrUnit* unit, DPoint offset, DPoint factor)
	:	Transformation(offset, factor)
	,	m_BaseUnit(unit)
{
	dms_assert(unit); 
	dms_assert(m_BaseUnit); 
	dbg_assert(m_BaseUnit->m_State.GetProgress() >= PS_MetaInfo || m_BaseUnit->WasFailed(FR_MetaInfo)  || m_BaseUnit->IsPassor()); // PRECONDITION / INVARIANT OF UnitProjection
	dbg_assert(m_BaseUnit->GetCurrUltimateItem() == m_BaseUnit);
}

UnitProjection::UnitProjection(const AbstrUnit* unit, const CrdTransformation& tr)
	:	Transformation(tr)
	,	m_BaseUnit(unit)
{
	dms_assert(unit); 
	dms_assert(m_BaseUnit); 
	dbg_assert(m_BaseUnit->m_State.GetProgress() >= PS_MetaInfo || m_BaseUnit->WasFailed(FR_MetaInfo) || m_BaseUnit->IsPassor()); // PRECONDITION / INVARIANT OF UnitProjection
	dbg_assert(m_BaseUnit->GetCurrUltimateItem() == m_BaseUnit);
}

UnitProjection::UnitProjection(const UnitProjection& src)
	:	Transformation(src)
	,	m_BaseUnit(src.m_BaseUnit)
{
	dms_assert(m_BaseUnit);
	dbg_assert(m_BaseUnit->m_State.GetProgress() >= PS_MetaInfo || m_BaseUnit->IsPassor()); // PRECONDITION / INVARIANT OF UnitProjection
	dbg_assert(m_BaseUnit->GetCurrUltimateItem() == m_BaseUnit);
}

UnitProjection::~UnitProjection()
{
	dms_assert(m_BaseUnit);
}

bool UnitProjection::operator ==(const UnitProjection& rhs) const
{
	return GetCompositeTransform(this) == GetCompositeTransform(&rhs) && GetCompositeBase() == rhs.GetCompositeBase();
}

const AbstrUnit* UnitProjection::GetCompositeBase() const
{
	const AbstrUnit* base = nullptr;
	const UnitProjection* curr = this;
	while (curr)
	{
		base = curr->m_BaseUnit;
		dms_assert(base); // INVARIANT OF UnitProjection
		dms_assert(!base->IsDefaultUnit() || !curr->IsIdentity());
		dbg_assert(base->m_State.GetProgress() >= PS_MetaInfo || base->WasFailed() || base->IsPassor()); // PRECONDITION / INVARIANT OF UnitProjection
		curr = base->GetCurrProjection();
	}
	return base;
}

Transformation<Float64> UnitProjection::GetCompositeTransform(const UnitProjection* curr)
{
	Transformation result;
	while (curr)
	{
		result *= (*curr);
		const AbstrUnit* base = curr->m_BaseUnit;
		dms_assert(base); // INVARIANT OF UnitProjection
		dms_assert(!base->IsDefaultUnit() || !curr->IsIdentity());
		dbg_assert(base->m_State.GetProgress() >= PS_MetaInfo || base->WasFailed() || base->IsPassor()); // PRECONDITION
		curr = base->GetCurrProjection();
	}
	return result;
}

SharedStr UnitProjection::AsString(FormattingFlags ff) const
{
	VectorOutStreamBuff resultBuff;
	FormattedOutStream resultStr(&resultBuff, ff);
	resultStr << (*this);
	CharPtr first = resultBuff.GetData();
	return SharedStr(first, first+resultBuff.CurrPos());
}

FormattedOutStream& operator <<(FormattedOutStream& str, const UnitProjection& repr)
{
	str << repr.Factor() << "*" << repr.GetBaseUnit()->GetNameOrCurrMetric(str.GetFormattingFlags()) << "+" << repr.Offset();
	return str;
}
