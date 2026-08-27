// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////
#pragma once

#if !defined(__TIC_CHECKEDDOMAIN_H)
#define __TIC_CHECKEDDOMAIN_H

//----------------------------------------------------------------------
// used modules and forward class references
//----------------------------------------------------------------------


#include "mci/CompositeCast.h"
#include "mci/ValueWrap.h"
#include "mci/ValueClass.h"

#include "AbstrDataItem.h"
#include "Unit.h"
#include "UnitClass.h"

//----------------------------------------------------------------------
// function checked_domain
//----------------------------------------------------------------------

inline CharPtr ExpandRole(CharPtr role)
{
	MG_CHECK(role);
	if (*role == 'a') switch (role[1])
	{
	case '1': return "first argument";
	case '2': return "second argument";
	case '3': return "third argument";
	case '4': return "fourth argument";
	case '5': return "fifth argument";
	}
	return role;
}

template <typename D>
const Unit<D>* checked_domain(const TreeItem* ti, CharPtr role)
{
	dms_assert(ti);
	const AbstrDataItem* adi = checked_cast<const AbstrDataItem*>(ti);
	const AbstrUnit*     au  = adi->GetAbstrDomainUnit();
	dms_assert(au);
	const Unit<D>* u = const_unit_dynacast<D>(au);
	if (!u) 
		ti->throwItemErrorF("{} attribute with Domain of type {} expected, but Domain is of type {}"
		,	ExpandRole(role)
		,	Unit<D>::GetStaticClass()->GetName().c_str()
		,	au->GetClsName().c_str()
		);
	return u;
}


#endif // __TIC_CHECKEDDOMAIN_H
