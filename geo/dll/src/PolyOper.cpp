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

#include "GeoPCH.h"
#pragma hdrstop

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
