// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

/*
 *  TypeInfoOrdering: strict-weak ordering of std::type_info pointers (by
 *  before()), plus GetName(type_info) returning the demangled class name
 *  without the "class "/"struct " prefix. Used by shv's DcHandle registry
 *  keyed on element type. Moved here from rtc/utl (2026-08): shv was its
 *  only consumer.
 */

#if !defined(__SHV_TYPEINFOORDERING_H)
#define __SHV_TYPEINFOORDERING_H

#include "cpc/Types.h"
#include <typeinfo>

struct TypeInfoOrdering
{
   bool operator () (const std::type_info* a, const std::type_info* b) const;
};

CharPtr GetName(const std::type_info& ti);

#endif // __SHV_TYPEINFOORDERING_H
