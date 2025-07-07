// Copyright (C) 1998-2025 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "ShvDllPch.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

#include "LayerClass.h"

//----------------------------------------------------------------------
// class  : LayerClass
//----------------------------------------------------------------------

static const LayerClass* s_First = 0;

LayerClass::LayerClass(
	ShvCreateFunc cFunc, 
	const Class*  baseCls, 
	TokenID       typeID, 
	AspectNrSet   possibleAspects,
	AspectNr      mainAspect,
	DimType       nrDims
)	:	ShvClass(cFunc, baseCls, typeID )
	,	m_PossibleAspects(possibleAspects)
	,	m_NrDims(nrDims)
	,	m_MainAspect(mainAspect)
{
}



IMPL_RTTI_METACLASS(LayerClass, "GRAPHIC", nullptr);

