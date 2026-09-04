// Copyright (C) 1998-2025 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif


#if !defined(__RTC_MCI_VALUECOMPOSITION_H)
#define __RTC_MCI_VALUECOMPOSITION_H

//----------------------------------------------------------------------
// enumeration composition types
//----------------------------------------------------------------------
// An enumeration type that represents the values that can be put in a unit

enum class ValueComposition : UInt8 {
	Single     = 0
,	Polygon    = 1
,	Sequence   = 2
,	MultiPoint = 3

,	Count      = 4
,	Unknown    = 7  // unit may choose
,	Range      = 5  // can never be used as ValuesType

,	Void   = Single
,	String = Single
};

inline bool IsAcceptableValuesComposition(ValueComposition vc)
{
	return vc == ValueComposition::Sequence || vc == ValueComposition::Polygon || vc == ValueComposition::MultiPoint;
}

// A value-type cast or unit conversion doesn't change geometric structure, so a
// sequence-typed result inherits the argument's actual composition (poly stays
// poly, multipoint stays multipoint) rather than the result type's default
// (Sequence, i.e. 'arc'). A non-sequence result (scalar, point, string) or a
// non-sequence argument keeps the operator's own composition: inheriting there
// would pair a composition with a value type that cannot carry it (#1038).
inline ValueComposition CastResultingValueComposition(ValueComposition operVC, ValueComposition argVC)
{
	return IsAcceptableValuesComposition(operVC) && IsAcceptableValuesComposition(argVC) ? argVC : operVC;
}

// The legacy fold: throws when exactly one side is Single, lets Polygon win over anything else,
// and keeps vc otherwise, which silently degrades MultiPoint to the seed. Only reached now as the
// fallback of UnifyValueComposition below; use that one.
RTC_CALL void Unify(ValueComposition& vc, ValueComposition rhs);

// Folds the ValueComposition of one more value argument into the composition that an operator
// will give its result. Operators that select or concatenate geometry without changing it (iif,
// union, union_data, lookup) must hand the composition of their arguments on, MultiPoint
// included, so seed vc with ValueComposition::Unknown and fold every value argument through this:
// the first one seeds, equal ones are kept.
//
// Arguments that disagree have no honest answer: no composition describes a set that is part ring
// and part polyline, and the label is what a storage writes (that is how #1038 wrote polygons as
// LINESTRING). Since GeoDMS 20.19.3 a mixture is reported as deprecated and resolved with the
// legacy rule, and from GeoDMS 21 it is an error (#1240). Single against a sequence composition
// stays the hard error it always was.
RTC_CALL void UnifyValueComposition(ValueComposition& vc, ValueComposition rhs, CharPtr operName);

const int ValueComposition_BitCount = 3;

//----------------------------------------------------------------------
// Section      : composition_of
//----------------------------------------------------------------------

#include "RtcBase.h" 

template <typename T> struct composition_of                      { static const ValueComposition value = ValueComposition::Single;   };
template <>           struct composition_of<Void>                { static const ValueComposition value = ValueComposition::Void;     };
template <>           struct composition_of<SharedStr>           { static const ValueComposition value = ValueComposition::String;   };
template <>           struct composition_of<CharPtr>             { static const ValueComposition value = ValueComposition::String;   };
template <typename T> struct composition_of<Range<T> >           { static const ValueComposition value = ValueComposition::Range;    };
template <typename T> struct composition_of<std::vector<T> >     { static const ValueComposition value = ValueComposition::Sequence; };
template <typename T> struct composition_of<locked_sequence<T> > { static const ValueComposition value = ValueComposition::Sequence; };
template <typename T> struct composition_of<my_vector<T> > { static const ValueComposition value = ValueComposition::Sequence; };

template <typename T> constexpr ValueComposition composition_of_v = composition_of<T>::value;

//----------------------------------------------------------------------
// ValueComposition helper functions
//----------------------------------------------------------------------

RTC_CALL TokenID          GetValueCompositionID(ValueComposition vc);
ValueComposition DetermineValueComposition(CharPtr featureType);


#endif // __RTC_MCI_VALUECOMPOSITION_H
