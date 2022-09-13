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
