// Copyright (C) 1998-2025 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "RtcPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

#include "act/garbage_can.h"

void garbage_can::clear() {
    for (auto& [_, bin] : bins) {
        if (bin.destroy) {
            bin.destroy(reinterpret_cast<std::byte*>(bin.storage.data()), bin.count);
        }
    }
    bins.clear();
}

void garbage_can::merge_from(garbage_can&& other) 
{
    for (auto& [tid, other_bin] : other.bins) {
        auto& bin = bins[tid];

        if (bin.storage.empty()) {
            bin = std::move(other_bin);
            continue;
        }

        assert(bin.stride == other_bin.stride);

        bin.ensure_capacity(bin.count + other_bin.count);

        std::byte* src = reinterpret_cast<std::byte*>(other_bin.storage.data());
        std::byte* dst = reinterpret_cast<std::byte*>(bin.storage.data());

        std::size_t dst_offset = bin.count * bin.stride;
        std::size_t src_bytes = other_bin.count * other_bin.stride;

        std::memmove(dst + dst_offset, src, src_bytes);

        bin.count += other_bin.count;
    }

    other.bins.clear();
}


