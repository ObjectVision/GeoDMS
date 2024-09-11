// Copyright (C) 1998-2024 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

#if !defined(DMS_GEO_BOOSTPOLYGON_H)
#define DMS_GEO_BOOSTPOLYGON_H

#pragma warning( disable : 4018) // warning C4018: '<=' : signed/unsigned mismatch
#pragma warning( disable : 4146) // warning C4146: unary minus operator applied to unsigned type, result still unsigned

#include "ipolygon/polygon.hpp"

#include "geo/GeoSequence.h"
#include "geo/PointIndexBuffer.h"
#include "geo/PointOrder.h"
#include "geo/RingIterator.h"
#include "geo/SequenceTraits.h"


enum class geometry_library { boost_polygon, boost_geometry, cgal, geos };

namespace bp = boost::polygon;
//using namespace bp::operators;


// *****************************************************************************
//	bp_assign from polygon_with_holes_data_set
// *****************************************************************************

template <typename V>
Point<V> ConvertPoint(const bp::point_data<V>& p)
{
	return shp2dms_order<V>(x(p), y(p));
}

/* REMOVE, WIP
template <typename R, typename P>
concept Ring = requires (const R& ring)
{
	{ ring.size() } -> std::same_as<SizeT>;
	{ ring.begin() } -> std::same_as<P*>;
	{ ring.end() } -> std::same_as<P*>;
};

template <typename PWH, typename R>
concept PolygonWithHoles = requires (const PWH& poly)
{
	{ outer_ring(poly) } -> std::same_as<R>;
	{ inner_rings(poly) } -> std::same_as<std::vector<R>>;
};

template <typename MP, typename PolygonWithHoles>
concept MultiPolygon = std::same_as<MP, std::vector<PolygonWithHoles>>;
*/

template <typename E, typename CPI >
void bp_assign_ring(E&& ref, CPI ringBegin, CPI ringEnd)
{
	assert(ringBegin != ringEnd);
	assert(ringBegin[0] == ringEnd[-1]);

	// REVERSE ORDER
	for (; ringBegin != ringEnd; )
		ref.emplace_back(ConvertPoint(*--ringEnd));
}

template <dms_sequence E, typename POLYGON>
void bp_assign_polygon(E&& ref, POLYGON&& polygonWithHoles)
{
	//for (auto pi=i->begin(), pe=i->end(); pi!=pe; ++pi)
	bp_assign_ring(std::forward<E>(ref), polygonWithHoles.begin(), polygonWithHoles.end());

	auto hb = polygonWithHoles.begin_holes(), he = polygonWithHoles.end_holes();
	if (hb == he)
		return;

	auto hi = hb;
	do {
		bp_assign_ring(std::forward<E>(ref), hi->begin(), hi->end());
	} while (++hi != he);

	assert(hi == he);
	assert(hi != hb);
	--hi;
	while (hi != hb)
	{
		--hi;
		ref.push_back(ConvertPoint(hi->begin()[0]));
	}
	ref.push_back(ConvertPoint(polygonWithHoles.begin()[0]));
}

template <dms_sequence E, typename MP>
void bp_assign_mp (E&& ref,  MP&& poly)
{
	ref.clear();

	if (!poly.size())
		return;

	SizeT count = poly.size() -1;

	auto b=poly.begin(), i=b, e=poly.end();
	assert(b!=e); // follows from poly.size()

	for (; i!=e; ++i)
	{
		assert(i->size());
		count += i->size();
		for (auto hi=i->begin_holes(), he=i->end_holes(); hi!=he; ++hi)
			count += hi->size() + 1;
	}

	ref.reserve(count);
	
	for (i=b; i!=e; ++i)
		bp_assign_polygon(std::forward<E>(ref), *i);

	assert(i==e);
	assert(b<i);
	--i;
	while (i!=b)
	{
		--i;
		ref.push_back(ConvertPoint(i->begin()[0]));
	}
	assert(ref.size() == count);
}

template <dms_sequence E, typename V>
void bp_assign (E&& ref, bp::polygon_set_data<V>& polyData, typename bp::polygon_set_data<V>::clean_resources& cleanResources)
{
	std::vector<bp::polygon_with_holes_data<Float64> > polyVect;
	polyData.get(polyVect, cleanResources);

	bp_assign_mp(std::forward<E>(ref), polyVect);
}


template <typename RI, typename MP >
auto bp_split_assign (RI resIter, MP& poly) -> RI
{
	for (auto i=poly.begin(), e=poly.end(); i!=e; ++resIter, ++i)
	{
		resIter->clear();

		assert(i->size());
		SizeT count = i->size();
		for (auto hi=i->begin_holes(), he=i->end_holes(); hi!=he; ++hi)
			count += hi->size() + 1;

		resIter->reserve(count);
	
		bp_assign_polygon(*resIter, *i);
		assert(resIter->size() == count);
	}
	return resIter;
}

// *****************************************************************************
//	Polygon traits
// *****************************************************************************

namespace boost { namespace polygon {

template <> struct geometry_concept<UInt32> { using type = coordinate_concept; };
template <> struct geometry_concept<UInt16> { using type = coordinate_concept; };
template <> struct geometry_concept< Int16> { using type = coordinate_concept; };

template <typename V, typename SP, typename UP>
struct coordinate_traits_base
{
    typedef V coordinate_type;
    typedef long double area_type;

    typedef SP manhattan_area_type;
    typedef UP unsigned_area_type;
    typedef DiffT coordinate_difference;

    typedef long double coordinate_distance;
};

template <> struct coordinate_traits<UInt32> : coordinate_traits_base<UInt32, Int64, UInt64> {};
template <> struct coordinate_traits<UInt16> : coordinate_traits_base<UInt16, Int32, UInt32> {};
template <> struct coordinate_traits< Int16> : coordinate_traits_base< Int16, Int32, UInt32> {};

template <> struct geometry_concept<IPoint> { using type = point_concept; };
template <> struct geometry_concept<UPoint> { using type = point_concept; };
template <> struct geometry_concept<SPoint> { using type = point_concept; };
template <> struct geometry_concept<WPoint> { using type = point_concept; };


template <typename CoordType>
struct point_traits<Point<CoordType> > 
{
  using point_type = Point<CoordType>;
  using coordinate_type = CoordType;

  static coordinate_type get(
      const point_type& point, orientation_2d orient) {
    return orient == HORIZONTAL ? point.X() : point.Y();
  }
};

template <typename CoordType>
struct point_mutable_traits<Point<CoordType> > 
{
	using point_type = Point<CoordType>;
	using coordinate_type = CoordType;

	static void set(point_type& point, orientation_2d orient, coordinate_type value) {
		if (orient == HORIZONTAL)
			point.X() = value;
		else
			point.Y() = value;
	}

	static point_type construct(coordinate_type x, coordinate_type y) {
		return shp2dms_order<coordinate_type>(x, y);
	}
};



template <> struct geometry_concept<IPolygon> { using type = polygon_concept; };
template <> struct geometry_concept<UPolygon> { using type = polygon_concept; };
template <> struct geometry_concept<SPolygon> { using type = polygon_concept; };
template <> struct geometry_concept<WPolygon> { using type = polygon_concept; };

template <> struct geometry_concept<SA_ConstRing<IPoint> > { using type = polygon_concept; };
template <> struct geometry_concept<SA_ConstRing<UPoint> > { using type = polygon_concept; };;
template <> struct geometry_concept<SA_ConstRing<SPoint> > { using type = polygon_concept; };
template <> struct geometry_concept<SA_ConstRing<WPoint> > { using type = polygon_concept; };

template <> struct geometry_concept<SA_ConstReference<IPoint> > { using type = polygon_set_concept; };
template <> struct geometry_concept<SA_ConstReference<UPoint> > { using type = polygon_set_concept; };
template <> struct geometry_concept<SA_ConstReference<SPoint> > { using type = polygon_set_concept; };
template <> struct geometry_concept<SA_ConstReference<WPoint> > { using type = polygon_set_concept; };

template <> struct geometry_concept<SA_Reference<IPoint> > { using type = polygon_set_concept; };
template <> struct geometry_concept<SA_Reference<UPoint> > { using type = polygon_set_concept; };
template <> struct geometry_concept<SA_Reference<SPoint> > { using type = polygon_set_concept; };
template <> struct geometry_concept<SA_Reference<WPoint> > { using type = polygon_set_concept; };

template <typename S> struct geometry_concept<std::vector<bp::point_data<S>> > { using type = polygon_concept; };

template <typename E>
struct point_sequence_traits
{
	typedef E                                            elem_type;
	typedef typename elem_type::value_type               value_type;
	typedef typename std::remove_const<value_type>::type point_type;
	typedef typename scalar_of<point_type>::type         coordinate_type;
	typedef typename elem_type::const_iterator           iterator_type;

	static inline iterator_type     begin_points(const elem_type& t) { return t.begin(); }
	static inline iterator_type     end_points  (const elem_type& t) { return t.end(); }
	static inline unsigned int      size        (const elem_type& t) { return t.size(); }
	static inline winding_direction winding     (const elem_type& t) { return clockwise_winding; }
};

template <typename E>
struct point_sequence_mutable_traits
{
	typedef E        elem_type;
	typedef typename elem_type::value_type point_type;

	template <typename iT>
	static inline elem_type& set_points(elem_type& t, iT input_begin, iT input_end) 
	{
		t.clear();
		t.reserve(std::distance(input_begin, input_end));
		while (input_begin != input_end)
		{
			t.push_back(shp2dms_order(point_type(input_begin->x(), input_begin->y())));
			++input_begin;
		}	
		return t;
	}
};


template <typename E>
struct poly_sequence_traits
{
	using elem_type = E;
	using value_type = typename elem_type::value_type;
	using point_type = std::remove_const_t<value_type>;
	using coordinate_type = scalar_of_t<point_type>;
	using iterator_type = SA_ConstRingIterator<point_type>;
	using clean_resources = typename polygon_set_data<coordinate_type>::clean_resources ;
	using execute_resources = typename arbitrary_boolean_op<coordinate_type>::execute_resources ;

	static inline auto begin  (const elem_type& t) -> iterator_type { return iterator_type(t,  0); }
	static inline auto end    (const elem_type& t) -> iterator_type { return iterator_type(t, -1); }
	static inline bool clean  (const elem_type& t, clean_resources& r) { return false; }
	static inline bool sorted (const elem_type& t) { return false; }
};

template <> struct polygon_traits<SA_ConstRing<IPoint> > : point_sequence_traits<SA_ConstRing<IPoint> > {};
template <> struct polygon_traits<SA_ConstRing<UPoint> > : point_sequence_traits<SA_ConstRing<UPoint> > {};
template <> struct polygon_traits<SA_ConstRing<SPoint> > : point_sequence_traits<SA_ConstRing<SPoint> > {};
template <> struct polygon_traits<SA_ConstRing<WPoint> > : point_sequence_traits<SA_ConstRing<WPoint> > {};

template <typename S> struct polygon_traits<std::vector<bp::point_data<S>> > : point_sequence_traits< std::vector<bp::point_data<S>>> {};

template <> struct polygon_set_traits<SA_ConstReference<IPoint> > : poly_sequence_traits<SA_ConstReference<IPoint> > {};
template <> struct polygon_set_traits<SA_ConstReference<UPoint> > : poly_sequence_traits<SA_ConstReference<UPoint> > {};
template <> struct polygon_set_traits<SA_ConstReference<SPoint> > : poly_sequence_traits<SA_ConstReference<SPoint> > {};
template <> struct polygon_set_traits<SA_ConstReference<WPoint> > : poly_sequence_traits<SA_ConstReference<WPoint> > {};

template <> struct polygon_set_traits<SA_Reference<IPoint> > : poly_sequence_traits<SA_Reference<IPoint> > {};
template <> struct polygon_set_traits<SA_Reference<UPoint> > : poly_sequence_traits<SA_Reference<UPoint> > {};
template <> struct polygon_set_traits<SA_Reference<SPoint> > : poly_sequence_traits<SA_Reference<SPoint> > {};
template <> struct polygon_set_traits<SA_Reference<WPoint> > : poly_sequence_traits<SA_Reference<WPoint> > {};

template <> struct polygon_set_traits<IPolygon> : poly_sequence_traits<IPolygon> {};
template <> struct polygon_set_traits<UPolygon> : poly_sequence_traits<UPolygon> {};
template <> struct polygon_set_traits<SPolygon> : poly_sequence_traits<SPolygon> {};
template <> struct polygon_set_traits<WPolygon> : poly_sequence_traits<WPolygon> {};

}} // namespace boost { namespace polygon {

template <typename C>
struct bp_union_poly_traits
{
	using coordinate_type = C;

	using polygon_with_holes_type = bp::polygon_with_holes_data<coordinate_type>;

	using point_type = bp::point_data<coordinate_type>;
	using rect_type = bp::rectangle_data<coordinate_type>;
	using ring_type = std::vector< point_type >;
	using multi_polygon_type = std::vector< polygon_with_holes_type >;
	using polygon_set_data_type = bp::polygon_set_data<coordinate_type>;

	using area_type = bp::coordinate_traits<coordinate_type>::area_type;
	using unsigned_area_type = bp::coordinate_traits<coordinate_type>::unsigned_area_type;
};

template<typename P> concept gtl_point = bp::is_point_concept    <typename bp::geometry_concept<P>::type>::type::value;
template<typename R> concept gtl_rect = bp::is_rectangle_concept<typename bp::geometry_concept<R>::type>::type::value;
template<typename P> concept gtl_PolygonSet = bp::is_any_polygon_set_type<P>::type::value;

using bp_point = bp::point_data<int>;
using bp_linestring = std::vector<bp_point>;

#include <ipolygon/polygon.hpp>

template <typename C>
void move(typename bp_union_poly_traits<C>::point_type& point, typename bp_union_poly_traits<C>::point_type delta)
{
	point.x(point.x() + delta.x());
	point.y(point.y() + delta.y());
}

template <typename C>
void move(typename bp_union_poly_traits<C>::ring_type& ring, typename bp_union_poly_traits<C>::point_type delta)
{
	for (auto& p : ring)
		move<C>(p, delta);
}


#endif //!defined(DMS_GEO_BOOSTPOLYGON_H)
