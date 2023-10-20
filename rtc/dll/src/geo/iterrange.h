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

#if !defined(__RTC_GEO_ITERRANGE_H)
#define __RTC_GEO_ITERRANGE_H

#include "geo/Couple.h"
#include "geo/sequenceTraits.h"
#include "geo/StringBounds.h"
#include "ptr/IterCast.h"

#include <xutility>

//=======================================
// IterRange
//=======================================
// IterRange<T*> is used as value_type for m_Seqs
// it identifies a sequence of T's by a beginning (first) and ending (second) iterator
// It can only be used to access (const) and modify (non-const) elements.
// For resizing a Sequence, one needs to get and use a sequence_array<T>::reference, which is implemented by SA_Reference<T>
//=======================================

template <typename Iter>
struct IterRange : Couple<Iter>
{
	using Couple<Iter>::first;
	using Couple<Iter>::second;

	using iterator        = Iter;
	using value_type      = typename value_type_of_iterator<iterator>::type;
	using const_iterator  = typename sequence_traits<value_type>::cseq_t::iterator;
	using reference       = typename ref_type_of_iterator<iterator>::type;
	using const_reference = typename ref_type_of_iterator<const_iterator>::type;
	using size_type       = typename sizetype_of_iterator<iterator>::type;
	using difference_type = typename difftype_of_iterator<iterator>::type;

	IterRange(Iter first, Iter second)   : Couple<Iter>(first, second)  { dms_assert((first == iterator() == (second == iterator()))); }
	IterRange(Iter first, std::size_t n) : Couple<Iter>(first, first+n) { dms_assert((first == iterator() == (second == iterator()))); }

	template <std::size_t N>
	IterRange(value_type values[N]) : Couple(values, values+ N) { dms_assert((first == iterator() == (second == iterator()))); }

//	special ctor to identify an Undefined sequence (as a NULL string value in SQL)
	IterRange() {} // value-initialisation

	template <typename CIter>
		IterRange(const IterRange<CIter>& src)
			:	Couple(src.begin(), src.end())
		{}

	template <typename Container>
	explicit IterRange(Container* cont)
			:	Couple<Iter>(begin_ptr(*cont), end_ptr(*cont) )
		{}

	iterator        begin ()       { return first;  }
	iterator        end   ()       { return second; }
	const_iterator  begin () const { return first;  }
	const_iterator  end   () const { return second; }
	const_iterator  cbegin() const { return begin(); }
	const_iterator  cend  () const { return end  (); }

	size_type       size  () const { return second - first; }
	bool            empty () const { return second == first; }

	bool      IsDefined() const { return first != iterator(); }

	reference       operator [](size_type i)       { dms_assert(i < size()); return begin()[i]; }
	const_reference operator [](size_type i) const { dms_assert(i < size()); return begin()[i]; }

	reference       front()       { dms_assert(size() > 0); return begin()[0]; }
	const_reference front() const { dms_assert(size() > 0); return begin()[0]; }

	reference       back ()       { dms_assert(size() > 0); return end()[-1]; }
	const_reference back () const { dms_assert(size() > 0); return end()[-1]; }

	void grow   (difference_type d) { second += d; }
	void setsize(size_type n) { second = first + n; }

	friend struct sequence_array<value_type>;
};

template <typename Iter>
IterRange<Iter> GetSeq(IterRange<Iter>& so)
{
	return { so.begin(), so.end() };
}

template <typename Iter>
auto GetConstSeq(IterRange<Iter>& so) -> sequence_traits<typename value_type_of_iterator<Iter>::type>::cseq_t
{
	return { so.begin(), so.end() };
}



template <typename Iter>
IterRange<Iter> UndefinedValue(const IterRange<Iter>*)
{
	return IterRange<Iter>();
}

struct CharPtrRange : IterRange<CharPtr> {
	CharPtrRange() {}
	CharPtrRange(Undefined) {} // creates an Undefined values, for an empty string, construct from ""

	CharPtrRange(CharPtr str) : IterRange(str, (str != nullptr) ? str + StrLen(str) : str) {}

//	template <typename Char, int N>
//	CharPtrRange(Char str[N]) : IterRange(str, str + (N > 0 && !str[N - 1] ? N - 1 : N))
//	{}
	CharPtrRange(iterator first, iterator second) : IterRange(first, second) 
	{}
	CharPtrRange(iterator first, std::size_t n) : IterRange(first, n) 
	{}
};

typedef IterRange<char*> MutableCharPtrRange;

template <typename Char, typename Traits>
std::basic_ostream<Char, Traits>& operator << (std::basic_ostream<Char, Traits>& os, CharPtrRange x)
{
	os.write(x.cbegin(), x.size());
	return os;
}


#endif // !defined(__RTC_GEO_ITERRANGE_H)
