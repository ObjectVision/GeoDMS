// Copyright (C) 1998-2025 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

#if !defined(__RTC_GEO_SIZECALCULATOR_H)
#define __RTC_GEO_SIZECALCULATOR_H

#include "geo/SequenceTraits.h"

//----------------------------------------------------------------------
// Section      : capacity_calculator
//----------------------------------------------------------------------

template <typename T>
struct capacity_calculator
{
	static SizeT Byte2Size(std::size_t nrBytes)
	{
		assert(nrBytes % sizeof(T) == 0);
		return nrBytes / sizeof(T);
	}
};

template <bit_size_t N>
struct capacity_calculator<bit_value<N> >
{
	static SizeT Byte2Size(std::size_t nrBytes)
	{
		static_assert(sizeof(SizeT) >= sizeof(std::size_t), "SizeT unexpected");
		dms_assert(nrBytes % sizeof(bit_block_t) == 0);
		SizeT nrBlocks = nrBytes / sizeof(bit_block_t);
		dms_assert(nrBytes == nrBlocks * sizeof(bit_block_t));
		SizeT nrElems = nrBlocks * sequence_traits<bit_value<N> >::container_type::nr_elem_per_block;
		MG_CHECK(nrBlocks == nrElems / sequence_traits<bit_value<N> >::container_type::nr_elem_per_block);
		return nrElems;
	}
};

//----------------------------------------------------------------------
// Section      : size_calculator
//----------------------------------------------------------------------

template <typename T>
struct size_calculator
{
	using block_type = typename sequence_traits<T>::block_type;

	constexpr static SizeT       nr_blocks(SizeT nrElems) { return nrElems; }
	constexpr static std::size_t nr_bytes(SizeT nrElems) { return std::size_t(nr_blocks(nrElems)) * sizeof(block_type); }
	constexpr static SizeT       max_elems(std::size_t nrBytes) { return capacity_calculator<T>::Byte2Size(nrBytes); }
};

template <> struct size_calculator<bool> {}; // PREVENT USING bool AS T

template <int N>
struct size_calculator<bit_value<N> >
{
	using block_type = typename sequence_traits<bit_value<N> >::block_type;

	constexpr static SizeT       nr_blocks(SizeT nrElems)       { return bit_info<N, block_type>::calc_nr_blocks( nrElems ); }
	constexpr static std::size_t nr_bytes (SizeT nrElems)       { return std::size_t(nr_blocks( nrElems )) * sizeof(block_type); }
	constexpr static SizeT       max_elems(std::size_t nrBytes) { return capacity_calculator<bit_value<N> >::Byte2Size(nrBytes); }
};

//----------------------------------------------------------------------
// Section      : NrBytesOf
//----------------------------------------------------------------------
#include "ser/BinaryStream.h"

template <typename T>
std::size_t NrBytesOf(const sequence_obj<T>& v, bool calcStreamSize)   { return size_calculator<T>().nr_bytes(v.Size()) + (calcStreamSize ? NrStreamBytesOf(v.Size()): 0); }

template <typename T>
std::size_t NrBytesOf(const IterRange<const T*>& v, bool calcStreamSize) { return size_calculator<T>().nr_bytes(v.size()) + (calcStreamSize ? NrStreamBytesOf(v.size()) : 0); }

template <typename T>
std::size_t NrBytesOf(const IterRange<T*>& v, bool calcStreamSize) { return size_calculator<T>().nr_bytes(v.size()) + (calcStreamSize ? NrStreamBytesOf(v.size()) : 0); }

template <typename E>
std::size_t NrBytesOf(const sequence_array<E>& v, bool calcStreamSize) { return v.nr_bytes(calcStreamSize); }

template <typename E>
std::size_t NrBytesOf(const sequence_array_cref<E>& ref, bool calcStreamSize) { return ref.get_sa().nr_bytes(calcStreamSize); }

template <typename E>
std::size_t NrBytesOf(const sequence_array_ref<E>& ref, bool calcStreamSize) { return ref.get_sa().nr_bytes(calcStreamSize); }

template <int N>
std::size_t NrBytesOf(const bit_sequence<N, bit_block_t>& v, bool calcStreamSize) { return size_calculator<bit_value<N>>().nr_bytes(v.size()) + (calcStreamSize ? NrStreamBytesOf(v.size()) : 0); }

template <int N>
std::size_t NrBytesOf(const bit_sequence<N, const bit_block_t>& v, bool calcStreamSize) { return size_calculator<bit_value<N>>().nr_bytes(v.size()) + (calcStreamSize ? NrStreamBytesOf(v.size()) : 0); }

#endif //!defined(__RTC_GEO_SIZECALCULATOR_H)
