// Copyright (C) 1998-2025 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif


#if !defined( __STX_PARSEDATABLOCK_H)
#define __STX_PARSEDATABLOCK_H

#include "mci/ValueClassID.h"

#include "DataBlockProd.h"
#include "StringParse.h"

#include "SpiritTools.h"

///////////////////////////////////////////////////////////////////////////////
//
//  The DMS C like White Space Skipper
//
///////////////////////////////////////////////////////////////////////////////

struct comment_skipper : public boost::spirit::grammar<comment_skipper>
{
    comment_skipper() {}

    template <typename ScannerT>
    struct definition
    {
        definition(comment_skipper const& /*self*/)
        {
            skip
                =   boost::spirit::space_p
                |   "//" >> *(boost::spirit::anychar_p - boost::spirit::eol_p)        //  c-style comment 1, or use: comment_p("//", '\n')
                |   "/*" >> *(boost::spirit::anychar_p - "*/") >> "*/" //  c-style comment 2, or use: comment_p("/*", "*/")
            ;
        }

        boost::spirit::rule<ScannerT> const& start() const { return skip; }

        boost::spirit::rule<ScannerT>  skip;
    };
};

///////////////////////////////////////////////////////////////////////////////
//
//  datablock_grammar
//
///////////////////////////////////////////////////////////////////////////////


struct datablock_grammar : public boost::spirit::grammar<datablock_grammar>
{
	AbstrDataBlockProd& m_DataBlockProd;

	datablock_grammar(AbstrDataBlockProd& dbp) : m_DataBlockProd(dbp) {}


	template <typename ScannerT>
	struct definition
	{
		string_definition<ScannerT, StringProd> m_StringDef;

		definition(datablock_grammar const& dbg)
			: m_StringDef(dbg.m_DataBlockProd.m_StringVal)
		{
			AbstrDataBlockProd& currDBP = dbg.m_DataBlockProd;

			using boost::spirit::chlit;
			using boost::spirit::assign;
			using boost::spirit::as_lower_d;
			using boost::spirit::uint_p;
			using boost::spirit::epsilon_p;
			using boost::spirit::strict_ureal_p;

			// ==== Some Tokens
			chlit<> LBRACE('{');
			chlit<> RBRACE('}');
			chlit<> LBRACK('[');
			chlit<> RBRACK(']');
			chlit<> LPAREN('(');
			chlit<> RPAREN(')');
			chlit<> PERCENT('%');

			chlit<> PLUS('+');
			chlit<> MINUS('-');
			chlit<> EQUAL('=');

			// ==== === Data

			dataBlockProp
				= (
					LBRACK
					>> (!arrayAssignments)
					>> assert_d("']' or literals expected after opening bracket")
					[RBRACK]
					);

			// ==== Array Assingments

			arrayAssignments = arrayAssignment >> *(',' >> arrayAssignment);
			arrayAssignment = basicValue[([&](...) { currDBP.DoArrayAssignment(); })];

			// ==== Basic elements

			basicValue
				= numericValue
				| m_StringDef.string_value[([&](...) { currDBP.SetValueType(ValueClassID::VT_SharedStr); })]
				| rgbValue
				| pointValue
				| xy_pointValue
				| yx_pointValue
				| boolValue
				| as_lower_d["null"][([&](...) { currDBP.DoNullValue(); })]
				;

			pointValue
				= LPAREN
				>> numericValue[([&](...) { currDBP.DoFirstPointValue(); })]
				>> ','
				>> numericValue[([&](...) { currDBP.DoSecondPointValue(); })]
				>> assert_d("')' after point value expected")[RPAREN]
				[([&](...) { currDBP.SetValueType(ValueClassID::VT_DPoint); })];

			xy_pointValue
				= as_lower_d["xy"] >> LPAREN
				>> assert_d("xy(numeric, numeric) expected")
					[  numericValue[([&](...) { currDBP.DoPointXValue(); })]
					>> ','
					>> numericValue[([&](...) { currDBP.DoPointYValue(); })]
					]
				>> assert_d("')' after x and y values expected")[RPAREN]
					[([&](...) { currDBP.SetValueType(ValueClassID::VT_DPoint); })];

			yx_pointValue
				= as_lower_d["yx"] >> LPAREN
				>> assert_d("yx(numeric, numeric) expected")
					[  numericValue[([&](...) { currDBP.DoPointYValue(); })]
					>> ','
					>> numericValue[([&](...) { currDBP.DoPointXValue(); })]
					]
				>> assert_d("')' after y and x values expected")[RPAREN]
					[([&](...) { currDBP.SetValueType(ValueClassID::VT_DPoint); })];

			rgbValue
				=	as_lower_d["rgb"] >> LPAREN
				>>	assert_d("rgb(RedValue, GreenValue, BlueValue) expected")
					[	uint_p[([&](UInt32 p) { currDBP.DoRgbValue1(p);})]
					>>	',' 
					>>	uint_p[([&](UInt32 p) { currDBP.DoRgbValue2(p);})]
					>>	',' 
					>>	uint_p[([&](UInt32 p) { currDBP.DoRgbValue3(p);})]
					]
				>> assert_d("')' after rgb(RedValue, GreenValue, BlueValue) expected")
					[ RPAREN ];


			numericValue 
				=	(	MINUS    [([&](...) { currDBP.SetSign(false);})]
					|	PLUS     [([&](...) { currDBP.SetSign(true);})]
					|	epsilon_p[([&](...) { currDBP.SetSign(true);})]
					)
				>>	( floatValue
					| unsignedInteger
					);

			floatValue = strict_ureal_p[([&](Float64 v) { currDBP.DoFloatValue(v);})];
			unsignedInteger = (uint64_p[([&](auto u64) { currDBP.DoUInt64(u64); })])
				| (PERCENT >> (hex64_p[([&](auto u64) { currDBP.DoUInt64(u64); })]));
				
			boolValue
				= as_lower_d["true" ][([&](...) { currDBP.DoBoolValue(true );})]
				| as_lower_d["false"][([&](...) { currDBP.DoBoolValue(false);})];

			main = dms_guard_d( dataBlockProp );
		}

		boost::spirit::rule<ScannerT>
			main, dataBlockProp, 
			arrayAssignments, 
			arrayAssignment, 
			basicValue, rgbValue, pointValue, xy_pointValue, yx_pointValue,
			numericValue, floatValue, unsignedInteger, 
			boolValue;

		boost::spirit::rule<ScannerT> const& start() const { return main; }
	};
};


#endif
