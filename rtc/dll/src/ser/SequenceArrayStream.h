// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#pragma once

#if !defined(__RTC_SER_SEQUENCEARRAYSTREAM_H)
#define __RTC_SER_SEQUENCEARRAYSTREAM_H

#include "geo/SequenceArray.h"
#include "ser/VectorStream.h"

// ====================================== BinaryStream sequences

template <typename Iter>
BinaryInpStream& operator >> (BinaryInpStream& ar, IterRange<Iter> seq)
{
	using value_type = typename IterRange<Iter>::value_type;
	typename IterRange<Iter>::size_type len;
	ar >> len;
	MG_CHECK(IsDefined(len) || !seq.size())
	if (!IsDefined(len))
	{
		MakeUndefined(seq);
	}
	else {
		if (len != seq.size())
			throwErrorF("ReadSequence", "the GeoDms is trying to read %u bytes of data, which does not match the expected size of %u bytes"
				, len
				, seq.size()
			);

		ReadBinRangeImpl(ar, seq.begin(), seq.end(), STREAMTYPE_TAG(value_type));
	}
	return ar;
}

template <class Iter>
BinaryOutStream& operator << (BinaryOutStream& ar, IterRange<Iter> seq)
{
	using value_type = typename IterRange<Iter>::value_type;
	typename IterRange<Iter>::size_type len = seq.size();
	MG_CHECK(IsDefined(len));
	if (!seq.IsDefined() )
		MakeUndefined(len);
	ar << len;
	if (seq.IsDefined())
		WriteBinRangeImpl(ar, seq.begin(), seq.end(), STREAMTYPE_TAG(value_type));
	return ar;
}

template <int N, typename Block>
BinaryInpStream& operator >> (BinaryInpStream& ar, bit_sequence<N, Block> seq)
{
	typename bit_sequence<N, Block>::size_type len;
	ar >> len;
	MG_CHECK(IsDefined(len));
	if (len != seq.size())
			throwErrorF("ReadSequence", "stream size %u conflicts with internal size %u"
			,	len
			,	seq.size()
			);

	ReadBinRangeImpl(ar, seq.data_begin(), seq.data_end(), STREAMTYPE_TAG(Block) );
	return ar;
}

template <int N, typename Block>
BinaryOutStream& operator << (BinaryOutStream& ar, bit_sequence<N, Block> seq)
{
	ar << seq.size();

	WriteBinRangeImpl(ar, seq.data_begin(), seq.data_end(), STREAMTYPE_TAG(Block));

	return ar;
}

template <class T>
BinaryInpStream& operator >> (BinaryInpStream& ar, SA_Reference<T> vec)
{
	ReadBinRange(ar, vec);
	return ar;
}

template <class T>
BinaryOutStream& operator << (BinaryOutStream& ar, SA_ConstReference<T> vec)
{
	WriteBinRange(ar, vec);
	return ar;
}

template <class T>
BinaryInpStream& operator >> (BinaryInpStream& ar, sequence_obj<T>& vec)
{
	ReadBinRange(ar, vec);
	return ar;
}

template <class T>
BinaryOutStream& operator << (BinaryOutStream& ar, const sequence_obj<T>& vec)
{
	WriteBinRange(ar, vec);
	return ar;
}

template <class T>
BinaryInpStream& operator >> (BinaryInpStream& ar, sequence_array_ref<T>& vec)
{
	vec.get_sa().StreamIn(ar, false);
	return ar;
}

template <class T>
BinaryOutStream& operator << (BinaryOutStream& ar, const sequence_array_cref<T>& vec)
{
	ar << vec.get_sa();
	return ar;
}

template <typename T>
BinaryOutStream& operator << (BinaryOutStream& ar, const sequence_array<T>& sa)
{
	sa.StreamOut(ar);
	return ar;
}
template <typename T>
BinaryInpStream& operator >> (BinaryInpStream& ar, sequence_array<T>& sa)
{
	sa.StreamIn(ar, true);
	return ar;
}

template <typename T>
PolymorphInpStream& operator >> (PolymorphInpStream& ar, sequence_array<T>& vec)
{
	typesafe_cast<BinaryInpStream&>(ar) >> vec;
	return ar;
}

template <typename T>
PolymorphOutStream& operator << (PolymorphOutStream& ar, const sequence_array<T>& vec)
{
	typesafe_cast<BinaryOutStream&>(ar) << vec;
	return ar;
}

// ====================================== FormattedStream sequences

template <typename T>
inline FormattedOutStream& operator << (FormattedOutStream& ar, SA_ConstReference<T> vec)
{
	WriteFormattedRange(ar, vec);
	return ar;
}

template <>
inline FormattedOutStream& operator << (FormattedOutStream& ar, SA_ConstReference<char> ref)
{
	ar.Buffer().WriteBytes(ref.begin(), ref.size());
	return ar;
}

template <class T>
inline FormattedInpStream& operator >> (FormattedInpStream& ar, SA_Reference<T>& vec)
{
	ReadFormattedRange(ar, vec);
	return ar;
}

template <class T>
inline FormattedInpStream& operator >> (FormattedInpStream& ar, std::vector<T>& vec)
{
	ReadFormattedRange(ar, vec);
	return ar;
}

#endif // __RTC_SER_SEQUENCEARRAYSTREAM_H
