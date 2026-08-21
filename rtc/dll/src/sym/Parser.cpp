// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////
#include "SymPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

#include "Parser.h"

#include "dbg/debug.h"
#include "vt/iterrange.h"
#include "ser/AsString.h"
#include "ser/StringStream.h"

#include <ctype.h>

/****************** Parser                        *******************/

//-------------------------GetExpr & GetExprList

#include "Assoc.h"

const LispRef& rightBrace()
{
	static LispRef result(")");
	return result;
}

LispRef GetExprList(FormattedInpStream& istr)
{
	LispRef H=GetExpr(istr);
	if (H==rightBrace())
		return LispRef();
	return LispRef(H, GetExprList(istr));
}

LispRef GetExpr(FormattedInpStream& istr)
{
	auto [tt, nextToken] = istr.NextToken();

	if (tt == FormattedInpStream::TokenType::EOF_)
		throwErrorD("SLisp", "GetExpr: invalid pairing of brackets");

	if (tt == FormattedInpStream::TokenType::Number)
	{
		Number_t nextNumber = 0.0;
		AssignValueFromCharPtrs(nextNumber, nextToken.begin(), nextToken.end() );
		return LispRef(Number(nextNumber));
	}
	if (tt == FormattedInpStream::TokenType::DoubleQuote)
		return LispRef(nextToken.begin(), nextToken.end());
	if (tt == FormattedInpStream::TokenType::Punctuation)
	{
		dms_assert(nextToken.size() == 1);
		char firstChar = nextToken.front();
		if (firstChar  == ')') 
			return rightBrace();
		if (firstChar  == '(') 
			return GetExprList(istr);
		if (firstChar == ']')
			throwErrorD("SLisp", "GetExpr: expr or ')' expected");
		if (firstChar == '[')
		{
  			LispRef key = GetExpr(istr);
			LispRef val = GetExpr(istr);
			auto result = istr.NextToken();
			if (result.first != FormattedInpStream::TokenType::Punctuation || result.second.front() != ']')
				throwErrorD("SLisp", "GetExpr: ']' expected");
			return Assoc(key, val);
		}
	}
	return LispRef(
		GetTokenID<mt_tag>(nextToken), 
		UInt32( nextToken.size() && (nextToken.front() == '_'))
	);
}

FormattedInpStream& operator >>(FormattedInpStream& is, LispRef& expr)
{
	expr = GetExpr(is);
	return is;
}

