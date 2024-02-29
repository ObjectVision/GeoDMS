// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

#if !defined(__GEO_SPATIALINTERFACE_H)
#define  __GEO_SPATIALINTERFACE_H

#include "SpatialBase.h"

extern "C" {

// *****************************************************************************
//	INTERFACE FUNCTIONS: Districting
// *****************************************************************************

MDL_CALL void DMS_CONV MDL_DistrictingI32 (const UCInt32Grid&  input, const UUInt32Grid& output, UInt32* resNrDistricts);
MDL_CALL void DMS_CONV MDL_DistrictingUI32(const UCUInt32Grid& input, const UUInt32Grid& output, UInt32* resNrDistricts);
MDL_CALL void DMS_CONV MDL_DistrictingUI8 (const UCUInt8Grid&  input, const UUInt32Grid& output, UInt32* resNrDistricts);
MDL_CALL void DMS_CONV MDL_DistrictingBool(const UCBoolGrid&   input, const UUInt32Grid& output, UInt32* resNrDistricts);

// *****************************************************************************
//	INTERFACE FUNCTIONS: Select 1 District
// *****************************************************************************

// The following functions are used in Shv for raster region selection
GEO_CALL void DMS_CONV MDL_DistrictUI32(const UCUInt32Grid& input, const sequence_traits<Bool>::seq_t output, const IGridPoint& seedPoint, IGridRect& resRect);
GEO_CALL void DMS_CONV MDL_DistrictUI8 (const UCUInt8Grid&  input, const sequence_traits<Bool>::seq_t output, const IGridPoint& seedPoint, IGridRect& resRect);
GEO_CALL void DMS_CONV MDL_DistrictBool(const UCBoolGrid&   input, const sequence_traits<Bool>::seq_t output, const IGridPoint& seedPoint, IGridRect& resRect);

// *****************************************************************************
//	INTERFACE FUNCTIONS: Diversity
// *****************************************************************************

MDL_CALL void DMS_CONV MDL_DiversityUI32(const UCUInt32Grid& input, UInt32 inputUpperBound, RadiusType radius, bool circle, const UUInt32Grid& output);
MDL_CALL void DMS_CONV MDL_DiversityUI8 (const UCUInt8Grid&  input, UInt8  inputUpperBound, RadiusType radius, bool circle, const UUInt8Grid&  output);

} // extern "C"


#endif //!defined(__GEO_SPATIALINTERFACE_H)
