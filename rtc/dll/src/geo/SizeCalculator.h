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

#if !defined(__RTC_GEO_SIZECALCULATOR_H)
#define __RTC_GEO_SIZECALCULATOR_H

#include "geo/SequenceTraits.h"

//----------------------------------------------------------------------
// Section      : capacity_calculator
//----------------------------------------------------------------------

template <typename T>
struct capacity_calculator
{
	SizeT Byte2Size(std::size_t nrBytes) const
	{
		assert(nrBytes % sizeof(T) == 0);
		return nrBytes / sizeof(T);
	}
};

template <bit_size_t N>
struct capacity_calculator<bit_value<N> >
{
	SizeT Byte2Size(std::size_t nrBytes) const
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
	typedef typename sequence_traits<T>::block_type block_type;
	constexpr SizeT       nr_blocks(SizeT nrElems) const { return nrElems; }
	constexpr std::size_t nr_bytes(SizeT nrElems) const { return std::size_t(nr_blocks(nrElems)) * sizeof(block_type); }
	constexpr SizeT       max_elems(std::size_t nrBytes) const { return nrBytes / sizeof(T); }
};

template <> struct size_calculator<bool> {}; // PREVENT USING bool AS T

template <int N>
struct size_calculator<bit_value<N> >
{
	typedef typename sequence_traits<bit_value<N> >::block_type     block_type;

	SizeT       nr_blocks(SizeT nrElems) const { return bit_info<N, block_type>::calc_nr_blocks( nrElems ); }
	std::size_t nr_bytes (SizeT nrElems) const { return std::size_t(nr_blocks( nrElems )) * sizeof(block_type); }
	SizeT       max_elems(std::size_t nrBytes) const { return capacity_calculator<bit_value<N> >().Byte2Size(nrBytes); }
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
