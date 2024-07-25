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

#include "geo/PointIndexBuffer.h"
#include "geo/PointOrder.h"
#include "geo/SequenceTraits.h"

namespace gtl = boost::polygon;
using namespace gtl::operators;


// *****************************************************************************
//	bp_assign from polygon_with_holes_data_set
// *****************************************************************************

template <typename V>
Point<V> ConvertPoint(const gtl::point_data<V>& p)
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


template <dms_sequence E, typename MP>
void bp_assign_mp (E ref,  MP&& poly)
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
	{
		assert(i->begin() != i->end ());
		assert(i->begin()[0] == i->end()[-1]);
		//for (auto pi=i->begin(), pe=i->end(); pi!=pe; ++pi)
		for (auto pi=i->end(), pe=i->begin(); pi!=pe; ) // REVERSE ORDER
			ref.push_back(ConvertPoint(*--pi));

		auto hb=i->begin_holes(), hi=hb, he=i->end_holes(); 
		if (hi!=he)
		{
			do {
				assert(hi->begin() != hi->end());
				assert(hi->begin()[0] == hi->end()[-1]);

//				for (auto phi=hi->begin(), phe=hi->end(); phi!=phe; ++phi)
				for (auto phi=hi->end(), phe=hi->begin(); phi!=phe;)
					ref.push_back(ConvertPoint(*--phi));

			} while (++hi != he);
			assert(hi==he);
			assert(hi!=hb);
			--hi;
			while (hi != hb)
			{
				--hi;
				ref.push_back(ConvertPoint(hi->end()[-1]));
			}
			ref.push_back(ConvertPoint(i->end()[-1]));
		}
	}
	assert(i==e);
	assert(b<i);
	--i;
	while (i!=b)
	{
		--i;
		ref.push_back(ConvertPoint(i->end()[-1]));
	}
	assert(ref.size() == count);
}

template <dms_sequence E, typename V>
void bp_assign (E ref, gtl::polygon_set_data<V>& polyData, typename gtl::polygon_set_data<V>::clean_resources& cleanResources)
{
	std::vector<gtl::polygon_with_holes_data<Float64> > polyVect;
	polyData.get(polyVect, cleanResources);

	bp_assign_mp(ref, polyVect);
}

template <typename RI, typename CPI >
void bp_assign_ring(RI resIter, CPI ringBegin, CPI ringEnd)
{
	// REVERSE ORDER
	for (; ringBegin != ringEnd; ) 
		resIter->emplace_back(ConvertPoint(*--ringEnd));
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
	
		assert(i->begin() != i->end ());
		assert(i->begin()[0] == i->end()[-1]);

		bp_assign_ring(resIter, i->begin(), i->end());

		auto hb=i->begin_holes(), hi=hb, he=i->end_holes(); 
		if (hi!=he)
		{
			do {
				assert(hi->begin() != hi->end());
				assert(hi->begin()[0] == hi->end()[-1]);

				bp_assign_ring(resIter, hi->begin(), hi->end());
			} while (++hi != he);
			assert(hi==he);
			assert(hi!=hb);
			--hi;
			while (hi != hb)
			{
				--hi;
				resIter->push_back(ConvertPoint(hi->end()[-1]));
			}
			resIter->push_back(ConvertPoint(i->end()[-1]));
		}
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




template <typename P> struct SA_ConstRing; //fwrd decl

typedef polygon_set_concept this_polygon_concept;

template <> struct geometry_concept<IPolygon> { typedef polygon_concept type; };
template <> struct geometry_concept<UPolygon> { typedef polygon_concept type; };
template <> struct geometry_concept<SPolygon> { typedef polygon_concept type; };
template <> struct geometry_concept<WPolygon> { typedef polygon_concept type; };

template <> struct geometry_concept<SA_ConstRing<IPoint> > { typedef polygon_concept type; };
template <> struct geometry_concept<SA_ConstRing<UPoint> > { typedef polygon_concept type; };
template <> struct geometry_concept<SA_ConstRing<SPoint> > { typedef polygon_concept type; };
template <> struct geometry_concept<SA_ConstRing<WPoint> > { typedef polygon_concept type; };

template <> struct geometry_concept<SA_ConstReference<IPoint> > { typedef this_polygon_concept type; };
template <> struct geometry_concept<SA_ConstReference<UPoint> > { typedef this_polygon_concept type; };
template <> struct geometry_concept<SA_ConstReference<SPoint> > { typedef this_polygon_concept type; };
template <> struct geometry_concept<SA_ConstReference<WPoint> > { typedef this_polygon_concept type; };

template <> struct geometry_concept<SA_Reference<IPoint> > { typedef this_polygon_concept type; };
template <> struct geometry_concept<SA_Reference<UPoint> > { typedef this_polygon_concept type; };
template <> struct geometry_concept<SA_Reference<SPoint> > { typedef this_polygon_concept type; };
template <> struct geometry_concept<SA_Reference<WPoint> > { typedef this_polygon_concept type; };

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


template <typename P>
struct SA_ConstRing : IterRange<const P*>
{
	typedef P                            point_type;
	typedef typename scalar_of<P>::type  coordinate_type;
	typedef typename IterRange<const P*>::iterator iterator_type;

	SA_ConstRing(const P* first, const P* last)
		:	IterRange<const P*>(first, last)
	{}
};

template <typename P>
struct SA_ConstRingIterator
{
//	typedef std::forward_iterator_tag iterator_category;
	typedef std::random_access_iterator_tag iterator_category;
	typedef SA_ConstRing<P>                 value_type;
	typedef DiffT                           difference_type;
	typedef SA_ConstRing<P>*                pointer;
	typedef SA_ConstRing<P>&                reference;

	SA_ConstRingIterator() {}

	SA_ConstRingIterator(SA_ConstRingIterator&& src) 
		:	m_SequenceBase(src.m_SequenceBase)
		,	m_IndexBuffer (std::move(src.m_IndexBuffer))
		,	m_RingIndex   (src.m_RingIndex)
	{
	}

	SA_ConstRingIterator(const SA_ConstRingIterator& src) 
		:	m_SequenceBase(src.m_SequenceBase)
		,	m_IndexBuffer (src.m_IndexBuffer)
		,	m_RingIndex   (src.m_RingIndex)
	{
	}

	template <typename PointRange>
	SA_ConstRingIterator(const PointRange& scr, SizeT index)
		:	m_SequenceBase(begin_ptr(scr))
		,	m_RingIndex(index)
	{
		if (index != -1)
		{
			fillPointIndexBuffer(m_IndexBuffer, scr.begin(), scr.end());
			if (m_IndexBuffer.empty())
				m_RingIndex = -1;
			else
				assert(m_RingIndex < m_IndexBuffer.size());
		}
	}

	SA_ConstRingIterator& operator =(const SA_ConstRingIterator& rhs) = default;
	bool operator ==(const SA_ConstRingIterator& rhs) const { assert(m_SequenceBase == rhs.m_SequenceBase); return m_RingIndex == rhs.m_RingIndex; }
	bool operator !=(const SA_ConstRingIterator& rhs) const { assert(m_SequenceBase == rhs.m_SequenceBase); return m_RingIndex != rhs.m_RingIndex; }

	void operator ++()
	{ 
		assert(m_RingIndex != -1);
		if (++m_RingIndex == m_IndexBuffer.size())
			m_RingIndex = -1;
	}
	void operator --()
	{
		assert(m_RingIndex != 0);
		if (m_RingIndex == -1)
			m_RingIndex = m_IndexBuffer.size() - 1;
		else
			--m_RingIndex;
	}
	SA_ConstRing<P> operator *() const
	{
		assert(m_RingIndex != -1);
		assert(m_RingIndex < m_IndexBuffer.size());
		return SA_ConstRing<P>(
			m_SequenceBase + m_IndexBuffer[m_RingIndex].first 
		,	m_SequenceBase + m_IndexBuffer[m_RingIndex].second
		);
	}
	difference_type operator -(const SA_ConstRingIterator& oth) const
	{
		SizeT index    = m_RingIndex;
		SizeT othIndex = oth.m_RingIndex;
		if (index == othIndex) // same, including both at end
			return 0;
		if (othIndex == -1)
			othIndex = m_IndexBuffer.size(); // oth is at end but not this
		if (index == -1)
			index = oth.m_IndexBuffer.size(); // this is at end but not oth
		return index - othIndex;
	}

	const P*            m_SequenceBase = nullptr;
	pointIndexBuffer_t  m_IndexBuffer;
	SizeT               m_RingIndex = -1;
};

template <typename E>
struct poly_sequence_traits
{
	typedef E                                            elem_type;
	typedef typename elem_type::value_type               value_type;
	typedef typename std::remove_const<value_type>::type point_type;
	typedef typename scalar_of<point_type>::type         coordinate_type;
	typedef typename SA_ConstRingIterator<point_type>    iterator_type;
	typedef typename polygon_set_data<coordinate_type>::clean_resources clean_resources;
	typedef typename arbitrary_boolean_op<coordinate_type>::execute_resources execute_resources;

	static inline iterator_type     begin  (const elem_type& t) { return iterator_type(t,  0); }
	static inline iterator_type     end    (const elem_type& t) { return iterator_type(t, -1); }
	static inline bool clean  (const elem_type& t, clean_resources& r) { return false; }
	static inline bool              sorted (const elem_type& t) { return false; }
};

template <> struct polygon_traits<SA_ConstRing<IPoint> > : point_sequence_traits<SA_ConstRing<IPoint> > {};
template <> struct polygon_traits<SA_ConstRing<UPoint> > : point_sequence_traits<SA_ConstRing<UPoint> > {};
template <> struct polygon_traits<SA_ConstRing<SPoint> > : point_sequence_traits<SA_ConstRing<SPoint> > {};
template <> struct polygon_traits<SA_ConstRing<WPoint> > : point_sequence_traits<SA_ConstRing<WPoint> > {};

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

	using polygon_with_holes_type = gtl::polygon_with_holes_data<coordinate_type>;

	using point_type = gtl::point_data<coordinate_type>;
	using rect_type = gtl::rectangle_data<coordinate_type>;
	using ring_type = std::vector< point_type >;
	using multi_polygon_type = std::vector< polygon_with_holes_type >;
	using polygon_set_data_type = gtl::polygon_set_data<coordinate_type>;

	using area_type = gtl::coordinate_traits<coordinate_type>::area_type;
	using unsigned_area_type = gtl::coordinate_traits<coordinate_type>::unsigned_area_type;
};

template<typename P> concept gtl_point = gtl::is_point_concept    <typename gtl::geometry_concept<P>::type>::type::value;
template<typename R> concept gtl_rect = gtl::is_rectangle_concept<typename gtl::geometry_concept<R>::type>::type::value;
template<typename P> concept gtl_PolygonSet = gtl::is_any_polygon_set_type<P>::type::value;




#endif //!defined(DMS_GEO_BOOSTPOLYGON_H)
