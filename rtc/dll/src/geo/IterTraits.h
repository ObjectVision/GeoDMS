// Copyright (C) 2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#pragma once

#if !defined(__RTC_GEO_ITER_TRAITS_H)
#define __RTC_GEO_ITER_TRAITS_H

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

template <typename Iter> //, typename std::enable_if < std::is_class_v<Iter>>::type* =nullptr >
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


#endif // !defined(__RTC_GEO_ITER_TRAITS_H)
