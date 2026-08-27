// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////
#pragma once
#ifndef __SET_SETFUNC_H
#define __SET_SETFUNC_H 1

#include "Range.h"
#include <set>
#include <algorithm>

template <class Key, class P, class A> 
Range<Key> GetBounds(const std::set<Key, P, A>&  s) // throw (DomainException)
{
	if (s.empty()) 
		GEN raise_throw("Bounds of an empty set not defined", (GEN DomainException*)0);
	std::set<Key, P, A >::const_iterator i = s.begin(), e = s.end();
	Range<Key > result(*i, *i);
	while (++i != e)
	{
		MakeLowerBound(result.first,  *i);
		MakeUpperBound(result.second, *i);
	}
	return result;
}

template <class T, class P, class A> 
std::set<T, P, A> Intersection(const std::set<T, P, A>& left, const std::set<T, P, A>& right)
{
	std::set<T, P, A> result;
	std::set_intersection(left.begin(), left.end(), right.begin(), right.end(), std::inserter(result, result.end()));
	return result;
}

template <class T, class P, class A> inline 
void MakeIntersection(std::set<T, P, A>& left, const std::set<T, P, A>& right)
{
	left = Intersection(left, right);
}

template <class T, class P, class A> inline 
void operator &= (std::set<T, P, A>& left, const std::set<T, P, A>& right)
{
	MakeIntersection(left, right);
}

template <class T, class P, class A> 
std::set<T, P, A> Inclusion(const std::set<T, P, A>& left, const std::set<T, P, A>& right)
{
	std::set<T, P, A> result;
	std::set_union(left.begin(), left.end(), right.begin(), right.end(), std::inserter(result, result.end()));
	return result;
}

template <class T, class P, class A> 
void MakeInclusion(std::set<T, P, A>& left, const std::set<T, P, A>& right)
{
	left = Inclusion(left, right);
}

template <class T, class P, class A> inline 
void operator |= (std::set<T, P, A>& left, const std::set<T, P, A>& right)
{
	MakeInclusion(left, right);
}


#endif // __SET_SETFUNC_H