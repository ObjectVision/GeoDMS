// Copyright (C) 1998-2024 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

#if !defined(__RTC_SET_MEMPAGETABLE_H)
#define __RTC_SET_MEMPAGETABLE_H

#include "set/FileView.h" 
#include "geo/IndexRange.h"
#include <set>


using FreeChunk = IndexRange<dms::filesize_t>;

struct LexiLess {
	bool operator ()(const FreeChunk& a, const FreeChunk& b) const
	{ 
		return a.first < b.first || a.first == b.first && a.second < b.second; 
	}
};

struct SizeLess {
	bool operator()(const FreeChunk& a, const FreeChunk& b) const
	{
		return a.size() < b.size() || a.size() == b.size() && m_FallbackCmp(a, b);
	}
	LexiLess m_FallbackCmp;
};

inline SizeT MinimalSeqFileSize(tile_id tn)
{
	if (tn <= 1)
		return 0;
	auto memPageAllocTableSize = safe_size_n<sizeof(FileChunkSpec)>(tn);
	return NrMemPages(memPageAllocTableSize) << GetLog2AllocationGrannularity();
}

//----------------------------------------------------------------------
// Section      : mempage_table
//----------------------------------------------------------------------

struct mempage_table : rw_file_view < FileChunkSpec >
{
	using rw_file_view < FileChunkSpec >::rw_file_view; // inherit ctors

	RTC_CALL void InitFreeSetsFromChunkSpecs(tile_id tn, dms::filesize_t fileSize);
	FreeChunk ReallocChunk(FreeChunk currChunk, dms::filesize_t newSize, dms::filesize_t fileSize);
	void FreeAllocatedChunk(FreeChunk currChunk);

	std::set<FreeChunk, LexiLess> m_FreeListByLexi;
	std::set<FreeChunk, SizeLess> m_FreeListBySize;
};


#endif //!defined(__RTC_SET_MEMPAGETABLE_H)
