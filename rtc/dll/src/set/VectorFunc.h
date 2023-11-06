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

#if !defined(__RTC_SET_VECTORFUNC_H)
#define __RTC_SET_VECTORFUNC_H

#include <vector>

#include "dbg/Check.h"
#include "geo/BaseBounds.h"
#include "set/rangefuncs.h"

#include <algorithm>

//======================================= 
// Range functions
//======================================= 

template <typename ConstIter>
typename std::iterator_traits<ConstIter>::value_type
LowerBoundFromSequence(ConstIter first, ConstIter last)
{
	typedef typename std::iterator_traits<ConstIter>::value_type value_type;
	value_type accumulator = MAX_VALUE(value_type);
	for(;first != last; ++first)
		MakeLowerBound(accumulator,*first);
	return accumulator;
}

template <typename ConstIter>
typename std::iterator_traits<ConstIter>::value_type
UpperBoundFromSequence(ConstIter first, ConstIter last)
{
	typedef typename std::iterator_traits<ConstIter>::value_type value_type;
	value_type accumulator = MIN_VALUE(value_type);
	for (;first != last; ++first)
		MakeUpperBound(accumulator,*first);
	return accumulator;
}

template <typename ConstIter>
Range<typename field_of<typename std::iterator_traits<ConstIter>::value_type>::type>
RangeFromSequence_SkipUndefined(ConstIter first, ConstIter last)
{
	Range<typename field_of<typename std::iterator_traits<ConstIter>::value_type>::type> result;
	for(;first != last; ++first)
		if (IsDefined(*first))
			result |= *first;
	return result;
//	The following code only works if elements are one-dimensional and LowerBound provides a complete ordening
//	std::pair<ConstIter, ConstIter> result = minmax_element(first, last);
//	return Range<typename boost::pointee<ConstIter>::type>(*result.first, *result.second);
}

template <typename ConstIter>
Range<typename field_of<typename std::iterator_traits<ConstIter>::value_type>::type>
RangeFromSequence(ConstIter first, ConstIter last)
{
	Range<typename field_of<typename std::iterator_traits<ConstIter>::value_type>::type> result;
	for(;first != last; ++first)
		result |= *first;
	return result;
//	The following code only works if elements are one-dimensional and LowerBound provides a complete ordening
//	std::pair<ConstIter, ConstIter> result = minmax_element(first, last);
//	return Range<typename boost::pointee<ConstIter>::type>(*result.first, *result.second);
}




template <typename InPtr, typename OutPtr>
inline OutPtr convert_copy(InPtr si, InPtr se, OutPtr di)
{
	for(; si != se; ++si, ++di)
		*di = Convert<typename std::iterator_traits<OutPtr>::value_type>(*si);
	return di;
}

template <typename InPtr, typename OutPtr, typename Functor>
inline OutPtr convert_copy(InPtr si, InPtr se, OutPtr di, Functor functor)
{
	for(; si != se; ++si, ++di)
		*di = functor( Convert<typename std::iterator_traits<OutPtr>::value_type>(*si) );
	return di;
}

template <typename FI>
void span_fill_sequential_index_numbers(FI first, FI last)
{
	SizeT count = 0;
	for(; first != last; ++ first, ++count)
		*first = count;
}
template <typename Container>
void insert_sequential_index_numbers(Container& c, SizeT n)
{
	assert(c.size() == 0);
	SizeT count = 0;
	c.reserve(n);
	while (count != n)
		c.emplace_back(count++);
}

//=======================================
// Vector functions
//=======================================

#ifdef DMS_64
#define DBG_MAX_ALLOC (UInt64(600) * 1000 * 1000 * 1000)
#else
#define DBG_MAX_ALLOC (600 * 1000 * 1000)
#endif

inline void CheckAllocSize(UInt32 n, UInt32 objSize, CharPtr context)
{
	if (n > (DBG_MAX_ALLOC / objSize))
		throwErrorF(
			"Memory","%s: Operation requested the allocation of %u elements of %u bytes,\n"
			"which exceeds the allocation limit of %I64u bytes.",
			context, n, objSize, 
			(UInt64)DBG_MAX_ALLOC
		);
}


/*
	vector_resize:
		assures that vec will have size n. and
		deletes extra elements or pushes back zero_values
*/

template <typename Vector>
inline void vector_cut(Vector& vec, typename Vector::size_type  n)
{
	dms_assert(vec.size() >= n);
	if (vec.size() > n)
	{
		if (vec.capacity() / 2 >= n)
		{
			Vector tmp(vec.begin(), vec.begin()+n);
			vec.swap(tmp);
		}
		else
			vec.erase(vec.begin()+n,vec.end());
	}
	dms_assert(vec.size() == n);
}

template <typename Vector>
void vector_resize(Vector& vec, typename Vector::size_type  n)
{
	CheckAllocSize(n, sizeof(Vector::value_type), "vector_resize");
	if (vec.size() > n)
		vector_cut(vec, n);
	else if (vec.size() < n)
		vec.resize(n); // use default
	dms_assert(vec.size() == n);
}

template <typename Vector>
void vector_resize_uninitialized(Vector& vec, typename Vector::size_type n)
{
	CheckAllocSize(n, sizeof(Vector::value_type), "vector_resize");
	if (vec.size() > n)
		vector_cut(vec, n);
	else if (vec.size() < n)
		vec.resize_uninitialized(n); // use default
	dms_assert(vec.size() == n);
}

template <typename Vector>
void vector_resize(Vector& vec, typename Vector::size_type  n, typename Vector::const_reference zero_value)
{
	CheckAllocSize(n, sizeof(Vector::value_type), "vector_resize");
	if (vec.size() > n)
		vector_cut(vec, n);
	else if (vec.size() < n)
		vec.resize(n, zero_value);
	dms_assert(vec.size() == n);
}

template <typename Vector>
void vector_resize(Vector& vec, typename Vector::size_type  n, Undefined)
{
	CheckAllocSize(n, sizeof(Vector::value_type), "vector_resize");
	if (vec.size() > n)
		vector_cut(vec, n);
	else if (vec.size() < n)
		vec.resize(n, Undefined());
	dms_assert(vec.size() == n);
}

/*
	vector_clear:
		assures that vec will become empty
*/

template <class Vector>
inline void vector_clear(Vector& vec)
{
	if (vec.size())
	{
		Vector tmp;
		vec.swap(tmp);
	}
}

template <class V>
inline void vector_clear(sequence_obj<V>& vec)
{
	vec.clear();
}

/*
	vector_fill_n:
		assures that vec will have size n. and that
		all n elements will have value fill_value

*/
template <typename Vector>
void vector_fill_n(Vector& vec, typename Vector::size_type n, typename Vector::const_reference fill_value)
{
	dms_assert(n < (DBG_MAX_ALLOC / sizeof(Vector::value_type)) );

	if (vec.capacity() < n || n <= vec.capacity()/2)
	{
		vector_clear(vec);
		vec.insert(vec.end(), n, fill_value);
	}
	else
	{
		if (vec.size() > n)
			vec.erase(vec.begin()+n,vec.end());
		fast_fill(vec.begin(),vec.end(), fill_value);
		if (vec.size() < n)
			vec.insert(vec.end(), n - vec.size(), fill_value);
	}
	dms_assert(vec.size() == n);
}


/*
	vector_zero_n:
		assures that vec will have size n. and that
		all n elements will have default value

*/

template <typename Vector>
void vector_zero_n_reuse(Vector& vec, typename Vector::size_type n)
{
	dms_assert(n < (DBG_MAX_ALLOC / sizeof(Vector::value_type)));

	if (vec.size() > n)
		vec.erase(vec.begin() + n, vec.end());
	fast_zero(vec.begin(), vec.end());
	if (vec.size() < n)
		vec.insert(vec.end(), n - vec.size(), Vector::value_type());

	dms_assert(vec.size() == n);
}

template <typename Vector>
void vector_zero_n(Vector& vec, typename Vector::size_type n)
{
	dms_assert(n < (DBG_MAX_ALLOC / sizeof(Vector::value_type)) );

	if (vec.capacity() < n || n <= vec.capacity() / 2)
		vec = Vector(n);
	else
		vector_zero_n_reuse(vec, n);

	dms_assert(vec.size() == n);
}


/*
	vector_copy:
		copies from any arbitrary random iterator.
	_vector_copy can be used with sequential iterators by supplying n
		for example, the contents of a set can be copied into a vector by providing the set.size()
		in order not to evaluate set.end() - set.begin()

*/

template <typename Vector, typename Iterator>
void _vector_copy(Vector& vec, Iterator first, Iterator last, typename Vector::size_type n)
{
	dms_assert(n < (DBG_MAX_ALLOC / sizeof(Vector::value_type)) );

	if (vec.capacity() < n)
	{
		vector_clear(vec);
		vec.reserve(n);
	}
	else
	{
		if (vec.size() > n)
			vec.erase(vec.begin()+n,vec.end());
	}
	dms_assert(vec.size() <= n && vec.capacity() >= n);

	typename Vector::iterator i = vec.begin(), e = vec.end();
	while (i!=e)
		*i++ = *first++;
	while (first != last)
		vec.push_back(*first++);

	dms_assert(vec.size() == n);
}

template <typename Vector, typename Iterator>
inline void vector_copy(Vector& vec, Iterator first, Iterator last)
{
	_vector_copy(vec, first, last, last - first);
}

template<typename Set, typename Vector>
inline void set2vector(const Set& set, Vector& vec)
{
	_vector_copy(vec, set.begin(), set.end(), set.size());
}

/*
	vector_find:
		returns the ordinal position of object in vec;
		returns UINT_MAX if not found.
		This func doesn;'t create and destruct RefPtr<T>'s as in vector<RefPtr<T>>
		to prevent destruction of non posessed objects by the RefPtr<T> destructor
*/

template <typename Vector, typename T>
inline typename Vector::size_type 
vector_find(const Vector& vec, const T& object, SizeT startPos = 0)
{
	dms_assert(startPos <= vec.size());
	auto p = std::find(vec.begin() + startPos, vec.end(), object);
	return p == vec.end()
		?	UNDEFINED_VALUE(typename Vector::size_type)
		:	p-vec.begin();
}

template <typename Vector, typename Cond>
inline typename Vector::size_type
vector_find_if(const Vector& vec, Cond&& cond, SizeT startPos = 0)
{
	dms_assert(startPos <= vec.size());
	auto p = std::find_if(vec.begin() + startPos, vec.end(), cond);
	return p == vec.end()
		? UNDEFINED_VALUE(typename Vector::size_type)
		: p - vec.begin();
}

template <class Vector> inline typename Vector::value_type
LowerBound(const Vector& vec)
{
	return LowerBoundFromSequence(vec.begin(), vec.end());
}

template <class Vector> inline typename Vector::value_type 
UpperBound(const Vector& vec)
{
	return UpperBoundFromSequence(vec.begin(), vec.end());
}

template <class Vector>
inline SizeT vector_erase(Vector& vec, const typename Vector::value_type& object)
{
	auto e = vec.end(), newE = std::remove(vec.begin(), e, object); 

	SizeT n = e -newE; // # of removed elements
	vec.erase( newE, e);
	return n; 
}

template <class Vector, typename ...Args>
inline bool vector_erase_first(Vector& vec, Args&& ...args)
{
	typename Vector::iterator e = vec.end();
	typename Vector::iterator i = std::find(vec.begin(), e, std::forward<Args>(args)...);

	if (i==e)
		return false;

	vec.erase(i);
	return true;
}

template <class Vector, typename Cond>
inline bool vector_erase_first_if(Vector& vec, Cond&& cond)
{
	typename Vector::iterator e = vec.end();
	typename Vector::iterator i = std::find_if(vec.begin(), e, cond);

	if (i == e)
		return false;

	vec.erase(i);
	return true;
}

template <class Vector>
inline bool vector_erase_last(Vector& vec, const typename Vector::value_type& object)
{
	unsigned int count = 0;
	typename Vector::reverse_iterator e = vec.rend();
	typename Vector::reverse_iterator i = std::find(vec.rbegin(),e,object);

	if (i==e)
		return false;

	vec.erase(i);
	return true;
}

#endif // __RTC_SET_VECTORFUNC_H
