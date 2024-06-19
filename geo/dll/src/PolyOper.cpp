// Copyright (C) 1998-2024 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "GeoPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif

#include "PolyOper.h"


// *****************************************************************************
//											INSTANTIATION helpers
// *****************************************************************************

template <typename TL, template <typename T> class MetaFunc>
struct BinaryPolyOperInstantiation
{
	using OperTemplate = BinaryPolyAttrAssignOper<MetaFunc<_> >;
	tl_oper::inst_tuple<TL, OperTemplate, AbstrOperGroup*> m_OperList;

	BinaryPolyOperInstantiation(AbstrOperGroup* gr)
		: m_OperList(gr)
	{}

};


// *****************************************************************************
//											INSTANTIATION
// *****************************************************************************
#include "RtcTypeLists.h"

using namespace typelists;

namespace {
	BinaryPolyOperInstantiation<typelists::sint_points, poly_and>  sAndPoly(&cog_bitand), sMulPoly(&cog_mul);
	BinaryPolyOperInstantiation<typelists::sint_points, poly_or >  sOrPoly(&cog_bitor), sAddPoly(&cog_add);
	BinaryPolyOperInstantiation<typelists::sint_points, poly_xor>  sXOrPoly(&cog_pow);
	BinaryPolyOperInstantiation<typelists::sint_points, poly_sub>  sSubPoly(&cog_sub);
//	BinaryPolyOperInstantiation<typelists::sint_points, poly_eq>   sEqPoly (&cog_eq);
//	BinaryPolyOperInstantiation<typelists::sint_points, poly_ne>   sEqPoly (&cog_ne);
} // namespace
