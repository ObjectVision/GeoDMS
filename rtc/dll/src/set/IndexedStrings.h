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
#if !defined(__RTC_SET_INDEXEDSTRINGS_H)
#define __RTC_SET_INDEXEDSTRINGS_H

/****************** TokenList  *******************/

#include "RtcBase.h"

#include <set>

#include "dbg/check.h"
#include "geo/SequenceArray.h"
#include "mem/HeapSequenceProvider.h"

RTC_CALL IndexedString_critical_section& GetCS();

struct SharedPtrInsensitiveCompare {
	RTC_CALL bool operator ()(CharPtr lhs, CharPtr rhs) const;

	bool operator ()(const SharedStr& lhs, CharPtr rhs) const { return operator ()(lhs.begin(), rhs); }
	bool operator ()(CharPtr lhs, const SharedStr& rhs) const { return operator ()(lhs, rhs.begin()); }
	bool operator ()(const SharedStr& lhs, const SharedStr& rhs) const { return operator ()(lhs.begin(), rhs.begin()); }
	typedef std::true_type is_transparent;
};

template <bool MustZeroTerminate>
struct StringIndexCompare
{
	typedef TokenT index_type;

	StringIndexCompare(const StringArray& container);

	CharPtrRange GetPtrs(index_type x) const;
	bool operator()(index_type a, index_type b) const;
	bool operator()(index_type a, CharPtrRange ib) const;
	bool operator()(CharPtrRange ia, index_type b) const;

	typedef std::true_type is_transparent;
private:
	const StringArray & r_Container;
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

template <bool MustZeroTerminate>
struct IndexedStrings : IndexedStringsBase
{	
	typedef StringIndexCompare<MustZeroTerminate>   compare_type;
	typedef std::set<index_type, compare_type>      index_set_type;
	typedef typename index_set_type::iterator       index_iterator;
	typedef typename index_set_type::const_iterator const_index_iterator;

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

typedef IndexedStrings<true > TokenStrings;
typedef IndexedStrings<false> IndexedStringValues; 

#endif // __RTC_SET_INDEXEDSTRINGS_H
