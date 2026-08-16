// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////

#include "ShvDllPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif

// TypeInfoOrdering + GetName: type_info ordering and naming, see TypeInfoOrdering.h.

#include "TypeInfoOrdering.h"

#include <typeinfo>

bool TypeInfoOrdering ::operator () (const std::type_info* a, const std::type_info* b) const
{
	return 0 != a->before(*b);
}

CharPtr GetName(const std::type_info& ti)
{
	return ti.name();
}
