// Copyright (C) 1998-2026 Object Vision B.V. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif


#if !defined(__CLC_BOOSTBLAS_H)
#define __CLC_BOOSTBLAS_H

#include "mem/Grid.h"


#include <boost/numeric/ublas/expression_types.hpp>

template <typename T>
struct MatrExprModel : boost::numeric::ublas::matrix_expression<MatrExprModel<T>>, UGrid<T>
{
	typedef boost::numeric::ublas::container_reference<MatrExprModel> closure_type;

	MatrExprModel(SizeT n1, SizeT n2, typename UGrid<T>::data_ptr data) : UGrid(UGridPoint(n1, n2), data) {}

	void insert_element(SizeT i, SizeT j, T t) { * this->elemptr(UGridPoint(i, j)) = t; }
	void erase_element (SizeT i, SizeT j)      { * this->elemptr(UGridPoint(i, j)) = 0; }
};

#endif //  !defined(__CLC_BOOSTBLAS_H)