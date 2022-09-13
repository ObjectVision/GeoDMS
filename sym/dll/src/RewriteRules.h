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
#ifndef __SYM_REWRITERULES_H
#define __SYM_REWRITERULES_H

#include "Assoc.h"

/**************** RewriteRules with some type security *****************/

typedef AssocPtr RewriteRulePtr;
typedef Assoc    RewriteRule;

bool CompareRewriteRules(RewriteRulePtr lhs, RewriteRulePtr rhs)
{
	return lhs.Key().Left() < rhs.Key().Left();
}

class RewriteRuleSet
{
	typedef	std::vector<RewriteRule> RewriteRuleVector; 

public:
	typedef RewriteRuleVector::iterator       iterator;
	typedef RewriteRuleVector::const_iterator const_iterator;

//	AssocList() {};	//	creates an empty AssocList
	RewriteRuleSet(AssocListPtr rewriteRuleList)
	{
		dms_assert(!rewriteRuleList.IsFailed());
		m_Data.reserve(rewriteRuleList.Length());
		while (!rewriteRuleList.IsEmpty())
		{
			RewriteRulePtr rule = rewriteRuleList.Head();
			m_Data.push_back(rule);
			rewriteRuleList = rewriteRuleList.Tail();
		}
		dms_assert(m_Data.size() == m_Data.capacity());
		std::stable_sort(m_Data.begin(), m_Data.end(), CompareRewriteRules);
	}

	// new methods
	const_iterator FindLowerBoundByFuncName(LispPtr funcName) const
	{
		const_iterator
			lb = m_Data.begin();
		std::size_t n = m_Data.size();
		while (n)
		{
			std::size_t n2 = n / 2;
			const_iterator
				mb = lb + n2;
			if (mb->Key().Left() < funcName)
				lb = mb+1, n -= (n2 +1);
			else
				n = n2;
		}
		return lb;
	} 

	const_iterator End() const { return m_Data.end(); }

	void swap(RewriteRuleSet& oth) { m_Data.swap(oth.m_Data); }

private:
	RewriteRuleVector m_Data;

friend void swap(RewriteRuleSet& a, RewriteRuleSet& b) { a.swap(b); }
};

#endif // __SYM_REWRITERULES_H
