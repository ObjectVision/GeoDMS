// Copyright (C) 1998-2025 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

#ifndef __TIC_METRIC_H
#define __TIC_METRIC_H

#include "set/VectorMap.h"

// *****************************************************************************
// UnitMetric, used to express a unit as term of base unit power factors and one numeric factor
// *****************************************************************************

struct UnitMetric : SharedBase
{
	using BaseUnitsMapType = vector_map<SharedStr, Int32>;
	using BaseUnitsIterator = BaseUnitsMapType::iterator;


	void Release() const
	{
		assert(!IsOwned());
		delete this;
	}

	TIC_CALL UnitMetric() : m_Factor(1.0) {}

	TIC_CALL SharedStr AsString(FormattingFlags ff) const;

	TIC_CALL void SetProduct (const UnitMetric* arg1SI, const UnitMetric* arg2SI);
	TIC_CALL void SetQuotient(const UnitMetric* arg1SI, const UnitMetric* arg2SI);
	TIC_CALL bool IsNumeric() const;

	TIC_CALL bool operator ==(const UnitMetric& rhs) const;
	bool operator !=(const UnitMetric& rhs) const { return !(*this == rhs); }

	bool Empty() const 
	{
		return m_Factor == 1.0 && m_BaseUnits.empty();
	}

	Float64          m_Factor;
	BaseUnitsMapType m_BaseUnits;

	friend bool IsEmpty(const UnitMetric* self) { return !self || self->Empty(); }
};

FormattedOutStream& operator <<(FormattedOutStream& str, const UnitMetric& repr);

#endif // __TIC_METRIC_H