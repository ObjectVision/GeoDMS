/*
	fixed_allocator is an allocator that has an efficient
	memory management implementation for allocating limited-size objects.
	It can only allocate and deallocate objects of the same size by
	maintaining a linked list of free areas. The area that is managed
	is allocated by a suppying allocator, called ArrayAlloc, which is used
	to claim large blocks at a time.

	Complexity:

	Apart from the incidental large block allocation, allocation and
	deallocation is done in constant time.

	Template Parameters:

	BclokAlloc, the original allocator for type T

	Defines:

	define MG_DEBUG_ALLOCATOR to maintain allocation statistics.

	This allocator fulfills the allocator requirements as described in
	section 20.1 of the April 1995 WP and the STL specification, as
	documented by A. Stephanov & M. Lee, December 6, 1994.

	Future directions:
	- allocate_at_least(size_t n) -> span of actual 2^K allocated bytes
*/

#include "RtcPCH.h"
#pragma hdrstop

#include "RtcBase.h"

#include "act/MainThread.h"
#include "dbg/SeverityType.h"
#include "geo/mpf.h"
#include "mem/FixedAlloc.h"
#include "set/VectorFunc.h"
#include "utl/IncrementalLock.h"
#include "utl/MemGuard.h"
#include "dbg/SeverityType.h"

#include <memory>

#define MG_CACHE_ALLOC

#define MG_DEBUG_ALLOC true
#define MG_DEBUG_ALLOC_SMALL false

#if defined(MG_DEBUG_ALLOCATOR)
void RegisterAlloc(void* ptr, size_t n MG_DEBUG_ALLOCATOR_SRC_ARG);
void RemoveAlloc(void* ptr, size_t n);
#endif //defined(MG_DEBUG_ALLOCATOR)

void PostReporting();
void ConsiderReporting();

std::allocator<UInt64> s_QWordArrayAllocator;

using PageSize_type = UInt32;
using BlockSize_type = UInt32;
using BlockCount_type = UInt32;
using block_index_t = UInt8;

using BYTE_PTR = char*;

constexpr UInt8 log2_page_size = 30;
constexpr PageSize_type PAGE_SIZE = 1 << log2_page_size;

template<typename Unsigned>
constexpr block_index_t highest_bit_rank(Unsigned value)
{
	return sizeof(Unsigned)*8 - std::countl_zero(value);
}

#include <Windows.h>

struct VirtualAllocPage
{
	VirtualAllocPage(SizeT BLOCK_PAGE_SIZE_)
		: BLOCK_PAGE_SIZE(BLOCK_PAGE_SIZE_)
	{
		WaitForAvailableMemory(BLOCK_PAGE_SIZE_);
		pagePtr = reinterpret_cast<BYTE_PTR>(VirtualAlloc(nullptr, BLOCK_PAGE_SIZE, MEM_RESERVE, PAGE_NOACCESS));
	}

	VirtualAllocPage(VirtualAllocPage&& rhs) noexcept
		: pagePtr(rhs.pagePtr)
		, BLOCK_PAGE_SIZE(rhs.BLOCK_PAGE_SIZE)
	{
		rhs.pagePtr = nullptr;
	}

	~VirtualAllocPage()
	{
		if (pagePtr)
			VirtualFree(pagePtr, 0, MEM_RELEASE);
	}

	SizeT   PageSize() const { return BLOCK_PAGE_SIZE; }

	BYTE_PTR begin() const { return pagePtr; }
	BYTE_PTR end  () const { return begin() + PageSize(); }

	static void commit(BYTE_PTR ptr, BlockSize_type sz)
	{
		dms_assert(sz);
		VirtualAlloc(ptr, sz, MEM_COMMIT, PAGE_READWRITE);
	}

	static void release(BYTE_PTR ptr, BlockSize_type sz)
	{
//		VirtualAlloc(ptr, sz, MEM_RESET, PAGE_NOACCESS);
	}
	static void recommit(BYTE_PTR ptr, BlockSize_type sz)
	{
//		VirtualAlloc(ptr, sz, MEM_RESET_UNDO, PAGE_READWRITE);
	}

private:
	BYTE_PTR pagePtr;
	SizeT BLOCK_PAGE_SIZE;
};


struct VirtualAllocPageAllocator
{
	using page_type = VirtualAllocPage;

	std::vector<page_type> pages;
	BlockCount_type nrUncommitedBlocks = 0;
	block_index_t log2BlockSize = 0;
	BlockSize_type blockSize = 0;
	SizeT nextPageSize = 0;
	BYTE_PTR lastPagePtr = nullptr;

	void Init_log2(block_index_t log2BlockSize_)
	{
		log2BlockSize = log2BlockSize_;
		blockSize = 1 << log2BlockSize;
		dms_assert(std::popcount(blockSize) == 1); // blockSize is assumed to be a power of 2. if and only if  !(blockSize & (blockSize-1))
		nextPageSize = blockSize;
		nextPageSize *= 2;
		MakeMax(nextPageSize, 1 << 20);
	}

	BYTE_PTR get_reserved_block(BlockSize_type sz)
	{
		dms_assert( sz <= blockSize && sz > 0);

		if (!nrUncommitedBlocks)
		{
			pages.emplace_back(nextPageSize);
			nrUncommitedBlocks = nextPageSize >> log2BlockSize;

			if (nextPageSize < PAGE_SIZE) // double up to 1[GB]
				nextPageSize <<= 1;
			lastPagePtr = pages.back().begin();
		}
		dms_assert(nrUncommitedBlocks);
		return lastPagePtr + (SizeT(--nrUncommitedBlocks) << log2BlockSize);
	}
	void commit(BYTE_PTR ptr, BlockSize_type sz)
	{
		assert(sz && sz <= blockSize);

		VirtualAllocPage::commit(ptr, blockSize);
	}

	void release(BYTE_PTR ptr, BlockSize_type sz)
	{
		assert(sz && sz <= blockSize);

		VirtualAllocPage::release(ptr, blockSize);
	}

	void recommit(BYTE_PTR ptr, BlockSize_type sz)
	{
		assert(sz && sz <= blockSize);

		VirtualAllocPage::recommit(ptr, blockSize);
	}
};

using FreeStackAllocSummary = std::tuple<SizeT, SizeT, SizeT, SizeT>;

FreeStackAllocSummary operator +(FreeStackAllocSummary lhs, FreeStackAllocSummary rhs)
{
	return FreeStackAllocSummary(std::get<0>(lhs) + std::get<0>(rhs), std::get<1>(lhs) + std::get<1>(rhs), std::get<2>(lhs) + std::get<2>(rhs), std::get<3>(lhs) + std::get<3>(rhs));
}

struct FreeStackAllocator
{
	VirtualAllocPageAllocator inner;
	std::vector<BYTE_PTR> freestack;
	BlockCount_type blockCount = 0;
	mutable std::mutex allocSection;

	void Init_log2(block_index_t log2BlockSize_) { inner.Init_log2(log2BlockSize_); }

	auto NrAllocatedBlocks() const { return blockCount; }
	std::pair<BYTE_PTR, bool> get_reserved_or_reset_block(BlockSize_type sz)
	{
		std::scoped_lock lock(allocSection);

		blockCount++;
		if (freestack.empty())
			return { inner.get_reserved_block(sz), true };

		auto ptr = freestack.back(); freestack.pop_back();
		return { ptr, false };
	}
	BYTE_PTR allocate(BlockSize_type sz)
	{
		auto reserved_or_rest_block = get_reserved_or_reset_block(sz);
		if (reserved_or_rest_block.second)
			inner.commit(reserved_or_rest_block.first, sz);
		else
			inner.recommit(reserved_or_rest_block.first, sz);
		return reserved_or_rest_block.first;
	}
	void add_to_freestack(BYTE_PTR ptr, BlockSize_type sz)
	{
		std::scoped_lock lock(allocSection);
		blockCount--;
		freestack.emplace_back(ptr);
	}
	void deallocate(BYTE_PTR ptr, BlockSize_type sz)
	{
		inner.release(ptr, sz);
		add_to_freestack(ptr, sz);
	}

	FreeStackAllocSummary ReportStatus() const
	{
		if (!inner.blockSize)
			return FreeStackAllocSummary(0, 0, 0, 0);

		SizeT pageCount, totalBytes = 0;
		SizeT nrAllocated;
		SizeT nrFreed;
		SizeT nrUncommitted;
		SizeT nrAllocatedBytes;
		{
			std::scoped_lock lock(allocSection);

			pageCount = inner.pages.size();
			totalBytes = 0; for (const auto& page : inner.pages) totalBytes += page.PageSize();
			nrAllocated = NrAllocatedBlocks();
			nrFreed = freestack.size();
			nrUncommitted = inner.nrUncommitedBlocks;
			nrAllocatedBytes = inner.blockSize * nrAllocated;
		}
		reportF(SeverityTypeID::ST_MinorTrace, "Block size: %d; pagecount: %d; alloc: %d; freed: %d; uncommitted: %d; total bytes: %d[MB] allocbytes = %d[MB]",
			inner.blockSize, pageCount, nrAllocated, nrFreed, nrUncommitted, totalBytes >> 20, nrAllocatedBytes >> 20);
		return FreeStackAllocSummary(totalBytes, nrAllocatedBytes, nrFreed << inner.log2BlockSize, nrUncommitted << inner.log2BlockSize);
	}
};
/*
struct FreeListAllocator
{
	VirtualAllocPageAllocator inner;
	BYTE_PTR freelist = nullptr, 
	BlockCount_type blockCount = 0;
	mutable std::mutex allocSection;

	void Init_log2(block_index_t log2BlockSize_) { inner.Init_log2(log2BlockSize_); }

	auto NrAllocatedBlocks() const { return blockCount; }
	std::pair<BYTE_PTR, bool> get_reserved_or_reset_block(BlockSize_type sz)
	{
		std::scoped_lock lock(allocSection);

		blockCount++;

		if (freestack.empty())
			return { inner.get_reserved_block(sz), true };

		auto ptr = freestack.back(); freestack.pop_back();
		return { ptr, false };
	}
	BYTE_PTR allocate(BlockSize_type sz)
	{
		auto reserved_or_rest_block = get_reserved_or_reset_block(sz);
		if (reserved_or_rest_block.second)
			inner.commit(reserved_or_rest_block.first, sz);
		else
			inner.recommit(reserved_or_rest_block.first, sz);
		return reserved_or_rest_block.first;
	}
	void add_to_freestack(BYTE_PTR ptr, BlockSize_type sz)
	{
		std::scoped_lock lock(allocSection);
		blockCount--;
		freestack.emplace_back(ptr);
	}
	void deallocate(BYTE_PTR ptr, BlockSize_type sz)
	{
		inner.release(ptr, sz);
		add_to_freestack(ptr, sz);
	}
};
*/

const std::size_t QWordSize = sizeof(UInt64);
static UInt32 g_ElemAllocCounter = 0;

#if defined(MG_CACHE_ALLOC)

const UInt8 log2_extended_segment_size = 20;

constexpr UInt32 nr_bits_in_byte = 8;
constexpr UInt32 ALLOC_ELEMSIZE_MIN_BITS =  3; //  log2_default_segment_size - mpf::log2_v<nr_bits_in_byte>;     // 2^13 ==  8[kB]
constexpr UInt32 ALLOC_ELEMSIZE_MAX_BITS = 28; //  log2_extended_segment_size + mpf::log2_v<sizeof(Float64) * 2>; // 2^28 ==256[MB]
constexpr SizeT ALLOC_ELEMSIZE_MIN = 1 << ALLOC_ELEMSIZE_MIN_BITS;
constexpr SizeT ALLOC_ELEMSIZE_MAX = 1 << ALLOC_ELEMSIZE_MAX_BITS;

//#define MG_MULTIPLE_SLOTS
#if defined(MG_MULTIPLE_SLOTS)
constexpr UInt32 NR_SLOT_BITS = 0;
constexpr UInt32 NR_SLOTS = 1 << NR_SLOT_BITS;
constexpr block_index_t NR_ELEM_ALLOC = (ALLOC_ELEMSIZE_MAX_BITS - ALLOC_ELEMSIZE_MIN_BITS) * NR_SLOTS + 1;
#else // defined(MG_MULTIPLE_SLOTS)
constexpr block_index_t NR_ELEM_ALLOC = ALLOC_ELEMSIZE_MAX_BITS - ALLOC_ELEMSIZE_MIN_BITS + 1;
#endif //defined(MG_MULTIPLE_SLOTS)

constexpr SizeT ALLOC_LIMIT = 0x1000000000000000;

//----------------------------------------------------------------------
// fixed_allocator arrays
//----------------------------------------------------------------------

// for each ALLOC_ELEMSIZE_MIN..ALLOC_ELEMSIZE_MAX
using FreeStackAllocatorArray = FreeStackAllocator[ALLOC_ELEMSIZE_MAX_BITS - ALLOC_ELEMSIZE_MIN_BITS + 1];

#if defined(MG_DEBUG)
FreeStackAllocatorArray* sd_FSA_ptr = nullptr;
#endif

FreeStackAllocatorArray& GetFreeStackAllocatorArray()
{
	static FreeStackAllocatorArray singleton;
	return singleton;
}

std::mutex s_fsaCS;

FreeStackAllocator& GetFreeStackAllocator(block_index_t i)
{
	auto& fsa = GetFreeStackAllocatorArray()[i];
	if (!fsa.inner.blockSize)
	{
		auto lock = std::scoped_lock(s_fsaCS);
		if (!fsa.inner.blockSize)
			fsa.Init_log2(i + ALLOC_ELEMSIZE_MIN_BITS);
		dms_assert(fsa.inner.blockSize);
#if defined(MG_DEBUG)
		sd_FSA_ptr = &GetFreeStackAllocatorArray();
#endif
	}
	return fsa;
}


ElemAllocComponent s_Initialize; // TODO: REMOVE


constexpr SizeT BlockSize(block_index_t i) {
	assert(i < NR_ELEM_ALLOC);
#if defined(MG_MULTIPLE_SLOTS)
	static_assert(std::popcount(NR_SLOTS) == 1);

	constexpr UInt32 SLOT_MASK = NR_SLOTS - 1;
	auto exp_bits = (i + SLOT_MASK) >> NR_SLOT_BITS;
	auto slot_nr = (i + SLOT_MASK & SLOT_MASK) + 1;

	return ((ALLOC_ELEMSIZE_MIN / 2 + (slot_nr << (ALLOC_ELEMSIZE_MIN_BITS - NR_SLOT_BITS - 1))) << exp_bits) / QWordSize;
#else // defined(MG_MULTIPLE_SLOTS)
	return (SizeT(1) << (ALLOC_ELEMSIZE_MIN_BITS + i)) / QWordSize;
#endif //defined(MG_MULTIPLE_SLOTS)
}



constexpr block_index_t BlockListIndex(SizeT sz) 
{
#if defined(MG_MULTIPLE_SLOTS)
	SizeT org_sz = sz;

		assert(sz > ALLOC_ELEMSIZE_MIN / 2);
		assert(sz <= ALLOC_ELEMSIZE_MAX);

		--sz;
		sz >>= ALLOC_ELEMSIZE_MIN_BITS;
		static_assert((1 << (ALLOC_ELEMSIZE_MAX_BITS - ALLOC_ELEMSIZE_MIN_BITS) & ~0xFFF0) == 0);
		block_index_t result = highest_bit_rank(sz);

		auto discard_bits = (result + (ALLOC_ELEMSIZE_MIN_BITS - NR_SLOT_BITS - 1));
		auto slot = (((((org_sz + (1 << discard_bits) - 1) >> discard_bits) - 1) & (NR_SLOTS - 1))) + 1;
		assert(slot > 0);
		assert(slot <= NR_SLOTS);
		result = (result << NR_SLOT_BITS) + slot - NR_SLOTS;
		assert(result >= 0);
		assert(result < NR_ELEM_ALLOC);
		assert(org_sz <= BlockSize(result) * QWordSize);
		assert(org_sz > (result ? BlockSize(result - 1) * QWordSize : ALLOC_ELEMSIZE_MIN / 2));
		return result;
#else // defined(MG_MULTIPLE_SLOTS)
		SizeT org_sz = sz;
		assert(sz);
//		assert(sz > ALLOC_ELEMSIZE_MIN / 2);
		assert(sz <= ALLOC_ELEMSIZE_MAX);

		--sz;
		sz >>= ALLOC_ELEMSIZE_MIN_BITS;
		block_index_t result = highest_bit_rank(sz);

		assert(result >= 0);
		assert(result < NR_ELEM_ALLOC);
		assert(org_sz <= BlockSize(result) * QWordSize);
		assert(org_sz > (result ? BlockSize(result - 1) * QWordSize : 0));
		return result;
#endif //defined(MG_MULTIPLE_SLOTS)
}


bool IsIntegralPowerOf2OrZero(SizeT sz)
{
	return !((sz - 1) & sz);
}

bool SpecialSize(SizeT sz)
{
	//return sz >= ALLOC_ELEMSIZE_MIN && sz <= ALLOC_ELEMSIZE_MAX && IsIntegralPowerOf2OrZero(sz);
//	return sz > ALLOC_ELEMSIZE_MIN / 2 && sz <= ALLOC_ELEMSIZE_MAX; // && IsIntegralPowerOf2OrZero(sz);
	assert(sz);
	return sz <= ALLOC_ELEMSIZE_MAX; // && IsIntegralPowerOf2OrZero(sz);
}

#endif defined(MG_CACHE_ALLOC)

//----------------------------------------------------------------------
// inmplement interface
//----------------------------------------------------------------------


void* AllocateFromStock_impl(size_t sz)
{
#if defined(MG_CACHE_ALLOC)
	assert(sz);

	if (SpecialSize(sz))
	{
		auto i = BlockListIndex(sz);
		assert(i < NR_ELEM_ALLOC);

		auto& freeStack = GetFreeStackAllocator(i);
		return freeStack.allocate(sz);
	}
	assert(sz > ALLOC_ELEMSIZE_MAX);
#endif //defined(MG_CACHE_ALLOC)
	SizeT blockSize = ((sz + (QWordSize - 1)) & ~(QWordSize - 1)) / QWordSize;

	WaitForAvailableMemory(blockSize * QWordSize);
	auto result = s_QWordArrayAllocator.allocate(blockSize);
	return result;

}

void* AllocateFromStock(size_t sz MG_DEBUG_ALLOCATOR_SRC_ARG)
{
	if (!sz)
		return nullptr;

	auto result = AllocateFromStock_impl(sz);

#if defined(MG_DEBUG_ALLOCATOR)
	RegisterAlloc(result, sz MG_DEBUG_ALLOCATOR_SRC_PARAM);
#endif //defined(MG_DEBUG_ALLOCATOR)

	ConsiderReporting();
	return result;
}

void LeaveToStock(void* ptr, size_t sz) {
	if (!sz)
	{
		dms_assert(ptr == nullptr);
		return;
	}
#if defined(MG_DEBUG_ALLOCATOR)
	RemoveAlloc(ptr, sz);
#endif //defined(MG_DEBUG_ALLOCATOR)

#if defined(MG_CACHE_ALLOC)
	if (SpecialSize(sz))
	{
		auto i = BlockListIndex(sz);
		dms_assert(i < NR_ELEM_ALLOC);

		auto bytePtr = reinterpret_cast<BYTE_PTR>(ptr);

		auto& freeStack = GetFreeStackAllocator(i);
		return freeStack.deallocate(bytePtr, sz);
	}
#endif //defined(MG_CACHE_ALLOC)

	SizeT blockSize = ((sz + (QWordSize - 1)) & ~(QWordSize - 1)) / QWordSize;
	s_QWordArrayAllocator.deallocate(reinterpret_cast<UInt64*>(ptr), blockSize);
}

std::atomic<bool> s_ReportingRequestPending = false;

void ReportFixedAllocStatus()
{
	s_ReportingRequestPending = false;

#if defined(MG_CACHE_ALLOC)

	reportD(SeverityTypeID::ST_MajorTrace, "ReportFixedAllocStatus");

	FreeStackAllocSummary cumulBytes;
	for (const auto& fsa : GetFreeStackAllocatorArray())
		cumulBytes = cumulBytes + fsa.ReportStatus();

	reportF(SeverityTypeID::ST_MajorTrace, "Reserved in Blocks %d[kB]; allocated: %d[kB]; freed: %d[kB]; uncommitted: %d[kB]"
		, std::get<0>(cumulBytes) >> 10
		, std::get<1>(cumulBytes) >> 10
		, std::get<2>(cumulBytes) >> 10
		, std::get<3>(cumulBytes) >> 10
	);

#endif //defined(MG_CACHE_ALLOC)
}


#if defined(MG_CACHE_ALLOC)

bool AreAllFreestackAllocatorsEmpty()
{
	for (const auto& fsa : GetFreeStackAllocatorArray())
		if (fsa.NrAllocatedBlocks())
			return false;
	return true;
}

#endif //defined(MG_CACHE_ALLOC)

//----------------------------------------------------------------------
// clean-up
//----------------------------------------------------------------------

ElemAllocComponent::ElemAllocComponent()
{
	if (g_ElemAllocCounter++)
		return;

	SetMainThreadID();

	GetFreeStackAllocatorArray();

#if defined(MG_DEBUG)
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF /*| _CRTDBG_CHECK_CRT_DF | _CRTDBG_CHECK_ALWAYS_DF*/);
//	_CrtSetBreakAlloc(6548); 
//	Known leak: iconv-2.dll->relocatable.c->DllMail contains  shared_library_fullname = strdup(location); which leaks 44 bytes of memory
#endif

}

ElemAllocComponent::~ElemAllocComponent()
{
	if (--g_ElemAllocCounter)
		return;

#if defined(MG_CACHE_ALLOC)

	if (!AreAllFreestackAllocatorsEmpty())
		ReportFixedAllocStatus();

#endif //defined(MG_CACHE_ALLOC)

}

#include "dbg/Timer.h"

void PostReporting()
{
	s_ReportingRequestPending = true;
	AddMainThreadOper([] {
		if (s_ReportingRequestPending)
			ReportFixedAllocStatus();
		}
	,	true
	);
}

static std::atomic<UInt32> s_ConsiderReportingReentranceCounter = 0;

void ConsiderReporting()
{
	static Timer t;
	
	if (s_ConsiderReportingReentranceCounter)
		return;

	StaticMtIncrementalLock<s_ConsiderReportingReentranceCounter> preventReentrance;
	if (t.PassedSecs(30))
		PostReporting();
}


#if defined(MG_DEBUG_ALLOCATOR)

#include <map>
#include "dbg/DebugReporter.h"

struct alloc_register_t
{
	std::map<void*, std::pair<CharPtr, size_t>> map;
	std::mutex mutex;
};

auto& GetAllocRegister()
{
	static alloc_register_t allocRegister;
	return allocRegister;
}

void RegisterAlloc(void* ptr, size_t sz MG_DEBUG_ALLOCATOR_SRC_ARG)
{
	auto& reg = GetAllocRegister();
	auto lock = std::scoped_lock(reg.mutex);

	dms_assert(reg.map.find(ptr) == reg.map.end()); // check that its not already assigned
	reg.map[ptr] = std::pair<CharPtr, size_t>{ srcStr, sz };
}

void RemoveAlloc(void* ptr, size_t sz)
{
	auto& reg = GetAllocRegister();
	auto lock = std::scoped_lock(reg.mutex);

	auto pos = reg.map.find(ptr);
	dms_assert(pos != reg.map.end() && pos->first == ptr && pos->second.second == sz); // check that it was assigned as now assumed
	reg.map.erase(pos);
}

void ReportAllocs()
{
	auto& reg = GetAllocRegister();
	auto lock = std::scoped_lock(reg.mutex);
	BlockCount_type i = 0;

	std::map<SizeT, SizeT> fequencyCounts;
	reportD(SeverityTypeID::ST_MinorTrace, "All Registered Memory Blocks");
	for (auto& registeredAlloc : reg.map)
	{
		SizeT sz = registeredAlloc.second.second;
		reportF(SeverityTypeID::ST_MajorTrace, "Alloc %d size %x src %s", i++, sz, registeredAlloc.second.first);
		fequencyCounts[sz]++;
	}

	SizeT cumulSize = 0;
	reportD(SeverityTypeID::ST_MinorTrace, "Frequency counts per size:");
	i = 0;
	for (auto& freq : fequencyCounts)
	{
		SizeT sz = freq.first;
		SizeT cnt = freq.second;
		SizeT totalSz = sz * cnt;
		reportF(SeverityTypeID::ST_MajorTrace, "#%.5d Size %x(%d) count %d total %x(%d)", i++, sz, sz, cnt, totalSz, totalSz);
		cumulSize += totalSz;
	}
	reportF(SeverityTypeID::ST_MajorTrace, "Total Size = %x(%d)", cumulSize, cumulSize);

	PostReporting(); // Warning: allocSection can be locked, so don't call ReportFixedAllocStatus() here.
}

struct AllocReporter : DebugReporter
{
	void Report() const override
	{
		ReportAllocs();
	}
};

static AllocReporter s_Reporter;


#endif //defined(MG_DEBUG_ALLOCATOR)

