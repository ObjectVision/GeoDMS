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

#if !defined(__SHV_GRAPHICCLASS_H)
#define __SHV_GRAPHICCLASS_H

#include "cpc/types.h"

//----------------------------------------------------------------------
// used modules and forward class references
//----------------------------------------------------------------------

#include "mci/Class.h"

#include "Aspect.h"

class GraphicObject;
class GraphicLayer;

//----------------------------------------------------------------------
// ShvClass
//----------------------------------------------------------------------
#include <boost/cast.hpp>

typedef std::shared_ptr<GraphicObject>(*ShvCreateFunc)(GraphicObject*);


class ShvClass : public Class 
{
public:
	ShvClass(ShvCreateFunc cf, const Class* baseType, TokenID id)
		:	Class(0, baseType, id)
		,	m_ShvCreateFunc(cf) 
	{}

	std::shared_ptr<GraphicObject> CreateShvObj(GraphicObject* parent) const
	{
		dms_assert(m_ShvCreateFunc);
		return m_ShvCreateFunc(parent);
	}
	bool HasShvCreator() const { return m_ShvCreateFunc != 0; }

private:
	ShvCreateFunc m_ShvCreateFunc;
};
 
template <typename CLS, typename OWN>
std::shared_ptr<GraphicObject> CreateShvFunc(GraphicObject* owner)
{
	return std::make_shared<CLS>(boost::polymorphic_cast<OWN*>(owner)); 
}

#define IMPL_SHVCLASS(CLS, CF) \
	const ShvClass* CLS::GetStaticClass() \
	{ \
		static ShvClass s_Cls(CF, CLS::base_type::GetStaticClass(), GetTokenID_st(#CLS) ); \
		return &s_Cls; \
	} 

#include  <boost/preprocessor/punctuation/comma.hpp> 

#define IMPL_RTTI_SHVCLASS(OBJ)     IMPL_RTTI (OBJ, ShvClass) IMPL_SHVCLASS(OBJ, nullptr)
#define IMPL_DYNC_SHVCLASS(OBJ,OWN) IMPL_RTTI(OBJ, ShvClass)  IMPL_SHVCLASS(OBJ, CreateShvFunc<OBJ BOOST_PP_COMMA() OWN>)

//----------------------------------------------------------------------
// class  : LayerClass
//----------------------------------------------------------------------

class LayerClass : public ShvClass
{
private:
	typedef Class base_type;
public:
	LayerClass(
		ShvCreateFunc cFunc, 
		const Class*  baseCls, 
		TokenID       typeID, 
		AspectNrSet   possibleAspects,
		AspectNr      mainAspect,
		DimType       nrDims
	);

	AspectNrSet GetPossibleAspects() const { return m_PossibleAspects; }
	AspectNr    GetMainAspect     () const { return m_MainAspect; }
	DimType     GetNrDims         () const { return m_NrDims;          }

private:
	AspectNrSet m_PossibleAspects;
	AspectNr    m_MainAspect;
	DimType     m_NrDims;

	DECL_RTTI(SHV_CALL, MetaClass);
};

#define IMPL_LAYERCLASS(CLS, CFUNC, ASPECTSET, MAINASPECT, NR_DIMS) \
	const LayerClass* CLS::GetStaticClass() \
	{ \
		static LayerClass s_Cls(CFUNC, CLS::base_type::GetStaticClass(), GetTokenID_st(#CLS), AspectNrSet(ASPECTSET), MAINASPECT, NR_DIMS); \
		return &s_Cls; \
	} 

#define IMPL_DYNC_LAYERCLASS(CLS, ASPECTSET, MAINASPECT, NR_DIMS) \
	IMPL_RTTI(CLS, LayerClass) IMPL_LAYERCLASS(CLS, CreateShvFunc<CLS BOOST_PP_COMMA() GraphicObject>, ASPECTSET, MAINASPECT, NR_DIMS)


#endif // __SHV_GRAPHICCLASS_H

