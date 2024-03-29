// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#pragma once

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
			using boost::spirit::hex_p;
			using boost::spirit::strict_ureal_p;

			//-----------------------------------------------------------------
			// OPERATORS
			//-----------------------------------------------------------------
			chlit<>     COMMA(',');
			chlit<>     UNDERSCORE('_');

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
			strlit<>    P_SCOPE("scope");


			chlit<>     C_IF('?');
			chlit<>     C_ELSE(':');

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
				| scopeCall // requires specific production at html, not for keyExpr generation
				| functionCallOrIdentifier // expr5
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
					[([&](auto first, auto last) { cp.ProdSuffix(first, last);})];

			exprList 
				= epsilon_p[([&](...) { cp.StartExprList();})]
				>>	(	nonEmptyExprList
					|	epsilon_p
					)
				>> epsilon_p[([&](...) { cp.CloseExprList();})];
			
			nonEmptyExprList
				=	expression % COMMA;

//			if (mustScanScope)
				scopeCall 
					= P_SCOPE 
					>> LPAREN >> identifier[([&](...) { cp.RefocusAfterScope(); })]
					>> COMMA  >> expression
					>> RPAREN [([&](...) { cp.ProdScope(); })];

			functionCallOrIdentifier
				 = identifier 
				 >> ! 
					( LPAREN 
						>> exprList 
						>> assert_d("')' or ',' expected after argument-list of function-call")[RPAREN]
						)[([&](...) { cp.ProdFunctionCall();})];
					
			identifier
				= (lexeme_d[ +DOT || +(!SLASH >> itemName_p) ])
				[([&](auto first, auto last) { cp.ProdIdentifier(first, last);})];

			unsignedInteger
				= (uint64_p[([&](auto u64) { cp.ProdUInt64(u64);})])
			| (PERCENT >> (hex64_p[([&](auto u64) { cp.ProdUInt64(u64); })]));

			unsignedReal
				= strict_ureal_p[([&](auto f64) { cp.ProdFloat64(f64);})];

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
			scopeCall, functionCallOrIdentifier, identifier, // dots, 
			unsignedInteger, unsignedReal;
	};
};

#endif //!defined(__STX_EXPRPARSE_H)
