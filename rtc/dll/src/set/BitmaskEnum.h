// Copyright (C) 2026 Object Vision b.v.
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////
// BitmaskEnum.h
//   The bitwise operators of a scoped enumeration whose enumerators are bits, for every
//   enumeration that opts in with one line after its definition:
//
//     template <> inline constexpr bool is_bitmask_enum<MyFlags> = true;
//
//   The operators |, &, ^ and ~ and their assignments keep the type of the enumeration, so a
//   mask of one enumeration does not combine with another one or with an integer; flags(x) tells
//   whether x has any bit set. They are templates constrained to the enumerations that opted in,
//   so they do not take part in the overload resolution of any other type.
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

#if !defined(__RTC_SET_BITMASKENUM_H)
#define __RTC_SET_BITMASKENUM_H

#include <type_traits>

template <typename E> inline constexpr bool is_bitmask_enum = false;

template <typename E>
concept bitmask_enum = std::is_enum_v<E> && is_bitmask_enum<E>;

template <bitmask_enum E>
constexpr auto bitmask_value(E x) noexcept { return static_cast<std::underlying_type_t<E>>(x); }

template <bitmask_enum E> constexpr E operator |(E x, E y) noexcept { return static_cast<E>(bitmask_value(x) | bitmask_value(y)); }
template <bitmask_enum E> constexpr E operator &(E x, E y) noexcept { return static_cast<E>(bitmask_value(x) & bitmask_value(y)); }
template <bitmask_enum E> constexpr E operator ^(E x, E y) noexcept { return static_cast<E>(bitmask_value(x) ^ bitmask_value(y)); }
template <bitmask_enum E> constexpr E operator ~(E x)      noexcept { return static_cast<E>(~bitmask_value(x)); }

template <bitmask_enum E> constexpr E& operator |=(E& x, E y) noexcept { return x = x | y; }
template <bitmask_enum E> constexpr E& operator &=(E& x, E y) noexcept { return x = x & y; }
template <bitmask_enum E> constexpr E& operator ^=(E& x, E y) noexcept { return x = x ^ y; }

template <bitmask_enum E> constexpr bool flags(E x) noexcept { return bitmask_value(x) != 0; }

#endif // __RTC_SET_BITMASKENUM_H
