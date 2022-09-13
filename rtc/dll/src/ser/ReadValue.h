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

#if !defined(__RTC_SER_READVALUE_H)
#define __RTC_SER_READVALUE_H



// *****************************************************************************
// Section:     Helper Funcs
// *****************************************************************************

inline void SkipSpace(CharPtr& src, CharPtr end)
{
	while ((src != end) && isspace(UChar(*src)))
	{
		dms_assert(*src); // NULL is not a space
		++src;
	}
}

inline void SkipSpace(CharPtr& src)
{
	while (isspace(UChar(*src)))
	{
		dms_assert(*src); // NULL is not a space
		++src;
	}
}

inline void SkipSpace(FormattedInpStream& str)
{
	while (isspace(UChar(str.NextChar())))
	{
		dms_assert(str.NextChar()); // NULL is not a space
		str.ReadChar();
	}
}

/********** Read Integers from chars **********/

#include <boost/spirit/include/classic_core.hpp>
#include <boost/spirit/include/classic_numerics.hpp>

template <typename ScannerT> bool ScanValue(double       & value, const ScannerT& scan) { return boost::spirit::real_p[boost::spirit::assign_a(value)].parse(scan); }
template <typename ScannerT> bool ScanValue(long double  & value, const ScannerT& scan) { return boost::spirit::real_p[boost::spirit::assign_a(value)].parse(scan); }
template <typename ScannerT> bool ScanValue(int     & value, const ScannerT& scan) { return boost::spirit::int_p [boost::spirit::assign_a(value)].parse(scan); }
template <typename ScannerT> bool ScanValue(unsigned& value, const ScannerT& scan) { return boost::spirit::uint_p[boost::spirit::assign_a(value)].parse(scan); }
#if defined(DMS_TM_HAS_INT64)

boost::spirit:: int_parser< Int64> const  int64_p;
boost::spirit::uint_parser<UInt64> const uint64_p;

template <typename ScannerT> bool ScanValue(         __int64& value, const ScannerT& scan) { return int64_p [boost::spirit::assign_a(value)].parse(scan); }
template <typename ScannerT> bool ScanValue(unsigned __int64& value, const ScannerT& scan) { return uint64_p[boost::spirit::assign_a(value)].parse(scan); }
#endif

///////////////////////////////////////////////////////////////////////////
//
//  continental_iteration_policy class which translates a COMMA to a PERIOD to support continental decimal separators
//
///////////////////////////////////////////////////////////////////////////

template <typename BaseT>
struct continental_iteration_policy : public BaseT
{
    typedef BaseT base_t;

    continental_iteration_policy()
    : BaseT() {}

    template <typename PolicyT>
    continental_iteration_policy(PolicyT const& other)
    : BaseT(other) {}

    template <typename T>
    T filter(T ch) const
    {
        ch = BaseT::filter(ch);
		if (ch == ',')
			return '.';
		return ch;
    }
};

///////////////////////////////////////////////////////////////////////////
//
//  continental_scanner
//
///////////////////////////////////////////////////////////////////////////

typedef boost::spirit::scanner<CharPtr> char_scanner;

    typedef boost::spirit::scanner_policies<
        continental_iteration_policy<
            char_scanner::iteration_policy_t>,
        char_scanner::match_policy_t,
        char_scanner::action_policy_t
    > continental_policies_t;

typedef boost::spirit::scanner<CharPtr, continental_policies_t> continental_char_scanner;

template <typename T>
T ReadValue(CharPtr src, CharPtr end)
{
	T value;
	if (ScanValue(value, char_scanner(src, end)))
		return value;
	return UNDEFINED_VALUE(T);
}

template <typename T>
T ReadValue(FormattedInpStream& str)
{
	T value;
	dms_assert(str.CurrPos() != -1);
	if (str.IsCommaDecimalSeparator())
	{
		continental_char_scanner::iterator_t f = str.Buffer().GetDataBegin() + str.CurrPos();
		continental_char_scanner scan(f, str.Buffer().GetDataEnd());
		if (ScanValue(value, scan))
		{
			str.SetCurrPos(scan.first - str.Buffer().GetDataBegin()); 
			return value;
		}
	}
	else
	{
		char_scanner::iterator_t f = str.Buffer().GetDataBegin() + str.CurrPos();
		char_scanner scan(f, str.Buffer().GetDataEnd());
		if (ScanValue(value, scan))
		{
			str.SetCurrPos(scan.first - str.Buffer().GetDataBegin()); 
			return value;
		}
	}
	return UNDEFINED_VALUE(T);
}

bool ReadNull(FormattedInpStream& str)
{
	if (str.NextChar() != 'n')
		return false;
	str.ReadChar();
	if (str.NextChar() == 'u')
	{
		str.ReadChar();
		if (str.NextChar() == 'l')
		{
			str.ReadChar();
			if (str.NextChar() == 'l')
				str.ReadChar();
		}
	}
	return true;
}

template <typename T>
T ReadValueOrNull(FormattedInpStream& str)
{
	if (ReadNull(str))
		return UNDEFINED_VALUE(T);
	return ReadValue<T>(str);
}

template <typename T>
T ReadValueAfterSpace(CharPtr src, CharPtr end)
{
	SkipSpace(src, end);
	return ReadValue<T>(src, end);
}

template <typename T>
T ReadValueAfterSpace(CharPtr src)
{
	SkipSpace(src);
	return ReadValue<T>(src, 0);
}

template <typename T>
T ReadValueAfterSpace(FormattedInpStream& str)
{
	SkipSpace(str);
	return ReadValue<T>(str);
}

template <typename T>
T ReadValueOrNullAfterSpace(FormattedInpStream& str)
{
	SkipSpace(str);
	return ReadValueOrNull<T>(str);
}


#endif //!defined(__RTC_SER_READVALUE_H)
