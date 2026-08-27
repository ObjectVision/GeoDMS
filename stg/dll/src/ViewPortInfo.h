// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////
#pragma once

#if !defined(__STGIMPL_VIEWPORTINFO_H)
#define __STGIMPL_VIEWPORTINFO_H

#include "ImplMain.h"

// *****************************************************************************
// ViewPortInfo
// *****************************************************************************

#include "vt/CheckedCalc.h"
#include "geom/Transform.h"
#include "geom/Geometry.h"

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
