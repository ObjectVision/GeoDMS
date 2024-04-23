// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

#if !defined(__GEO_SPATIALBASE_H)
#define  __GEO_SPATIALBASE_H

#include "GeoBase.h"

#include "mem/Grid.h"
#define MG_DEBUG_DISTRICT false

// *****************************************************************************
//											INTERFACE
// *****************************************************************************

typedef UInt16 RadiusType;
typedef Int16  FormType;   // signed version of RadiusType to support all 4 quadrants
typedef UInt32 SizeType;

using FormPoint = Point< FormType>;
using FormRect = Range<FormPoint>;


#endif //!defined(__GEO_SPATIALBASE_H)
