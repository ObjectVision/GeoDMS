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