// Copyright (C) 1998-2026 Object Vision B.V. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif


#if !defined(__RTC_TYPEMODEL_H)
#define __RTC_TYPEMODEL_H

//----------------------------------------------------------------------
// element-types control
// (DMS_32 / DMS_64 are defined in cpc/Types.h based on pointer size, so
//  they work portably on Windows-x64, Linux-x64, etc.)
//----------------------------------------------------------------------

#define DMS_TM_HAS_UINT64_AS_DOMAIN
#define DMS_TM_HAS_UINT4
#define DMS_TM_HAS_UINT2
#define DMS_TM_HAS_INT64

#if defined(CC_LONGDOUBLE_80)
#	define DMS_TM_HAS_FLOAT80
#endif

#endif // __RTC_TYPEMODEL_H
