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

const assertive_parser_creator<typename assertion_type_traits<ErrMsg>::type>
inline assert_d(CharPtr descriptor)
{
	return assertive_parser_creator<typename assertion_type_traits<ErrMsg>::type>(ErrMsg{ SharedStr(descriptor) });
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
			throw parser_error_t(scan.first, x.AsErrMsg());
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

//template <typename ErrorDescrT>
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
inline syntaxError_gen // <typename assertion_type_traits<ErrorArgT>::type>
syntaxError(ErrorArgT errMsg) 
{ 
	return syntaxError_gen( // <typename assertion_type_traits<ErrorArgT>::type>
		error_descr_t(errMsg)
	);
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

void CheckInfo(const parse_info_t& info);


#endif //!defined(__STX_SPRITTOOLS_H)

