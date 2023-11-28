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
#pragma once

#if !defined(__STGIMPL_VIEWPORTINFO_H)
#define __STGIMPL_VIEWPORTINFO_H

#include "ImplMain.h"

// *****************************************************************************
// ViewPortInfo
// *****************************************************************************

#include "geo/CheckedCalc.h"
#include "geo/Transform.h"
#include "geo/Geometry.h"

typedef long countcolor_t;

template<typename SignedType>
struct ViewPortInfo : CrdTransformation
{
	typedef SignedType signed_type;
	typedef typename unsigned_type<SignedType>::type unsigned_type;
	typedef Point<signed_type> point_type;
	typedef Point<unsigned_type> upoint_type;
	typedef Range<point_type> rect_type;

	SizeT GetNrViewPortCells() const { return Cardinality(m_ViewPortExtents); }
	const rect_type&  GetViewPortExtents() const { return m_ViewPortExtents; }
	const point_type& GetViewPortOrigin () const { return m_ViewPortExtents.first; }
	      upoint_type  GetViewPortSize   () const { return Size(m_ViewPortExtents); }
		  DRect   GetViewPortInGrid() const  { return this->Apply( Convert<DRect>(m_ViewPortExtents) ); }
		  rect_type GetViewPortInGridAsIRect() const  { return Round<sizeof(signed_type)>(GetViewPortInGrid()); }

	countcolor_t GetCountColor() const { return m_CountColor;  }
	StorageMetaInfoPtr m_smi = nullptr;

protected:
	rect_type    m_ViewPortExtents; // [in ViewPort units]
	countcolor_t m_CountColor = -1;  // color to be counted
};

using StgViewPortInfo = ViewPortInfo<Int32>;
using ClcViewPortInfo = ViewPortInfo<Int64>;

#endif // __STGIMPL_VIEWPORTINFO_H
