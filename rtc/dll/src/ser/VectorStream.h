//<HEADER> // Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

#if !defined(__SER_VECTORSTREAM_H)
#define __SER_VECTORSTREAM_H

#include "dbg/Check.h"
#include "geo/Conversions.h"
#include "geo/SequenceTraits.h"
#include "geo/Undefined.h"
#include "ptr/IterCast.h"
#include "ser/FormattedStream.h"
#include "ser/PointStream.h"
#include "ser/PolyStream.h"
#include "ser/StreamTraits.h"
#include "set/VectorFunc.h"
#include "xct/DmsException.h"
#include "ptr/IterCast.h"

//======================== Decl

template <int N, typename Block> struct bit_iterator;

//======================== ReadBinRange

template <typename InpStream, typename T> inline
void ReadBinRangeImpl(InpStream& ar, T* first, T* last, const directcpy_streamable_tag*)
{
	ar.Buffer().ReadBytes(reinterpret_cast<Byte*>(first), ThrowingConvert<SizeT>(sizeof(T) * (last-first)));
}

template <typename InpStream, typename FwdIt> inline
void ReadBinRangeImpl(InpStream& ar, FwdIt first, FwdIt last, const polymorph_streamable_tag*)
{
	while (first != last)
		ar >> *first++;
} 

// Specialization for bool
template <typename InpStream, int N, typename B> inline
void ReadBinRangeImpl(InpStream& ar, bit_iterator<N, B> first, bit_iterator<N, B> last, const directcpy_streamable_tag*)
{
	ReadBinRangeImpl(ar, first.data_begin(), last.data_end(), TYPEID(directcpy_streamable_tag));
}

template <typename Vector> inline
void resizeSO(Vector& vec, SizeT len, bool mustDefaultInitialize MG_DEBUG_ALLOCATOR_SRC_ARG)
{
	vec.resizeSO(len, mustDefaultInitialize MG_DEBUG_ALLOCATOR_SRC_PARAM);
}

template <typename Vector> inline
void reallocSO(Vector& vec, SizeT len, bool mustDefaultInitialize MG_DEBUG_ALLOCATOR_SRC_ARG)
{
	vec.reallocSO(len, mustDefaultInitialize MG_DEBUG_ALLOCATOR_SRC_PARAM);
}

template <typename V, typename A> inline
void resizeSO(std::vector<V, A>& vec, SizeT len, bool mustDefaultInitialize MG_DEBUG_ALLOCATOR_SRC_ARG)
{
	vec.resize(len);
}

template <typename V, typename A> inline
void reallocSO(std::vector<V, A>& vec, SizeT len, bool mustDefaultInitialize MG_DEBUG_ALLOCATOR_SRC_ARG)
{
	if (vec.capacity() < len)
		vec = std::vector<V, A>(len);
	else
		vec.resize(len);
}

template <typename InpStream, typename Vector> inline
void ReadBinRange(InpStream& ar, Vector& vec)
{
	typename Vector::size_type len;
	ar >> len;
	if (!IsDefined(len))
		MakeUndefined(vec);
	else
	{
		resizeSO(vec, len, false MG_DEBUG_ALLOCATOR_SRC("ReadBinRange"));

		ReadBinRangeImpl(ar, 
			begin_ptr(vec), 
			end_ptr  (vec), 
			STREAMTYPE_TAG(typename Vector::value_type)
		);
	}
}

//======================== WriteBinRange

template <typename OutStream, typename T> inline
void WriteBinRangeImpl(OutStream& ar, const T* first, const T* last, const directcpy_streamable_tag*)
{
	ar.Buffer().WriteBytes(reinterpret_cast<const Byte*>(first), sizeof(T)*(last-first));
}

template <typename OutStream, typename CFwdIt> inline
void WriteBinRangeImpl(OutStream& ar, CFwdIt first, CFwdIt last, const polymorph_streamable_tag*)
{
	while (first != last)
		ar << *first++;
} 

// Specialization for bool
template <typename OutStream, int N, typename B> inline
void WriteBinRangeImpl(OutStream& ar, bit_iterator<N, B> first, bit_iterator<N, B> last, const directcpy_streamable_tag*)
{
	WriteBinRangeImpl(ar, first.data_begin(), last.data_end(), TYPEID(directcpy_streamable_tag) );
} 


template <typename OutStream, typename Vector> inline
void WriteBinRange(OutStream& ar, const Vector& vec)
{
	if (!IsDefined(vec))
		ar << UNDEFINED_VALUE(typename Vector::size_type);
	else
	{
		ar << vec.size();
		WriteBinRangeImpl(ar, begin_ptr(vec), end_ptr(vec), STREAMTYPE_TAG(typename Vector::value_type));
	}
}

template <typename T, typename A> inline
BinaryInpStream& operator >> (BinaryInpStream& ar, std::vector<T, A>& vec)
{
	ReadBinRange(ar, vec);
	return ar;
}

template <typename T, typename A> inline
BinaryOutStream& operator << (BinaryOutStream& ar, const std::vector<T, A>& vec)
{
	WriteBinRange(ar, vec);
	return ar;
}

// required to do PolymorphInpStream& >> TreeItem* for vector<TreeItem*>
template <typename T, typename A> inline
PolymorphInpStream& operator >> (PolymorphInpStream& ar, std::vector<T, A>& vec)
{
	ReadBinRange(ar, vec);
	return ar;
}

// required to do PolymorphOutStream& << TreeItem* for vector<TreeItem*>
template <typename T, typename A> inline
PolymorphOutStream& operator << (PolymorphOutStream& ar, const std::vector<T, A>& vec)
{
	WriteBinRange(ar, vec);
	return ar;
}

template <typename Vector> inline
void WriteFormattedRange(FormattedOutStream& os, const Vector& vec)
{
	os << '{' << size_t(vec.size()) << ':';
	typename Vector::const_iterator first = vec.begin();
	typename Vector::const_iterator last  = vec.end();
	while (first != last && !os.Buffer().AtEnd())
		os << ' ' << *first++;
	os << "}";
}

template <typename Vector> inline
void ReadFormattedRange(FormattedInpStream& is, Vector& vec)
{
	SizeT len;
	is >> "{" >> len >> ":";
	vec.resize(len, typename Vector::value_type());
	typename Vector::iterator first = vec.begin();
	typename Vector::iterator last  = vec.end();
	while (first != last && !is.Buffer().AtEnd())
		is >> *first++;
	is >> "}";
}

template <typename T, typename A> inline
FormattedOutStream& operator << (FormattedOutStream& os, const std::vector<T, A>& vec)
{
	WriteFormattedRange(os, vec);
	return os;
}

template <typename T, typename A> inline
FormattedInpStream& operator >> (FormattedInpStream& is, std::vector<T, A>& vec)
{
	ReadFormattedRange(is, vec);
	return is;
}

template <typename Iter> inline
FormattedOutStream& operator << (FormattedOutStream& os, const IterRange<Iter>& vec)
{
	WriteFormattedRange(os, vec);
	return os;
}

#endif // __SER_VECTORSTREAM_H
