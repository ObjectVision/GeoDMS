#include "RtcPCH.h"

/****************** token_list_with_emptyzero  *******************/
#include "set/Token.h"

#include "act/MainThread.h"
#include "ptr/StaticPtr.h"
#include "ptr/IterCast.h"
#include "ptr/SharedArray.h"
#include "set/IndexedStrings.h"

#if defined(MG_DEBUG)
std::atomic<UInt32> gd_TokenCreationBlockCount = 0;
#endif

/****************** token_list_with_emptyzero  *******************/
namespace
{
	UInt32                   s_nrTokenComponents = 0; 
	static_ptr<TokenStrings> s_TokenListPtr;

	struct token_list_with_emptyzero : TokenStrings
	{
		token_list_with_emptyzero()
		{
			reserve(4096 MG_DEBUG_ALLOCATOR_SRC_IS); // be prepared for heavy use
			TokenID emptyID = TokenID( GetOrCreateID_st(TokenID::GetEmptyStr()), TokenID::TokenKey() );
			dms_assert(emptyID == TokenID::GetEmptyID() );
		}
	};
}	// end anonymous namespace

/****************** token_list_with_emptyzero  *******************/

TokenComponent::TokenComponent()
{
	if (!s_nrTokenComponents++)
	{
		dms_assert(!s_TokenListPtr);
		s_TokenListPtr.assign( new token_list_with_emptyzero );
	}
	dms_assert(s_TokenListPtr);
}

TokenComponent::~TokenComponent()
{
	dms_assert(s_TokenListPtr);
	dms_assert(s_nrTokenComponents);

	if (!--s_nrTokenComponents)
		s_TokenListPtr.reset();
}

// ===================== TokenID

CharPtr TokenID::s_EmptyStr = "";

TokenID::TokenID(CharPtr tokenStr, st_tag*)
{
	dms_assert(NoOtherThreadsStarted());
#if defined(MG_DEBUG)
	SizeT c = s_TokenListPtr->size();
#endif
	dms_assert(tokenStr);
	m_ID = (tokenStr && *tokenStr) ? s_TokenListPtr->GetOrCreateID_st(tokenStr) : 0;
	dms_assert(m_ID < s_TokenListPtr->size());
	dbg_assert(gd_TokenCreationBlockCount == 0 || s_TokenListPtr->size() == c);
}

TokenID::TokenID(CharPtr tokenStr, mt_tag*)
{
#if defined(MG_DEBUG)
	SizeT c = s_TokenListPtr->size();
#endif
	dms_assert(tokenStr);
	m_ID = (tokenStr && *tokenStr) ? s_TokenListPtr->GetOrCreateID_mt(tokenStr) : 0;
	dms_assert(m_ID < s_TokenListPtr->size());
	dbg_assert(gd_TokenCreationBlockCount == 0 || s_TokenListPtr->size() == c);
}

TokenID::TokenID(CharPtr tokenStr, st_tag*, existing_tag*)
{
	dms_assert(NoOtherThreadsStarted());
#if defined(MG_DEBUG)
	SizeT c = s_TokenListPtr->size();
#endif
	dms_assert(tokenStr);
	m_ID = (tokenStr && *tokenStr) ? s_TokenListPtr->GetExisting_st(tokenStr) : 0;
	if (!IsDefined(m_ID))
		throwErrorF("TOKEN", "%s is not registered as token");
	dms_assert(m_ID < s_TokenListPtr->size());
	dbg_assert(s_TokenListPtr->size() == c);
}

TokenID::TokenID(CharPtr tokenStr, mt_tag*, existing_tag*)
{
#if defined(MG_DEBUG)
	SizeT c = s_TokenListPtr->size();
#endif
	dms_assert(tokenStr);
	m_ID = (tokenStr && *tokenStr) ? s_TokenListPtr->GetExisting_mt(tokenStr) : 0;
	if (!IsDefined(m_ID))
		throwErrorF("TOKEN", "%s is not registered as token");
	dms_assert(m_ID < s_TokenListPtr->size());
	dbg_assert(s_TokenListPtr->size() == c);
}

TokenID::TokenID(CharPtr first, CharPtr last, mt_tag*)
{
#if defined(MG_DEBUG)
	SizeT c = s_TokenListPtr->size();
#endif
	m_ID = (first != last)
		?	s_TokenListPtr->GetOrCreateID_mt(first, last)
		:	0;

	dms_assert(m_ID < s_TokenListPtr->size());
	dbg_assert(gd_TokenCreationBlockCount == 0 || s_TokenListPtr->size() == c);
}

TokenID::TokenID(CharPtr first, CharPtr last, st_tag*)
{
	dms_assert(IsMainThread());
#if defined(MG_DEBUG)
	SizeT c = s_TokenListPtr->size();
#endif
	m_ID = (first != last)
		? s_TokenListPtr->GetOrCreateID_st(first, last)
		: 0;

	dms_assert(m_ID < s_TokenListPtr->size());
	dbg_assert(gd_TokenCreationBlockCount == 0 || s_TokenListPtr->size() == c);
}

TokenID::TokenID(CharPtr first, CharPtr last, mt_tag*, existing_tag*)
{
#if defined(MG_DEBUG)
	SizeT c = s_TokenListPtr->size();
#endif
	m_ID = (first != last)
		?	s_TokenListPtr->GetExisting_mt(first, last)
		:	0;
	if (!IsDefined(m_ID))
		throwErrorF("TOKEN", "%s is not registered as token");
	dms_assert(m_ID < s_TokenListPtr->size());
	dbg_assert(s_TokenListPtr->size() == c);
}

TokenID::TokenID(CharPtr first, CharPtr last, st_tag*, existing_tag*)
{
	dms_assert(IsMainThread());
#if defined(MG_DEBUG)
	SizeT c = s_TokenListPtr->size();
#endif
	m_ID = (first != last)
		? s_TokenListPtr->GetExisting_st(first, last)
		: 0;
	if (!IsDefined(m_ID))
		throwErrorF("TOKEN", "%s is not registered as token");
	dms_assert(m_ID < s_TokenListPtr->size());
	dbg_assert(s_TokenListPtr->size() == c);
}

TokenID::TokenID(WeakStr str)
:	m_ID(str.IsDefined() ? TokenID(str.begin(), str.send(), (mt_tag*)nullptr).m_ID : UNDEFINED_VALUE(UInt32) )
{}

TokenID TokenID::GetExisting(CharPtr tokenStr)
{
	return TokenID( s_TokenListPtr->GetExisting_mt(tokenStr) );
}

TokenID TokenID::GetExisting(CharPtr first, CharPtr last, mt_tag*)
{
	return TokenID( s_TokenListPtr->GetExisting_mt(first, last) );
}

TokenID TokenID::GetExisting(CharPtr first, CharPtr last, st_tag*)
{
	return TokenID(s_TokenListPtr->GetExisting_st(first, last));
}

TokenStr TokenID::GetStr() const
{
	IndexedString_shared_lock guard{ GetCS() };
	return TokenStr{ std::move(guard),
		IsDefined(m_ID) ? (*s_TokenListPtr)[m_ID] : TokenID::GetEmptyStr()
	};
}

TokenStr TokenID::GetStrEnd() const
{
	IndexedString_shared_lock guard{ GetCS() };
	return TokenStr{ std::move(guard),
		IsDefined(m_ID) ? s_TokenListPtr->item_end(m_ID) : TokenID::GetEmptyStr()
	};
}

UInt32 TokenID::GetStrLen() const 
{
	if (!IsDefined(m_ID))
		return 0;
	IndexedString_shared_lock guard{ GetCS() };
	return s_TokenListPtr->item_end(m_ID) - (*s_TokenListPtr)[m_ID];
}

CharPtr TokenID::c_str_st() const // UNSAFE, As another thread might reallocate
{
	dms_assert(NoOtherThreadsStarted()); // and check that no other thread exists, i.e. only in DllLoad
	dms_assert(s_TokenListPtr);
	return (*s_TokenListPtr)[m_ID];
}


RTC_CALL TokenStrRange TokenID::AsStrRange() const
{
	IndexedString_shared_lock guard{ GetCS() };
	return TokenStrRange{ std::move(guard),
		IsDefined(m_ID)
		?	CharPtrRange((*s_TokenListPtr)[m_ID], s_TokenListPtr->item_end(m_ID))
		:	CharPtrRange(Undefined())
	};
}

RTC_CALL SharedStr TokenID::AsSharedStr() const
{
	if (!IsDefined(m_ID))
		return SharedStr(Undefined());

	IndexedString_shared_lock guard{ GetCS() };
	return SharedStr{(*s_TokenListPtr)[m_ID], s_TokenListPtr->item_end(m_ID)};
}

RTC_CALL std::string TokenID::AsStdString() const
{
	if (!IsDefined(m_ID))
		return std::string(UNDEFINED_VALUE_STRING, UNDEFINED_VALUE_STRING_LEN);

	IndexedString_shared_lock guard{ GetCS() };
	return std::string{ (*s_TokenListPtr)[m_ID], s_TokenListPtr->item_size(m_ID) };
}


RTC_CALL CharPtrRange TokenID::str_range_st() const
{
	dms_assert(NoOtherThreadsStarted()); // and check that no other thread exists, i.e. only in DllLoad
	dms_assert(s_TokenListPtr);

	return
		IsDefined(m_ID)
		? CharPtrRange((*s_TokenListPtr)[m_ID], s_TokenListPtr->item_end(m_ID))
		: CharPtrRange(Undefined());
}

RTC_CALL void Trim(CharPtrRange& range)
{
	while (!range.empty() && isspace(*range.first))
		++range.first;
	while (!range.empty() && isspace(range.second[-1]))
		--range.second;
}


RTC_CALL TokenID GetTrimmedTokenID(CharPtr first, CharPtr last)
{
	CharPtrRange range(first, last);
	Trim(range);
	return GetTokenID_mt(range.first, range.second);
}
