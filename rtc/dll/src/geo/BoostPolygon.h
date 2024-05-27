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

#if !defined(DMS_GEO_BOOSTPOLYGON_H)
#define DMS_GEO_BOOSTPOLYGON_H

#pragma warning( disable : 4018) // warning C4018: '<=' : signed/unsigned mismatch
#pragma warning( disable : 4146) // warning C4146: unary minus operator applied to unsigned type, result still unsigned

#include "ipolygon/polygon.hpp"

#include "geo/PointIndexBuffer.h"
#include "geo/PointOrder.h"

namespace gtl = boost::polygon;
using namespace gtl::operators;


// *****************************************************************************
//	dms_assign from polygon_with_holes_data_set
// *****************************************************************************

template <typename V>
Point<V> ConvertPoint(const gtl::point_data<V>& p)
{
	return shp2dms_order<V>(x(p), y(p));
}


template <typename E, typename V>
typename std::enable_if<is_dms_sequence<E>::value>::type
dms_assign (E& ref,  const std::vector<gtl::polygon_with_holes_data<V> >& poly)
{
	ref.clear();

	if (!poly.size())
		return;

	SizeT count = poly.size() -1;

	auto b=poly.begin(), i=b, e=poly.end();
	dms_assert(b!=e); // follows from poly.size()

	for (; i!=e; ++i)
	{
		dms_assert(i->size());
		count += i->size();
		for (auto hi=i->begin_holes(), he=i->end_holes(); hi!=he; ++hi)
			count += hi->size() + 1;
	}

	ref.reserve(count);
	
	for (i=b; i!=e; ++i)
	{
		dms_assert(i->begin() != i->end ());
		dms_assert(i->begin()[0] == i->end()[-1]);
		//for (auto pi=i->begin(), pe=i->end(); pi!=pe; ++pi)
		for (auto pi=i->end(), pe=i->begin(); pi!=pe; ) // REVERSE ORDER
			ref.push_back(ConvertPoint(*--pi));

		auto hb=i->begin_holes(), hi=hb, he=i->end_holes(); 
		if (hi!=he)
		{
			do {
				dms_assert(hi->begin() != hi->end());
				dms_assert(hi->begin()[0] == hi->end()[-1]);

//				for (auto phi=hi->begin(), phe=hi->end(); phi!=phe; ++phi)
				for (auto phi=hi->end(), phe=hi->begin(); phi!=phe;)
					ref.push_back(ConvertPoint(*--phi));

			} while (++hi != he);
			dms_assert(hi==he);
			dms_assert(hi!=hb);
			--hi;
			while (hi != hb)
			{
				--hi;
				ref.push_back(ConvertPoint(hi->end()[-1]));
			}
			ref.push_back(ConvertPoint(i->end()[-1]));
		}
	}
	dms_assert(i==e);
	dms_assert(b<i);
	--i;
	while (i!=b)
	{
		--i;
		ref.push_back(ConvertPoint(i->end()[-1]));
	}
	dms_assert(ref.size() == count);
}

template <typename E, typename V>
typename std::enable_if<is_dms_sequence<E>::value>::type
dms_assign (E& ref, gtl::polygon_set_data<V>& polyData, typename gtl::polygon_set_data<V>::clean_resources& cleanResources)
{
	std::vector<gtl::polygon_with_holes_data<V> > polyVect;
	polyData.get(polyVect, cleanResources);
	dms_assign(ref, polyVect);
}

template <typename RI, typename V>
void dms_split_assign (RI resIter,  std::vector<gtl::polygon_with_holes_data<V> >& poly)
{
	if (!poly.size())
		return;

	for (auto i=poly.begin(), e=poly.end(); i!=e; ++resIter, ++i)
	{
		resIter->clear();

		dms_assert(i->size());
		SizeT count = i->size();
		for (auto hi=i->begin_holes(), he=i->end_holes(); hi!=he; ++hi)
			count += hi->size() + 1;

		resIter->reserve(count);
	
		dms_assert(i->begin() != i->end ());
		dms_assert(i->begin()[0] == i->end()[-1]);

		//for (auto pi=i->begin(), pe=i->end(); pi!=pe; ++pi)
		for (auto pi=i->end(), pe=i->begin(); pi!=pe; ) // REVERSE ORDER
			resIter->push_back(ConvertPoint(*--pi));

		auto hb=i->begin_holes(), hi=hb, he=i->end_holes(); 
		if (hi!=he)
		{
			do {
				dms_assert(hi->begin() != hi->end());
				dms_assert(hi->begin()[0] == hi->end()[-1]);

//				for (auto phi=hi->begin(), phe=hi->end(); phi!=phe; ++phi)
				for (auto phi=hi->end(), phe=hi->begin(); phi!=phe;) // REVERSE ORDER
					resIter->push_back(ConvertPoint(*--phi));

			} while (++hi != he);
			dms_assert(hi==he);
			dms_assert(hi!=hb);
			--hi;
			while (hi != hb)
			{
				--hi;
				resIter->push_back(ConvertPoint(hi->end()[-1]));
			}
			resIter->push_back(ConvertPoint(i->end()[-1]));
		}
		dms_assert(resIter->size() == count);
	}
}

// *****************************************************************************
//	Polygon traits
// *****************************************************************************

namespace boost { namespace polygon {

template <> struct geometry_concept<UInt32> { typedef coordinate_concept type; };
template <> struct geometry_concept<UInt16> { typedef coordinate_concept type; };
template <> struct geometry_concept< Int16> { typedef coordinate_concept type; };

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

template <> struct geometry_concept<IPoint> { typedef point_concept type; };
template <> struct geometry_concept<UPoint> { typedef point_concept type; };
template <> struct geometry_concept<SPoint> { typedef point_concept type; };
template <> struct geometry_concept<WPoint> { typedef point_concept type; };


template <typename CoordType>
struct point_traits<Point<CoordType> > 
{
  typedef Point<CoordType> point_type;
  typedef CoordType        coordinate_type;

  static coordinate_type get(
      const point_type& point, orientation_2d orient) {
    return orient == HORIZONTAL ? point.X() : point.Y();
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
		:	m_SequenceIter(src.m_SequenceIter)
		,	m_IndexBuffer (std::move(src.m_IndexBuffer))
		,	m_RingIndex   (src.m_RingIndex)
	{
	}

	SA_ConstRingIterator(const SA_ConstRingIterator& src) 
		:	m_SequenceIter(src.m_SequenceIter)
		,	m_IndexBuffer (src.m_IndexBuffer)
		,	m_RingIndex   (src.m_RingIndex)
	{
	}

	SA_ConstRingIterator(const SA_ConstReference<P>& scr, SizeT index)
		:	m_SequenceIter(scr.makePtr())
		,	m_RingIndex(index)
	{
		if (index != -1)
		{
			fillPointIndexBuffer(m_IndexBuffer, scr.begin(), scr.end());
			if (m_IndexBuffer.empty())
				m_RingIndex = -1;
			else
				dms_assert(m_RingIndex < m_IndexBuffer.size());
		}
	}
	SA_ConstRingIterator& operator =(const SA_ConstRingIterator& rhs) = default;
	bool operator ==(const SA_ConstRingIterator& rhs) const { dms_assert(m_SequenceIter == rhs.m_SequenceIter); return m_RingIndex == rhs.m_RingIndex; }
	bool operator !=(const SA_ConstRingIterator& rhs) const { dms_assert(m_SequenceIter == rhs.m_SequenceIter); return m_RingIndex != rhs.m_RingIndex; }

	void operator ++()
	{ 
		dms_assert(m_RingIndex != -1);
		if (++m_RingIndex == m_IndexBuffer.size())
			m_RingIndex = -1;
	}
	void operator --()
	{
		dms_assert(m_RingIndex != 0);
		if (m_RingIndex == -1)
			m_RingIndex = m_IndexBuffer.size() - 1;
		else
			--m_RingIndex;
	}
	SA_ConstRing<P> operator *() const
	{
		dms_assert(m_RingIndex != -1);
		dms_assert(m_RingIndex < m_IndexBuffer.size());
		return SA_ConstRing<P>(
			m_SequenceIter->begin() + m_IndexBuffer[m_RingIndex].first 
		,	m_SequenceIter->begin() + m_IndexBuffer[m_RingIndex].second
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

	SA_ConstIterator<P> m_SequenceIter;
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

}} // namespace boost { namespace polygon {


template <typename P>
struct union_poly_traits
{
	typedef P                       PointType;
	typedef std::vector<PointType>  PolygonType;

	typedef typename scalar_of<P>::type                   coordinate_type;
	typedef gtl::polygon_with_holes_data<coordinate_type> polygon_with_holes_type;

	typedef gtl::point_data<coordinate_type>       point_type;
	typedef gtl::rectangle_data<coordinate_type>   rect_type;
	typedef std::vector< point_type >              point_seq_type;
	typedef std::vector< polygon_with_holes_type > polygon_result_type;
	typedef gtl::polygon_set_data<coordinate_type> polygon_set_data_type;

	typedef typename gtl::coordinate_traits<coordinate_type>::area_type          area_type;
	typedef typename gtl::coordinate_traits<coordinate_type>::unsigned_area_type unsigned_area_type;
};

template <typename C, typename geometry_type_2>
typename std::enable_if<gtl::is_any_polygon_set_type<geometry_type_2>::type::value>::type
dms_insert(gtl::polygon_set_data<C>& lvalue, const geometry_type_2& rvalue, typename gtl::polygon_set_data<C>::clean_resources& r)
{
	lvalue.insert(
		gtl::polygon_set_traits<geometry_type_2>::begin(rvalue), 
		gtl::polygon_set_traits<geometry_type_2>::end  (rvalue)
	);
}

#endif //!defined(DMS_GEO_BOOSTPOLYGON_H)
