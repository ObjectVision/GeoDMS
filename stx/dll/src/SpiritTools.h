// Copyright (C) 1998-2025 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif


#if !defined(__STX_SPRITTOOLS_H)
#define __STX_SPRITTOOLS_H

#include "stxBase.h"
#include "utl/IncrementalLock.h"
#include "xct/DmsException.h"

#include "TextPosition.h"

#include <boost/spirit/include/classic_symbols.hpp>

#pragma warning( disable : 4761 ) // boost/spirit/core/scanner/impl/skipper.ipp(136) has an integral size mismatch in argument.

///////////////////////////////////////////////////////////////////////////////
//
//  assert_d(descriptor)[ parser_expr ]
//
// for example:
//  assert_d("huh?")[ pizza_p("HelloWorld") ]
//
///////////////////////////////////////////////////////////////////////////////

template <typename ErrorArgT>
struct assertion_type_traits
{
	typedef error_descr_t type;
};

template <typename ErrorDescrT>
struct assertive_parser_creator
{
	assertive_parser_creator(const ErrorDescrT& descr) : m_Descr(descr) {}

	template <typename ParserT>
		boost::spirit::assertive_parser<ErrorDescrT, ParserT>
	operator [](ParserT const& parser) const 
	{
		return boost::spirit::assertive_parser<ErrorDescrT, ParserT>(parser, m_Descr); 
	}

	ErrorDescrT m_Descr;
};

inline auto assert_d(CharPtr descriptor)
{
	return assertive_parser_creator<typename assertion_type_traits<SharedStr>::type>( SharedStr(descriptor MG_DEBUG_ALLOCATOR_SRC("assert_d")) );
}

///////////////////////////////////////////////////////////////////////////////
//
//  adapted fallback_parser class that catches stuff
//
///////////////////////////////////////////////////////////////////////////////

template <typename ParserT>
struct dms_fallback_parser
:	public boost::spirit::unary<ParserT,
		boost::spirit::parser<dms_fallback_parser<ParserT> > >
{
	typedef dms_fallback_parser<ParserT>                                  self_t;
	typedef boost::spirit::unary<ParserT, boost::spirit::parser<self_t> > base_t;
	typedef boost::spirit::unary_parser_category                          parser_category_t;

	dms_fallback_parser(ParserT const& parser): base_t(parser) {}

	template <typename ScannerT>
	struct result
	{ 
		typedef typename boost::spirit::parser_result<ParserT, ScannerT>::type type; 
	};

	template <typename ScannerT>
	typename boost::spirit::parser_result<self_t, ScannerT>::type
	parse(ScannerT const& scan) const
	{
		try
		{
			return this->subject().parse(scan);
		}

		catch (const DmsException& x)
		{
//			if (x.m_Why.contains(") at\n"))
//				throw;
			throw parser_error_t(scan.first, x.AsErrMsg()->m_Why);
		}
	}
};

template <typename ParserT>
dms_fallback_parser<ParserT>
inline dms_guard_d(ParserT const& p)
{
	return dms_fallback_parser<ParserT>(p);
}

///////////////////////////////////////////////////////////////////////////////
//
//  syntaxError functor
//
///////////////////////////////////////////////////////////////////////////////

struct syntaxError_gen
{
	syntaxError_gen(error_descr_t errMsg) : m_ErrMsg(errMsg) {}

	template <typename IteratorT>
	void operator()(IteratorT first, IteratorT last) const
	{
		throw boost::spirit::parser_error<error_descr_t, IteratorT>(first, m_ErrMsg);
	}
	error_descr_t m_ErrMsg;
};

template <typename ErrorArgT>
inline syntaxError_gen
syntaxError(ErrorArgT errMsg) 
{ 
	return SharedStr(errMsg);
}

///////////////////////////////////////////////////////////////////////////////
//
//  AuthErrorDisplayLock
//
///////////////////////////////////////////////////////////////////////////////

extern std::atomic<UInt32> s_AuthErrorDisplayLockRecursionCount;
extern UInt32 s_AuthErrorDisplayLockCatchCount;

struct AuthErrorDisplayLock : StaticMtIncrementalLock<s_AuthErrorDisplayLockRecursionCount>
{
	AuthErrorDisplayLock()
	{
		if(s_AuthErrorDisplayLockRecursionCount == 1) // already incremented
			s_AuthErrorDisplayLockCatchCount = 0;
	}
	~AuthErrorDisplayLock()
	{
		if(s_AuthErrorDisplayLockRecursionCount == 1) // not yet decremented
			s_AuthErrorDisplayLockCatchCount = 0;
	}
};

///////////////////////////////////////////////////////////////////////////////
//
//  textblock helper functions
//
///////////////////////////////////////////////////////////////////////////////

UInt32 eolpos(CharPtr first, CharPtr last);
UInt32 bolpos(CharPtr first, CharPtr last);

UInt32 untabbed_size(CharPtr first, CharPtr last,                  UInt32 tabSize, UInt32 pos = 0);
UInt32 untab        (CharPtr first, CharPtr last, char* outBuffer, UInt32 tabSize, UInt32 pos = 0);

SYNTAX_CALL SharedStr problemlocAsString(CharPtr bufferBegin, CharPtr bufferEnd, CharPtr problemLoc);

///////////////////////////////////////////////////////////////////////////////
//
//  parse helper functions
//
///////////////////////////////////////////////////////////////////////////////

extern const boost::spirit::uint_parser<UInt64>  uint64_p;
extern const boost::spirit::uint_parser<UInt64, 16> hex64_p;

//auto const uint64_p = boost::spirit::uint_parser<UInt64>();
//auto const hex64_p = boost::spirit::uint_parser<UInt64, 16>();

void CheckInfo(const parse_info_t& info);


#endif //!defined(__STX_SPRITTOOLS_H)

