// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////
#ifndef __SYM_REWRITERULES_H
#define __SYM_REWRITERULES_H

#include "Assoc.h"

/**************** RewriteRules with some type security *****************/

using RewriteRulePtr = AssocPtr;
using RewriteRule = Assoc;

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
		assert(!rewriteRuleList.IsFailed());
		m_Data.reserve(rewriteRuleList.Length());
		while (!rewriteRuleList.IsEmpty())
		{
			RewriteRulePtr rule = rewriteRuleList.Head();
			m_Data.emplace_back(rule);
			rewriteRuleList = rewriteRuleList.Tail();
		}
		assert(m_Data.size() == m_Data.capacity());
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
