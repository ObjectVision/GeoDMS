
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

#include "AssocTower.h"


template<typename Elem, typename BinAssocFunc, typename IsEmptyFunc> 
void AssocTower<Elem, BinAssocFunc, IsEmptyFunc>::operator |=(Elem&& rgn) // move semantics
{
	if (m_IsEmptyFunc(rgn))
		return;

	for (
		RegionCollection::iterator 
			currRgnPtr = m_Regions.begin(),
			lastRgnPtr = m_Regions.end();
		currRgnPtr != lastRgnPtr;
		++currRgnPtr
		)
	{
		if (m_IsEmptyFunc(*currRgnPtr))
		{
			m_Regions.move_and_clear(currRgnPtr, &rgn);
			goto checkPostConditions;
			return;
		}
		m_BinAssocFunc(rgn, *currRgnPtr);

		m_Regions.clear_value(currRgnPtr);
		dms_assert(m_IsEmptyFunc(*currRgnPtr));
	}
	m_Regions.push_back(std::move(rgn));

checkPostConditions: // post-conditions
	dms_assert(!m_Regions.empty());
	dms_assert(!m_IsEmptyFunc(m_Regions.back()));
	dms_assert(m_IsEmptyFunc(rgn));
}

template<typename Elem, typename BinAssocFunc, typename IsEmptyFunc> 
Elem AssocTower<Elem, BinAssocFunc, IsEmptyFunc>::GetResult()
{
	if (m_Regions.empty())
		return Elem();

	dms_assert(!m_IsEmptyFunc(m_Regions.back())); // class invariant 

	while (m_Regions.size() > 1)
	{
		if (m_IsEmptyFunc(m_Regions.end()[-2]))
			m_Regions.erase(m_Regions.end()-2);
		else
		{
			m_BinAssocFunc(m_Regions.end()[-2],  m_Regions.back());
			m_Regions.pop_back();
		}
	}
	Elem rgn = std::move(m_Regions.front());
	m_Regions.pop_back();
	dms_assert(m_Regions.empty());
	return rgn; // std::move(rgn); REMOVE see which destructor gets called here
}
