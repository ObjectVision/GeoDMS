/*
  Copyright 2008 Intel Corporation

  Use, modification and distribution are subject to the Boost Software License,
  Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
  http://www.boost.org/LICENSE_1_0.txt).
*/
namespace boost { namespace polygon { namespace detail {

template <typename coordinate_type>
struct minkowski_offset {
  typedef point_data<coordinate_type> point;
  typedef polygon_set_data<coordinate_type> polygon_set;
  typedef polygon_with_holes_data<coordinate_type> polygon;
  typedef std::pair<point, point> edge;

  static void convolve_two_segments(std::vector<point>& figure, const edge& a, const edge& b) {
    figure.clear();
    figure.push_back(point(a.first));
    figure.push_back(point(a.first));
    figure.push_back(point(a.second));
    figure.push_back(point(a.second));
    convolve(figure[0], b.second);
    convolve(figure[1], b.first);
    convolve(figure[2], b.first);
    convolve(figure[3], b.second);
  }

  template <typename itrT1>
  static void convolve_point_sequence_with_point(polygon_set& result, itrT1 ab, itrT1 ae, point pnt, bool substract)
  {
      polygon poly;

      std::vector<point> vec(ab, ae);

      for (auto i = vec.begin(), e = vec.end(); i != e; ++i)
          convolve(*i, pnt);

      set_points(poly, vec.begin(), vec.end());
      result.insert(poly, substract);
  }

  template <typename itrT1, typename itrT2>
  static void convolve_two_point_sequences(polygon_set& result, itrT1 ab, itrT1 ae, itrT2 bb, itrT2 be, bool substract) {
    if(ab == ae || bb == be)
      return;

    point prev_a = *ab++;
    std::vector<point> vec;
    polygon poly;

    for( ; ab != ae; ++ab) 
    {
        itrT2 tmpb = bb;
        auto prev_b = *tmpb++;

        for (; tmpb != be; ++tmpb)
        {
            convolve_two_segments(vec, std::make_pair(prev_b, *tmpb), std::make_pair(prev_a, *ab));
            set_points(poly, vec.begin(), vec.end());
            result.insert(poly, substract);
            prev_b = *tmpb;
        }
        prev_a = *ab;
    }
  }

  template <typename itrT>
  static void convolve_point_sequence_with_polygons(polygon_set& result, itrT b, itrT e, const std::vector<polygon>& polygons, bool substract) {
    for(std::size_t i = 0; i < polygons.size(); ++i) {
      convolve_two_point_sequences(result, b, e, begin_points(polygons[i]), end_points(polygons[i]), substract);
      for(auto itrh = begin_holes(polygons[i]); itrh != end_holes(polygons[i]); ++itrh) 
        convolve_two_point_sequences(result, b, e, begin_points(*itrh), end_points(*itrh), substract);
    }
  }

  // convolve geoometryData with kernel; assume (0, 0) is in kernel, 
// thus result doesn't need to be convolved to begin_points(kernel);
// Furthermore, kernel is a single ring (no holes nor islands).

  template <typename itrT>
  static void convolve_kernel_with_polygon_set(polygon_set& result, const std::vector<polygon>& polygons, itrT b, itrT e, bool substract)
  {
      convolve_point_sequence_with_polygons(result, b, e, polygons, substract);
      for (SizeT i = 0, n = polygons.size(); i != n; ++i)
      {
          dms_assert(begin_points(polygons[i]) != end_points(polygons[i]));
          convolve_point_sequence_with_point(result, b, e, *(begin_points(polygons[i])), substract);

          for (auto hptr = begin_holes(polygons[i]), hend = end_holes(polygons[i]); hptr != hend; ++hptr)
          {
              dms_assert(begin_points(*hptr) != end_points(*hptr));
              convolve_point_sequence_with_point(result, b, e, *(begin_points(*hptr)), substract);
          }
      }
  }

  static void convolve_two_polygon_sets(polygon_set& result, const polygon_set& a, const polygon_set& b, typename polygon_set::convolve_resources& resources) {
    result.clear();
	auto& a_polygons = resources.a_polygons; a_polygons.clear();
	auto& b_polygons = resources.b_polygons; b_polygons.clear();

    a.get(a_polygons, resources.cleanResources);
    b.get(b_polygons, resources.cleanResources);

    bool substract = false;

    for(std::size_t ai = 0; ai < a_polygons.size(); ++ai) {
      convolve_point_sequence_with_polygons(result, begin_points(a_polygons[ai]), end_points(a_polygons[ai]), b_polygons, substract);
      for(auto itrh = begin_holes(a_polygons[ai]); itrh != end_holes(a_polygons[ai]); ++itrh) 
      {
        convolve_point_sequence_with_polygons(result, begin_points(*itrh), end_points(*itrh), b_polygons, substract);
      }
      for(std::size_t bi = 0; bi < b_polygons.size(); ++bi) {
        polygon tmp_poly = a_polygons[ai];
        result.insert(convolve(tmp_poly, *(begin_points(b_polygons[bi]))));
        tmp_poly = b_polygons[bi];
        result.insert(convolve(tmp_poly, *(begin_points(a_polygons[ai]))));
      }
    }
  }
};

}
  template<typename T>
  inline polygon_set_data<T>&
  polygon_set_data<T>::resize(coordinate_type resizing, bool corner_fill_arc, unsigned int num_circle_segments, convolve_resources& resources) 
  {
    using namespace ::boost::polygon::operators;
    if(!corner_fill_arc) {
      if(resizing < 0)
        return shrink(-resizing, resources.cleanResources);
      if(resizing > 0)
        return bloat(resizing, resources.cleanResources);
      return *this;
    }
    if(resizing == 0) return *this;
    if(empty()) return *this;
    if(num_circle_segments < 3) num_circle_segments = 4;
    rectangle_data<coordinate_type> rect;
    extents(rect, resources.cleanResources);
    if(resizing < 0) {
      ::boost::polygon::bloat(rect, 10);
      operator_is(rect - (*this), resources.cleanResources); //invert
    }
    //make_arc(std::vector<point_data< T> >& return_points,
    //point_data< double> start, point_data< double>  end,
    //point_data< double> center,  double r, unsigned int num_circle_segments)
    std::vector<point_data<coordinate_type> > circle;
    point_data<double> center(0.0, 0.0), start(0.0, (double)resizing);
    make_arc(circle, start, start, center, std::abs((double)resizing),
             num_circle_segments);
    polygon_data<coordinate_type> poly;
    set_points(poly, circle.begin(), circle.end());
    polygon_set_data<coordinate_type> offset_set;
	self_assignment_boolean_op<polygon_set_data<coordinate_type>, polygon_data<coordinate_type>, 0>(offset_set, poly, resources.cleanResources);
    polygon_set_data<coordinate_type> result;
    detail::minkowski_offset<coordinate_type>::convolve_two_polygon_sets(result, *this, offset_set, resources);
    if(resizing < 0) {
      result.operator_is(result & rect, resources.cleanResources);//eliminate overhang
      result.operator_is(result ^ rect, resources.cleanResources);//invert
    }
    *this = result;
    return *this;
  }

}}
