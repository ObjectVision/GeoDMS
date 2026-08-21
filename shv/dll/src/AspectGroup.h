// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////

#if !defined(__SHV_ASPECTGROUP_H)
#define __SHV_ASPECTGROUP_H

#include "Aspect.h"

struct AspectGroupData
{
	typedef void(*MenuFillFunc)(GraphicLayer*, AspectGroup, MenuData&);

	CharPtr       name;
	AspectNrSet   aspects;
	MenuFillFunc  menuFunc;
};

extern AspectGroupData AspectGroupArray[];

TokenID GetAspectGroupNameID(AspectGroup ag);


#endif // __SHV_ASPECTGROUP_H
