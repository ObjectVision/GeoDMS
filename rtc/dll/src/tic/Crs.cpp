// Copyright (C) 1998-2025 Object Vision b.v.
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "TicPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

#include "Crs.h"

#include "ser/FormattedStream.h"

// *****************************************************************************
// UnitCrs -- see Crs.h and doc/development/crs-metric-decoupling.md
// *****************************************************************************

SharedStr UnitCrs::AsString(FormattingFlags /*ff*/) const
{
	// The spatial reference is an opaque authority string ("EPSG:28992") or WKT as read
	// by GDAL; there is nothing to format, and it must NOT be normalised -- token
	// identity is what makes two separately declared same-CRS units unify.
	return SharedStr(m_SpatialRef);
}

bool AreEqual(const UnitCrs* lhs, const UnitCrs* rhs)
{
	if (lhs == rhs)
		return true;
	bool lhsEmpty = IsEmpty(lhs);
	bool rhsEmpty = IsEmpty(rhs);
	if (lhsEmpty || rhsEmpty)
		return lhsEmpty && rhsEmpty;
	return lhs->m_SpatialRef == rhs->m_SpatialRef;
}

FormattedOutStream& operator <<(FormattedOutStream& str, const UnitCrs& repr)
{
	str << repr.m_SpatialRef;
	return str;
}
