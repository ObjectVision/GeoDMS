// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////
#pragma once

#if !defined(__SHV_GRAPHICCLASS_H)
#define __SHV_GRAPHICCLASS_H

#include "cpc/Types.h"

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
#include "dbg/DebugCast.h" // checked_cast

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
		assert(m_ShvCreateFunc);
		return m_ShvCreateFunc(parent);
	}
	bool HasShvCreator() const { return m_ShvCreateFunc != 0; }

private:
	ShvCreateFunc m_ShvCreateFunc;
};
 
template <typename CLS, typename OWN>
std::shared_ptr<GraphicObject> CreateShvFunc(GraphicObject* owner)
{
	return std::make_shared<CLS>(checked_cast<OWN*>(owner));
}

#define IMPL_SHVCLASS(CLS, CF) \
	const ShvClass* CLS::GetStaticClass() \
	{ \
		static ShvClass s_Cls(CF, CLS::base_type::GetStaticClass(), GetTokenID_st(#CLS) ); \
		return &s_Cls; \
	} 

#define SHV_COMMA() , // defer a comma inside a macro argument (was boost's BOOST_PP_COMMA)

#define IMPL_RTTI_SHVCLASS(OBJ)     IMPL_RTTI (OBJ, ShvClass) IMPL_SHVCLASS(OBJ, nullptr)
#define IMPL_DYNC_SHVCLASS(OBJ,OWN) IMPL_RTTI(OBJ, ShvClass)  IMPL_SHVCLASS(OBJ, CreateShvFunc<OBJ SHV_COMMA() OWN>)

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

	DECL_RTTI(, MetaClass)
};

#define IMPL_LAYERCLASS(CLS, CFUNC, ASPECTSET, MAINASPECT, NR_DIMS) \
	const LayerClass* CLS::GetStaticClass() \
	{ \
		static LayerClass s_Cls(CFUNC, CLS::base_type::GetStaticClass(), GetTokenID_st(#CLS), AspectNrSet(ASPECTSET), MAINASPECT, NR_DIMS); \
		return &s_Cls; \
	} 

#define IMPL_DYNC_LAYERCLASS(CLS, ASPECTSET, MAINASPECT, NR_DIMS) \
	IMPL_RTTI(CLS, LayerClass) IMPL_LAYERCLASS(CLS, CreateShvFunc<CLS SHV_COMMA() GraphicObject>, ASPECTSET, MAINASPECT, NR_DIMS)


#endif // __SHV_GRAPHICCLASS_H

