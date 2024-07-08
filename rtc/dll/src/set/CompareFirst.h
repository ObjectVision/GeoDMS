// Copyright (C) 1998-2024 Object Vision b.v. 
// License: GNU GPL 3

#if defined(_MSC_VER)
#pragma once
#endif

#if !defined(__SET_COMPAREFIRST_H)
#define __SET_COMPAREFIRST_H

#include <functional>
#include "set/DataCompare.h"

struct CompareFirst
{
	template <typename KeyValueType>
	bool operator () (const KeyValueType& lhs, const KeyValueType& rhs)
	{
		if constexpr (has_undefines_v<typename KeyValueType::first_type>)
		{
			assert(IsDefined(lhs.first));
			assert(IsDefined(rhs.first));
		}

		return lhs.first < rhs.first;
	}
};

template <typename Key, typename KeyValuePair, typename Comp = DataCompare<Key> >
struct comp_first
{
	typedef Comp         key_compare;
	typedef KeyValuePair value_type;

	comp_first(const key_compare& keyCompare = key_compare()) : m_KeyCompare(keyCompare) {}

	bool operator()(const value_type& a, const value_type& b) { return m_KeyCompare(a.first, b.first); }

	bool operator()(const value_type& a, const Key& b) { return m_KeyCompare(a.first, b); }

	bool operator()(const Key& a, const value_type& b) { return m_KeyCompare(a, b.first); }

	key_compare m_KeyCompare;
};


#endif // __SET_COMPAREFIRST_H
