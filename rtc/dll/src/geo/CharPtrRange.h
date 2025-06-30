// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#pragma once

#if !defined(__RTC_GEO_CHARPTRRANGE_H)
#define __RTC_GEO_CHARPTRRANGE_H

#include <string_view>
#include <functional>

#include "cpc/Types.h"
#include "geo/iterrange.h"

struct CharPtrRange : IterRange<CharPtr> {
	CharPtrRange() {}
	CharPtrRange(Undefined) {} // creates an Undefined values, for an empty string, construct from ""

	CharPtrRange(CharPtr str) : IterRange(str, (str != nullptr) ? str + StrLen(str) : str) {}

//	template <typename Char, int N>
//	CharPtrRange(Char str[N]) : IterRange(str, str + (N > 0 && !str[N - 1] ? N - 1 : N))
//	{}
	CharPtrRange(iterator first, iterator second) : IterRange(first, second) 
	{}
	CharPtrRange(iterator first, std::size_t n) : IterRange(first, n) 
	{}
//	CharPtrRange(IterRange<CharPtr> range) : IterRange(range)
//	{}

	struct hasher {
		std::hash<std::string_view> hash_fn;

		std::size_t operator()(const CharPtrRange& range) const noexcept {
			const char* first = range.first;
			const char* last = range.second;

			return hash_fn(std::string_view{ first, last });
	/*
			const std::size_t len = static_cast<std::size_t>(last - first);

			std::size_t hash = 0xcbf29ce484222325; // FNV-1a 64-bit base
			constexpr std::size_t prime = 0x100000001b3;

			// Process in word-sized chunks
			while (last - first >= sizeof(std::size_t)) {
				std::size_t chunk;
				std::memcpy(&chunk, first, sizeof(std::size_t));
				if constexpr (std::endian::native != std::endian::little) {
					chunk = std::byteswap(chunk);
				}

				hash ^= chunk;
				hash *= prime;
				first += sizeof(std::size_t);
			}

			// Handle tail
			std::size_t chunk = 0;
			std::memcpy(&chunk, first, last - first);
			hash ^= chunk;

			return hash;
	*/
		}
	};
};


struct MutableCharPtrRange : IterRange<char*>
{
	using IterRange<char*>::IterRange;
};

template <typename Char, typename Traits>
std::basic_ostream<Char, Traits>& operator << (std::basic_ostream<Char, Traits>& os, CharPtrRange x)
{
	os.write(x.cbegin(), x.size());
	return os;
}


#endif // !defined(__RTC_GEO_CHARPTRRANGE_H)
