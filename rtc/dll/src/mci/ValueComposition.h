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
	Single   = 0
,	Polygon  = 1
,	Sequence = 2

,	Count    = 3
,	Unknown  = 3  // unit may choose
,	Range = 4  // can never be used as ValuesType

,	Void   = Single
,	String = Single
};

inline bool IsAcceptableValuesComposition(ValueComposition vc)
{
	return vc == ValueComposition::Sequence || vc == ValueComposition::Polygon;
}

RTC_CALL void Unify(ValueComposition& vc, ValueComposition rhs);

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
RTC_CALL ValueComposition DetermineValueComposition(CharPtr featureType);


#endif // __RTC_MCI_VALUECOMPOSITION_H
