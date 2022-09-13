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