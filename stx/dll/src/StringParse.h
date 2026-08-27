// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////


#if !defined(__STX_PARSESTRING_H)
#define __STX_PARSESTRING_H

#include "SpiritTools.h"
#include "StringProd.h"


///////////////////////////////////////////////////////////////////////////////
//
//  Functors for Production of string grammar result
//
///////////////////////////////////////////////////////////////////////////////

template <typename StringProdType>
struct ProdStringLiteral1 : WeakPtr<StringProdType>
{ 
	ProdStringLiteral1(StringProdType* csp)
		: WeakPtr<StringProdType>(csp) 
	{}

	template<typename IteratorT>
	void operator()(IteratorT first, IteratorT last) const
	{
		if constexpr (requires { first.get_position(); }) // position_iterator: tell where a warning applies
		{
			auto pos = first.get_position();
			(*this)->ProdStringLiteral1(&*first, &*last, &pos);
		}
		else
			(*this)->ProdStringLiteral1(&*first, &*last);
	}
};

template <typename StringProdType>
struct ProdStringLiteral2 : WeakPtr<StringProdType>
{ 
	ProdStringLiteral2(StringProdType* csp)	: WeakPtr<StringProdType>(csp) {}

	template<typename IteratorT>
	void operator()(IteratorT first, IteratorT last) const
	{
		if constexpr (requires { first.get_position(); }) // position_iterator: tell where a warning applies
		{
			auto pos = first.get_position();
			(*this)->ProdStringLiteral2(&*first, &*last, &pos);
		}
		else
			(*this)->ProdStringLiteral2(&*first, &*last);
	}
};

///////////////////////////////////////////////////////////////////////////////
//
//  multi purpose string grammar
//
///////////////////////////////////////////////////////////////////////////////


template <typename ScannerT, typename StringProdType>
struct string_definition
{
	string_definition(StringProdType& sp)
	{
		using boost::spirit::lexeme_d;
		using boost::spirit::anychar_p;
		using boost::spirit::chlit;
		using boost::spirit::strlit;

		//-----------------------------------------------------------------
		//  Start grammar definition
		//-----------------------------------------------------------------

		string_value
			= string_literal[([&](...) { sp.ProdFirstStringValue();})]
			>> *(string_literal[([&](...) { sp.ProdNextStringValue();})]);

		string_literal
			= string_literal1 | string_literal2;
		
		string_literal1
			= lexeme_d[ 
					chlit<>('\'') 
				>> (
						*	( 
								strlit<>("\'\'") 
							|	anychar_p-chlit<>('\'') 
							)
					)[ ProdStringLiteral1(&sp) ]
				>> assert_d("string terminator expected")[chlit<>('\'') ] 
				];

		string_literal2
			= lexeme_d[ 
					chlit<>('\"') 
				>>	(
						*	(
								( chlit<>('\\')  >> anychar_p )
							|	anychar_p-chlit<>('\"')
							)
					)[ ProdStringLiteral2(&sp) ]
				>> assert_d("string terminator expected")[chlit<>('\"')] 
				];

		//-----------------------------------------------------------------
		//  End grammar definition
		//-----------------------------------------------------------------
	}
	boost::spirit::rule<ScannerT>
		string_value, string_literal, string_literal1, string_literal2;
};


template <typename Prod>
struct string_grammar : public boost::spirit::grammar<string_grammar<Prod>>
{
	string_grammar(Prod&) {}

	template <typename ScannerT>
		struct definition : string_definition<ScannerT, Prod>
	{
		definition(string_grammar const&) {}

		boost::spirit::rule<ScannerT> const& start() const { return this->string_value; }
	};
};


#endif //!defined(__STX_PARSESTRING_H)

