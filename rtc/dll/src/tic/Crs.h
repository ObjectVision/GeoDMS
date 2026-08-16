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

	SharedStr AsString(FormattingFlags ff) const;

	bool Empty() const
	{
		return m_SpatialRef.empty();
	}

	TokenID m_SpatialRef;

	friend bool IsEmpty(const UnitCrs* self) { return !self || self->Empty(); }
};

// *****************************************************************************
// Background-reference registry: spatial reference -> the WMS/WMTS background layer
// declared alongside it (a unit's DialogData).
//
// DialogData deliberately does NOT travel in UnitCrs, is NOT compared, and is NOT part
// of the DataController key (RULING 2026-07-29): a CRS mismatch is a correctness bug,
// a background mismatch is cosmetic, and packing the background into the metric is what
// currently makes two units differing ONLY in background hint fail to unify with
// "(incompatible Metrics)". Keeping it out of identity is the point of the separation.
//
// The consequence -- accepted -- is that sigma is the finest granularity available to a
// SHARED cache unit, so when one config declares the same CRS with two different
// backgrounds the first non-empty registration wins and the rest are reported. A real
// example exists: tst/Projects/lus_demo_2023 (regression t611) declares EPSG:28992 twice
// with different DialogData, one of them a computed expression.
//
// Cleared per configuration load (SessionData::Create), so a GUI session that reopens a
// different config does not inherit the previous one's backgrounds.
// *****************************************************************************

void      RegisterCrsBackgroundRef(TokenID spatialRef, const SharedStr& backgroundRef);
SharedStr GetCrsBackgroundRef     (TokenID spatialRef);
void      ClearCrsBackgroundRefs  ();

// nullptr is treated as an empty UnitCrs.
// Strict, like AreEqual(const UnitMetric*, const UnitMetric*): an empty CRS unifies
// ONLY with an empty CRS. A bare coordinate unit must not silently acquire a CRS from
// a sibling -- that is the failure this whole separation exists to prevent. The
// leniency lives outside this predicate, in the same two places metric's does: the
// UM_AllowDefault* short-circuits in AbstrUnit::UnifyValues and the absorption rule in
// compatible_values_unit_creator_func (UnitCreators.cpp).
bool AreEqual(const UnitCrs* lhs, const UnitCrs* rhs);

FormattedOutStream& operator <<(FormattedOutStream& str, const UnitCrs& repr);

#endif // __TIC_CRS_H
