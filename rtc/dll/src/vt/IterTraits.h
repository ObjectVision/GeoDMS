// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

/*
 *  Iterator traits: size, difference and reference types of iterators
 *  (sizetype_of_iterator and kin).
 */

#if !defined(__VT_ITERTRAITS_H)
#define __VT_ITERTRAITS_H

//=======================================
// get sizetype for an iterator to dms sequences
//=======================================

template <typename Iter>
struct sizetype_of_iterator
{
	using type = SizeT;
};

//=======================================
// get difftype for an iterator to dms sequences
//=======================================

template <typename Iter>
struct difftype_of_iterator
{
	using type = DiffT;
};

//=======================================
// get reference type for an iterator
//=======================================

template <typename Iter>
struct ref_type_of_iterator
{
	using type = typename Iter::reference;
};

template <typename T>
struct ref_type_of_iterator<T*>
{
	using type = T&;
};

//=======================================
// get value_type for an iterator
//=======================================

template <typename Iter>
struct value_type_of_iterator
{
	using type = typename Iter::value_type;
};

template <typename T>
struct value_type_of_iterator<T*>
{
	using type = T;
};

template <typename T>
struct value_type_of_iterator<const T*>
{
	using type = T;
};


#endif // !defined(__VT_ITERTRAITS_H)
