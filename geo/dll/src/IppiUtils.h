// Copyright (C) 1998-2026 Object Vision B.V. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif


#if !defined(__GEO_IPPIUTILS_H)
#define __GEO_IPPIUTILS_H

#include "IppBase.h"

#if defined(DMS_USE_INTEL_IPPI)

#include "ippi.h"

// *****************************************************************************
//											INTERFACE
// *****************************************************************************

template<typename ExceptFunc, typename ConvertFunc>
inline IppiSize Convert4(const GridPoint& src, const IppiSize*, const ExceptFunc*, const ConvertFunc*)
{
	IppiSize result;
	result.width = src.Col();
	result.height= src.Row();
	return result;
}

template<typename ExceptFunc, typename ConvertFunc>
inline IppiPoint Convert4(const GridPoint& src, const IppiPoint*, const ExceptFunc*, const ConvertFunc*)
{
	IppiPoint result;
	result.x = src.Col();
	result.y = src.Row();
	return result;
}

// *****************************************************************************

#endif //defined(DMS_USE_INTEL_IPPI)

#endif //!defined(__GEO_IPPIUTILS_H)
