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

