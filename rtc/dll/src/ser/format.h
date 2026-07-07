// Copyright (C) 1998-2026 Object Vision b.v.
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#pragma once


#if !defined(__RTC_SER_FORMAT_H)
#define __RTC_SER_FORMAT_H

//----------------------------------------------------------------------
// used modules and forward class references
//----------------------------------------------------------------------

#include <format>
#include <sstream>
#include <tuple>
#include <type_traits>
#include <utility>
#include "cpc/Types.h"

//----------------------------------------------------------------------
// std::format based formatting (replaces the former boost::format engine)
//
// Format strings use std::format syntax ({} placeholders), not printf %-directives.
// Arguments that std::format cannot format natively (SharedStr, TokenID, CharPtrRange,
// item pointers, enums with an operator<<, ...) are rendered up-front through their
// std::ostream operator<< -- exactly the path boost::format used -- and materialized
// as a std::string. Materializing eagerly also drops any TokenStr locks held by the
// arguments before the format call returns.
//----------------------------------------------------------------------

// The set of types std::format handles natively that these format calls actually use.
// Deliberately explicit rather than std::formattable<>: MSVC's std::formattable accepts a
// few pointer types (e.g. unsigned char* = SQLCHAR*) that std::make_format_args then rejects,
// and only arithmetic types ever carry a numeric spec ({:x}/{:f}) that needs a native value.
// Everything else -- SharedStr, TokenID, CharPtrRange, item/void/SQLCHAR pointers, enums with
// an operator<< -- is rendered up-front through its std::ostream operator<<, as boost did.
template <typename D>
concept mg_native_formattable =
	   std::is_arithmetic_v<D>
	|| std::is_same_v<D, char*> || std::is_same_v<D, const char*>
	|| std::is_same_v<D, std::string> || std::is_same_v<D, std::string_view>;

template <typename T>
auto mgFormatArg(T&& value)
{
	using D = std::decay_t<T>;               // arrays/functions -> pointers, strips cv/ref
	if constexpr (std::is_same_v<D, bool>)
		return int(value);                   // preserve stream default ("0"/"1"), not "true"/"false"
	else if constexpr (mg_native_formattable<D>)
		return std::forward<T>(value);
	else
	{
		std::ostringstream os;
		os << value;                         // the same operator<< boost::format fed into
		return std::move(os).str();
	}
}

template <typename ...Args>
std::string mgFormat2string(CharPtr msg, Args&&... args)
{
	// after mgFormatArg, every tuple element is natively std::format-able
	auto formatArgs = std::make_tuple(mgFormatArg(std::forward<Args>(args))...);
	try
	{
		return std::apply(
			[msg](auto&... a) { return std::vformat(msg, std::make_format_args(a...)); },
			formatArgs);
	}
	catch (const std::format_error&)
	{
		// never let a malformed format string mask the underlying report:
		// emit the raw format followed by the stringified arguments. Every tuple element is
		// natively formattable here (mgFormatArg already stringified the rest); vformat keeps
		// this off the consteval path so a bad element can't turn into a compile error.
		std::string result(msg);
		std::apply([&](auto&... a) { ((result += ' ', result += std::vformat("{}", std::make_format_args(a))), ...); }, formatArgs);
		return result;
	}
}

template <typename ...Args>
std::string mgFormat(CharPtr msg, Args&&... args)
{
	return mgFormat2string(msg, std::forward<Args>(args)...);
}

#endif // __RTC_SER_FORMAT_H
