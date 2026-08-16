// Copyright (C) 1998-2026 Object Vision b.v.
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

#if !defined(__RTC_MEM_MYCONTAINERS_H)
#define __RTC_MEM_MYCONTAINERS_H

#include "vt/ElemTraits.h"      // is_bitvalue_v
#include "vt/SequenceTraits.h"  // sequence_traits<T>::container_type (BitVector for bit values)
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
// bit_iterators, which std::vector cannot hold (a std::vector<bit_value<N>, my_allocator<...>>
// simply does not compile -- the specialization's allocate() returns an iterator, not a pointer).
// Code that is generic over element types that may be sub-byte (Bool, UInt2, UInt4 -- instantiated
// via typelists::value_elements / typelists::fields in e.g. unique and index) uses my_elem_vec_t:
// bit values take sequence_traits' own packed BitVector -- my_allocator-backed, so census-visible
// and 8x-64x denser than an unpacked vector -- and every byte-addressable type routes through the
// allocation stocks with the unchanged std::vector API.
template <typename T> using my_elem_vec_t
	= std::conditional_t<is_bitvalue_v<T>, typename sequence_traits<T>::container_type, my_vec_t<T>>;

template <typename K, typename T, typename Pr = std::less<K>> using my_map_t
	= std::map<K, T, Pr, my_allocator<std::pair<const K, T>>>;

template <typename T, typename Pr = std::less<T>> using my_set_t
	= std::set<T, Pr, my_allocator<T>>;

#endif //!defined(__RTC_MEM_MYCONTAINERS_H)
