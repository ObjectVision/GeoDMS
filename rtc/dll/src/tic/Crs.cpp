// Copyright (C) 1998-2025 Object Vision b.v.
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "TicPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

#include "Crs.h"

#include "dbg/Diagnostics.h"
#include "ser/FormattedStream.h"

#include <map>
#include <mutex>

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

// *****************************************************************************
// Background-reference registry -- see Crs.h
// *****************************************************************************

namespace {
	// Written from the meta thread while building key expressions, but read from any
	// thread that renders a map layer, so it carries its own mutex. (The side table this
	// project replaces had none, which is one reason it could never hold a property that
	// operators must write.)
	std::mutex                   s_CrsBackgroundMutex;
	std::map<TokenID, SharedStr> s_CrsBackgroundRefs;
}

void RegisterCrsBackgroundRef(TokenID spatialRef, const SharedStr& backgroundRef)
{
	if (spatialRef.empty() || backgroundRef.empty())
		return;

	SharedStr kept;
	{
		auto lock = std::scoped_lock(s_CrsBackgroundMutex);
		auto [it, inserted] = s_CrsBackgroundRefs.try_emplace(spatialRef, backgroundRef);
		if (inserted || it->second == backgroundRef)
			return;
		kept = it->second; // first non-empty wins
	}
	// Report OUTSIDE the lock, and without the cancellation check: reportF calls
	// ASyncContinueCheck, which THROWS when a GUI cancel is pending -- from here that
	// would abort an unrelated config load.
	reportF_without_cancellation_check(SeverityTypeID::ST_Warning
		, "SpatialReference '{}' is declared with two different background layers: keeping '{}', ignoring '{}'. "
		  "The background hint is not part of a unit's type, so units sharing a CRS share one background."
		, SharedStr(spatialRef).c_str(), kept.c_str(), backgroundRef.c_str());
}

SharedStr GetCrsBackgroundRef(TokenID spatialRef)
{
	if (spatialRef.empty())
		return {};
	auto lock = std::scoped_lock(s_CrsBackgroundMutex);
	auto it = s_CrsBackgroundRefs.find(spatialRef);
	return (it == s_CrsBackgroundRefs.end()) ? SharedStr() : it->second;
}

void ClearCrsBackgroundRefs()
{
	auto lock = std::scoped_lock(s_CrsBackgroundMutex);
	s_CrsBackgroundRefs.clear();
}
