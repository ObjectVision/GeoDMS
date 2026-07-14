// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

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
			using boost::spirit::epsilon_p;

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
			strlit<> FUNCTION("function");
			strlit<> VARIANT("variant");
			strlit<> ATTRIBUTE("attribute");
			strlit<> PARAMETER("parameter");
			strlit<> UNIT("unit");
			strlit<> ENTITY("entity");
			strlit<> USING("using");
			strlit<> ARROW("->");
			strlit<> UTF8_BOM("\xEF\xBB\xBF");

			// ==== ITEM

			ConfigProd& cp = cg.m_ConfigProd;

			main
				= (! UTF8_BOM ) >> dms_guard_d(+item); // multiple item declarations are only allowed in #included sub-configurations

			//<item> ::=
			//	<function decl> | <alias decl> | <anonymous fn decl> | <item decl> <item block> | include file
			item
				= functionDecl
				| aliasFunctionSig
				| aliasPlain
				| anonFnDecl
				| bareExprDecl
				| (
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

			itemDecl = ((itemHeading | colonHeading)
				>> !(COLON >> itemProp >> *(',' >> itemProp)))
				//[ ([&cp](auto,auto) { cp.OnItemDecl(); }) ];
				[([&cp](...) { cp.OnItemDecl();})];

			// name:type declaration style: 'name1, name2 : <type>' where <type> is a
			// classic signature (unit<vt>, attribute<vu>, parameter<vu>, container, ...)
			// or, inside function declarations, an item reference to a composite type
			colonHeading =
				( identifier[([&cp](...) { cp.StartPendingNames();})]
					>> *( ',' >> identifier[([&cp](...) { cp.AddPendingName();})] )
					>> COLON
					>> ( itemSignature
					   | (itemRef[([&cp](...) { cp.DoRefTypeSignature();})] >> typeArgsOpt) )
					>> !(LPAREN >> itemParam >> *(',' >> itemParam) >> RPAREN)
				)
				[([&cp](auto _1, auto _2) { cp.DoColonItemHeading(_1, _2);})];

			// ==== FUNCTION DECL
			// function F(<param decls>) -> [name:] <result type> := <result expr> [;] [{ <body items> }]

			// generic type variables: function f<V: numerics>(...)
			chlit<> LANGLE('<');
			chlit<> RANGLE('>');
			typeParamsClause =
				LANGLE
				>> (identifier[([&cp](...) { cp.OnTypeVarName();})]
					>> COLON
					>> assert_d("type-variable constraint expected after ':'")[identifier]
					)[([&cp](...) { cp.OnTypeVarConstraint();})]
				>> *( ',' >> (identifier[([&cp](...) { cp.OnTypeVarName();})]
					>> COLON
					>> assert_d("type-variable constraint expected after ':'")[identifier]
					)[([&cp](...) { cp.OnTypeVarConstraint();})] )
				>> assert_d("'>' expected after type-variable declarations")[RANGLE];

			// type application: 'sig<V, D>' — each argument must name an active type
			// variable (documentation-level in v1, §5.10 Stage 2)
			typeArgsOpt =
				!( LANGLE
					>> identifier[([&cp](...) { cp.OnParamSigTypeArg();})]
					>> *( ',' >> identifier[([&cp](...) { cp.OnParamSigTypeArg();})] )
					>> assert_d("'>' expected after type-application arguments")[RANGLE] );

			functionDecl =
				(as_lower_d[FUNCTION] >> identifier[([&cp](auto _1, auto) { cp.DoItemName(); cp.SetPendingNameLoc(_1);})])
				>> ( functionBody | variantSet );

			// §5.11 anonymous whole-rule function: value := function[<typevars>](params) ...
			// — the declared item IS the function (identical to 'function value(params) ...'),
			// so a following unbracketed block is unambiguously its body/sub-item block
			anonFnDecl =
				( (identifier >> COLON >> EQUAL >> epsilon_p(as_lower_d[FUNCTION] >> (LANGLE | LPAREN)))
					[([&cp](auto _1, auto) { cp.DoItemName(); cp.SetPendingNameLoc(_1);})]
				>> as_lower_d[FUNCTION]
				>> functionBody );

			// §5.12 auto-typed declaration: name := expr; — a plain item whose type
			// follows from its calculation (the DC layer derives class and domain;
			// inside function bodies the definition-time walker infers it). Ordered
			// AFTER anonFnDecl, which claims ':= function' literals.
			bareExprDecl =
				( (identifier >> COLON >> EQUAL)
					[([&cp](auto _1, auto) { cp.OnBareExprHeading(_1); })]
				>> m_ExprDef.start()[([&cp](auto _1, auto _2) { cp.DoExprProp(_1, _2);})]
				>> assert_d("';' expected after auto-typed item definition")[SEMICOLON] );

			// header through result spec, shared by the named, anonymous and
			// result-position forms; OnFunctionDeclEnd placement differs (it must run
			// AFTER an optional body block, so the block's items can be designated)
			functionSigAndResult =
				(!typeParamsClause >> LPAREN)
					[([&cp](auto _1, auto) { cp.OnFunctionHeading(_1); cp.DoBeginBlock();})]
				>> !(functionParamItem >> *(SEMICOLON >> !functionParamItem))
				>> assert_d("')' expected after function parameter declarations")[RPAREN]
					[([&cp](...) { cp.OnEndFunctionParams();})]
				>> *( ',' >> as_lower_d[USING] >> EQUAL
						>> assert_d("namespace reference expected after 'using ='")[
							itemRef[([&cp](...) { cp.DoFunctionUsing();})] ] )
				>> assert_d("'->' and result specification expected after function parameter list")[ARROW]
				>> functionResultSpec;

			// single function: function name[<typevars>](params) -> result := expr [;] [{ body }] [;]
			functionBody =
				( functionSigAndResult
				>> !SEMICOLON
				>> !( LBRACE
						>> *item
						>> assert_d("item definition or function-body terminator '}' expected")[RBRACE] )
				>> !SEMICOLON // tolerated after the block: the anonymous form reads like an assignment
				)
				[([&cp](auto _1, auto) { cp.OnFunctionDeclEnd(_1);})];

			// §5.11 result-position anonymous function (':= function(params) -> T := e;'):
			// declares the nested function under the enclosing function's result name.
			// No brace tail here — per the disambiguation rule, an unbracketed '{' after
			// the rule belongs to the ENCLOSING function's body block; the literal's
			// locals live there.
			anonResultFunction =
				( (as_lower_d[FUNCTION] >> epsilon_p(LANGLE | LPAREN))
					[([&cp](auto _1, auto) { cp.OnAnonResultFunction(); cp.SetPendingNameLoc(_1);})]
				>> functionSigAndResult
				>> !SEMICOLON
				)
				[([&cp](auto _1, auto) { cp.OnFunctionDeclEnd(_1);})];

			// variant set: function name { variant v1(params) -> R := expr; variant v2(...) ... }
			// dispatches to the matching variant by argument type (§5.7)
			variantSet =
				LBRACE[([&cp](...) { cp.OnVariantSetHeading(); cp.DoBeginBlock();})]
				>> assert_d("at least one 'variant' declaration expected")[+variantDecl]
				>> assert_d("'variant' or '}' expected in variant set")[RBRACE]
					[([&cp](...) { cp.OnVariantSetEnd();})];

			variantDecl =
				( (as_lower_d[VARIANT] >> identifier[([&cp](...) { cp.DoItemName();})] >> !typeParamsClause >> LPAREN)
					[([&cp](auto _1, auto) { cp.OnFunctionHeading(_1); cp.DoBeginBlock();})]
				>> !(functionParamItem >> *(SEMICOLON >> !functionParamItem))
				>> assert_d("')' expected after variant parameter declarations")[RPAREN]
					[([&cp](...) { cp.OnEndFunctionParams();})]
				>> *( ',' >> as_lower_d[USING] >> EQUAL
						>> assert_d("namespace reference expected after 'using ='")[
							itemRef[([&cp](...) { cp.DoFunctionUsing();})] ] )
				>> assert_d("'->' and result specification expected after variant parameter list")[ARROW]
				>> functionResultSpec
				>> !SEMICOLON
				>> !( LBRACE
						>> *item
						>> assert_d("item definition or variant-body terminator '}' expected")[RBRACE] )
				)
				[([&cp](auto _1, auto) { cp.OnFunctionDeclEnd(_1);})];

			functionParamItem =
				itemDecl[([&cp](...) { cp.OnFunctionParamDecl();})] // count BEFORE the member block: declarations inside it overwrite the multi-name count
				>> !itemBlock;

			functionResultType =
				( ( as_lower_d[FUNCTION][([&cp](...) { cp.OnFunctionResultIsFunction();})] // §5.10 function-valued result
				  | itemSignature
				  | (itemRef[([&cp](...) { cp.DoRefTypeSignature();})] >> typeArgsOpt) )
					>> !(LPAREN >> itemParam >> *(',' >> itemParam) >> RPAREN)
				)
				[([&cp](...) { cp.OnFunctionResultSig();})];

			// ':= expr' is optional in the grammar: without it, OnFunctionDeclEnd
			// designates a body item by the result name (default 'result') — for
			// '-> function' AND for data results followed by a body block (§5.11).
			// ':= function(...)' declares an anonymous result-position function.
			functionResultSpec =
				!( (identifier >> COLON)[([&cp](...) { cp.DoFunctionResultName();})] )
				>> assert_d("function result type expected after '->'")[functionResultType]
				>> !( (COLON >> EQUAL)
					>> ( anonResultFunction
					   | m_ExprDef.start()[([&cp](auto _1, auto _2) { cp.OnFunctionResultExpr(_1, _2);})] ) );

			// ==== TYPE ALIASES: alias = type;   (vs. 'name : type;' which declares an item)

			// function-signature alias: alias = [function[<typeVars>]] (params) -> resultType;
			// parameters may be anonymous type specs (a positional name is synthesized)
			aliasFunctionSig =
				( (identifier >> EQUAL)[([&cp](...) { cp.OnAliasName();})] // capture the name BEFORE typeParamsClause overwrites m_strIdentifierID
				>> !(as_lower_d[FUNCTION] >> !typeParamsClause)
				>> (LPAREN >> epsilon_p)
					[([&cp](auto _1, auto) { cp.OnFunctionSigHeading(_1); cp.DoBeginBlock();})]
				>> !(functionSigParamItem >> *(SEMICOLON >> !functionSigParamItem))
				>> assert_d("')' expected after signature parameter declarations")[RPAREN]
					[([&cp](...) { cp.OnEndFunctionParams();})]
				>> assert_d("'->' and result type expected in a function-signature alias")[ARROW]
				>> !( (identifier >> COLON)[([&cp](...) { cp.DoFunctionResultName();})] )
				>> assert_d("result type expected after '->'")[functionResultType]
				>> assert_d("';' expected after function-signature alias")[SEMICOLON]
				)
				[([&cp](auto _1, auto) { cp.OnFunctionDeclEnd(_1);})];

			// a signature parameter: a named declaration, or an anonymous type spec
			functionSigParamItem =
				functionParamItem
				| ( ( itemSignature
					  | itemRef[([&cp](...) { cp.DoRefTypeSignature();})] )
					>> !(LPAREN >> itemParam >> *(',' >> itemParam) >> RPAREN)
				  )[([&cp](auto _1, auto) { cp.OnAnonSigParam(_1);})];

			// plain type alias: alias = unit<vt>; | attribute<vu> [(domain)]; | previously declared item;
			// with optional refinement properties (e.g. ', IntegrityCheck = "...")
			aliasPlain =
				( (identifier >> EQUAL)[([&cp](...) { cp.OnAliasName();})]
				>> ( itemSignature
				   | itemRef[([&cp](...) { cp.DoRefTypeSignature();})] )
				>> !(LPAREN >> itemParam >> *(',' >> itemParam) >> RPAREN)
				)
				[([&cp](auto _1, auto _2) { cp.DoAliasDecl(_1, _2);})]
				>> !(',' >> itemProp >> *(',' >> itemProp))
				>> assert_d("';' expected after type alias")[SEMICOLON];

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
			itemParam = itemRef[([&cp](...) { cp.DoEntityParam(); })];


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
			itemDecl, itemHeading, itemSignature, colonHeading,
			functionDecl, functionBody, functionSigAndResult, anonFnDecl, anonResultFunction, bareExprDecl, variantSet, variantDecl,
			functionParamItem, functionSigParamItem, functionResultType, functionResultSpec, typeArgsOpt,
			typeParamsClause, aliasFunctionSig, aliasPlain,
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

		std::lock_guard<std::recursive_mutex> spiritLock(GetSpiritGrammarMutex()); // serialize Spirit grammar use (replaces BOOST_SPIRIT_THREADSAFE)
		
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
		auto fullDescr = mySSPrintF("{}\n{}({},{}) at\n{}"
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
	return m_pCurrent.get();
}

void ConfigProd::ParseNestedDeclaration(CharPtr declString)
{
	// §5.11 tier B lambda lifting: parse ONE synthesized function declaration into
	// the current context. Re-entrant: the caller is itself inside a config parse
	// (the Spirit mutex is recursive); the nested functionDecl pushes and pops its
	// own state frames, and the caller saves/restores what item declarations clobber.
	CharPtr declStringEnd = declString + StrLen(declString);
	try {

		std::lock_guard<std::recursive_mutex> spiritLock(GetSpiritGrammarMutex());

		parse_info_t info
			=	boost::spirit::parse(
					iterator_t(declString, declStringEnd, position_t())
				,	iterator_t()
				,	config_grammar(*this) >> boost::spirit::end_p
				,	comment_skipper()
				);
		CheckInfo(info);
	}
	catch (const parser_error_t& problem)
	{
		SharedStr strAtProblemLoc = problemlocAsString(declString, declStringEnd, &*problem.where);
		auto fullDescr = mySSPrintF("{}\nin anonymous function\n{}"
			,	problem.descriptor
			,	strAtProblemLoc.c_str()
			);
		DmsException::throwMsgD(fullDescr);
	}
}

TreeItem* ConfigProd::ParseFile(CharPtr fileName)
{
	AuthErrorDisplayLock recursionLock;

	m_CurrFileName = SharedStr(fileName);

	auto fv = ConstFileViewHandle(std::make_shared<ConstMappedFileHandle>(m_CurrFileName, true, false), 0, -1, -1); // SFWA
	fv.MapView();
	try {

		std::lock_guard<std::recursive_mutex> spiritLock(GetSpiritGrammarMutex()); // serialize Spirit grammar use (replaces BOOST_SPIRIT_THREADSAFE)
		
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

//		fv.CloseMCFMH(); // enable user to change and save the file from error display and the press Reload

		position_t  problemLoc = problem.where.get_position();
		auto fullDescr = mySSPrintF("{}\n{}({},{}) at\n{}"
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
	return m_pCurrent.get();
}
