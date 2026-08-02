// Copyright (C) 1998-2026 Object Vision b.v.
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

#if !defined(__RTC_MEM_MYCONTAINERS_H)
#define __RTC_MEM_MYCONTAINERS_H

#include "geo/ElemTraits.h" // is_bitvalue_v
#include "mem/MyAllocator.h"

#include <map>
#include <set>
#include <type_traits>
#include <vector>

// Operator intermediates belong in my_allocator, i.e. in AllocateFromStock, for two reasons.
// They are the actual working set of many operators -- full-domain index buffers, priority heaps,
// spatial-index nodes, per-thread combinables -- so with the default allocator they bypass the
// lock-free allocation stocks AND stay invisible to the always-on allocation census that the
// operation memory estimates are calibrated against (schedule-with-lookahead.md SS8.1.16).
//
// Kept separate from MyAllocator.h so that <map>/<set> do not land in every TU that only needs
// the allocator itself.

template <typename T> using my_vec_t = std::vector<T, my_allocator<T>>;

// my_vec_t requires a byte-addressable T: the my_allocator<bit_value<N>> specialization hands out
// bit_iterators, which std::vector cannot hold. Code that is generic over element types that may
// be sub-byte (Bool, UInt2, UInt4) uses my_elem_vec_t instead: bit values fall back to the plain
// default-allocated vector -- unpacked, exactly what std::vector<V> did there before -- while every
// byte-addressable type routes through the allocation stocks.
template <typename T> using my_elem_vec_t
	= std::conditional_t<is_bitvalue_v<T>, std::vector<T>, my_vec_t<T>>;

template <typename K, typename T, typename Pr = std::less<K>> using my_map_t
	= std::map<K, T, Pr, my_allocator<std::pair<const K, T>>>;

template <typename T, typename Pr = std::less<T>> using my_set_t
	= std::set<T, Pr, my_allocator<T>>;

#endif //!defined(__RTC_MEM_MYCONTAINERS_H)
