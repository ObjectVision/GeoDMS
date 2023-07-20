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

#pragma once

#if !defined(__CLC_ATTRUNISTRUCTSTR_H)
#define __CLC_ATTRUNISTRUCTSTR_H

#include <ctype.h>

#include "AttrUniStruct.h"
#include "UnitCreators.h"

#include "geo/StringBounds.h"
#include "geo/StringArray.h"
#include "utl/Encodes.h"
#include "utl/Quotes.h"
#include "utl/Case.h"

#include "StxInterface.h"

// *****************************************************************************
//							STRING FUNCTOR BASES
// *****************************************************************************

struct unary_string_assign : unary_assign<SharedStr, SharedStr>
{};

struct same_length_unary_string_assign : unary_string_assign
{
	static void PrepareTile(sequence_traits<assignee_type>::seq_t res, sequence_traits<arg1_type>::cseq_t a1)
	{
		res.get_sa().data_reserve(a1.get_sa().actual_data_size() MG_DEBUG_ALLOCATOR_SRC("same_length_unary_string_assign: data_reserve"));
	}
};

// *****************************************************************************
//							UNARY STRING FUNCTORS
// *****************************************************************************

struct strlen32_func : unary_func<UInt32, SharedStr>
{
	static ConstUnitRef unit_creator(const AbstrOperGroup* gr, const ArgSeqType& args) { return default_unit_creator<UInt32>(); }

	res_type operator ()(arg1_cref arg) const
	{
		return StrLen(arg.begin(), arg.end());
	}
};

struct strlen64_func : unary_func<UInt64, SharedStr>
{
	static ConstUnitRef unit_creator(const AbstrOperGroup* gr, const ArgSeqType& args) { return default_unit_creator<UInt64>(); }

	res_type operator ()(arg1_cref arg) const
	{
		return StrLen(arg.begin(), arg.end());
	}
};

struct trim_assign : same_length_unary_string_assign
{
	static ConstUnitRef unit_creator(const AbstrOperGroup* gr, const ArgSeqType& args) { return arg1_values_unit(args); }

	void operator ()(assignee_ref res, arg1_cref arg) const
	{
		sequence_traits<char>::const_pointer
			i = arg.begin(), 
			e = arg.end();
		while (i!=e && isspace(UChar(*i)))
			++i;
		while (i!=e && isspace(UChar(*(e-1))))
			--e;
		res.assign(i, e);
	}
};

struct ltrim_assign : same_length_unary_string_assign
{
	static ConstUnitRef unit_creator(const AbstrOperGroup* gr, const ArgSeqType& args) { return arg1_values_unit(args); }

	void operator ()(assignee_ref res, arg1_cref arg) const
	{
		sequence_traits<char>::const_pointer
			i = arg.begin(), 
			e = arg.end();
		while (i!=e && isspace(UChar(*i)))
			++i;
		res.assign(i, e);
	}
};

struct rtrim_assign : same_length_unary_string_assign
{
	static ConstUnitRef unit_creator(const AbstrOperGroup* gr, const ArgSeqType& args) { return arg1_values_unit(args); }

	void operator ()(assignee_ref res, arg1_cref arg) const
	{
		sequence_traits<char>::const_pointer
			i = arg.begin(), 
			e = arg.end();
		while (i!=e && isspace(UChar(*(e-1))))
			--e;
		res.assign(i, e);
	}
};

template <class T>
struct asstring_assign : unary_assign<SharedStr, T>
{
	static ConstUnitRef unit_creator(const AbstrOperGroup* gr, const ArgSeqType& args) { return default_unit_creator<SharedStr>(); }

	void operator ()(typename asstring_assign::assignee_ref res, typename asstring_assign::arg1_cref arg) const
	{
		AsString(res, arg);
	}
};

template <class T>
struct ashex_assign : unary_assign<SharedStr, T>
{
	static ConstUnitRef unit_creator(const AbstrOperGroup* gr, const ArgSeqType& args) { return default_unit_creator<SharedStr>(); }

	void operator ()(typename ashex_assign::assignee_ref res, typename ashex_assign::arg1_cref arg) const
	{
		AsHex(res, arg);
	}
};

struct quote_assign: unary_assign<SharedStr, SharedStr>
{
	static ConstUnitRef unit_creator(const AbstrOperGroup* gr, const ArgSeqType& args) { return arg1_values_unit(args); }

	void operator ()(assignee_ref res, arg1_cref arg) const
	{
		SingleQuote(res, begin_ptr( arg ), end_ptr( arg ));
	}
};

struct unquote_assign : unary_assign<SharedStr, SharedStr>
{
	static ConstUnitRef unit_creator(const AbstrOperGroup* gr, const ArgSeqType& args) { return arg1_values_unit(args); }

	void operator ()(typename unary_assign::assignee_ref res, typename unary_assign::arg1_cref arg) const
	{
		if (arg.empty() || (*(arg.begin()) != '\'') || (*(arg.end()-1) != '\''))
			Assign(res, Undefined());
		else
			SingleUnQuote(res, begin_ptr( arg ), end_ptr( arg ));
	}
};

struct dquote_assign: unary_assign<SharedStr, SharedStr>
{
	static ConstUnitRef unit_creator(const AbstrOperGroup* gr, const ArgSeqType& args) { return arg1_values_unit(args); }

	void operator ()(typename unary_assign::assignee_ref res, typename unary_assign::arg1_cref arg) const
	{
		DoubleQuote(res, begin_ptr( arg ), end_ptr( arg ));
	}
};

struct undquote_assign : unary_assign<SharedStr, SharedStr>
{
	static ConstUnitRef unit_creator(const AbstrOperGroup* gr, const ArgSeqType& args) { return arg1_values_unit(args); }

	void operator ()(typename unary_assign::assignee_ref res, typename unary_assign::arg1_cref arg) const
	{
		if (arg.empty() || (*(arg.begin()) != '\"') || (*(arg.end()-1) != '\"'))
			Assign(res, Undefined());
		else
			DoubleUnQuote(res, begin_ptr( arg ), end_ptr( arg ));
	}
};

struct urldecode_assign : unary_assign<SharedStr, SharedStr>
{
	static ConstUnitRef unit_creator(const AbstrOperGroup* gr, const ArgSeqType& args) { return arg1_values_unit(args); }

	void operator ()(typename unary_assign::assignee_ref res, typename unary_assign::arg1_cref arg) const
	{
		Assign(res, UrlDecode(SharedStr(arg)));
	}
};

struct to_utf_assign : unary_assign<SharedStr, SharedStr>
{
	static ConstUnitRef unit_creator(const AbstrOperGroup* gr, const ArgSeqType& args) { return arg1_values_unit(args); }

	void operator ()(typename unary_assign::assignee_ref res, typename unary_assign::arg1_cref arg) const
	{
		Assign(res, to_utf(arg.begin(), arg.end()));
	}
};


struct from_utf_assign : unary_assign<SharedStr, SharedStr>
{
	static ConstUnitRef unit_creator(const AbstrOperGroup* gr, const ArgSeqType& args) { return arg1_values_unit(args); }

	void operator ()(typename unary_assign::assignee_ref res, typename unary_assign::arg1_cref arg) const
	{
		Assign(res, from_utf(arg.begin(), arg.end()));
	}
};

struct item_name_assign : unary_assign<SharedStr, SharedStr>
{
	static ConstUnitRef unit_creator(const AbstrOperGroup* gr, const ArgSeqType& args) { return arg1_values_unit(args); }

	void operator ()(typename unary_assign::assignee_ref res, typename unary_assign::arg1_cref arg) const
	{
		Assign(res, as_item_name(arg.begin(), arg.end()));
	}
};



struct UpperCase_assign : same_length_unary_string_assign
{
	static ConstUnitRef unit_creator(const AbstrOperGroup* gr, const ArgSeqType& args) { return arg1_values_unit(args); }

	void operator ()(typename same_length_unary_string_assign::assignee_ref res, typename same_length_unary_string_assign::arg1_cref arg) const
	{
		UpperCase(res, begin_ptr( arg ), end_ptr( arg ));
	}
};

struct LowerCase_assign : same_length_unary_string_assign
{
	static ConstUnitRef unit_creator(const AbstrOperGroup* gr, const ArgSeqType& args) { return arg1_values_unit(args); }

	void operator ()(typename same_length_unary_string_assign::assignee_ref res, typename same_length_unary_string_assign::arg1_cref arg) const
	{
		LowerCase(res, begin_ptr( arg ), end_ptr( arg ));
	}
};


#endif //!defined(__CLC_ATTRUNISTRUCTSTR_H)