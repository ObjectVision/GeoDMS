// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

/*
 *  data_ptr_traits<Seq>: raw-pointer access to the contiguous storage
 *  behind sequence types.
 */

#if !defined(__VT_DATAPTRTRAITS_H)
#define __VT_DATAPTRTRAITS_H


template <typename Seq> struct data_ptr_traits {};

template <typename E>
struct data_ptr_traits<sequence_array_ref<E> >
{
	static Byte* begin(sequence_array_ref<E>) { throwIllegalAbstract(MG_POS, "data_begin"); }
	static Byte* end(sequence_array_ref<E>) { throwIllegalAbstract(MG_POS, "data_end"); }
};

template <typename E>
struct data_ptr_traits<sequence_array_cref<E> >
{
	static const Byte* begin(sequence_array_cref<E>) { throwIllegalAbstract(MG_POS, "data_begin"); }
	static const Byte* end(sequence_array_cref<E>) { throwIllegalAbstract(MG_POS, "data_end"); }
};

template <int N, typename Block>
struct data_ptr_traits<bit_sequence<N, Block> >
{
	static Byte* begin(typename sequence_traits<bit_value<N> >::seq_t s) { return reinterpret_cast<Byte*>(s.data_begin()); }
	static Byte* end(typename sequence_traits<bit_value<N> >::seq_t s) { return reinterpret_cast<Byte*>(s.data_end()); }
};

template <int N, typename Block>
struct data_ptr_traits<bit_sequence<N, const Block> >
{
	static const Byte* begin(typename sequence_traits<bit_value<N> >::cseq_t cs) { return reinterpret_cast<const Byte*>(cs.data_begin()); }
	static const Byte* end(typename sequence_traits<bit_value<N> >::cseq_t cs) { return reinterpret_cast<const Byte*>(cs.data_end()); }
};

template <typename V>
struct data_ptr_traits< IterRange<const V*> >
{
	static const Byte* begin(IterRange<const V*> cs)
	{
		return reinterpret_cast<const Byte*>(cs.begin());
	}
	static const Byte* end(IterRange<const V*> cs)
	{
		return reinterpret_cast<const Byte*>(cs.end());
	}
};

template <typename V>
struct data_ptr_traits< IterRange<V*> >
{
	static Byte* begin(IterRange<V*> s)
	{
		return reinterpret_cast<Byte*>(s.begin());
	}
	static Byte* end(IterRange<V*> s)
	{
		return reinterpret_cast<Byte*>(s.end());
	}
};


#endif // !defined(__VT_DATAPTRTRAITS_H)
