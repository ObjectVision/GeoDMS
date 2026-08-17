// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

#ifndef __MG_SYMBOL_TOKEN_H
#define __MG_SYMBOL_TOKEN_H

#include <atomic>
#include <type_traits>

#include "act/MainThread.h"
#include "cpc/Types.h"
#include "vt/Undefined.h"
#include "Parallel.h"
#include "vt/CharPtrRange.h"
#include "RtcComponents.h" // ElemAllocComponent / IndexedStringsComponent / TokenComponent (StaticTokenID base, StaticLateTokenID check)

using IndexedString_critical_section = leveled_counted_section;
struct IndexedString_shared_lock : RequestMainThreadOperProcessingBlocker, leveled_counted_section::shared_lock { using leveled_counted_section::shared_lock::shared_lock; };
struct IndexedString_scoped_lock : RequestMainThreadOperProcessingBlocker, leveled_counted_section::scoped_lock { using leveled_counted_section::scoped_lock::scoped_lock; };


struct TokenStr
{
	TokenStr() = default;
	TokenStr(IndexedString_shared_lock&& m_Guard, CharPtr ptr)
		: m_Guard(std::move(m_Guard))
		, m_CharPtr(ptr)
	{}

	TokenStr(TokenStr&& src) noexcept
		: m_Guard(std::move(src.m_Guard))
		, m_CharPtr(src.m_CharPtr)
	{}

	TokenStr(const TokenStr& src)
		: m_Guard(src.m_Guard)
		, m_CharPtr(src.m_CharPtr)
	{}

	void operator =(TokenStr&& src) noexcept
	{
		m_Guard = std::move(src.m_Guard);
		m_CharPtr = src.m_CharPtr;
	}

	CharPtr c_str() const { return m_CharPtr;  }
	void operator ++() { ++m_CharPtr; }

	IndexedString_shared_lock m_Guard;
	CharPtr m_CharPtr;
};

inline TokenStr operator +(TokenStr&& src, UInt32 offset)
{
	src.m_CharPtr += offset;
	return std::move(src);
}

struct TokenStrRange
{
	TokenStrRange() = default;
	TokenStrRange(IndexedString_shared_lock&& m_Guard, CharPtrRange ptrRange)
		: m_Guard(std::move(m_Guard))
		, m_CharPtrRange(ptrRange)
	{}

	TokenStrRange(TokenStrRange&& src) noexcept
		: m_Guard(std::move(src.m_Guard))
		, m_CharPtrRange(src.m_CharPtrRange)
	{}

	TokenStrRange(const TokenStrRange& src) = delete;
/*
		: m_Guard(src.m_Guard)
		, m_CharPtrRange(src.m_CharPtrRange)
	{}
*/
	void operator =(TokenStrRange&& src) noexcept
	{
		m_Guard = std::move(src.m_Guard);
		m_CharPtrRange = src.m_CharPtrRange;
	}

	auto begin() const { return m_CharPtrRange.begin(); }
	auto end  () const { return m_CharPtrRange.end(); }
	auto size () const { return m_CharPtrRange.size(); }

	operator CharPtrRange() const { return m_CharPtrRange; }

	IndexedString_shared_lock m_Guard;
	CharPtrRange m_CharPtrRange;
};

struct existing_tag;
struct mt_tag;
struct st_tag;

mt_tag* const multi_threading_tag_v = nullptr;
st_tag* const single_threading_tag_v = nullptr;
existing_tag* const existing_tag_v = nullptr;

struct TokenID 
{
	TokenID() noexcept
		: m_ID() 
	{}

//	get or create id
	RTC_CALL explicit TokenID(CharPtr tokenStr, mt_tag*);
	RTC_CALL explicit TokenID(CharPtr tokenStr, st_tag*);
	RTC_CALL explicit TokenID(CharPtr tokenStr, mt_tag*, existing_tag*);
//	RTC_CALL explicit TokenID(CharPtr tokenStr, st_tag*, existing_tag*);
	RTC_CALL explicit TokenID(CharPtr first, CharPtr last, mt_tag*);
	RTC_CALL explicit TokenID(CharPtr first, CharPtr last, st_tag*);
	RTC_CALL explicit TokenID(CharPtr first, CharPtr last, mt_tag*, existing_tag*);
//	RTC_CALL explicit TokenID(CharPtr first, CharPtr last, st_tag*, existing_tag*);
	RTC_CALL explicit TokenID(const SharedStr& str);
	RTC_CALL explicit TokenID(WeakStr str);
	constexpr explicit TokenID(Undefined) : m_ID(UNDEFINED_VALUE(TokenT)) {}
//	get id or -1 if not found
	static TokenID GetExisting(CharPtr tokenStr);
	static TokenID GetExisting(CharPtr first, CharPtr last, mt_tag*);
	static TokenID GetExisting(CharPtr first, CharPtr last, st_tag*);

//	retieve string from token
	RTC_CALL TokenStr GetStr   () const;
	TokenStr GetStrEnd() const;
	UInt32   GetStrLen() const;
	RTC_CALL TokenStrRange AsStrRange() const;
	RTC_CALL SharedStr AsSharedStr() const;
	RTC_CALL std::string AsStdString() const;

	static CharPtr  GetEmptyStr()    { return s_EmptyStr; }
	static TokenStr GetEmptyTokenStr() { return TokenID().GetStr(); }
	static TokenID  GetEmptyID()     { return TokenID(TokenT()); }
	static TokenID  GetUndefinedID() { return TokenID(Undefined() ); }

	bool operator < (TokenID oth) const { return m_ID < oth.m_ID; }
	bool operator > (TokenID oth) const { return m_ID > oth.m_ID; }
	bool operator <=(TokenID oth) const { return m_ID <=oth.m_ID; }
	bool operator >=(TokenID oth) const { return m_ID >=oth.m_ID; }
	bool operator ==(TokenID oth) const { return m_ID ==oth.m_ID; }
	bool operator !=(TokenID oth) const { return m_ID !=oth.m_ID; }
	bool operator !() const             { return !m_ID; }

//	String like interface
	bool    empty() const { return !m_ID;    }

	struct TokenKey {};
	explicit TokenID(TokenT id, TokenKey) : m_ID(id) {}
	TokenT GetNr(TokenKey) const { return m_ID; }

	explicit operator bool() const { return m_ID; }

private: 
	friend class AbstrPropDef;
	template <typename T> friend TokenID GetDataItemClassID();
	template <typename T> friend TokenID GetUnitClassID();

	friend struct AbstrOperGroup; friend struct SpecialOperGroup;

	// UNSAFE, As another thread might reallocate token list, only use if singly threaded
	CharPtr c_str_st() const; 
	CharPtrRange str_range_st() const;

	explicit TokenID(TokenT id) : m_ID(id) {}
	TokenT m_ID;
	static CharPtr s_EmptyStr; 
};

#if defined(MG_DEBUG)
RTC_CALL extern std::atomic<UInt32> gd_TokenCreationBlockCount;
#endif

// A TokenID with static storage duration must guarantee the token subsystem is up before it
// registers its string. Deriving from TokenComponent (constructed first, as a base) does exactly
// that, making the object safe regardless of cross-TU static-init order within a single DLL.
// Use StaticTokenID for every namespace-scope / file-static / class-static TokenID INSIDE DmRtc,
// where no such order guarantee exists.
struct StaticTokenID : TokenComponent, TokenID
{
	StaticTokenID(CharPtr tokenStr) : TokenID(tokenStr, single_threading_tag_v) {}
	StaticTokenID(CharPtr first, CharPtr last) : TokenID(first, last, single_threading_tag_v) {}

	// A copy would carry a TokenComponent of its own: pointless (the original keeps the
	// subsystem up for as long as the copy can live) and, in a module outside DmRtc, it
	// needs that ctor/dtor exported. `auto id = token::org_rel;` deduces StaticTokenID and
	// hits exactly this; write `TokenID id = ...` to keep the plain id.
	StaticTokenID(const StaticTokenID&) = delete;
};

// The same, for a module OUTSIDE DmRtc (Stg, Stx, Clc, Geo, Shv, the GUI, ...). Such a module
// imports from Rtc.dll, so the loader has already run DmRtc's initializers -- and with them the
// TokenComponent that brings the token subsystem up -- before any initializer here. Holding a
// component of its own would only bump a reference count that can never reach zero first, and
// would force TokenComponent's ctor/dtor to be exported, so this variant just uses the subsystem
// and checks the guarantee in Debug.
struct StaticLateTokenID : TokenID
{
	StaticLateTokenID(CharPtr tokenStr) : TokenID(CheckedUp(tokenStr), single_threading_tag_v) {}
	StaticLateTokenID(CharPtr first, CharPtr last) : TokenID(CheckedUp(first), last, single_threading_tag_v) {}

private:
	// runs as part of evaluating the base-ctor argument, i.e. before the token is registered
	static CharPtr CheckedUp(CharPtr str)
	{
#if defined(MG_DEBUG)
		assert(TokenComponent::IsUp()); // this module's Rtc.dll import should have brought it up
#endif
		return str;
	}
};

inline constexpr TokenID UndefinedValue(const TokenID*) { return TokenID(Undefined()); }

// ===================== Interface

void Trim(CharPtrRange& range);

// get or create id
RTC_CALL TokenID GetTokenID_st(CharPtr tokenStr);
RTC_CALL TokenID GetTokenID_st(CharPtr first, CharPtr last);
inline TokenID GetTokenID_st(CharPtrRange range) { return GetTokenID_st(range.first, range.second); }


inline TokenID GetTokenID_mt(CharPtr tokenStr) { return TokenID(tokenStr, multi_threading_tag_v); }
inline TokenID GetTokenID_mt(CharPtr first, CharPtr last) { return TokenID(first, last, multi_threading_tag_v); }
TokenID GetTrimmedTokenID(CharPtr first, CharPtr last);
inline TokenID GetTokenID_mt(CharPtrRange range) { return GetTokenID_mt(range.first, range.second); }

template<typename TAG = mt_tag>
inline TokenID GetTokenID(CharPtrRange tokenStr, TAG* dummy = nullptr) { return TokenID(tokenStr.first, tokenStr.second, dummy); }

// get id or -1 if not found
inline TokenID GetExistingTokenID(CharPtr tokenStr) { return TokenID::GetExisting(tokenStr); }
inline TokenID GetExistingTokenID_st(CharPtr first, CharPtr last) { return TokenID::GetExisting(first, last, single_threading_tag_v); }
inline TokenID GetExistingTokenID_mt(CharPtr first, CharPtr last) { return TokenID::GetExisting(first, last, multi_threading_tag_v); }
template<typename TAG = mt_tag>
inline TokenID GetExistingTokenID(CharPtrRange tokenStr, TAG* dummy=nullptr) { return TokenID::GetExisting(tokenStr.first, tokenStr.second, dummy); }

// retieve string from token
inline TokenStr GetTokenStr(TokenID tokenID) { return tokenID.GetStr(); }
inline TokenStr GetTokenStrEnd(TokenID tokenID) { return tokenID.GetStrEnd(); }
inline UInt32   GetTokenStrLen(TokenID tokenID) { return tokenID.GetStrLen(); }

//----------------------------------------------------------------------
// Section      : Element operations on strings
//----------------------------------------------------------------------

Float64 AsFloat64(TokenID x);

template <typename Char, typename Traits>
std::basic_ostream<Char, Traits>& operator << (std::basic_ostream<Char, Traits>& os, TokenStr str)
{
	os << str.c_str();
	return os;
}

template <typename Char, typename Traits>
std::basic_ostream<Char, Traits>& operator << (std::basic_ostream<Char, Traits>& os, TokenID id)
{
	return os << id.GetStr();
}

namespace std {
	template<>
	struct hash<TokenID> {
		std::size_t operator()(const TokenID& id) const noexcept {
			return std::hash<decltype(id.GetNr(TokenID::TokenKey()))>()(id.GetNr(TokenID::TokenKey()));
		}
	};
}



#endif // __MG_SYMBOL_TOKEN_H
