// Copyright (C) 1998-2024 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "ShvDllPch.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)


#include "RegionTower.h"

void RegionTower::Add(Region&& rgn) // move semantics
{
	if (rgn.Empty())
		return;
	for (
		RegionCollection::iterator 
			currRgnPtr = m_Regions.begin(),
			lastRgnPtr = m_Regions.end();
		currRgnPtr != lastRgnPtr;
		++currRgnPtr
		)
	{
		if (!currRgnPtr->GetHandle())
		{
			currRgnPtr->swap(rgn);
			dms_assert(rgn.Empty()); // post-condition
			return;
		}
		rgn |= *currRgnPtr;
		*currRgnPtr = Region();
	}
	m_Regions.push_back(std::move(rgn));
	assert(rgn.Empty()); // post-condition
}

Region RegionTower::GetResult()
{
	if (m_Regions.empty())
		return Region();
	while (m_Regions.size() > 1)
	{
		m_Regions[m_Regions.size()-2] |= m_Regions.back();
		m_Regions.pop_back();
	}
	Region rgn = std::move(m_Regions.front());
	m_Regions.pop_back();
	assert(m_Regions.empty());
	return rgn;
}
