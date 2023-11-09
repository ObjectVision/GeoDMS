// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

#ifndef __GEO_GEOBASE_H
#define __GEO_GEOBASE_H

#include "ClcBase.h"

#if defined(GEO_EXPORTS)
#	define GEO_CALL __declspec(dllexport)
#elif GEO_STATIC
#	define GEO_CALL
#else
#	define GEO_CALL __declspec(dllimport)
#endif

#if defined(MDL_EXPORTS)
#	define MDL_CALL GEO_CALL
#else
#	define MDL_CALL
#endif

#endif