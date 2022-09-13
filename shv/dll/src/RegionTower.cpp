
//<HEADER> 
/*
Data & Model Server (DMS) is a server written in C++ for DSS applications. 
Version: see srv/dms/rtc/dll/src/RtcVersion.h for version info.

Copyright (C) 1998-2004  YUSE GSO Object Vision BV. 

Documentation on using the Data & Model Server software can be found at:
http://www.ObjectVision.nl/DMS/

See additional guidelines and notes in srv/dms/Readme-srv.txt 

This library is free software; you can use, redistribute, and/or
modify it under the terms of the GNU General Public License version 2 
(the License) as published by the Free Software Foundation,
provided that this entire header notice and readme-srv.txt is preserved.

See LICENSE.TXT for terms of distribution or look at our web site:
http://www.objectvision.nl/DMS/License.txt
or alternatively at: http://www.gnu.org/copyleft/gpl.html

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
General Public License for more details. However, specific warranties might be
granted by an additional written contract for support, assistance and/or development
*/
//</HEADER>

#include "ShvDllPch.h"

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
	dms_assert(rgn.Empty()); // post-condition
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
	dms_assert(m_Regions.empty());
	return rgn;
}
