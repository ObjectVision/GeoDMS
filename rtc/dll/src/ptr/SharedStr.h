// Copyright (C) 1998-2026 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#pragma once

#if !defined(__RTC_PTR_SHAREDCHARARRAY_H)
#define __RTC_PTR_SHAREDCHARARRAY_H

#include "geo/BaseBounds.h"
#include "geo/iterrangefuncs.h"
#include "geo/CharPtrRange.h"
#include "geo/StringBounds.h"
#include "geo/Undefined.h"
#include "ptr/IterCast.h"
#include "ptr/SharedPtr.h"
#include "ptr/SharedArray.h"
#include "ptr/WeakPtr.h"
#include "ser/format.h"

struct TokenID;
struct CharPtrRange;
struct MutableCharPtrRange;

//----------------------------------------------------------------------
// Section      : String compare functions that override std::lexicographical_compare
//----------------------------------------------------------------------
#if !defined(_MSC_VER)

#include <cctype>

inline int strncmp(CharPtr lhs, CharPtr rhs, SizeT maxCount)
{
	for (; maxCount; ++lhs, ++rhs, --maxCount)
	{
		char lhsCh = *lhs, rhsCh = *rhs;
		if (lhsCh < rhsCh)
			return -1;
		if (lhsCh > rhsCh)
			return +1;
		if (!lhsCh)
			return 0;
	}
}

inline int strnicmp(CharPtr lhs, CharPtr rhs, SizeT maxCount)
{
	for (; maxCount; ++lhs, ++rhs, --maxCount)
	{
		auto lhsCh = tolower(*lhs), rhsCh = tolower(*rhs);
		if (lhsCh < rhsCh)
			return -1;
		if (lhsCh > rhsCh)
			return +1;
		if (!lhsCh)
			return 0;
	}
}

inline int stricmp(CharPtr lhs, CharPtr rhs)
{
	for (; ; ++lhs, ++rhs)
	{
		auto lhsCh = tolower(*lhs), rhsCh = tolower(*rhs);
		if (lhsCh < rhsCh)
			return -1;
		if (lhsCh > rhsCh)
			return +1;
		if (!lhsCh)
			return 0;
	}
}

#endif

struct GenericEqual {
	RTC_CALL bool operator()(CharPtrRange a, CharPtrRange b) const noexcept;
};

struct GenericHasher {
	RTC_CALL std::size_t operator()(CharPtrRange str) const noexcept;
};

struct Utf8CaseInsensitiveEqual {
	RTC_CALL bool operator()(CharPtrRange a, CharPtrRange b) const noexcept;
};

struct Utf8CaseInsensitiveHasher
{
	RTC_CALL std::size_t operator()(CharPtrRange input) const noexcept;
};


struct AsciiFoldedCaseInsensitiveEqual {
	RTC_CALL bool operator()(CharPtrRange a, CharPtrRange b) const noexcept;
};

struct AsciiFoldedChunkedCaseInsensitiveHasher {
	RTC_CALL std::size_t operator()(CharPtrRange str) const noexcept;
};

inline bool lex_compare_cs(CharPtr f1, CharPtr l1, CharPtr f2, CharPtr l2)
{
	auto sz1 = l1-f1, sz2 = l2-f2;
	auto szMin = Min(sz1, sz2);
	if (szMin)
	{
		auto cmpRes = strncmp(f1, f2,  szMin );
		if (cmpRes < 0) return true;
		if (cmpRes > 0) return false;
	}
	return sz1 < sz2;
}

inline bool lex_compare_ci(CharPtr f1, CharPtr l1, CharPtr f2, CharPtr l2)
{
	auto sz1 = l1-f1, sz2 = l2-f2;
	auto szMin = Min(sz1, sz2);
	if (szMin)
	{
		auto cmpRes = strnicmp(f1, f2,  szMin );
		if (cmpRes < 0) return true;
		if (cmpRes > 0) return false;
	}
	return sz1 < sz2;
}


template <typename T> struct is_char : std::false_type {};
template <> struct is_char<Char> : std::true_type {};

template <typename T> const bool is_char_v = is_char<T>::value;

template <typename CI1, typename CI2>
inline bool lex_compare(CI1 f1, CI1 l1, CI2 f2, CI2 l2)
{
	static_assert(!is_char_v<typename std::iterator_traits<CI1>::value_type>);
	static_assert(!is_char_v<typename std::iterator_traits<CI2>::value_type>);
	return std::lexicographical_compare(f1, l1, f2, l2);
}

template <>
inline bool lex_compare<CharPtr, CharPtr>(CharPtr f1, CharPtr l1, CharPtr f2, CharPtr l2)
{
	// compare like strncmp, as if the characters are unsigned according to the standard and existing practice, but also look beyond intermediate zeroes, to avoid inconsistency with equaility that must match hashed equivalence
	// i.e. if hash(a) != hash(b) then a != b and a<b or b<a, which would be violated if character ranges only differ beyond a null-terminator for strncmp

	return std::lexicographical_compare(
		reinterpret_cast<const unsigned char*>(f1)
	,	reinterpret_cast<const unsigned char*>(l1)
	,	reinterpret_cast<const unsigned char*>(f2)
	,	reinterpret_cast<const unsigned char*>(l2)
	);
}

inline bool equal_cs(CharPtr f1, CharPtr l1, CharPtr f2, CharPtr l2)
{
	auto size = l1-f1;

	if (size != l2-f2) return false;
	if (!size)         return true;

	return strncmp(f1, f2, size) == 0;
}

inline bool equal_ci(CharPtr f1, CharPtr l1, CharPtr f2, CharPtr l2)
{
	auto size = l1-f1;

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

//----------------------------------------------------------------------

RTC_CALL SharedCharArray* SharedCharArray_Create(CharPtr str MG_DEBUG_ALLOCATOR_SRC_ARG);
RTC_CALL SharedCharArray* SharedCharArray_Create(CharPtr begin, CharPtr end MG_DEBUG_ALLOCATOR_SRC_ARG);
RTC_CALL SharedCharArray* SharedCharArray_CreateEmpty();
inline constexpr SharedCharArray* SharedCharArray_CreateUndefined() { return nullptr; }

inline bool IsDefined(const SharedCharArray* sca) { return sca != nullptr; }

template <typename BasePtr>
struct SharedCharArrayPtrWrap : protected BasePtr
{
	using array_type = SharedCharArray;
	using pointer = array_type*;
	using const_pointer = const array_type*;
	using iterator = array_type::iterator;
	using const_iterator = array_type::const_iterator;
	using this_type = SharedCharArrayPtrWrap<BasePtr>;

	using BasePtr::BasePtr;

	using BasePtr::has_ptr;
	using BasePtr::operator bool;
	const_pointer get_ptr() const { return BasePtr::get(); }
	pointer       get_ptr()       { return BasePtr::get(); }

	bool IsDefined() const { return has_ptr(); }
 	bool empty() const { return ssize() == 0; }

	CharPtr c_str() const { return has_ptr() ?  get_ptr()->begin() : ""; }
	CharPtr begin() const { return has_ptr() ?  get_ptr()->begin() : nullptr;  }
	CharPtr send () const { return has_ptr() ?  get_ptr()->end()-1 : nullptr;  }
	CharPtr cbegin() const { return begin();  }
	CharPtr csend () const { return send();  }
	SizeT   ssize() const { return has_ptr() ?  get_ptr()->size()-1: 0;  }
	char    sback() const { assert(has_ptr()); assert(get_ptr()->size() > 1);  return get_ptr()->end()[-2]; }
	char operator [] (SizeT i) const { assert(get_ptr() && i < get_ptr()->size()); return get_ptr()->begin()[i]; }

	CharPtr find(char ch)                     const { return std::find(begin(), send(), ch); }
	CharPtr find(CharPtr first, CharPtr last) const { return Search(CharPtrRange(begin(), send()), CharPtrRange(first, last)); }
	CharPtr find(CharPtr str)                 const { return find(str, str + StrLen(str)); }

	template <typename OthBasePtr>
	bool operator < (const SharedCharArrayPtrWrap<OthBasePtr>& b) const
	{
		if (!IsDefined())
			return b.IsDefined();
		if (!b.IsDefined())
			return false;
		return lex_compare(begin(), send(), b.begin(), b.send());
	}

	template <typename OthBasePtr>
	bool operator ==(const SharedCharArrayPtrWrap<OthBasePtr>& b) const
	{
		if (!IsDefined())
			return !b.IsDefined();
		if (!b.IsDefined())
			return false;
		return equal(begin(), send(), b.begin(), b.send());
	}

	template <typename OthBasePtr>
	bool operator !=(const SharedCharArrayPtrWrap<OthBasePtr>& b) const
	{
		if (!IsDefined())
			return b.IsDefined();
		if (!b.IsDefined())
			return true;
		return !equal(begin(), send(), b.begin(), b.send());
	}

	bool operator < (CharPtr b) const
	{
		if (!::IsDefined(b))
			return false;
		if (!IsDefined())
			return true;
		assert(b && IsDefined());
		if (empty())
			return *b;
		assert(has_ptr());
		auto sz = get_ptr()->size();
		assert(sz);
		assert(begin()[sz - 1] == char(0));
		return strncmp(begin(), b, sz) < 0;
	}

	bool operator ==(CharPtr b) const
	{
		if (!IsDefined())
			return !::IsDefined(b);
		if (!::IsDefined(b))
			return false;
		assert(b && IsDefined());
		if (empty())
			return !*b;
		assert(has_ptr());
		auto sz = get_ptr()->size();
		assert(sz);
		assert(begin()[sz - 1] == char(0));
		return strncmp(begin(), b, sz) == 0 && b[sz] == char(0); // ensure that b is also terminated with 0
	}

	bool operator !=(CharPtr b) const
	{
		if (!IsDefined())
			return ::IsDefined(b);
		if (!::IsDefined(b))
			return true;
		assert(b && IsDefined());
		if (empty())
			return *b;
		assert(has_ptr());
		auto sz = get_ptr()->size();
		assert(sz);
		assert(begin()[sz - 1] == char(0));
		return strnicmp(begin(), b, sz) != 0;
	}

	CharPtrRange AsRange() const
	{
		if (!has_ptr())
			return {};
		return CharPtrRange(get_ptr()->begin(), get_ptr()->end() - 1);
	}


	std::string AsStdString() const
	{
		auto ptr = get_ptr();
		if (!::IsDefined(ptr))
			return std::string(UNDEFINED_VALUE_STRING, UNDEFINED_VALUE_STRING_LEN);
		assert(ptr);
		assert(ptr->size());
		return std::string(ptr->begin(), ptr->size() - 1);
	}

//	template <typename OthBasePtr>
//	SharedStr operator +(const SharedCharArrayPtrWrap<OthBasePtr>& b) const
//	{
//		return this->AsRange() + b.AsRange();
//	}

	SharedStr operator +(const CharPtrRange& b) const;
	SharedStr operator +(CharPtr b) const;

	SharedStr operator +(const SharedStr& b) const;
	SharedStr operator +(const WeakStr& b) const;

	struct hasher {
		std::size_t operator()(const this_type& v) const noexcept
		{
			AsciiFoldedChunkedCaseInsensitiveHasher hasherFunc;
			return hasherFunc(CharPtrRange{ v.begin(), v.send() });
		}

	};

	void swap(this_type& oth) { BasePtr::swap(oth); }
};

//----------------------------------------------------------------------

struct WeakStr: SharedCharArrayPtrWrap< WeakPtr<SharedCharArray> >
{
	using base_type = SharedCharArrayPtrWrap< WeakPtr<SharedCharArray> >;

	WeakStr(const SharedStr& str);
	RTC_CALL operator CharPtrRange() const;
};

//----------------------------------------------------------------------

struct SharedStr : SharedCharArrayPtrWrap<SharedPtr<SharedCharArray> >
{
private:
	using base_type = SharedCharArrayPtrWrap<SharedPtr<SharedCharArray> >;
	using size_t = SharedCharArray::size_t;

public:
	RTC_CALL SharedStr() noexcept;
//	SharedStr(CharPtr begin, CharPtr end MG_DEBUG_ALLOCATOR_SRC(CharPtr srcStr = "SharedStr")): base_type(SharedCharArray_Create(begin, end MG_DEBUG_ALLOCATOR_SRC_PARAM) ){}
	SharedStr(WeakStr str) : base_type(str.get_ptr(), existing_obj{}) { assert(get_ptr() || !str.get_ptr()); }
	SharedStr(SharedStr&& str) noexcept  : base_type(std::move(str)) {}

	explicit SharedStr(const SharedStr& rhs) = default;
	explicit SharedStr(CharPtr zStr MG_DEBUG_ALLOCATOR_SRC(CharPtr srcStr = "SharedStr")) : base_type(SharedCharArray_Create(zStr MG_DEBUG_ALLOCATOR_SRC_PARAM), newly_obj{}) {}
	explicit SharedStr(const std::string& strStr MG_DEBUG_ALLOCATOR_SRC(CharPtr srcStr = "SharedStr")): base_type(SharedCharArray_Create(begin_ptr(strStr), end_ptr(strStr) MG_DEBUG_ALLOCATOR_SRC_PARAM), newly_obj{}) {}
//	template <unsigned int N> explicit SharedStr(const char(&str)[N] )     : base_type(SharedCharArray_Create(str, str+N-1 MG_DEBUG_ALLOCATOR_SRC("SharedStr::ctor")) ){ dms_assert(!str[N-1]); }
//	template <unsigned int N> explicit SharedStr(const char8_t(&str)[N])   : base_type(SharedCharArray_Create(reinterpret_cast<CharPtr>(str), reinterpret_cast<CharPtr>(str) + N - 1 MG_DEBUG_ALLOCATOR_SRC("SharedStr::ctor"))) { assert(!str[N - 1]); }
	explicit SharedStr(SharedCharArray* arrayPtr): base_type(arrayPtr, newly_obj{}) {}
	constexpr explicit SharedStr(const Undefined&) : base_type(SharedCharArray_CreateUndefined(), newly_obj{}) {}
	RTC_CALL explicit SharedStr(MutableCharPtrRange range MG_DEBUG_ALLOCATOR_SRC(CharPtr srcStr = "SharedStr::SharedStr(MutableCharPtrRange range)"));
	RTC_CALL explicit SharedStr(CharPtrRange range MG_DEBUG_ALLOCATOR_SRC(CharPtr srcStr = "SharedStr::SharedStr(CharPtrRange range)"));
//	RTC_CALL explicit SharedStr(IterRange<CharPtr> range MG_DEBUG_ALLOCATOR_SRC(CharPtr srcStr = "SharedStr::SharedStr(CharPtrRange range)"));
	RTC_CALL explicit SharedStr(TokenID id MG_DEBUG_ALLOCATOR_SRC(CharPtr srcStr = "SharedStr::SharedStr(TokenID id)"));
	RTC_CALL explicit SharedStr(const struct TokenStr& str MG_DEBUG_ALLOCATOR_SRC(CharPtr srcStr = "SharedStr::SharedStr(const struct TokenStr&)"));
	RTC_CALL SharedStr(const SA_ConstReference<char>& range MG_DEBUG_ALLOCATOR_SRC(CharPtr srcStr = "SharedStr::SharedStr(const SA_ConstReference<char>&)"));

	void operator = (CharPtr zStr) { this->reset(SharedCharArray_Create(zStr MG_DEBUG_ALLOCATOR_SRC("SharedStr::operator = (CharPtr)"))); }
	void operator = (const char8_t* zStr) { this->reset(SharedCharArray_Create(reinterpret_cast<CharPtr>(zStr) MG_DEBUG_ALLOCATOR_SRC("SharedStr::operator = (const char8_t*)"))); }

	void operator = (WeakStr  str) { auto tmp = SharedStr(str); this->swap(tmp); }
	RTC_CALL void operator = (const TokenID& id);
	void operator = (SharedCharArray* id) { reset(id); }
	SharedStr& operator = (SharedStr&& str) noexcept { this->swap(str); return *this; }
	RTC_CALL void operator = (const SA_ConstReference<char>& range);

	SharedStr& operator = (const SharedStr&) = default;

	RTC_CALL operator CharPtrRange() const;

	RTC_CALL void clear();

	template <typename RHS>
	void operator +=(RHS&& rhs) {
		if (IsDefined())
		{
			auto tmp = this->AsRange() + std::forward<RHS>(rhs);
			this->swap(tmp);
		}
	}

	using base_type::begin;
	using base_type::send;
	using base_type::cbegin;
	using base_type::csend;
	using base_type::has_ptr;

	auto get_ptr() const { return base_type::get(); }

	char* begin() { MakeUnique(); return has_ptr() ?  get_ptr()->begin() : nullptr;  }
	char* send () { MakeUnique(); return has_ptr() ?  get_ptr()->end()-1 : nullptr;  }

	char* end()   { return send(); }
	const char* end() const { return send(); }

	void erase(SizeT pos, SizeT c=1) { MakeUnique(); assert(has_ptr()); return get_ptr()->erase (pos, c); }
	RTC_CALL void insert(SizeT pos, char ch);
	RTC_CALL SharedStr replace(CharPtr key, CharPtr val) const;

	RTC_CALL MutableCharPtrRange GetAsMutableRange();
	SharedCharArray* GetAsMutableCharArray()   { MakeUnique(); return const_cast<SharedCharArray*>(get_ptr()); }

	RTC_CALL void resize(SizeT sz MG_DEBUG_ALLOCATOR_SRC_ARG);
	RTC_CALL bool contains(CharPtrRange subStr) const;
	RTC_CALL bool contains_case_insensitive(CharPtrRange subStr) const;

	struct cs_hasher
	{
		RTC_CALL size_t operator()(const SharedStr& str) const noexcept;
	};

	struct ci_hasher
	{
		RTC_CALL size_t operator()(const SharedStr& str) const noexcept;
	};

private:
	RTC_CALL void MakeUnique();

friend WeakStr;
friend inline void MakeUndefined(SharedStr& v) { v.reset( SharedCharArray_CreateUndefined() ); }
friend inline SharedStr UndefinedValue(const SharedStr*) { return SharedStr(Undefined()); }
};

inline WeakStr::WeakStr(const SharedStr& str)
	:	base_type(str.get()) 
{}

RTC_CALL SharedStr operator + (CharPtr lhs, WeakStr rhs);
RTC_CALL SharedStr operator + (CharPtrRange lhs, CharPtrRange rhs);
RTC_CALL SharedStr operator + (CharPtrRange lhs, Char rhs);
RTC_CALL SharedStr operator + (Char lhs, CharPtrRange rhs);
inline SharedStr operator + (CharPtr lhs, CharPtrRange rhs) { return CharPtrRange(lhs, StrLen(lhs)) + rhs; }
inline SharedStr operator + (CharPtrRange lhs, const SharedStr& rhs) { return IsDefined(lhs) && rhs.IsDefined() ? lhs + rhs.AsRange() : SharedStr(Undefined{}); }
inline SharedStr operator + (CharPtr lhs, const SharedStr& rhs)      { return lhs            && rhs.IsDefined() ? CharPtrRange(lhs, StrLen(lhs)) + rhs.AsRange() : SharedStr(Undefined{}); }

//RTC_CALL SharedStr operator + (CharPtrRange lhs, Char    ch );
//RTC_CALL SharedStr operator + (Char    ch , CharPtrRange rhs);

//----------------------------------------------------------------------
// Section      : forwarded declarations of template member functions
//----------------------------------------------------------------------

template <typename BasePtr>
inline auto SharedCharArrayPtrWrap<BasePtr>::operator +(const CharPtrRange& b) const -> SharedStr
{
	return this->AsRange() + b;
}

template <typename BasePtr>
inline auto SharedCharArrayPtrWrap<BasePtr>::operator +(CharPtr b) const -> SharedStr
{
	return this->AsRange() + CharPtrRange(b, StrLen(b));
}

template <typename BasePtr>
inline auto SharedCharArrayPtrWrap<BasePtr>::operator +(const SharedStr& b) const -> SharedStr
{ 
	return this->AsRange() + b.AsRange(); 
}

template <typename BasePtr>
inline auto SharedCharArrayPtrWrap<BasePtr>::operator +(const WeakStr& b) const -> SharedStr
{ 
	return this->AsRange() + b.AsRange(); 
}

template <typename Char, typename Traits, typename BasePtr>
std::basic_ostream<Char, Traits>& operator << (std::basic_ostream<Char, Traits>& os, const SharedCharArrayPtrWrap<BasePtr>& x)
{
	os.write(x.cbegin(), x.ssize());
	return os;
}



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

inline CharPtr begin_ptr(const std::string& data) { return data.empty() ? nullptr : &(data.begin()[ 0]); }
inline CharPtr end_ptr(const std::string& data)   { return data.empty() ? nullptr : &(data.end  ()[-1])+1; }

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

#endif //defined(MG_DEBUG_ALLOCATOR)

//----------------------------------------------------------------------
// StreamableDataTime
//----------------------------------------------------------------------

struct StreamableDateTime // Display operating system-style date and time. 
{
	RTC_CALL StreamableDateTime();

private:
	time_t m_time;
	friend RTC_CALL FormattedOutStream& operator <<(FormattedOutStream& fos, const StreamableDateTime& self);
};

//----------------------------------------------------------------------
// Section      : MsgData
//----------------------------------------------------------------------

struct MsgData {
	SeverityTypeID m_SeverityType;
	MsgCategory m_MsgCategory : 7;
	bool        m_IsFollowup : 1 = false;
	dms_thread_id m_ThreadID;
	StreamableDateTime m_DateTime;
	SharedStr m_Txt;
};

extern "C" RTC_CALL CharPtr DMS_CONV RTC_MsgData_GetMsgAsCStr(MsgData * msgData);

#endif // __RTC_PTR_SHAREDCHARARRAY_H

