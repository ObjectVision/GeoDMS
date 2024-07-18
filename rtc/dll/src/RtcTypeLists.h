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

#include <boost/mpl/vector.hpp>
#include <boost/mpl/placeholders.hpp>
using namespace boost::mpl::placeholders;

//----------------------------------------------------------------------
// MPL style enumeration type for element-types
//----------------------------------------------------------------------

namespace typelists {

//	typedef boost::mpl::vector<float,double,long double> floats;
#if defined(DMS_TM_HAS_FLOAT80)
	typedef boost::mpl::vector<float, double, long double> floats;
#else
	typedef boost::mpl::vector<float, double> floats;
#endif

	using sshorts = boost::mpl::vector< Int8, Int16>;   // supported signed short integer types
	using ushorts = boost::mpl::vector<UInt8, UInt16>;  // supported unsigned sort integer types

	using slongs = boost::mpl::vector< Int32, Int64>;   // supported signed long integer types
	using ulongs = boost::mpl::vector<UInt32, UInt64>;  // supported unsigned long integer types

	using sints = tl::jv2<sshorts, slongs>;             // supported signed integer types
	using uints = tl::jv2<ushorts, ulongs>;             // supported unsigned integer types
		

#if defined (DMS_TM_HAS_UINT2)
	typedef boost::mpl::vector<Bool, UInt2, UInt4>   bints;  // supported bit (sub-byte) unsigned integer types
#else
	typedef boost::mpl::vector<Bool, UInt4>          bints;  // supported bit (sub-byte) unsigned integer types
#endif
	typedef boost::mpl::vector<Bool, SharedStr>      others;
	typedef boost::mpl::vector<DPoint, FPoint>       float_points;

	typedef boost::mpl::vector<SPoint, IPoint>       sint_points;
	typedef boost::mpl::vector<WPoint, UPoint>       uint_points;
	typedef tl::jv2<sint_points, uint_points>        int_points;
//	typedef boost::mpl::vector<SRect, WRect, IRect, URect, FRect, DRect > rects;

#if defined (DMS_TM_HAS_INT_SEQ)
	typedef boost::mpl::vector<UInt64Seq, UInt32Seq, UInt16Seq, UInt8Seq> uint_sequences;
	typedef boost::mpl::vector<Int64Seq, Int32Seq, Int16Seq, Int8Seq> sint_sequences;
	typedef boost::mpl::vector<Float32Seq, Float64Seq> float_sequences;
	typedef tl::jv2<uint_sequences, sint_sequences> int_sequences;
	typedef tl::jv2<int_sequences, float_sequences> numeric_sequences;


	typedef boost::mpl::vector6<SPolygon, IPolygon, WPolygon, UPolygon, FPolygon, DPolygon > point_sequences;
	typedef boost::mpl::vector4<SPolygon, IPolygon,  WPolygon, UPolygon>                     int_point_sequences;

	typedef tl::jv2<uint_points, float_points>                                               seq_unsinged_points;
	typedef tl::jv2<sint_points, float_points>                                               seq_signed_points;
	typedef tl::jv2<int_points,  float_points>                                               seq_points;
#else
	typedef boost::mpl::vector2<FPolygon,  DPolygon > sequences;
	typedef boost::mpl::vector2<FPoint,    DPoint   > seq_points;
	typedef seq_points                                seq_signed_points;
#endif

	typedef boost::mpl::vector1<SharedStr>           strings;
	typedef boost::mpl::vector1<Void>                voids;

	// Void, SharedStr

	typedef tl::jv2<ushorts, sshorts>                      shorts;
	typedef tl::jv2<ulongs, slongs>                        longs;
	typedef tl::jv2<uints, sints>                          ints;
	typedef tl::jv2<uints, sints>                          ints;
	typedef tl::jv2<shorts, bints>                         ashorts;// all shorts
	typedef tl::jv2<ints,  bints>                          aints;  // all ints
	typedef tl::jv2<uints, bints>                          auints; // all unsigned ints (including sub-object elements)
	typedef tl::jv2<aints, floats>                         numerics;     // all numerics (including bit ints)
	typedef tl::jv2<ints,  floats>                         num_objects;  // all numeric objects (excluding sub-object elements)
	typedef tl::jv2<numerics, strings>                     scalars; 
	typedef tl::jv2<num_objects, strings>                  scalar_objects; 

	typedef tl::jv2<int_points, float_points>              points;
	typedef tl::jv2<scalars, points>                       fields;
	typedef tl::jv2<scalar_objects, points>                field_objects;
	typedef tl::jv2<numeric_sequences, point_sequences>    sequences;
	typedef tl::jv2<num_objects, points>                   sequence_fields;
	typedef tl::jv2<fields, sequences>                     value_elements; // used in RLookup, Unique, Sort
//	typedef tl::jv2<strings, sequences>                    composites; // used in RLookup, Unique, Sort
	typedef tl::jv2<field_objects, sequences>              value_objects;  // used in operator IsDefined en IsUndefined
	typedef tl::jv2<value_elements, voids>                 value_types;

	typedef ints								           domain_int_objects;
	typedef boost::mpl::vector<Int32, UInt32, Int64, UInt64> tiled_domain_ints;

	typedef tl::jv2<domain_int_objects, bints>             domain_ints;
	typedef tl::jv2<domain_ints, voids>                    domain_int_types;


	typedef int_points			                           domain_points;

	typedef tl::jv2<domain_ints, domain_points>            domain_elements;
	typedef tl::jv2<domain_int_objects, domain_points>     domain_objects;
	typedef tl::jv2<tiled_domain_ints, domain_points>      tiled_domain_elements;

	typedef domain_elements				                   partition_elements;

	typedef tl::jv2<domain_elements, voids>                domain_types;

	typedef tl::jv2<numerics,    points>                   fixed_size_elements;
	typedef tl::jv2<num_objects, points>                   ranged_unit_objects; // all numeric objects (excluding sub-object elements
	typedef tl::jv2<fields, voids>                         all_unit_types;

	typedef tl::transform<ranged_unit_objects, Range<_> >  range_objects;
//	typedef boost::mpl::vector <SRect, WRect, IRect, URect, FRect, DRect> point_ranges;

	typedef boost::mpl::vector1<TokenID>                   other_types;
	typedef tl::jv2<value_types, other_types>              vc_types;
//	typedef tl::jv2<vc_types, range_objects>                  all_vc_types;

}	//	namespace typelists


#endif // __RTC_TYPELISTS_H
