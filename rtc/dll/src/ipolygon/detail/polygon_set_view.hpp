/*
  Copyright 2008 Intel Corporation

  Use, modification and distribution are subject to the Boost Software License,
  Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
  http://www.boost.org/LICENSE_1_0.txt).
*/
#ifndef BOOST_POLYGON_POLYGON_SET_VIEW_HPP
#define BOOST_POLYGON_POLYGON_SET_VIEW_HPP
namespace boost { namespace polygon{

	template <typename coordinate_type>
	struct polygon_set_data<coordinate_type>::clean_resources {
		typename arbitrary_boolean_op<coordinate_type>::execute_resources executeResources;
		polygon_set_data<coordinate_type> tmp2;
		arbitrary_boolean_op<coordinate_type> abo;
	};

	template <typename coordinate_type>
	struct polygon_set_data<coordinate_type>::convolve_resources
	{
		typedef boost::polygon::polygon_with_holes_data<coordinate_type> polygon;
		std::vector<polygon> a_polygons;
		std::vector<polygon> b_polygons;
		typename boost::polygon::polygon_set_data<coordinate_type>::clean_resources cleanResources;
	};

  template <typename coordinate_type>
  inline void polygon_set_data<coordinate_type>::clean(clean_resources& r) const {
    if(dirty_) {
      sort();
	  auto&  abo = r.abo; abo.clear();
      r.tmp2.clear();
      abo.execute_clean(r.tmp2, begin(), end(), r.executeResources);
      data_.swap(r.tmp2.data_);
      is_45_ = r.tmp2.is_45_; // TODO: Remove
      dirty_ = false;
    }
  }

  template <typename value_type, typename arg_type>
  inline void insert_into_view_arg(value_type& dest, const arg_type& arg);

  template <typename ltype, typename rtype, int op_type>
  class polygon_set_view;

  template <typename ltype, typename rtype, int op_type>
  struct polygon_set_traits<polygon_set_view<ltype, rtype, op_type> > {
    typedef typename polygon_set_view<ltype, rtype, op_type>::coordinate_type coordinate_type;
    typedef typename polygon_set_view<ltype, rtype, op_type>::iterator_type iterator_type;
    typedef typename polygon_set_view<ltype, rtype, op_type>::operator_arg_type operator_arg_type;
	typedef typename polygon_set_view<ltype, rtype, op_type>::execute_resources execute_resources;
	typedef typename polygon_set_data<coordinate_type>::clean_resources clean_resources;

    static inline iterator_type begin(const polygon_set_view<ltype, rtype, op_type>& polygon_set);
    static inline iterator_type end(const polygon_set_view<ltype, rtype, op_type>& polygon_set);

    static inline bool clean(const polygon_set_view<ltype, rtype, op_type>& polygon_set, clean_resources&);
    static inline bool sort(const polygon_set_view<ltype, rtype, op_type>& polygon_set);
  };

  template <typename value_type, typename geometry_type_1, typename geometry_type_2, int op_type>
  void execute_boolean_op(value_type& output_, const geometry_type_1& lvalue_, const geometry_type_2& rvalue_
	  , typename arbitrary_boolean_op<typename polygon_set_traits<geometry_type_1>::coordinate_type>::execute_resources& executeResources
  ) 
  {
    typedef geometry_type_1 ltype;
    //typedef geometry_type_2 rtype;
    typedef typename polygon_set_traits<ltype>::coordinate_type coordinate_type;
    value_type linput_;
    value_type rinput_;
    insert_into_view_arg(linput_, lvalue_);
    insert_into_view_arg(rinput_, rvalue_);
    arbitrary_boolean_op<coordinate_type> abo;
    abo.execute<op_type>(output_, linput_.begin(), linput_.end(),
                rinput_.begin(), rinput_.end(), executeResources);
  }

  template <typename ltype, typename rtype, int op_type>
  class polygon_set_view {
  public:
    typedef typename polygon_set_traits<ltype>::coordinate_type coordinate_type;
    typedef polygon_set_data<coordinate_type> value_type;
    typedef typename value_type::iterator_type iterator_type;
    typedef polygon_set_view operator_arg_type;
	using execute_resources = typename arbitrary_boolean_op<coordinate_type>::execute_resources;
  private:
    const ltype& lvalue_;
    const rtype& rvalue_;
    mutable value_type output_;
    mutable bool evaluated_;
    polygon_set_view& operator=(const polygon_set_view&);
  public:
    polygon_set_view(const ltype& lvalue, const rtype& rvalue)
	  : lvalue_(lvalue), rvalue_(rvalue), output_(), evaluated_(false) 
	{
//		execute_boolean_op<value_type, ltype, rtype, op_type>(output_, lvalue, rvalue, executeResources);
	}

    // get iterator to begin vertex data
  public:

	void evaluate(execute_resources& executeResources) const 
	{
		if (!evaluated_) {
			evaluated_ = true;
			execute_boolean_op<value_type, ltype, rtype, op_type>(output_, lvalue_, rvalue_, executeResources);
		}
	}
	const value_type& value(execute_resources& executeResources) const { // TODO: REMOVE ?
	  evaluate(executeResources);
      return output_;
    }

  public:
	iterator_type begin() const { dms_assert(evaluated_); return output_.begin(); }
    iterator_type end  () const { dms_assert(evaluated_); return output_.end(); }

    bool dirty() const { return false; } //result of a boolean is clean
    bool sorted() const { return true; } //result of a boolean is sorted

    void sort() const {} //is always sorted
  };

  template <typename ltype, typename rtype, int op_type>
  typename polygon_set_traits<polygon_set_view<ltype, rtype, op_type> >::iterator_type
  polygon_set_traits<polygon_set_view<ltype, rtype, op_type> >::
  begin(const polygon_set_view<ltype, rtype, op_type>& polygon_view) {
    return polygon_view.begin();
  }
  template <typename ltype, typename rtype, int op_type>
  typename polygon_set_traits<polygon_set_view<ltype, rtype, op_type> >::iterator_type
  polygon_set_traits<polygon_set_view<ltype, rtype, op_type> >::
  end(const polygon_set_view<ltype, rtype, op_type>& polygon_set) {
    return polygon_set.end();
  }
  template <typename ltype, typename rtype, int op_type>
  bool polygon_set_traits<polygon_set_view<ltype, rtype, op_type> >::
  clean(const polygon_set_view<ltype, rtype, op_type>& polygonView, clean_resources& r)
  {
	  polygonView.evaluate(r.executeResources);
	  return true;
  }

  template <typename ltype, typename rtype, int op_type>
  bool polygon_set_traits<polygon_set_view<ltype, rtype, op_type> >::
  sort(const polygon_set_view<ltype, rtype, op_type>& ) {
    return true; }

  template <typename value_type, typename arg_type>
  inline void insert_into_view_arg(value_type& dest, const arg_type& arg) {
    typedef typename polygon_set_traits<arg_type>::iterator_type literator;
    literator itr1, itr2;
    itr1 = polygon_set_traits<arg_type>::begin(arg);
    itr2 = polygon_set_traits<arg_type>::end(arg);
    dest.insert(itr1, itr2);
  }

  template <typename geometry_type_1, typename geometry_type_2, int op_type>
  geometry_type_1& self_assignment_boolean_op(geometry_type_1& lvalue_, const geometry_type_2& rvalue_
	  , typename polygon_set_data<typename polygon_set_traits<geometry_type_1>::coordinate_type>::clean_resources& r) {
    typedef geometry_type_1 ltype;
    typedef typename polygon_set_traits<ltype>::coordinate_type coordinate_type;
    typedef polygon_set_data<coordinate_type> value_type;
    value_type output_;
    execute_boolean_op<value_type, geometry_type_1, geometry_type_2, op_type>(output_, lvalue_, rvalue_, r.executeResources);
//    polygon_set_mutable_traits<geometry_type_1>::set(lvalue_, output_, r);
	polygon_set_mutable_traits<geometry_type_1>::set(lvalue_, output_.begin(), output_.end(), r);
	return lvalue_;
  }

  // copy constructor
  template <typename coordinate_type>
  template <typename ltype, typename rtype, int op_type>
  polygon_set_data<coordinate_type>::polygon_set_data(const polygon_set_view<ltype, rtype, op_type>& that) :
    data_(that.value().data_), dirty_(that.value().dirty_), unsorted_(that.value().unsorted_), is_45_(that.value().is_45_) {}

    // equivalence operator
  template <typename coordinate_type>
  inline bool polygon_set_data<coordinate_type>::operator==(const polygon_set_data<coordinate_type>& p) const {
    typedef polygon_set_data<coordinate_type> value_type;
    value_type output_;
    execute_boolean_op<value_type, value_type, value_type, 2>(output_, (*this), p);  
    return output_.data_.empty();
  }

  template <typename ltype, typename rtype, int op_type>
  struct geometry_concept<polygon_set_view<ltype, rtype, op_type> > { typedef polygon_set_concept type; };
}
}
#endif
