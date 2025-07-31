// Copyright (C) 1998-2025 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

#if !defined(__RTC_SET_INDEXEDSTRINGS_H)
#define __RTC_SET_INDEXEDSTRINGS_H

/****************** TokenList  *******************/

#include "RtcBase.h"

#include <set>

#include "dbg/Check.h"
#include "geo/SequenceArray.h"
#include "mem/HeapSequenceProvider.h"

RTC_CALL IndexedString_critical_section& GetCS();

struct StringIndexer
{
	using index_type = TokenT;

	StringIndexer(const StringArray& container)
		: r_Container(container)
	{}

	template<bool MustZeroTerminate> CharPtrRange GetPtrs(index_type x) const noexcept;

private:
	const StringArray& r_Container;
};

template <bool MustZeroTerminate, typename CharPtrRangeEqCmp>
struct StringIndexEqualityCompare : StringIndexer
{
	using is_transparent = std::true_type;
	using index_type = TokenT;

	using StringIndexer::StringIndexer;

	bool operator()(index_type a, index_type b) const noexcept
	{
		return equaler(this->GetPtrs<MustZeroTerminate>(a), this->GetPtrs<MustZeroTerminate>(b));
	}
	bool operator()(index_type a, CharPtrRange ib) const noexcept
	{
		return equaler(this->GetPtrs<MustZeroTerminate>(a), ib);
	}
	bool operator()(CharPtrRange ia, index_type b) const noexcept
	{
		return equaler(ia, this->GetPtrs<MustZeroTerminate>(b));
	}

private:
	CharPtrRangeEqCmp equaler;
};

template <bool MustZeroTerminate, typename CharPtrRangeHasher>
struct StringIndexHasher : StringIndexer
{
	using is_transparent = std::true_type;
	using index_type = TokenT;

	using StringIndexer::StringIndexer;

	auto operator()(index_type a) const noexcept
	{
		return hasher(this->GetPtrs<MustZeroTerminate>(a));
	}
	auto operator()(CharPtrRange ia) const noexcept
	{
		return hasher(ia);
	}

private:
	CharPtrRangeHasher hasher;
};


struct IndexedStringsBase 
{
	typedef TokenT index_type;

	RTC_CALL  IndexedStringsBase();
	RTC_CALL ~IndexedStringsBase();
	
	index_type size() const { return index_type(m_Vec.size()); }

	RTC_CALL void reserve(index_type sz MG_DEBUG_ALLOCATOR_SRC_ARG);

	const StringVector& GetVec() const { return m_Vec; }

  protected:
	StringVector m_Vec;
};

#include <unordered_set>

template <bool MustZeroTerminate, typename CharPtrRangeEqCmp, typename CharPtrRangeHasher>
struct IndexedStrings : IndexedStringsBase
{	
	using equality_compare = StringIndexEqualityCompare<MustZeroTerminate, CharPtrRangeEqCmp>;
	using hasher = StringIndexHasher<MustZeroTerminate, CharPtrRangeHasher>;
	using index_set_type = std::unordered_set <index_type, hasher, equality_compare>;

	using index_iterator = typename index_set_type::iterator;
	using const_index_iterator = typename index_set_type::const_iterator;

	RTC_CALL IndexedStrings();

	RTC_CALL index_type GetOrCreateID_st(CharPtr keyFirst, CharPtr keyLast); // range of chars excluding null terminator
	RTC_CALL index_type GetOrCreateID_mt(CharPtr keyFirst, CharPtr keyLast); // range of chars excluding null terminator
	RTC_CALL index_type GetExisting_st (CharPtr keyFirst, CharPtr keyLast) const; // range of chars excluding null terminator
	RTC_CALL index_type GetExisting_mt (CharPtr keyFirst, CharPtr keyLast) const; // range of chars excluding null terminator
	index_type GetOrCreateID_st(CharPtr key) { return GetOrCreateID_st(key, key+StrLen(key)); }
	index_type GetOrCreateID_mt(CharPtr key) { return GetOrCreateID_mt(key, key + StrLen(key)); }
	index_type GetExisting_st  (CharPtr key) const { return GetExisting_st(key, key + StrLen(key)); }
	index_type GetExisting_mt  (CharPtr key) const { return GetExisting_mt(key, key + StrLen(key)); }

	index_iterator IdxBegin() { return m_Idx.begin(); }
	index_iterator IdxEnd  () { return m_Idx.end(); }
	const_index_iterator IdxBegin() const { return m_Idx.begin(); }
	const_index_iterator IdxEnd  () const { return m_Idx.end(); }

	CharPtr item(index_type id) const
	{
		assert(m_Vec[id].size());
		assert(!MustZeroTerminate || m_Vec[id].end()[-1] == 0);
		return &*(m_Vec[id].begin());
	}
	CharPtr item_end(index_type id) const
	{
		assert(m_Vec[id].size());
		assert(!MustZeroTerminate || m_Vec[id].end()[-1] == 0);
		return &(m_Vec[id].end()[MustZeroTerminate ? -1 : 0]);
	}
	SizeT item_size(index_type id) const
	{
		assert(m_Vec[id].size());
		assert(!MustZeroTerminate || m_Vec[id].end()[-1] == 0);
		return m_Vec[id].size() + (MustZeroTerminate ? -1 : 0);
	}
	CharPtr operator [](index_type id) const
	{
		return item(id);
	}
private:
	index_set_type m_Idx;
	index_type GetOrCreateID_impl(CharPtr keyFirst, CharPtr keyLast);
	index_type GetExisting_impl(CharPtr keyFirst, CharPtr keyLast) const;
};

//	Utf8CaseInsensitiveEqual equaler;
//	Utf8CaseInsensitiveHasher hasher;

using TokenStrings = IndexedStrings<true, AsciiFoldedCaseInsensitiveEqual, AsciiFoldedChunkedCaseInsensitiveHasher>;

#endif // __RTC_SET_INDEXEDSTRINGS_H
