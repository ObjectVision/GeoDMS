/*
	FixedAlloc provides an allocator that has an efficient memory management.

	FreeStackAllocator(i) can allocate and deallocate objectStores that span 2^i pages (4kB), thus 2^(i+12) bytes.
	It mantains a FreeStack of deallocated objectStores for reallocation. Allocated objects must have accessible memory.

	ObjectStore Memory is provided by VirtualAllocChunkArray(i), which manages a vector of VirtualAllocChunks(i+j)
	to reserve address space for chunks of 2^j objectStores at a time. 
	A VirtualAllocChunkArray can commit address space per objectStore and has a deallocate and recommit policy.

	TODO: It may have a lazy MEM_RESET (following deallocation) and MEM_RESET_UNDO (for reallocation) policy 
		to adapt to RAM page reclaimation as needed without decommitting hot deallocated objects.
	NOW: release leaves the memory of deallocated objectStores always committed and recommit doesn't have to do anything.

	FreeListAllocator(i) can allocate and deallocate small objects of size 8*2^i < 4[kB], thus i < 12-3. 
	Memory is provided by the FreeStackAlocator(i) that can store 2^9=512 small objects. deallocated small object remain committed.

	It keeps a wait free unused counter.
	It keeps a wait-free freelist inside the deallocated object storage for reallocation.

	Both benefit from the following advantages over malloc, VirtualAlloc and HeapAlloc
	- requiring the object size at destruction
	- keeping memory reserved for reuse during a process
	- betting on having many objects of the same size due to default sized tiling, thus making recycling 

	Complexity:

	Apart from the incidental large block allocation, allocation and
	deallocation is done in constant time.

	Defines:

	#define MG_CACHE_ALLOC to enable these allocators; when undefined, default allocation is used.
	#define MG_DEBUG_ALLOCATOR to maintain allocation statistics.
	#define MG_DEBUG_ALLOCATOR_SMALL to maintain allocation statistics on small object too.

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

// =========================================  implementation

// =========================================  types and constants section

#if defined(MG_CACHE_ALLOC)

using ChunkSize_type = UInt32;
using object_size_t = UInt32;
using objectstore_count_t = UInt32;
using alloc_index_t = UInt8;

using BYTE = unsigned char;
using BYTE_PTR = BYTE*;

constexpr alloc_index_t log2_min_chunk_size = 20;
constexpr alloc_index_t log2_max_chunk_size = 30;
constexpr alloc_index_t log2_default_tile_elem_count = 16;
constexpr alloc_index_t log2_max_elem_size = 4; // sizeof(DPoint) == sizof(IndexRange<UInt64>) == 16


constexpr ChunkSize_type MIN_CHUNK_SIZE = 1 << log2_min_chunk_size;
constexpr ChunkSize_type MAX_CHUNK_SIZE = 1 << log2_max_chunk_size;

constexpr UInt32 nr_bits_in_byte = 8;
constexpr UInt32 ALLOC_OBJSSIZE_MIN_BITS = 03; //  log2_default_segment_size - mpf::log2_v<nr_bits_in_byte>;      // 2^03 ==  8[B]
constexpr UInt32 ALLOC_PAGESIZE_MIN_BITS = 12; //  log2_default_segment_size - mpf::log2_v<nr_bits_in_byte>;      // 2^12 ==  4[kB]
constexpr UInt32 ALLOC_OBJSSIZE_MAX_BITS = 28; //  log2_extended_segment_size + mpf::log2_v<sizeof(Float64) * 2>; // 2^28 ==256[MB]
constexpr SizeT ALLOC_OBJSSIZE_MIN = 1 << ALLOC_OBJSSIZE_MIN_BITS;
constexpr SizeT ALLOC_PAGESIZE_MIN = 1 << ALLOC_PAGESIZE_MIN_BITS;
constexpr SizeT ALLOC_OBJSSIZE_MAX = 1 << ALLOC_OBJSSIZE_MAX_BITS;
constexpr UInt32 FIRST_PAGE_INDEX = ALLOC_PAGESIZE_MIN_BITS - ALLOC_OBJSSIZE_MIN_BITS;

constexpr alloc_index_t NR_ELEM_ALLOC = ALLOC_OBJSSIZE_MAX_BITS - ALLOC_OBJSSIZE_MIN_BITS + 1;

constexpr SizeT ALLOC_LIMIT = 0x1000000000000000;

// =========================================  forward decl section

#if defined(MG_DEBUG_ALLOCATOR)

void RegisterAlloc(void* ptr, size_t n MG_DEBUG_ALLOCATOR_SRC_ARG);
void RemoveAlloc(void* ptr, size_t n);

#endif //defined(MG_DEBUG_ALLOCATOR)

void PostReporting();
void ConsiderReporting();

std::allocator<UInt64> s_QWordArrayAllocator;

template<typename Unsigned>
constexpr alloc_index_t highest_bit_rank(Unsigned value)
{
	return sizeof(Unsigned)*8 - std::countl_zero(value);
}

// =========================================  VirtualAlloc section

#include <Windows.h>

struct VirtualAllocChunk
{
	VirtualAllocChunk(SizeT chunkSize_)
		: chunkSize(chunkSize_)
	{
		WaitForAvailableMemory(chunkSize_);
		chunkPtr = reinterpret_cast<BYTE_PTR>(VirtualAlloc(nullptr, chunkSize_, MEM_RESERVE, PAGE_NOACCESS));
	}

	VirtualAllocChunk(VirtualAllocChunk&& rhs) noexcept
		: chunkPtr(rhs.chunkPtr)
		, chunkSize(rhs.chunkSize)
	{
		rhs.chunkPtr = nullptr;
	}

	~VirtualAllocChunk()
	{
		if (chunkPtr)
			VirtualFree(chunkPtr, 0, MEM_RELEASE);
	}

	SizeT   ChunkSize() const { return chunkSize; }

	BYTE_PTR begin() const { return chunkPtr; }
	BYTE_PTR end  () const { return begin() + ChunkSize(); }

	// commit release and recommit are done without reference to the actual block in which the object memory will be (re)committed or was committed.
	static void commit(BYTE_PTR objectPtr, object_size_t objectSize, object_size_t objectStoreSize)
	{
		assert(std::popcount(objectStoreSize) == 1); // objectStoreSize is assumed to be a power of 2
		//		VirtualAlloc(objectPtr, objectSize, MEM_COMMIT, PAGE_READWRITE);
		VirtualAlloc(objectPtr, objectStoreSize, MEM_COMMIT, PAGE_READWRITE); // next occupant may use more of this store
	}

	static void release(BYTE_PTR objectPtr, object_size_t objectSize)
	{
//		VirtualAlloc(objectPtr, objectSize, MEM_RESET, PAGE_NOACCESS);
	}

	static void recommit(BYTE_PTR objectPtr, object_size_t objectSize)
	{
//		VirtualAlloc(objectPtr, objectSize, MEM_RESET_UNDO, PAGE_READWRITE);
	}

private:
	BYTE_PTR chunkPtr;
	SizeT chunkSize;
};


struct VirtualAllocChunkArray
{
	std::vector<VirtualAllocChunk> chunks;
	objectstore_count_t nrResevedButUncommitedObjectStores = 0;
	alloc_index_t log2ObjectStoreSize = 0;
	object_size_t objectStoreSize = 0;
	SizeT nextChunkSize = 0;
	BYTE_PTR lastChunkPtr = nullptr;

	VirtualAllocChunkArray() {}
	VirtualAllocChunkArray(const VirtualAllocChunkArray&) = delete;
	VirtualAllocChunkArray(VirtualAllocChunkArray&&) = delete;

	void Init_log2(alloc_index_t log2ObjectStoreSize_) // first block will be created at first call to get_reserved_objectstore
	{
		log2ObjectStoreSize = log2ObjectStoreSize_;
		objectStoreSize = 1 << log2ObjectStoreSize;
		assert(std::popcount(objectStoreSize) == 1); // objectStoreSize is assumed to be a power of 2.
		nextChunkSize = objectStoreSize;
		nextChunkSize *= 2;
		MakeMax(nextChunkSize, MIN_CHUNK_SIZE);
	}

	BYTE_PTR get_reserved_objectstore()
	{
		if (!nrResevedButUncommitedObjectStores)
		{
			chunks.emplace_back(nextChunkSize);
			nrResevedButUncommitedObjectStores = nextChunkSize >> log2ObjectStoreSize;

			if (nextChunkSize < MAX_CHUNK_SIZE) // double up to 1[GB]
				nextChunkSize <<= 1;
			lastChunkPtr = chunks.back().begin();
		}
		dms_assert(nrResevedButUncommitedObjectStores);
		auto currReservedButUncommittedObjectStoreIndex = --nrResevedButUncommitedObjectStores;
		return lastChunkPtr + (SizeT(currReservedButUncommittedObjectStoreIndex) << log2ObjectStoreSize);
	}
	void commit(BYTE_PTR ptr, object_size_t objectSize)
	{
		assert(objectSize && objectSize <= objectStoreSize);
		VirtualAllocChunk::commit(ptr, objectSize, objectStoreSize);
	}

	void release(BYTE_PTR ptr, object_size_t objectSize)
	{
		assert(objectSize && objectSize <= objectStoreSize);
		VirtualAllocChunk::release(ptr, objectSize);
	}

	void recommit(BYTE_PTR ptr, object_size_t objectSize)
	{
		assert(objectSize && objectSize <= objectStoreSize);
		VirtualAllocChunk::recommit(ptr, objectSize);
	}
};

// =========================================  FreeStackAllocator definition section

struct FreeStackAllocator
{
	VirtualAllocChunkArray inner;
	std::vector<BYTE_PTR> freeStack;
	mutable std::mutex allocSection;

	void Init_log2(alloc_index_t log2ObjectStoreSize) 
	{ 
		inner.Init_log2(log2ObjectStoreSize); 
	}

	std::pair<BYTE_PTR, bool> get_reserved_or_reset_objectstore()
	{
		// critical section from here to result in thread-local ownership of to be committed or recommitted span of [ptr, ptr+objectSize]
		std::scoped_lock lock(allocSection);

		if (freeStack.empty())
			return { inner.get_reserved_objectstore(), true };

		auto ptr = freeStack.back(); freeStack.pop_back();
		return { ptr, false };
	}

	BYTE_PTR allocate(object_size_t objectSize)
	{
		auto reserved_or_rest_block = get_reserved_or_reset_objectstore();
		if (reserved_or_rest_block.second)
			inner.commit(reserved_or_rest_block.first, objectSize);
		else
			inner.recommit(reserved_or_rest_block.first, objectSize);
		return reserved_or_rest_block.first;
	}

	BYTE_PTR allocate() { return allocate(inner.objectStoreSize); }

	void add_to_freestack(BYTE_PTR ptr)
	{
		std::scoped_lock lock(allocSection); // critical section here too
		freeStack.emplace_back(ptr);
	}
	void deallocate(BYTE_PTR ptr, object_size_t objectSize)
	{
		assert(objectSize);
		assert(objectSize <= inner.objectStoreSize);
		assert(objectSize > inner.objectStoreSize / 2);

		inner.release(ptr, objectSize); // still thread-local ownership during release of object's memory
		add_to_freestack(ptr); // will sync with other threads
	}
};

// =========================================  FreeStackAllocator instance section

#if defined(MG_DEBUG)
struct FreeStackAllocatorArray* sd_FSA_ptr = nullptr;
#endif

constexpr alloc_index_t NR_FREE_STACK_ALLOCS = ALLOC_OBJSSIZE_MAX_BITS - ALLOC_PAGESIZE_MIN_BITS + 1;


struct FreeStackAllocatorArray
{
	FreeStackAllocatorArray()
	{
		for (alloc_index_t i = ALLOC_PAGESIZE_MIN_BITS; i <= ALLOC_OBJSSIZE_MAX_BITS; ++i)
			(*this)[i - ALLOC_PAGESIZE_MIN_BITS].Init_log2(i);
#if defined(MG_DEBUG)
		sd_FSA_ptr = this;
#endif
	}
	FreeStackAllocator& operator [](alloc_index_t i)
	{ 
		assert(i < NR_FREE_STACK_ALLOCS);
		return freeStackAllocators[i]; 
	}

private:
	FreeStackAllocator freeStackAllocators[NR_FREE_STACK_ALLOCS];
};


FreeStackAllocatorArray& GetFreeStackAllocatorArray()
{
	static FreeStackAllocatorArray singleton;
	return singleton;
}

FreeStackAllocator& GetFreeStackAllocator(alloc_index_t i)
{
	auto& fsa = GetFreeStackAllocatorArray()[i];
	assert(fsa.inner.objectStoreSize);
	return fsa;
}

// =========================================  FreeListAllocator definition section

#include <boost/lockfree/stack.hpp>
#include <boost/lockfree/detail/freelist.hpp>
#include <boost/lockfree/detail/tagged_ptr_ptrcompression.hpp>

struct FreeListAllocator
{
	using SOS_PTR = BYTE_PTR; // pointer to Small Object Storage;
	using tagged_sos_ptr = boost::lockfree::detail::tagged_ptr<BYTE>; // 48 bit pointer, 16 bit tag to make ABA issue unlikely, depends on 64 bit CAS

	FreeStackAllocator* freeStackAllocator = nullptr;
	alloc_index_t log2SosSize = 0;
	object_size_t sosSize = 0;
	objectstore_count_t nrSosPerObjectStore = 0;

	std::atomic<tagged_sos_ptr> freeListPtr;
	std::atomic<UInt32> taggedNrReservedSosses = 0;

	mutable std::mutex allocSection;
	std::atomic<SOS_PTR> firstSosInCurrObjectStore = nullptr;
	

	void Init_log2(alloc_index_t log2SosSize_)
	{ 
		log2SosSize = log2SosSize_;
		sosSize = 1 << log2SosSize_;
		freeStackAllocator = &(GetFreeStackAllocatorArray()[log2SosSize_ - ALLOC_OBJSSIZE_MIN_BITS]);
		nrSosPerObjectStore = (freeStackAllocator->inner.objectStoreSize >> log2SosSize_);
		assert(nrSosPerObjectStore == 512);
	}

	SOS_PTR allocate(object_size_t smallObjectSize)
	{
		assert(freeStackAllocator);
		assert(freeListPtr.is_lock_free());

		// first try the free list - wait free
		tagged_sos_ptr currFreeTaggedPtr = freeListPtr.load(std::memory_order::consume);
		while (currFreeTaggedPtr.get_ptr()) {
			SOS_PTR poppedFreeListPtr = reinterpret_cast<tagged_sos_ptr*>(currFreeTaggedPtr.get_ptr())->get_ptr();
			tagged_sos_ptr newFreeTaggedPtr(poppedFreeListPtr, currFreeTaggedPtr.get_next_tag());

			if (freeListPtr.compare_exchange_weak(currFreeTaggedPtr, newFreeTaggedPtr))
			{
				auto ptr = currFreeTaggedPtr.get_ptr();
				assert(ptr);
				return ptr;
			}
		}
		assert(currFreeTaggedPtr.get_ptr() == nullptr);

		// then try reserved small objects - wait free
		while (true)
		{
			UInt32 currTaggedNrReservedSosses = taggedNrReservedSosses;
			while (currTaggedNrReservedSosses & 0x0000FFFF)
			{
				UInt32 newTaggedNrReservedSosses = currTaggedNrReservedSosses - 1; // same tag, one less reserved
				auto currfirstSosInCurrObjectStore = firstSosInCurrObjectStore.load(std::memory_order::acquire);
				if (taggedNrReservedSosses.compare_exchange_weak(currTaggedNrReservedSosses, newTaggedNrReservedSosses))
				{
					assert(currTaggedNrReservedSosses & 0x0000FFFF);
					assert(newTaggedNrReservedSosses == currTaggedNrReservedSosses - 1);
					return currfirstSosInCurrObjectStore + (SizeT(newTaggedNrReservedSosses & 0x0000FFFF) << log2SosSize);
				}
			}

			// critical section from here: allocate a ObjectStore from freeStackAllocator
			std::scoped_lock lock(allocSection);
			// already done ?
			if (currTaggedNrReservedSosses == taggedNrReservedSosses)
			{
				firstSosInCurrObjectStore = freeStackAllocator->allocate();
				taggedNrReservedSosses = nrSosPerObjectStore + ((currTaggedNrReservedSosses & 0xFFFF0000) + 0x00010000); // grant next tag
			}
		}
	}
	void deallocate(SOS_PTR smallObjectPtr, object_size_t objectSize)
	{
		assert(objectSize);
		assert(objectSize <= sosSize);
		assert(objectSize > sosSize/2 || sosSize == 8);

		tagged_sos_ptr currFreeListTaggedPtr = freeListPtr.load(std::memory_order::consume);
		while (true)
		{
			*reinterpret_cast<tagged_sos_ptr*>(smallObjectPtr) = currFreeListTaggedPtr;
			if (freeListPtr.compare_exchange_weak(currFreeListTaggedPtr, tagged_sos_ptr(smallObjectPtr, currFreeListTaggedPtr.get_tag())))
				break;
		}
	}
};

// =========================================  FreeListAllocator instance section

#if defined(MG_DEBUG)
struct FreeListAllocatorArray* sd_FLA_ptr = nullptr;
#endif

constexpr alloc_index_t NR_FREE_LIST_ALLOCS = ALLOC_PAGESIZE_MIN_BITS - ALLOC_OBJSSIZE_MIN_BITS;

struct FreeListAllocatorArray
{
	FreeListAllocatorArray()
	{
		for (alloc_index_t i = ALLOC_OBJSSIZE_MIN_BITS; i < ALLOC_PAGESIZE_MIN_BITS; ++i)
			(*this)[i- ALLOC_OBJSSIZE_MIN_BITS].Init_log2(i);
#if defined(MG_DEBUG)
		sd_FLA_ptr = this;
#endif
	}
	FreeListAllocator& operator [](alloc_index_t i)
	{
		assert(i < NR_FREE_LIST_ALLOCS);
		return freeListAllocators[i];
	}

private:
	FreeListAllocator freeListAllocators[NR_FREE_LIST_ALLOCS];
};


FreeListAllocatorArray& GetFreeListAllocatorArray() // todo: wrap in ComponentService to avoid checking static initialisation all the time
{
	static FreeListAllocatorArray singleton;
	return singleton;
}

FreeListAllocator& GetFreeListAllocator(alloc_index_t i) 
{
	auto& fla = GetFreeListAllocatorArray()[i];
	assert(fla.freeStackAllocator);
	return fla;
}

//----------------------------------------------------------------------
// fixed_allocator arrays
//----------------------------------------------------------------------

constexpr alloc_index_t BlockListIndex(SizeT sz)
{
	SizeT org_sz = sz;
	assert(sz);
	assert(sz <= ALLOC_OBJSSIZE_MAX);

	--sz;
	sz >>= ALLOC_OBJSSIZE_MIN_BITS;
	return highest_bit_rank(sz);
}


static UInt32 g_ElemAllocCounter = 0;
ElemAllocComponent s_Initialize;

#endif defined(MG_CACHE_ALLOC)

//----------------------------------------------------------------------
// inmplement interface
//----------------------------------------------------------------------


void* AllocateFromStock_impl(size_t objectSize)
{

#if defined(MG_CACHE_ALLOC)
	assert(objectSize);
	auto i = BlockListIndex(objectSize);
	if (i < FIRST_PAGE_INDEX)
	{
		auto& freeStack = GetFreeListAllocator(i);
		return freeStack.allocate(objectSize);
	}
	i -= FIRST_PAGE_INDEX;
	if (i < NR_FREE_STACK_ALLOCS)
	{
		auto& freeStack = GetFreeStackAllocator(i);
		return freeStack.allocate(objectSize);
	}
	assert(objectSize > ALLOC_OBJSSIZE_MAX);
#endif //defined(MG_CACHE_ALLOC)

	SizeT qWordCount = ((objectSize + (sizeof(UInt64) - 1)) & ~(sizeof(UInt64) - 1)) / sizeof(UInt64);

	WaitForAvailableMemory(qWordCount * sizeof(UInt64));
	auto result = s_QWordArrayAllocator.allocate(qWordCount);
	return result;

}

void* AllocateFromStock(size_t objectSize MG_DEBUG_ALLOCATOR_SRC_ARG)
{
	if (!objectSize)
		return nullptr;

	auto result = AllocateFromStock_impl(objectSize);

#if defined(MG_DEBUG_ALLOCATOR)
	RegisterAlloc(result, objectSize MG_DEBUG_ALLOCATOR_SRC_PARAM);
#endif //defined(MG_DEBUG_ALLOCATOR)

	ConsiderReporting();
	return result;
}

void LeaveToStock(void* objectPtr, size_t objectSize) {
	if (!objectSize)
	{
		dms_assert(objectPtr == nullptr);
		return;
	}
#if defined(MG_DEBUG_ALLOCATOR)
	RemoveAlloc(objectPtr, objectSize);
#endif //defined(MG_DEBUG_ALLOCATOR)

#if defined(MG_CACHE_ALLOC)
	auto i = BlockListIndex(objectSize);
	if (i < FIRST_PAGE_INDEX)
	{
		auto& alloc = GetFreeListAllocator(i);
		return alloc.deallocate(reinterpret_cast<BYTE_PTR>(objectPtr), objectSize);
	}
	i -= FIRST_PAGE_INDEX;
	if (i < NR_FREE_STACK_ALLOCS)
	{
		auto& alloc = GetFreeStackAllocator(i);
		return alloc.deallocate(reinterpret_cast<BYTE_PTR>(objectPtr), objectSize);
	}

#endif //defined(MG_CACHE_ALLOC)

	SizeT qWordCount = ((objectSize + (sizeof(UInt64) - 1)) & ~(sizeof(UInt64) - 1)) / sizeof(UInt64);
	s_QWordArrayAllocator.deallocate(reinterpret_cast<UInt64*>(objectPtr), qWordCount);
}

//----------------------------------------------------------------------
// Reporting
//----------------------------------------------------------------------

std::atomic<bool> s_ReportingRequestPending = false;

void ReportFixedAllocStatus()
{
	s_ReportingRequestPending = false;
/*
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
*/

}

//----------------------------------------------------------------------
// clean-up
//----------------------------------------------------------------------

ElemAllocComponent::ElemAllocComponent()
{
	if (g_ElemAllocCounter++)
		return;

	SetMainThreadID();

	GetFreeStackAllocatorArray();
	GetFreeListAllocatorArray();

#if defined(MG_DEBUG)
//	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF /*| _CRTDBG_CHECK_CRT_DF | _CRTDBG_CHECK_ALWAYS_DF*/);
//	_CrtSetBreakAlloc(6548); 
//	Known leak: iconv-2.dll->relocatable.c->DllMail contains  shared_library_fullname = strdup(location); which leaks 44 bytes of memory
#endif

}

ElemAllocComponent::~ElemAllocComponent()
{
	if (--g_ElemAllocCounter)
		return;
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
	objectstore_count_t i = 0;

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

