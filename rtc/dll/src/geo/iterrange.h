// Copyright (C) 2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#pragma once

#if !defined(__RTC_GEO_ITERRANGE_H)
#define __RTC_GEO_ITERRANGE_H

#include "geo/Couple.h"
#include "geo/IterTraits.h"
#include "geo/StringBounds.h"
#include "ptr/IterCast.h"

// REMOVE ? #include <xutility>

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

#endif // !defined(__RTC_GEO_ITERRANGE_H)
