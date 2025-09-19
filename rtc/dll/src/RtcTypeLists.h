// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#pragma once

//  -----------------------------------------------------------------------
//  This file contains the enumeration types used by 
//  RtcInterface.h. This been added to a separate file
//  to make the code in the COM interfaces less complex.
//  -----------------------------------------------------------------------

#if !defined(__RTC_TYPELISTS_H)
#define __RTC_TYPELISTS_H

#include "rtctypemodel.h"
#include "cpc/transform.h"
#include "geo/GeoSequence.h"

//----------------------------------------------------------------------
// MPL style enumeration type for element-types
//----------------------------------------------------------------------

namespace typelists {

	using namespace tl;

	// supported basic types
	typedef type_list<Void, Bool, SharedStr> basics;
	typedef type_list<Bool>                  bools;
//	typedef type_list<float,double,long double> floats;
#if defined(DMS_TM_HAS_FLOAT80)
	typedef type_list<float, double, long double> floats;
#else
	using floats = type_list<float, double>;
#endif

	using sshorts = type_list< Int8, Int16>;   // supported signed short integer types
	using ushorts = type_list<UInt8, UInt16>;  // supported unsigned sort integer types

	using slongs = type_list< Int32, Int64>;   // supported signed long integer types
	using ulongs = type_list<UInt32, UInt64>;  // supported unsigned long integer types

	using sints = jv2_t<sshorts, slongs>;      // supported signed integer types
	using uints = jv2_t<ushorts, ulongs>;      // supported unsigned integer types
		

#if defined (DMS_TM_HAS_UINT2)
	typedef type_list<Bool, UInt2, UInt4>   bints;  // supported bit (sub-byte) unsigned integer types
#else
	typedef type_list<Bool, UInt4>          bints;  // supported bit (sub-byte) unsigned integer types
#endif
	typedef type_list<Bool, SharedStr>      others;
	typedef type_list<DPoint, FPoint>       float_points;

	typedef type_list<SPoint, IPoint>       sint_points;
	typedef type_list<WPoint, UPoint>       uint_points;
	typedef jv2_t<sint_points, uint_points>        int_points;
//	typedef type_list<SRect, WRect, IRect, URect, FRect, DRect > rects;

#if defined (DMS_TM_HAS_INT_SEQ)
	typedef type_list<UInt64Seq, UInt32Seq, UInt16Seq, UInt8Seq> uint_sequences;
	typedef type_list<Int64Seq, Int32Seq, Int16Seq, Int8Seq> sint_sequences;
	typedef type_list<Float32Seq, Float64Seq> float_sequences;
	typedef jv2_t<uint_sequences, sint_sequences> int_sequences;
	typedef jv2_t<int_sequences, float_sequences> numeric_sequences;


	typedef type_list<SPolygon, IPolygon, WPolygon, UPolygon, FPolygon, DPolygon > point_sequences;
	typedef type_list<SPolygon, IPolygon,  WPolygon, UPolygon>                     int_point_sequences;

	typedef jv2_t<uint_points, float_points>                                               seq_unsinged_points;
	typedef jv2_t<sint_points, float_points>                                               seq_signed_points;
	typedef jv2_t<int_points,  float_points>                                               seq_points;
#else
	typedef boost::mpl::vector2<FPolygon,  DPolygon > sequences;
	typedef boost::mpl::vector2<FPoint,    DPoint   > seq_points;
	typedef seq_points                                seq_signed_points;
#endif

	typedef type_list<SharedStr>           strings;
	typedef type_list<Void>                voids;

	// Void, SharedStr

	typedef jv2_t<ushorts, sshorts>                      shorts;
	typedef jv2_t<ulongs, slongs>                        longs;
	typedef jv2_t<uints, sints>                          ints;
	typedef jv2_t<uints, sints>                          ints;
	typedef jv2_t<shorts, bints>                         ashorts;// all shorts
	typedef jv2_t<ints,  bints>                          aints;  // all ints
	typedef jv2_t<uints, bints>                          auints; // all unsigned ints (including sub-object elements)
	typedef jv2_t<aints, floats>                         numerics;     // all numerics (including bit ints)
	typedef jv2_t<ints,  floats>                         num_objects;  // all numeric objects (excluding sub-object elements)
	typedef jv2_t<numerics, strings>                     scalars; 
	typedef jv2_t<num_objects, strings>                  scalar_objects; 

	typedef jv2_t<int_points, float_points>              points;
	typedef jv2_t<scalars, points>                       fields;
	typedef jv2_t<scalar_objects, points>                field_objects;
	typedef jv2_t<numeric_sequences, point_sequences>    sequences;
	typedef jv2_t<num_objects, points>                   sequence_fields;
	typedef jv2_t<fields, sequences>                     value_elements; // used in RLookup, Unique, Sort
//	typedef jv2_t<strings, sequences>                    composites; // used in RLookup, Unique, Sort
	typedef jv2_t<field_objects, sequences>              value_objects;  // used in operator IsDefined en IsUndefined
	typedef jv2_t<value_elements, voids>                 value_types;

	typedef ints								           domain_int_objects;
	typedef type_list<Int32, UInt32, Int64, UInt64> tiled_domain_ints;

	typedef jv2_t<domain_int_objects, bints>             domain_ints;
	typedef jv2_t<domain_ints, voids>                    domain_int_types;


	typedef int_points			                           domain_points;

	typedef jv2_t<domain_ints, domain_points>            domain_elements;
	typedef jv2_t<domain_int_objects, domain_points>     domain_objects;
	typedef jv2_t<tiled_domain_ints, domain_points>      tiled_domain_elements;

	typedef domain_elements				                   partition_elements;

	typedef jv2_t<domain_elements, voids>                domain_types;

	typedef jv2_t<numerics,    points>                   fixed_size_elements;
	typedef jv2_t<num_objects, points>                   ranged_unit_objects; // all numeric objects (excluding sub-object elements
	typedef jv2_t<fields, voids>                         all_unit_types;

	using range_objects = transform_t<ranged_unit_objects, bind_placeholders<Range, ph::_1> >;
//	typedef type_list <SRect, WRect, IRect, URect, FRect, DRect> point_ranges;

	typedef type_list<TokenID>                   other_types;
	typedef jv2_t<value_types, other_types>              vc_types;
//	typedef jv2_t<vc_types, range_objects>                  all_vc_types;

}	//	namespace typelists


#endif // __RTC_TYPELISTS_H
