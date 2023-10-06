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
#pragma once

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

template <typename T> struct composition_of                  { static const ValueComposition value = ValueComposition::Single;   };
template <>           struct composition_of<Void>            { static const ValueComposition value = ValueComposition::Void;     };
template <>           struct composition_of<SharedStr>       { static const ValueComposition value = ValueComposition::String;   };
template <>           struct composition_of<CharPtr>         { static const ValueComposition value = ValueComposition::String;   };
template <typename T> struct composition_of<Range<T> >       { static const ValueComposition value = ValueComposition::Range;    };
template <typename T> struct composition_of<std::vector<T> > { static const ValueComposition value = ValueComposition::Sequence; };

template <typename T> constexpr ValueComposition composition_of_v = composition_of<T>::value;

//----------------------------------------------------------------------
// ValueComposition helper functions
//----------------------------------------------------------------------

RTC_CALL TokenID          GetValueCompositionID(ValueComposition vc);
RTC_CALL ValueComposition DetermineValueComposition(CharPtr featureType);


#endif // __RTC_MCI_VALUECOMPOSITION_H
