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

#if !defined(__RTC_GEO_SEQUENCE_TRAITS_H)
#define __RTC_GEO_SEQUENCE_TRAITS_H

//=======================================
// Wrapping of myown facilities library
//=======================================

#include "geo/Geometry.h"
#include "geo/BitValue.h"

//#include "set/BitVector.h"

template <typename E>
struct is_dms_sequence : std::false_type {};

template <typename P>
struct is_dms_sequence<SA_Reference<P>> : std::true_type {};

template <typename P>
struct is_dms_sequence<std::vector<P>> : std::true_type {};


template <typename T> struct sequence_obj;
template <typename T> struct OwningPtrSizedArray;

//=======================================
// refval_of
//=======================================

template <typename T> struct refval_of                 { typedef T& type; };
template <>           struct refval_of<SharedStr>      { typedef SA_Reference<char> type; };
template <typename V> struct refval_of<std::vector<V> > { typedef SA_Reference<V> type; };

template <typename T> struct crefval_of                 { typedef T type; };
template <>           struct crefval_of<SharedStr>      { typedef SA_ConstReference<char> type; };
template <typename V> struct crefval_of<std::vector<V> > { typedef SA_ConstReference<V> type; };


//=======================================
// sequence_traits
//=======================================

template <typename T>
struct sequence_traits
{ 
	typedef T                        value_type;
	typedef T                        block_type;
	typedef std::vector<T>           container_type;
	typedef OwningPtrSizedArray<T>   tile_container_type;

	typedef T*                       pointer;
	typedef const T*                 const_pointer;

	typedef T&                       reference;
	typedef const T&                 const_reference;

 	typedef IterRange<pointer>       seq_t;
 	typedef IterRange<const_pointer> cseq_t;

	typedef sequence_obj<T>          polymorph_vec_t;
};


template <bit_size_t N>
struct sequence_traits<bit_value<N> >
{ 
	typedef bit_value<N>                        value_type;
	typedef bit_block_t                         block_type;
	typedef std::allocator<block_type>          allocator;

	typedef BitVector<N, block_type, allocator> container_type;
	typedef container_type                      tile_container_type;

	typedef bit_iterator<N, block_type>         pointer;
	typedef bit_iterator<N, const block_type>   const_pointer;

	typedef bit_reference<N, block_type>        reference;
	typedef value_type                          const_reference;

	typedef bit_sequence<N, block_type>         seq_t;
	typedef bit_sequence<N, const block_type>   cseq_t;

	typedef sequence_obj<value_type>            polymorph_vec_t;
};

template <> struct sequence_traits<bool> : sequence_traits<bit_value<1>>{};

#if !defined(MG_USE_VECTOR_CHAR)

template <> struct sequence_traits<char>
{ 
	typedef char                      value_type;
	typedef char                      block_type;

	typedef SharedStr                 container_type;
	typedef OwningPtrSizedArray<char> tile_container_type;

	typedef char*                     pointer;
	typedef const char*               const_pointer;

	typedef char&                     reference;
	typedef const char&               const_reference;

	typedef IterRange<pointer>       seq_t;  // not StringRef  = SA_Reference     <char>
 	typedef IterRange<const_pointer> cseq_t; // not StringCRef = SA_ConstReference<char>

	typedef sequence_obj<char>       polymorph_vec_t;
};

#endif

//=======================================
// sequence_traits of vector<V> (container of vector<V> is sequence_array<V>)
//=======================================

template <typename V>
struct sequence_traits<std::vector<V> >
{
	//	typedef value_type rebuilds V according to:
	//	vector<V=T>          -> vector<T, pd_alloc>
#if !defined(MG_USE_VECTOR_CHAR)
	//	vector<V=char>       -> SharedStr
#else
	//	SharedStr            -> vector<V=char>
#endif
	//	vector<V=vector<T> > -> sequence_array<T>
	typedef std::vector<V>*        pointer;
	typedef const std::vector<V>*  const_pointer;
	typedef std::vector<V>&        reference;
	typedef const std::vector<V>&  const_reference;

	typedef typename sequence_traits<V>::container_type value_type;

	typedef sequence_vector<V>     container_type;
	typedef sequence_array<V>      polymorph_vec_t;
	typedef container_type         tile_container_type;

	typedef sequence_array_ref<V>  seq_t;
	typedef sequence_array_cref<V> cseq_t;
};

template <typename V>
struct sequence_traits<SA_Reference<V> >
	:	sequence_traits<typename sequence_traits<V>::container_type> {};

template <typename V>
struct sequence_traits<SA_ConstReference<V> >
	:	sequence_traits<typename sequence_traits<V>::container_type> {};


//=======================================
// sequence_traits of SharedStr(container of String is sequence_array<char>)
//=======================================

template <> struct sequence_traits<SharedStr>
{
	typedef SharedStr value_type;
//RMOVE	typedef sequence_traits<char>::container_type value_type;
	typedef StringVector                          container_type; 
	typedef container_type                        tile_container_type;
	typedef StringArray                           polymorph_vec_t;

	typedef sequence_array_ref <char> seq_t;
	typedef sequence_array_cref<char> cseq_t;
};

template <> struct sequence_traits< StringRef > : sequence_traits<SharedStr> {};
template <> struct sequence_traits< StringCRef> : sequence_traits<SharedStr> {};

//----------------------------------------------------------------------
// Section      : iter_creator
//----------------------------------------------------------------------

template <typename T>
struct iter_creator
{
	typedef typename sequence_traits<T>::pointer       iterator;
	typedef typename sequence_traits<T>::const_pointer const_iterator;

	template <typename U> iterator       operator () (      U* bytePtr, SizeT n) { return reinterpret_cast<      iterator>(bytePtr)+n; }
	template <typename U> const_iterator operator () (const U* bytePtr, SizeT n) { return reinterpret_cast<const_iterator>(bytePtr)+n; }
};

template <> struct iter_creator<bool> {}; // PREVENT using bool as T

template <int N>
struct iter_creator<bit_value<N> >
{
	typedef typename sequence_traits<bit_value<N> >::pointer       iterator;
	typedef typename sequence_traits<bit_value<N> >::const_pointer const_iterator;
	typedef typename sequence_traits<bit_value<N> >::block_type    block_type;
	template <typename U> iterator       operator () (      U* bytePtr, SizeT n) { return       iterator(reinterpret_cast<      block_type*>(bytePtr), n); }
	template <typename U> const_iterator operator () (const U* bytePtr, SizeT n) { return const_iterator(reinterpret_cast<const block_type*>(bytePtr), n); }
};

//=======================================
// get sizetype for an iterator to dms sequences
//=======================================

template <typename Iter>
struct sizetype_of_iterator
{
	typedef SizeT type;
};

//=======================================
// get difftype for an iterator to dms sequences
//=======================================

template <typename Iter>
struct difftype_of_iterator
{
	typedef DiffT type;
};

//=======================================
// get reference type for an iterator
//=======================================

template <typename Iter>
struct ref_type_of_iterator
{
	typedef typename Iter::reference type;
};

template <typename T>
struct ref_type_of_iterator<T*>
{
	typedef T& type;
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


#endif // !defined(__RTC_GEO_SEQUENCE_TRAITS_H)
