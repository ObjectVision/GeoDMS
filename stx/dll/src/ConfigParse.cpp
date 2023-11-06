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
#include "StxPch.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

#include "geo/StringBounds.h"
#include "mci/ValueComposition.h"
#include "set/FileView.h"
#include "utl/Environment.h"
#include "utl/mySPrintF.h"

#include "SpiritTools.h"
#include "ConfigProd.h"
#include "DataBlockParse.h"
#include "ExprParse.h"

#include <time.h>
///////////////////////////////////////////////////////////////////////////////
//
//  Our dms grammar
//
///////////////////////////////////////////////////////////////////////////////

struct config_grammar : public boost::spirit::grammar<config_grammar>
{
	ConfigProd&  m_ConfigProd;

	datablock_grammar           m_DataBlockGrammar;
	expr_grammar<EmptyExprProd> m_ExprGrammar;


	config_grammar(ConfigProd& cp)
		: m_ConfigProd(cp)
		, m_DataBlockGrammar(cp)
		, m_ExprGrammar(cp.m_ExprProd)
	{}

	template <typename ScannerT>
	struct definition
	{
		using datablock_definition_t = datablock_grammar::definition<ScannerT>;
		using expr_definition_t = expr_grammar<EmptyExprProd>::definition<ScannerT>;

		datablock_definition_t m_DataBlockDef;
		expr_definition_t      m_ExprDef;

		definition(config_grammar const& cg)
			: m_DataBlockDef(cg.m_DataBlockGrammar)
			, m_ExprDef(cg.m_ExprGrammar)
		{
			using boost::spirit::chlit;
			using boost::spirit::strlit;
			using boost::spirit::as_lower_d;
			using boost::spirit::lexeme_d;
			using boost::spirit::alpha_p;
			using boost::spirit::alnum_p;
			using boost::spirit::pizza_p;
			using boost::spirit::assign;

			// ==== Some Tokens

			chlit<> LBRACE('{');
			chlit<> RBRACE('}');
			chlit<> LPAREN('(');
			chlit<> RPAREN(')');
			chlit<> LBRACK('[');
			chlit<> RBRACK(']');
			chlit<> UNDERSCORE('_');
			chlit<> SLASH('/');
			chlit<> BACKSLASH('\\');
			chlit<> DOT('.');
			chlit<> SPACE(' ');
			chlit<> HEKJE('#');
			chlit<> PERCENT('%');
			chlit<> COLON(':');
			chlit<> SEMICOLON(';');
			chlit<> EQUAL('=');

			strlit<> CONTAINER("container");
			strlit<> TEMPLATE("template");
			strlit<> ATTRIBUTE("attribute");
			strlit<> PARAMETER("parameter");
			strlit<> UNIT("unit");
			strlit<> ENTITY("entity");
			strlit<> UTF8_BOM("\xEF\xBB\xBF");

			// ==== ITEM

			ConfigProd& cp = cg.m_ConfigProd;

			main
				= (! UTF8_BOM ) >> dms_guard_d(item);

			//<item> ::=
			//	<item decl> <item block> | include file
			item
				= (
					itemDecl
					>> assert_d("item terminator ';' expected after item definition")[
						SEMICOLON
							| itemBlock
					]
					)
				| (HEKJE
					>> assert_d("preprocessor directive expected")[
						preprocStatement
					]
					);

						preprocStatement
							= "include"
							>> assert_d("'<' relative filename '>' expected after include directive")[
								'<'
									>> fileRef
									>> '>'
							][([&cp](...) { cp.DoInclude();})];


								//<item block> ::=	<begin block> <items> <end block> | <empty>
								itemBlock =
									LBRACE
									[([&cp](...) { cp.DoBeginBlock();})]
								>> *item
									>> assert_d("item definition or block terminator '}' expected")[RBRACE]
									[([&cp](...) { cp.DoEndBlock(); })];

								// ==== ITEM DECL

								itemDecl = (itemHeading
									>> !(COLON >> itemProp >> *(',' >> itemProp)))
									//[ ([&cp](auto,auto) { cp.OnItemDecl(); }) ];
									[([&cp](...) { cp.OnItemDecl();})];

								// ==== ==== ITEM HEADING

								itemHeading =
									(itemSignature
										>> identifier[([&cp](...) { cp.DoItemName();})]
										>> !(LPAREN >> itemParam >> *(',' >> itemParam) >> RPAREN)
										)
									[([&cp](auto _1, auto _2) { cp.DoItemHeading(_1, _2);})];

								itemSignature =
									as_lower_d[CONTAINER][([&cp](...) { cp.SetSignature(SignatureType::TreeItem);})]
									| as_lower_d[TEMPLATE][([&cp](...) { cp.SetSignature(SignatureType::Template);})]
									| (as_lower_d[ATTRIBUTE] >> '<' >> unitIdentifier >> '>')[([&cp](...) { cp.DoAttrSignature();})]
									| (as_lower_d[PARAMETER] >> '<' >> unitIdentifier >> '>')[([&cp](...) { cp.SetSignature(SignatureType::Parameter);})]
									| (as_lower_d[UNIT] >> '<' >> basicType >> '>')[([&cp](...) { cp.SetSignature(SignatureType::Unit);})]
									| (as_lower_d[ENTITY])[([&cp](...) { cp.DoEntitySignature();})]
									;

								//<unit identifier> ::=	<item ref>
								unitIdentifier = itemRef[([&cp](...) { cp.DoUnitIdentifier(); })];

								//<basic type> ::= IDENTIFIER
								basicType = identifier[([&cp](...) { cp.DoBasicType(); })];

								itemRef
									= lexeme_d[+DOT || +(!SLASH >> itemName_p)]
									[([&cp](auto _1, auto _2) { cp.ProdIdentifier(_1, _2);})];

								fileRef
									= lexeme_d[+(alnum_p | UNDERSCORE | SLASH | BACKSLASH | DOT | SPACE | COLON | PERCENT)]
									[([&cp](auto _1, auto _2) { cp.ProdIdentifier(_1, _2);})]
								| m_DataBlockDef.m_StringDef.string_value
									[([&cp](...) { cp.ProdQuotedIdentifier(); })];

								identifier = lexeme_d[itemName_p]
									[([&cp](auto _1, auto _2) { cp.ProdIdentifier(_1, _2);})];

								// ==== ==== ITEM PARAMS
								itemParam =
									pizza_p("arc")[([&cp](...) { cp.SetVC(ValueComposition::Sequence); })]
									| pizza_p("polygon")[([&cp](...) { cp.SetVC(ValueComposition::Polygon); })]
									| pizza_p("poly")[([&cp](...) { cp.SetVC(ValueComposition::Polygon); })]
									| itemRef[([&cp](...) { cp.DoEntityParam(); })];


								// ==== ==== ITEM PROP LIST

								itemProp
									= assert_d("property definition expected")[
										anyProp
											| storageProp
											| usingProp
											| entityNrOfRowsProp
											| directExpr
											| dataBlock
									];

								anyPropImpl
									= (
										identifier
										>> '='
										>> m_DataBlockDef.m_StringDef.string_value
										);
								anyProp
									= anyPropImpl[([&cp](...) { cp.DoAnyProp(); })]; // action at anyprop gives weird error

								storageProp
									= pizza_p("storage")
									>> '='
									>> identifier[([&cp](...) { cp.DoFileType(); })]
									>> assert_d("property format 'storage = FILETYPE ( FILENAME )'  expected")
									['(' >> fileRef >> ')']
								[([&cp](...) { cp.DoStorageProp(); })];

								usingProp = (as_lower_d["using"]
									>> EQUAL
									>> itemRef
									)[([&cp](...) { cp.DoUsingProp(); })];

								//<entity nr of rows prop> ::= nrofrows = <integer value> ;
								entityNrOfRowsProp = (as_lower_d["nrofrows"]
									>> EQUAL[([&cp](...) { cp.SetSign(true); })]
									>> m_DataBlockDef.unsignedInteger
									)[([&cp](...) { cp.DoNrOfRowsProp(); })];

								directExpr = EQUAL >> m_ExprDef.start()[([&cp](auto _1, auto _2) { cp.DoExprProp(_1, _2);})];
								dataBlock = m_DataBlockDef.dataBlockProp[([&cp](auto _1, auto _2) {cp.DataBlockCompleted(_1, _2); })];
		};

		boost::spirit::rule<ScannerT> const& start() const { return main; }

		boost::spirit::rule<ScannerT> main, item, itemBlock, 
			preprocStatement,
			itemDecl, itemHeading, itemSignature, 
			itemParam, unitIdentifier, basicType,
			itemProp, anyPropImpl, anyProp, storageProp, usingProp, entityNrOfRowsProp, dataBlock, directExpr,
			itemRef, fileRef, identifier;
	};
};

TreeItem* ConfigProd::ParseString(CharPtr configString)
{
	AuthErrorDisplayLock recursionLock;

	CharPtr configStringEnd = configString + StrLen(configString);
	try {

		parse_info_t info
			=	boost::spirit::parse(
					iterator_t(configString, configStringEnd, position_t())
				,	iterator_t() 
				,	config_grammar(*this) >> boost::spirit::end_p
				,	comment_skipper()
				);
		CheckInfo(info);
		dms_assert(!s_AuthErrorDisplayLockCatchCount); // should have resulted in throw, thus catch below
	}
	catch (const parser_error_t& problem)
	{
		++s_AuthErrorDisplayLockCatchCount;

		SharedStr strAtProblemLoc = problemlocAsString(configString, configStringEnd, &*problem.where);

		position_t  problemLoc = problem.where.get_position();
		auto fullDescr = mySSPrintF("%s\n%s(%d,%d) at\n%s"
			,	problem.descriptor
			,	"ConfigParse FromString", problemLoc.line, problemLoc.column
			,	strAtProblemLoc.c_str()
			);
		DmsException::throwMsgD(fullDescr);
	}
	dbg_assert(CurrentIsTop());
	if (s_AuthErrorDisplayLockCatchCount)
		return nullptr;
	m_ResultCommitted = true;
	return m_pCurrent;
}

TreeItem* ConfigProd::ParseFile(CharPtr fileName)
{
	AuthErrorDisplayLock recursionLock;

	MappedConstFileMapHandle fv(SharedStr(fileName), nullptr, true, false); // SFWA
	try {

		parse_info_t info
			=	boost::spirit::parse(
					iterator_t(fv.DataBegin(), fv.DataEnd(), position_t())
				,	iterator_t()
				,	config_grammar(*this) >> boost::spirit::end_p
				,	comment_skipper()
				);
		CheckInfo(info);
		dms_assert(!s_AuthErrorDisplayLockCatchCount); // should have resulted in throw, thus catch below
	}
	catch (const parser_error_t& problem)
	{
		++s_AuthErrorDisplayLockCatchCount;

		SharedStr strAtProblemLoc = problemlocAsString(fv.DataBegin(), fv.DataEnd(), &*problem.where);

		fv.CloseMCFMH(); // enable user to change and save the file from error display and the press Reload

		position_t  problemLoc = problem.where.get_position();
		auto fullDescr = mySSPrintF("%s\n%s(%d,%d) at\n%s"
		,	problem.descriptor
		,	fileName, problemLoc.line, problemLoc.column
		,	strAtProblemLoc.c_str()
		);
		DmsException::throwMsgD(fullDescr);
	}
	dbg_assert(CurrentIsTop());
	if (s_AuthErrorDisplayLockCatchCount)
		return nullptr;
	m_ResultCommitted = true;
	return m_pCurrent;
}
