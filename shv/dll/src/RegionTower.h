// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////
#pragma once

#if !defined(__SHV_REGIONTOWER_H)
#define __SHV_REGIONTOWER_H

//----------------------------------------------------------------------
// used modules and forward class references
//----------------------------------------------------------------------

#include "Region.h"

using RegionCollection= std::vector<Region>;

//----------------------------------------------------------------------
// class  : RegionTower
//	purpose: to efficiently collect a large set of simple regions 
//	         by merging them hierarchically in order to minimize merges
//	         of large regions 
//----------------------------------------------------------------------

struct RegionTower
{
	void Add(Region&& rgn); // move semantics

	Region GetResult();
	bool Empty() const { return m_Regions.empty(); }

private:
	RegionCollection m_Regions;
};

#endif // __SHV_REGIONTOWER_H

