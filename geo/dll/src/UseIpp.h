// Copyright (C) 1998-2026 Object Vision B.V. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif


#if !defined(__GEO_USE_IPP_H)
#define __GEO_USE_IPP_H


#include "ippdefs.h"

#include "GeoBase.h"

//----------------------------------------------------------------------

GEO_CALL void DmsIppCheckResult(IppStatus status, CharPtr func, CharPtr file, int line);


#endif // __GEO_USE_IPP_H
