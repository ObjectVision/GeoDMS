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
#include "SymPch.h"
#pragma hdrstop

#include "parser.h"

#include "dbg/debug.h"
#include "geo/IterRange.h"
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
	DBG_START("Parser", "operator >>", true);

	expr = GetExpr(is);

	DBG_TRACE(("result = %s", AsString(expr).c_str()));

	return is;
}

