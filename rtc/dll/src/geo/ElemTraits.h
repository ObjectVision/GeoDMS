// Copyright (C) 2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#pragma once

#if !defined(__RTC_GEO_ELEMTRAITS_H)
#define __RTC_GEO_ELEMTRAITS_H

//----------------------------------------------------------------------
// includes and defines
//----------------------------------------------------------------------

#include "RtcBase.h"

#include <type_traits>

#define COMPOSITION(T)  composition_of<T>::value

#define MG_DEBUG_ELEMTRAITS

//----------------------------------------------------------------------
// Section      : type_tags
//----------------------------------------------------------------------

struct crd_type_tag {}; // countable domain:  int, spoint, ipoint)
struct inf_type_tag {}; // uncountable domain:float, string, polygon

struct string_type_tag: inf_type_tag {};
struct float_type_tag : inf_type_tag {};

struct ord_type_tag   : crd_type_tag {};
struct bool_type_tag  : ord_type_tag {};
struct void_type_tag  : ord_type_tag {};

struct int_type_tag   : ord_type_tag {};
struct uint_type_tag  : int_type_tag {};
struct sint_type_tag  : int_type_tag {};

struct point_type_tag {};
struct crd_point_type_tag : crd_type_tag, point_type_tag {};
struct inf_point_type_tag : inf_type_tag, point_type_tag{};

struct range_type_tag   {};
struct sequece_type_tag {};

//=======================================
// api_type
//=======================================

template <typename T>   struct api_type                { typedef T     type; };
template <bit_size_t N> struct api_type<bit_value<N> > { typedef UInt8 type; };
template <>             struct api_type<bit_value<1> > { typedef bool  type; };

//=======================================
// param_type
//=======================================

template <typename T> using add_constref_t = std::add_lvalue_reference_t<std::add_const_t<T>>;

template <typename T>   struct param_type : std::conditional<sizeof(T) <= 8, T, add_constref_t<T>> {};
template <bit_size_t N> struct param_type<bit_value<N> > { typedef bit_value<N> type; };
template <>             struct param_type<SharedStr>     { typedef WeakStr      type; };

template<typename T> using param_type_t = typename param_type<T>::type;

//=======================================
// dms_type
//=======================================

template <typename T> struct dms_type       { typedef T     type; };
template <>           struct dms_type<bool> { typedef Bool  type; };

#if defined(CC_LONGDOUBLE_80)
template <>           struct dms_type<long double> { typedef Float80 type; };
#endif
#if defined(CC_LONGDOUBLE_64)
template <>           struct dms_type<long double> { typedef Float64 type; };
#endif

#if defined(CC_LONG_32)
template <>           struct dms_type<long>          { typedef Int32  type; };
template <>           struct dms_type<unsigned long> { typedef UInt32 type; };
#endif

//----------------------------------------------------------------------
// Section      : dimension_of
//----------------------------------------------------------------------

template <typename T> struct dimension_of            : std::integral_constant<std::size_t, 1> {};
template <>           struct dimension_of<Void>      : std::integral_constant<std::size_t, 0> {};
template <typename T> struct dimension_of<Point<T> > : std::integral_constant<std::size_t, 2 * dimension_of<T>::value> {};
template <typename T> struct dimension_of<Range<T> > {}; // Not Applicable
template <typename T> struct dimension_of<std::vector<T> > : dimension_of<T> {};
template <typename T> const int dimension_of_v = dimension_of<T>::value;

//----------------------------------------------------------------------
// Section      : predicates
//----------------------------------------------------------------------

template <typename T> struct is_signed : std::false_type {};
template <> struct is_signed<  Int64> : std::true_type {};
template <> struct is_signed<  Int32> : std::true_type {};
template <> struct is_signed<  Int16> : std::true_type {};
template <> struct is_signed<   Int8> : std::true_type {};
template <> struct is_signed<Float32> : std::true_type {};
template <> struct is_signed<Float64> : std::true_type {};
template <typename T> struct is_signed< Point<T> > : is_signed<T> {};
template <typename P> struct is_signed< Range<P> > : is_signed<P> {};

#if defined(DMS_TM_HAS_FLOAT80)
template <> struct is_signed<Float80> : std::true_type {};
#endif
template <typename T> constexpr bool is_signed_v = is_signed<T>::value;

template <typename T>   struct is_integral                : std::is_integral<T> {};
template <>             struct is_integral<UInt64>        : std::true_type {};
template <>             struct is_integral<Int64>         : std::true_type{};
template <bit_size_t N> struct is_integral<bit_value<N> > : std::true_type{}; // bit_value<1> (pseudo bool) is not considered as a numeric.
template <>             struct is_integral<Void         > : std::true_type {};
template <typename T> constexpr bool is_integral_v = is_integral<T>::value;

template <typename T>
concept IntegralValue = is_integral_v<T> && std::regular<T>;

template <typename T>   struct is_simple : std::is_arithmetic<T> {};
template <>             struct is_simple<Int64> : std::true_type {};
template <>             struct is_simple<UInt64> : std::true_type {};
template <bit_size_t N> struct is_simple<bit_value<N> > : std::true_type {};
template <typename T>   struct is_simple<Point<T> > : is_simple<T> {};

template <typename T>   struct is_numeric : std::is_arithmetic<T> {};
template <>             struct is_numeric<Int64> : std::true_type {};
template <>             struct is_numeric<UInt64> : std::true_type {};
template <bit_size_t N> struct is_numeric<bit_value<N> > : std::true_type {};
template <>             struct is_numeric<bit_value<1> > : std::false_type {}; // bit_value<1> (pseudo bool) is not considered as a numeric.


template <>           struct is_numeric<bool> {};      // PREVENT USING bool directly

template <typename T> constexpr bool is_numeric_v = is_numeric<T>::value;

template <typename T>
concept NumericValue = is_numeric_v<T> && std::regular<T>;

template <typename T>   struct is_void : std::false_type {};
template <>             struct is_void<Void> : std::true_type {};
template <typename T> constexpr bool is_void_v = is_void<T>::value;

//----------------------------------------------------------------------
// Section      : nrbits_of
//----------------------------------------------------------------------

template <typename T>   struct is_bitvalue                : std::false_type{};
template <bit_size_t N> struct is_bitvalue<bit_value<N> > : std::true_type {};
template <typename T> constexpr bool is_bitvalue_v = is_bitvalue<T>::value;

template <typename T>   struct nrbits_of               : std::integral_constant<std::size_t,sizeof(T)*8> {};
template <bit_size_t N> struct nrbits_of<bit_value<N> >: std::integral_constant<std::size_t, N> {};
template <>             struct nrbits_of<Void>         : std::integral_constant<std::size_t, 1> {};

template <typename T> const std::size_t nrbits_of_v = nrbits_of<T>::value;

//template <typename T>   struct nrvalbits_of: boost::mpl::eval_if<is_signed<T>, boost::mpl::minus<nrbits_of<T>, std::integral_constant<std::size_t,1> >, nrbits_of<T> > {};
template <typename T>   struct nrvalbits_of: std::conditional<is_signed<T>::value, std::integral_constant<std::size_t,nrbits_of<T>::value - 1>, nrbits_of<T> >::type {};
template <>             struct nrvalbits_of<Float32> : std::integral_constant<std::size_t, (1<< 7) > {};
template <>             struct nrvalbits_of<Float64> : std::integral_constant<std::size_t, (1<<10) > {};

#if defined(DMS_TM_HAS_FLOAT80)
template <>             struct nrvalbits_of<Float80> : std::integral_constant<std::size_t, (1<<15) > {};
#endif

template <typename T> struct is_composite                 : std::false_type{};
template <typename T> struct is_composite<std::vector<T> > : std::true_type{};

template <typename T> constexpr bool is_composite_v = is_composite<T>::value;

template <typename T> struct has_fixed_elem_size                 : std::true_type{};
template <typename T> struct has_fixed_elem_size<std::vector<T> >: std::false_type{};
template <> struct has_fixed_elem_size<SharedStr>                : std::false_type{};

template <typename T> constexpr bool has_fixed_elem_size_v = has_fixed_elem_size<T>::value;

template <class _Ty>
concept fixed_elem = has_fixed_elem_size_v<_Ty>;

template <class _Ty>
concept sequence_or_string = !has_fixed_elem_size_v<_Ty>;

//----------------------------------------------------------------------
// Section      : type converters
//----------------------------------------------------------------------

template <typename T> struct field_of                  { typedef T type; };
template <typename T> struct field_of<Range<T> >       { typedef T type; };
template <typename T> struct field_of<std::vector<T> > { typedef T type; };

template <typename T> using field_of_t = typename field_of<T>::type;

template <typename T> struct elem_of                   { typedef T        type; }; 
template <typename T> struct elem_of<std::vector<T> >  { typedef T        type; };
template <>           struct elem_of<SharedStr>        { typedef CharType type; };

template <typename T> using elem_of_t = typename elem_of<T>::type;

template <typename T> struct scalar_of                 { typedef T type; };
template <typename T> struct scalar_of<Range<T> >      { typedef typename scalar_of<T>::type type; };
template <typename T> struct scalar_of<std::vector<T> >{ typedef typename scalar_of<T>::type type; };
template <typename T> struct scalar_of<Point<T> >      { typedef typename scalar_of<T>::type type; };
template <typename T> struct scalar_of<const T>        {}; // illegal use of scalar_of
template <typename T> using scalar_of_t = typename scalar_of<T>::type;

template <typename T> struct signed_type            { typedef T     type; };
template <>           struct signed_type<UInt32>    { typedef Int32 type; };
template <>           struct signed_type<UInt64>    { typedef Int64 type; };
template <>           struct signed_type<UInt16>    { typedef Int16 type; };
template <>           struct signed_type<UInt8 >    { typedef Int8  type; };
template <typename T> struct signed_type<Range<T> > { typedef Range<typename signed_type<T>::type> type; };
template <typename T> using signed_type_t = typename signed_type<T>::type;

template <typename T> struct unsigned_type            { typedef T     type; };
template <>           struct unsigned_type<Int32>     { typedef UInt32 type; };
template <>           struct unsigned_type<long>      { typedef unsigned long type; };
//template <>         struct unsigned_type<long long> { typedef unsigned long long type; }; equals Int64 == __int64
template <>           struct unsigned_type<Int64>     { typedef UInt64 type; };
template <>           struct unsigned_type<Int16>     { typedef UInt16 type; };
template <>           struct unsigned_type<Int8 >     { typedef UInt8  type; };
template <typename T> struct unsigned_type<Range<T> > { typedef Range<typename unsigned_type<T>::type> type; };
template <typename T> struct unsigned_type<Point<T> > { typedef Point<typename unsigned_type<T>::type> type; };
template <typename T> struct unsigned_type<std::vector<T> > { typedef std::vector<typename unsigned_type<T>::type> type; };
template <typename T> using unsigned_type_t = typename unsigned_type<T>::type;

// use acc_type<T>::type as accumulator for the summation and multiplication of many numbers of type T such as sumof squares 
// It is also debatably used for substraction
template <typename T> struct acc_type            : std::conditional<is_integral_v<T>, typename std::conditional_t<is_signed_v<T>, DiffT, SizeT>, Float64> {};
template <typename T> struct acc_type<Range<T> > { typedef Range<typename acc_type<T>::type> type; };
template <typename T> struct acc_type<Point<T> > { typedef Point<typename acc_type<T>::type> type; };
template <typename T> struct acc_type<const T>   {}; // illegal use

template <typename T> using acc_type_t = typename acc_type<T>::type;

template <typename T> struct sqr_acc_type : unsigned_type<typename acc_type<T>::type> {};

// use aggr_type<T>::type as (intermediate) accumulator for operations that involve division, such as mean, (co)var, correlation, stddev
template <typename T> struct aggr_type            { typedef Float64 type; };
template <typename T> struct aggr_type<Point<T> > { typedef Point<typename aggr_type<T>::type> type; };
template <typename T> struct aggr_type<const T>   {}; // illegal use

// use div_type<T> as result type of operations that involve division. 
// Aggregation operations combine this with aggr_type for intermediate results
template <typename T> struct div_type            { typedef Float32 type; };
template <>           struct div_type<UInt32>    { typedef Float64 type; };
template <>           struct div_type< Int32>    { typedef Float64 type; };
template <>           struct div_type<UInt64>    { typedef Float64 type; };
template <>           struct div_type< Int64>    { typedef Float64 type; };
template <>           struct div_type<Float64>   { typedef Float64 type; };
template <typename T> struct div_type<Range<T> > { typedef Range<typename div_type<T>::type> type; }; 
template <typename T> struct div_type<Point<T> > { typedef Point<typename div_type<T>::type> type; }; 
template <typename T> struct div_type<const T>   {}; // illegal use
template <typename T> using div_type_t = typename div_type<T>::type;

template <typename T> struct product_type { using type = T; };
template <typename T> using product_type_t = product_type<T>::type;

template <typename T> struct square_type;
template <> struct square_type<UInt8>  { using type = UInt16; };
template <> struct square_type<UInt16> { using type = UInt32; };
template <> struct square_type<UInt32> { using type = UInt64; };
template <> struct square_type<Float32> { using type = Float32; };
template <> struct square_type<Float64> { using type = Float64; };
template <typename T> using square_type_t = square_type<T>::type;

template <typename T> struct cardinality_type : unsigned_type < T > {};
template <bit_size_t N> struct cardinality_type<bit_value<N>> { using type = bit_block_t; };
template <typename T> struct cardinality_type<Point<T>> : square_type < typename cardinality_type<T>::type >{};
template <typename T> struct cardinality_type<Range<T>> : cardinality_type<T> {};
template <typename T> struct cardinality_type<std::vector<T>> : cardinality_type<T> {};

template <typename T> using cardinality_type_t = typename cardinality_type<T>::type;
//----------------------------------------------------------------------
// Section      : composite type predicates
//----------------------------------------------------------------------

template <typename T> constexpr bool is_separable_v = has_fixed_elem_size_v<T> && !is_bitvalue_v<T>; ;
template <typename T> constexpr bool has_var_range_v = is_separable_v<T> && !is_void_v<T>;
template <typename T> constexpr bool has_var_range_field_v = has_var_range_v<field_of_t<T>>;

template <typename T> constexpr bool has_range_v = has_var_range_v<T> || is_bitvalue_v<T> || is_void_v<T>;

//----------------------------------------------------------------------
// Section      : elem_traits
//----------------------------------------------------------------------

template <typename T> struct elem_traits;

template <> struct elem_traits<UInt64> : uint_type_tag {};
template <> struct elem_traits<UInt32> : uint_type_tag {};
template <> struct elem_traits<UInt16> : uint_type_tag {};
template <> struct elem_traits<UInt8 > : uint_type_tag {};

template <> struct elem_traits<Int64 > : sint_type_tag {};
template <> struct elem_traits<Int32 > : sint_type_tag {};
template <> struct elem_traits<Int16 > : sint_type_tag {};
template <> struct elem_traits<Int8  > : sint_type_tag {};

template <> struct elem_traits<Float64>:float_type_tag {};
template <> struct elem_traits<Float32>:float_type_tag {};

template <> struct elem_traits<SharedStr>:string_type_tag {};
template <> struct elem_traits<Void>     :void_type_tag {};

template<typename T>    struct elem_traits<Range<T> >     : range_type_tag {};
template <bit_size_t N> struct elem_traits<bit_value<N> > : bool_type_tag  {};

template <typename T> struct elem_traits<Point<T> >
:	std::conditional_t<is_integral<T>::value, 
		crd_point_type_tag, 
		inf_point_type_tag
	> {};

//----------------------------------------------------------------------
// some assumptions
//----------------------------------------------------------------------

#if defined(MG_DEBUG_ELEMTRAITS)

	static_assert( is_integral_v< UInt32>);
	static_assert( is_integral_v<  Int32>);
	static_assert( is_integral_v< UInt16>);
	static_assert( is_integral_v<  Int16>);
	static_assert( is_integral_v<  UInt8>);
	static_assert( is_integral_v<   Int8>);
	static_assert( is_integral_v<  UInt4>);
	static_assert( is_integral_v<  UInt2>);
	static_assert( is_integral_v<  Bool >);
	static_assert(!is_integral_v<Float32>);
	static_assert(!is_integral_v<Float64>);
	static_assert(!is_integral_v<Range<Int32  > >);
	static_assert(!is_integral_v<Range<Float64> >);

	static_assert( is_numeric_v< UInt32>);
	static_assert( is_numeric_v<  Int32>);
	static_assert( is_numeric_v< UInt16>);
	static_assert( is_numeric_v<  Int16>);
	static_assert( is_numeric_v<  UInt8>);
	static_assert( is_numeric_v<   Int8>);
	static_assert( is_numeric_v<  UInt4>);
	static_assert( is_numeric_v<  UInt2>);
	static_assert(!is_numeric_v<  Bool >);
	static_assert( is_numeric_v<Float32>);
	static_assert( is_numeric_v<Float64>);
	static_assert(!is_numeric_v<Range<Int32  > >);
	static_assert(!is_numeric_v<Range<Float64> >);

	static_assert(!is_numeric_v <Void>);
	static_assert( is_integral_v<Void>);
	static_assert(!is_signed_v  <Void>);

	static_assert(!is_numeric_v <SharedStr>);
	static_assert(!is_integral_v<SharedStr>);
	static_assert(!is_signed_v  <SharedStr>);

	static_assert(!is_numeric_v <SharedStr>);
	static_assert(!is_integral_v<SharedStr>);
	static_assert(!is_signed_v  <SharedStr>);

	static_assert(std::is_same_v<acc_type_t<Int8>,  DiffT>);
	static_assert(std::is_same_v<acc_type_t<UInt8>, SizeT>);

#endif

#endif // !defined(__RTC_GEO_ELEMTRAITS_H)
