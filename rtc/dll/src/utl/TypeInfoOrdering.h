// Copyright (C) 2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#pragma once

#if !defined(__RTC_UTL_TYPEINFOORDERING_H)
#define __RTC_UTL_TYPEINFOORDERING_H

#include "cpc/Types.h"
#include <typeinfo>

struct TypeInfoOrdering 
{
   RTC_CALL bool operator () (const std::type_info* a, const std::type_info* b) const;
};

RTC_CALL CharPtr GetName(const std::type_info& ti);

#endif // __RTC_UTL_TYPEINFOORDERING_H
