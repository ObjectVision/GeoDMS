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

#if !defined(__RTC_PTR_SHAREDCHARARRAY_H)
#define __RTC_PTR_SHAREDCHARARRAY_H

#include "geo/BaseBounds.h"
#include "geo/IterRangeFuncs.h"
#include "geo/StringBounds.h"
#include "geo/Undefined.h"
#include "ptr/IterCast.h"
#include "ptr/SharedPtr.h"
#include "ptr/SharedArray.h"
#include "ptr/WeakPtr.h"
#include "ser/format.h"

struct TokenID;

//----------------------------------------------------------------------
// Section      : String compare functions that override std::lexicographical_compare
//----------------------------------------------------------------------

inline bool lex_compare_cs(CharPtr f1, CharPtr l1, CharPtr f2, CharPtr l2)
{
	UInt32 
		sz1 = l1-f1, 
		sz2 = l2-f2;
	UInt32 szMin = Min<UInt32>(sz1, sz2);
	if (szMin)
	{
		int cmpRes = strncmp(f1, f2,  szMin );
		if (cmpRes < 0) return true;
		if (cmpRes > 0) return false;
	}
	return sz1 < sz2;
}

inline bool lex_compare_ci(CharPtr f1, CharPtr l1, CharPtr f2, CharPtr l2)
{
	UInt32 
		sz1 = l1-f1, 
		sz2 = l2-f2;
	UInt32 szMin = Min<UInt32>(sz1, sz2);
	if (szMin)
	{
		int cmpRes = strnicmp(f1, f2,  szMin );
		if (cmpRes < 0) return true;
		if (cmpRes > 0) return false;
	}
	return sz1 < sz2;
}


template <typename T> struct is_char : std::false_type {};
template <> struct is_char<Char> : std::true_type {};

template <typename T> bool is_char_v = is_char<T>::value;

template <typename CI1, typename CI2>
inline bool lex_compare(CI1 f1, CI1 l1, CI2 f2, CI2 l2)
{
//	static_assert(!is_char_v<typename std::iterator_traits<CI1>::value_type>);
//	static_assert(!is_char_v<typename std::iterator_traits<CI2>::value_type>);
	static_assert(!is_char<typename std::iterator_traits<CI1>::value_type>::value);
	static_assert(!is_char<typename std::iterator_traits<CI2>::value_type>::value);
	return std::lexicographical_compare(f1, l1, f2, l2);
}

template <>
inline bool lex_compare<CharPtr, CharPtr>(CharPtr f1, CharPtr l1, CharPtr f2, CharPtr l2)
{
	return lex_compare_cs(f1, l1, f2, l2);
}


inline bool equal_cs(CharPtr f1, CharPtr l1, CharPtr f2, CharPtr l2)
{
	UInt32 size = l1-f1;

	if (size != l2-f2) return false;
	if (!size)         return true;

	return strncmp(f1, f2, size) == 0;
}

inline bool equal_ci(CharPtr f1, CharPtr l1, CharPtr f2, CharPtr l2)
{
	UInt32 size = l1-f1;

	if (size != l2-f2) return false;
	if (!size)         return true;

	return strnicmp(f1, f2, size) == 0;
}

template <typename CI1, typename CI2>
inline bool equal(CI1 f1, CI1 l1, CI2 f2, CI2 l2)
{
	return	(l1-f1) == (l2-f2)
		&&	std::equal(f1, l1, f2);
}

template <>
inline bool equal<CharPtr>(CharPtr f1, CharPtr l1, CharPtr f2, CharPtr l2)
{
	return equal_cs(f1, l1, f2, l2);
}

//----------------------------------------------------------------------

RTC_CALL SharedCharArray* SharedCharArray_Create(CharPtr str);
RTC_CALL SharedCharArray* SharedCharArray_Create(CharPtr begin, CharPtr end);
RTC_CALL SharedCharArray* SharedCharArray_CreateEmpty();
inline   SharedCharArray* SharedCharArray_CreateUndefined() { return nullptr; }

inline bool IsDefined(const SharedCharArray* sca) { return sca != nullptr; }

struct SharedCharArrayPtr : protected WeakPtrWrap<ptr_wrap<SharedCharArray, copyable> >
{
	typedef SharedCharArray            array_type;
	typedef array_type*                pointer;
	typedef const array_type*          const_pointer;
	typedef array_type::iterator       iterator;
	typedef array_type::const_iterator const_iterator;

	SharedCharArrayPtr(pointer p=0) noexcept : WeakPtrWrap(p) {}

	using WeakPtrWrap::has_ptr;
	const_pointer get_ptr() const { return WeakPtrWrap::get_ptr(); }
	pointer       get_ptr()       { return WeakPtrWrap::get_ptr(); }

	bool IsDefined() const { return ::IsDefined(get_ptr()); }
 	bool empty() const { return ssize() == 0; }

	CharPtr c_str() const { return has_ptr() ?  get_ptr()->begin() : ""; }
	CharPtr begin() const { return has_ptr() ?  get_ptr()->begin() : nullptr;  }
	CharPtr send () const { return has_ptr() ?  get_ptr()->end()-1 : nullptr;  }
	CharPtr cbegin() const { return begin();  }
	CharPtr csend () const { return send();  }
	SizeT   ssize() const { return has_ptr() ?  get_ptr()->size()-1: 0;  }
	char    sback() const { dms_assert(has_ptr()); return m_Ptr->end()[-2]; }
	char operator [] (SizeT i) const { dms_assert(m_Ptr && i < m_Ptr->size()); return m_Ptr->begin()[i]; }

	RTC_CALL CharPtr find(char ch)                     const;
	RTC_CALL CharPtr find(CharPtr first, CharPtr last) const;
	RTC_CALL CharPtr find(CharPtr str)                 const;
	RTC_CALL bool operator < (SharedCharArrayPtr b) const;
	RTC_CALL bool operator ==(SharedCharArrayPtr b) const;
	RTC_CALL bool operator !=(SharedCharArrayPtr b) const;

	RTC_CALL bool operator < (CharPtr b) const;
	RTC_CALL bool operator ==(CharPtr b) const;
	RTC_CALL bool operator !=(CharPtr b) const;

	CharPtrRange AsRange() const  { return has_ptr() ? CharPtrRange(get_ptr()->begin(), get_ptr()->end()-1) : CharPtrRange(); }

	void swap(SharedCharArrayPtr& oth) { WeakPtrWrap::swap(oth); }
};

//----------------------------------------------------------------------

struct WeakStr: WeakPtrWrap<SharedCharArrayPtr>
{
	WeakStr(const SharedStr& str);
	operator CharPtrRange() const { return { begin(), send() }; }
};

//----------------------------------------------------------------------

struct SharedStr : SharedPtrWrap<SharedCharArrayPtr> 
{
private:
	typedef SharedPtrWrap base_type;
	typedef SharedCharArray::size_t size_t;
public:
	RTC_CALL SharedStr() noexcept;
	SharedStr(CharPtr begin, CharPtr end): base_type(SharedCharArray_Create(begin, end) ){}
	SharedStr(WeakStr str)               : base_type(str.get_ptr()) {}
	SharedStr(SharedStr&& str) noexcept  : base_type(std::move(str)) {}

	explicit SharedStr(const SharedStr& rhs) = default;
	explicit SharedStr(CharPtr zStr)             : base_type(SharedCharArray_Create(zStr) ) {}
	explicit SharedStr(const std::string& strStr): base_type(SharedCharArray_Create(begin_ptr(strStr), end_ptr(strStr))) {}
	template <unsigned int N> explicit SharedStr(const char(&str)[N] )     : base_type(SharedCharArray_Create(str, str+N-1) ){ dms_assert(!str[N-1]); }
	template <unsigned int N> explicit SharedStr(const char8_t(&str)[N])   : base_type(SharedCharArray_Create(reinterpret_cast<CharPtr>(str), reinterpret_cast<CharPtr>(str) + N - 1)) { dms_assert(!str[N - 1]); }
	explicit SharedStr(SharedCharArray* arrayPtr): base_type(arrayPtr) {}
	explicit SharedStr(MutableCharPtrRange range): base_type(SharedCharArray_Create(range.begin(), range.end()) ){}
	explicit SharedStr(CharPtrRange range)       : base_type(SharedCharArray_Create(range.begin(), range.end()) ){}
	explicit SharedStr(const Undefined&)         : base_type(SharedCharArray_CreateUndefined() ) {}
	RTC_CALL explicit SharedStr(TokenID id);
	RTC_CALL explicit SharedStr(const struct TokenStr& str);
	RTC_CALL SharedStr(const SA_ConstReference<char>& range);

	void operator = (CharPtr zStr) { assign(SharedCharArray_Create(zStr)); }
	void operator = (const char8_t* zStr) { assign(SharedCharArray_Create(reinterpret_cast<CharPtr>(zStr))); }
	//	template <unsigned int N> void operator = (const char str[N])    { assign(SharedCharArray_Create(str, str + N - 1)); dms_assert(!str[N - 1]); }
	//	template <unsigned int N> void operator = (const char8_t str[N]) { assign(SharedCharArray_Create(reinterpret_cast<CharPtr>(str), reinterpret_cast<CharPtr>(str)+N-1)); dms_assert(!str[N - 1]); }
	void operator = (WeakStr  str) { assign(str.get_ptr()); }
	RTC_CALL void operator = (const TokenID& id);
	void operator = (SharedCharArray* id) { assign(id); }
	void operator = (SharedStr&& str) noexcept { swap(str); }
	RTC_CALL void operator = (const SA_ConstReference<char>& range);

	SharedStr& operator = (const SharedStr&) = default;

	operator CharPtrRange() const { return AsRange(); }

	RTC_CALL void clear();

	RTC_CALL void operator +=(Char    rhs);
	RTC_CALL void operator +=(CharPtrRange rhs);

	using base_type::begin;
	using base_type::send;
	using base_type::cbegin;
	using base_type::csend;
	using base_type::has_ptr;
	using base_type::get_ptr;

	char* begin() { MakeUnique(); return has_ptr() ?  get_ptr()->begin() : nullptr;  }
	char* send () { MakeUnique(); return has_ptr() ?  get_ptr()->end()-1 : nullptr;  }
	char* end()   { return send(); }
	const char* end() const { return send(); }

	void erase(SizeT pos, SizeT c=1) { MakeUnique(); dms_assert(has_ptr()); return get_ptr()->erase (pos, c); }
	RTC_CALL void insert(SizeT pos, char ch);
	RTC_CALL SharedStr replace(CharPtr key, CharPtr val) const;

	MutableCharPtrRange GetAsMutableRange() { MakeUnique(); return MutableCharPtrRange(const_cast<char*>(begin()), const_cast<char*>(send())); }
	SharedCharArray* GetAsMutableCharArray()   { MakeUnique(); return const_cast<SharedCharArray*>(get_ptr()); }
//REMOVE	CharPtrRange AsRange() const { return has_ptr() ? get_ptr()->GetAsRange() : CharPtrRange(); }

	RTC_CALL void resize(SizeT sz);

private:
	RTC_CALL void MakeUnique();

friend WeakStr;
friend inline void MakeUndefined(SharedStr& v) { v.assign( SharedCharArray_CreateUndefined() ); }
friend inline SharedStr UndefinedValue(const SharedStr* x) { return SharedStr(Undefined()); }
};

inline WeakStr::WeakStr(const SharedStr& str)
	:	WeakPtrWrap(str.WeakPtrWrap::get_ptr()) 
{}

RTC_CALL SharedStr operator + (CharPtrRange lhs, CharPtrRange rhs);
RTC_CALL SharedStr operator + (CharPtrRange lhs, Char    ch );
RTC_CALL SharedStr operator + (Char    ch , CharPtrRange rhs);

//----------------------------------------------------------------------
// Section      : helper funcs
//----------------------------------------------------------------------

RTC_CALL SharedStr substr(WeakStr str, SizeT pos);
RTC_CALL SharedStr substr(WeakStr str, SizeT pos, SizeT len);

//----------------------------------------------------------------------
// Section      : MinMax
//----------------------------------------------------------------------

template<> struct minmax_traits<SharedStr>
{
	static SharedStr MinValue() { return SharedStr(); }
	static SharedStr MaxValue() { return SharedStr("\xFF\xFF\xFF\xFF"); } 
};

inline SharedStr LowerBound(WeakStr a, WeakStr b)   { return Min<SharedStr>(a, b); } 
inline SharedStr UpperBound(WeakStr a, WeakStr b)   { return Max<SharedStr>(a, b); } 
inline bool IsLowerBound(WeakStr a, WeakStr b)      { return !(b<a); }
inline bool IsUpperBound(WeakStr a, WeakStr b)      { return !(a<b); }
inline bool	IsStrictlyLower(WeakStr a, WeakStr b)   { return a < b; }
inline bool	IsStrictlyGreater(WeakStr a, WeakStr b) { return b < a; }
inline void MakeLowerBound(SharedStr& a, WeakStr b) { MakeMin <SharedStr>(a, b); }
inline void MakeUpperBound(SharedStr& a, WeakStr b) { MakeMax <SharedStr>(a, b); }
inline void MakeBounds(SharedStr& a, SharedStr& b)  { MakeRange<SharedStr>(a, b); }  

//----------------------------------------------------------------------
// Section      : Undefined
//----------------------------------------------------------------------


inline bool IsDefined(WeakStr v)          { return v.IsDefined(); }
inline bool IsDefined(const SharedStr& v) { return v.IsDefined(); }

inline CharPtr begin_ptr(const std::string& data) { return &*data.begin(); }
inline CharPtr end_ptr(const std::string& data)   { return &*data.end(); }

inline CharPtr begin_ptr(WeakStr data)          { return data.begin(); }
inline CharPtr end_ptr  (WeakStr data)          { return data.send();  }
inline CharPtr begin_ptr(const SharedStr& data) { return data.begin(); }
inline CharPtr end_ptr  (const SharedStr& data) { return data.send();  }
inline char*   begin_ptr(      SharedStr& data) { return data.begin(); }
inline char*   end_ptr  (      SharedStr& data) { return data.send();  }

inline CharPtr cbegin_ptr(const SharedStr& data) { return data.begin(); }
inline CharPtr cend_ptr  (const SharedStr& data) { return data.send();  }

//----------------------------------------------------------------------
// Section      : String specific functions 
//----------------------------------------------------------------------

inline streamsize_t StrLen(WeakStr str, streamsize_t maxLen) 
{
	return Min<streamsize_t>(maxLen, str.ssize());
}	

RTC_CALL Float64 AsFloat64(WeakStr x );

inline void Assign(SharedStr& lhs, WeakStr rhs) { lhs = rhs; }

template<typename ...Args>
SharedStr mgFormat2SharedStr(CharPtr msg, Args&&... args)
{
	return SharedStr(mgFormat2string<Args...>(msg, std::forward<Args>(args)...));
}

//----------------------------------------------------------------------
// Section      : MG_DEBUG_ALLOCATOR
//----------------------------------------------------------------------

#if defined(MG_DEBUG_ALLOCATOR)

//RTC_CALL SharedStr SequenceArrayString();
//RTC_CALL SharedStr IndexedString();

#endif defined(MG_DEBUG_ALLOCATOR)


#endif // __RTC_PTR_SHAREDCHARARRAY_H

