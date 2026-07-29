// Copyright (C) 1998-2025 Object Vision b.v.
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

#ifndef __TIC_CRS_H
#define __TIC_CRS_H

// *****************************************************************************
// UnitCrs, the coordinate reference system ("SpatialReference") of a unit.
//
// Deliberately shaped as a peer of UnitMetric (Metric.h): an immutable,
// intrusively refcounted, unit-independent value object where nullptr means
// "absent". That symmetry is the point -- a CRS gets the same derivation and
// unification treatment a metric gets, instead of being smuggled through the
// metric as a "<SpatialReference>\xFF<DialogData>" base-unit symbol.
// See doc/development/crs-metric-decoupling.md.
// *****************************************************************************

struct UnitCrs : SharedBase
{
	void Release() const
	{
		assert(!IsOwned());
		delete this;
	}

	TIC_CALL UnitCrs(TokenID spatialRef) : m_SpatialRef(spatialRef) {}

	TIC_CALL SharedStr AsString(FormattingFlags ff) const;

	bool Empty() const
	{
		return m_SpatialRef.empty();
	}

	TokenID m_SpatialRef;

	friend bool IsEmpty(const UnitCrs* self) { return !self || self->Empty(); }
};

// nullptr is treated as an empty UnitCrs.
// Strict, like AreEqual(const UnitMetric*, const UnitMetric*): an empty CRS unifies
// ONLY with an empty CRS. A bare coordinate unit must not silently acquire a CRS from
// a sibling -- that is the failure this whole separation exists to prevent. The
// leniency lives outside this predicate, in the same two places metric's does: the
// UM_AllowDefault* short-circuits in AbstrUnit::UnifyValues and the absorption rule in
// compatible_values_unit_creator_func (UnitCreators.cpp).
TIC_CALL bool AreEqual(const UnitCrs* lhs, const UnitCrs* rhs);

FormattedOutStream& operator <<(FormattedOutStream& str, const UnitCrs& repr);

#endif // __TIC_CRS_H
