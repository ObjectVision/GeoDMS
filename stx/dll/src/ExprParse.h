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

#if !defined(__STX_EXPRPARSE_H)
#define __STX_EXPRPARSE_H

#include "SpiritTools.h"

#include "ptr/PtrBase.h"
#include "utl/encodes.h"
#include "ExprProd.h"
#include "StringParse.h"

struct itennameFirstChar_parser : public boost::spirit::char_parser<itennameFirstChar_parser>
{
	typedef itennameFirstChar_parser self_t;

	template <typename CharT>
	bool test(CharT ch) const
	{
		return itemNameFirstChar_test(ch);
	}
};

struct itennameNextChar_parser : public boost::spirit::char_parser<itennameFirstChar_parser>
{
	typedef itennameNextChar_parser self_t;

	template <typename CharT>
	bool test(CharT ch) const
	{
		return itemNameNextChar_test(ch);
	}
};

auto const itemNameFirstChar_p = itennameFirstChar_parser();
auto const itemNameNextChar_p = itemNameFirstChar_p | boost::spirit::digit_p;
auto const itemName_p = itemNameFirstChar_p >> *itemNameNextChar_p;

///////////////////////////////////////////////////////////////////////////////
//
//  Our expr grammar
//
///////////////////////////////////////////////////////////////////////////////

template <typename Prod>
struct expr_grammar : public boost::spirit::grammar<expr_grammar<Prod>>
{
	Prod& m_Prod;
	using string_prod_type = typename Prod::string_prod_type;

	expr_grammar(Prod& cp): m_Prod(cp) {}

	template <typename ScannerT>
	struct definition
	{
		string_definition<ScannerT, typename Prod::string_prod_type> m_StringDef;

		definition(expr_grammar const& cg)
			:	m_StringDef(cg.m_Prod.m_StringProd)
		{
			Prod& cp = cg.m_Prod;

//			using boost::spirit;
			using boost::spirit::chlit;
			using boost::spirit::strlit;
			using boost::spirit::lexeme_d;
			using boost::spirit::epsilon_p;
			using boost::spirit::space_p;
			using boost::spirit::alpha_p;
			using boost::spirit::alnum_p;
			using boost::spirit::as_lower_d;
			using boost::spirit::uint_p;
			using boost::spirit::strict_ureal_p;

			//-----------------------------------------------------------------
			// OPERATORS
			//-----------------------------------------------------------------
			chlit<>     COMMA(',');
			chlit<>     UNDERSCORE('_');

			strlit<>    TRUE("true");
			strlit<>    FALSE("false");

			chlit<>     LPAREN('(');
			chlit<>     RPAREN(')');

			chlit<>     LBRACK('[');
			chlit<>     RBRACK(']');
			chlit<>     DOT('.');
			chlit<>     EXCLAIM('!');
			chlit<>     FENCE('#');

			chlit<>     POWER('^');

			chlit<>     STAR('*');
			chlit<>     SLASH('/');
			chlit<>     PERCENT('%');
			strlit<>    ARROW("->");

			chlit<>     PLUS('+');
			chlit<>     MINUS('-');


			strlit<>    C_EQ("==");
			chlit<>     P_EQ('=');
			strlit<>    C_NE("!=");
			strlit<>    P_NE("<>");
			chlit<>     C_LT('<');
			strlit<>    C_LE("<=");
			strlit<>    C_GE(">=");
			chlit<>     C_GT('>');

			chlit<>     C_NOT('!');
//			strlit<>    P_NOT("not");

			strlit<>    C_AND("&&");
			strlit<>    P_AND("and");

			strlit<>    C_OR("||");
			strlit<>    P_OR("or");


			chlit<>     C_IF('?');
			chlit<>     C_ELSE(':');

			keywords = "true", "false";
			//-----------------------------------------------------------------
			// TOKENS
			//-----------------------------------------------------------------


			//-----------------------------------------------------------------
			//  Start grammar definition
			//-----------------------------------------------------------------
			expression
				= (P_EQ >> exprL0)[([&](...) { cp.ProdUnaryOper(token::eval);})]
				|	exprL0;

			exprLW = exprL1
				>> !(C_ELSE >> expression)[([&](...) { cp.ProdBinaryOper(token::scope); })];

			exprL0
				= exprL1
				>> !(C_IF   >> exprL0 >> C_ELSE >> expression)[([&](...) { cp.ProdIIF();})]
			;

			exprL1
				= exprL2
				>> *((C_OR| as_lower_d[P_OR]) >> exprL2)[([&](...) { cp.ProdBinaryOper(token::or_);})];

			exprL2
				= exprL3 
				>> *((C_AND| as_lower_d[P_AND]) >> exprL3)[([&](...) { cp.ProdBinaryOper(token::and_);})];

			exprL3
				= (C_NOT >> exprL3)[([&](...) { cp.ProdUnaryOper(token::not_);})]
				| exprL4;

			exprL4
				= 	exprN0 >> !(
					(C_EQ >> exprN0)[([&](...) { cp.ProdBinaryOper(token::eq);})]
				|	(P_EQ >> exprN0)[([&](...) { cp.ProdBinaryOper(token::eq);})]
				|	(C_NE >> exprN0)[([&](...) { cp.ProdBinaryOper(token::ne);})]
				|	(P_NE >> exprN0)[([&](...) { cp.ProdBinaryOper(token::ne);})]
				|	(C_GE >> exprN0)[([&](...) { cp.ProdBinaryOper(token::ge);})]
				|	(C_LE >> exprN0)[([&](...) { cp.ProdBinaryOper(token::le);})]
				|	(C_GT >> exprN0)[([&](...) { cp.ProdBinaryOper(token::gt);})]
				|	(C_LT >> exprN0)[([&](...) { cp.ProdBinaryOper(token::lt);})]
				);

			exprN0 
				=	term
				>> *(
					(PLUS  >> assert_d("term expected after '+'")[term])[([&](...) { cp.ProdBinaryOper(token::add);})]
				|	(MINUS >> assert_d("term expected after '-'")[term])[([&](...) { cp.ProdBinaryOper(token::sub);})]
				);

			term	// expr1
				 =	factor
				>> *(   
					(STAR    >> assert_d("factor expected after '*'")[factor])[([&](...) { cp.ProdBinaryOper(token::mul);})]
				|	(SLASH   >> assert_d("factor expected after '/'")[factor])[([&](...) { cp.ProdBinaryOper(token::div);})]
				|	(PERCENT >> assert_d("factor expected after '%'")[factor])[([&](...) { cp.ProdBinaryOper(token::mod);})]
				);

			factor
				=	pow_element 
				>>* (
						(POWER   >> pow_element)
							[([&](...) { cp.ProdBinaryOper(GetTokenID_mt("pow"));})]
					);

			pow_element
				=	compound_element 
				>>*	(
						(LBRACK  >> expression>> RBRACK)
					[([&](...) { cp.ProdBracketedExpr();})]
					);

			compound_element
				=	element 
				>>*	(
						(DOT     >> element)
							[syntaxError("dotted subItem access in expression syntax NYI")]
					|	(EXCLAIM >> element)
						[([&](...) { cp.ProdBinaryOper(token::subitem);})]
					| ((ARROW [([&](...){ cp.RefocusAfterArrow(); })] ) >> element)
						[([&](...) { cp.ProdArrow(); })]
					);

			element = // expr4
				numericValueElement
				| stringValueElement
				| functionCallOrIdentifier // expr5
				| bool_literal
				//				|	dots
				| (LBRACK >> exprList >> RBRACK)[syntaxError("value-array syntax in expression NYI")]
				| (LPAREN
					>> assert_d("sub-expression expected after '('")[expression]
					>> assert_d("')' expected after sub-expresion after '('")[RPAREN]
					)
				| (PLUS >> element)
				| (MINUS >> element)[([&](...) { cp.ProdUnaryOper(token::neg);})]
				| (FENCE >> element)[([&](...) { cp.ProdUnaryOper(token::NrOfRows);})]
				;

			numericValueElement // expr4
				=	unsignedReal  >> !suffix
				| unsignedInteger >> (suffix | epsilon_p[([&](...) { cp.ProdUInt32WithoutSuffix();})]);

			stringValueElement // expr 4
				= m_StringDef.string_value[([&](...) { cp.ProdStringValue();})];

 			suffix
				=	lexeme_d[itemName_p]
					[([&](auto _1, auto _2) { cp.ProdSuffix(_1, _2);})];

			exprList 
				= epsilon_p[([&](...) { cp.StartExprList();})]
				>>	(	nonEmptyExprList
					|	epsilon_p
					)
				>> epsilon_p[([&](...) { cp.CloseExprList();})];
			
			nonEmptyExprList
				=	expression % COMMA;

			functionCallOrIdentifier
				 = identifier 
				 >> ! 
					( LPAREN 
						>> exprList 
						>> assert_d("')' or ',' expected after argument-list of function-call")[RPAREN]
						)[([&](...) { cp.ProdFunctionCall();})];
					
			identifier
				= (lexeme_d[ +DOT || +(!SLASH >> itemName_p) ] - as_lower_d[keywords])
				[([&](auto _1, auto _2) { cp.ProdIdentifier(_1, _2);})];

			unsignedInteger
				= uint_p[([&](auto _1) { cp.ProdUInt32(_1);})];

			unsignedReal
				= strict_ureal_p[([&](auto _1) { cp.ProdFloat64(_1);})];

			bool_literal
				= as_lower_d[TRUE][([&](...) { cp.ProdNullaryOper(token::true_);})]
				| as_lower_d[FALSE][([&](...) { cp.ProdNullaryOper(token::false_);})];
			//-----------------------------------------------------------------
			//  End grammar definition
			//-----------------------------------------------------------------
		}


		boost::spirit::rule<ScannerT> const& start() const { return expression; }

		boost::spirit::symbols<> keywords;
		boost::spirit::rule<ScannerT>
			expression, exprLW, exprL0, exprL1, exprL2, exprL3, exprL4, exprN0, term, factor, pow_element, compound_element, 
			element, numericValueElement, suffix, stringValueElement,
			exprList, 
			nonEmptyExprList,
			functionCallOrIdentifier, identifier, // dots, 
			unsignedInteger, unsignedReal, bool_literal;
	};
};

#endif //!defined(__STX_EXPRPARSE_H)
