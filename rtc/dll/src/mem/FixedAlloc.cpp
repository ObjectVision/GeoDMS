// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////

#include "RtcPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif

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

	It keeps a lock-free unused counter.
	It keeps a lock-free freelist inside the deallocated object storage for reallocation.

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

#include "RtcBase.h"

#include "act/MainThread.h"
#include "dbg/SeverityType.h"
#include "vt/mpf.h"
#include "mem/FixedAlloc.h"
#include "set/VectorFunc.h"
#include "utl/Environment.h"
#include "utl/Registry.h" // RegDWordEnum::MemoryDrainage: the drainage on/off setting
#include "utl/IncrementalLock.h"
#include "utl/MemGuard.h"    // IsLowOnFreeRAM: RAM use vs MemoryFlushThreshold, the drainage trigger
#include "xct/DmsException.h"

// Per-operation allocation accounting. Hooked at the SAME two points as RegisterAlloc/RemoveAlloc
// (AllocateFromStock / LeaveToStock) so it sees every allocation; an earlier version hooked
// FreeStackAllocator::allocate and therefore only saw the pooled SpecialSize subset, a minority of
// the traffic (t641_1: fs inUse 5.5 GB vs req 42.9 GB).
//
// The register lives here and is owned by ElemAllocComponent, exactly like s_AllocRegister: that
// component is the base every allocating component derives from, so it is constructed first and
// destroyed last and the register's lifetime brackets all allocation by construction. No lifetime
// flags, no leaked singleton -- the dependency is expressed by the component hierarchy
// (RtcComponents.h) and checked by assert on use.
//
#if defined(MG_CACHE_COLLECTDATA)
// Only these three come from tic, type-erased so the allocator never needs OperationContext defined
// (including tic/OperationContext.h here does not compile: this TU has no tic prerequisites).
// Conditional together with the register: without it a free cannot find its owner, so tic compiles
// the charge counters and these entry points out as well (OperationContext.h).
RTC_CALL auto CurrentOperationRef() -> std::shared_ptr<void>;
RTC_CALL void ChargeOperationAlloc(void* ocPtr, SizeT bytes);
RTC_CALL void ChargeOperationFree(void* ocPtr, SizeT bytes);

// Defined below, beside the register they use; declared here because AllocateFromStock and
// LeaveToStock appear earlier in this file.
void NoteAllocation(void* ptr, SizeT bytes);
void NoteDeallocation(void* ptr, SizeT bytes);
#endif //defined(MG_CACHE_COLLECTDATA)

#include <chrono>
#include <memory>
#include <unordered_map>

#define MG_CACHE_ALLOC

//#define MG_CACHE_ALLOC_SMALL
#define MG_CACHE_ALLOC_ONLY_SPECIALSIZE

// Per-operation allocation accounting for estimator calibration (§8.1.16). Each object store
// remembers the OperationContext that was this thread's CancelableFrame::CurrActive() when it was
// handed out, so the matching free can discharge the SAME operation -- which is what a plain
// counter cannot do, because at deallocation the frame that allocated is long gone.
//
// Diagnostic only. Costs a hash-map insert/erase per large (de)allocation, under the allocator's
// existing allocSection, so it must not ship on by default.
//
// NOTE, deviating from the proposal: this does NOT enable MG_CACHE_ALLOC_SMALL. Doing so would
// change which allocator serves every sub-8 KB object -- a live behavioural change to production
// paths that have been compiled out for a long time -- and the memory being calibrated is in the
// large classes anyway. Small-object attribution can be added later by lifting that switch
// deliberately rather than as a side effect of turning on diagnostics.
//
// The MG_CACHE_COLLECTDATA switch itself lives in mem/FixedAlloc.h: OperationContext.h conditions
// its per-operation counters on it, so it must be visible identically to every module.

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
constexpr alloc_index_t NR_FREE_STACK_ALLOCS = ALLOC_OBJSSIZE_MAX_BITS - ALLOC_PAGESIZE_MIN_BITS + 1;

constexpr SizeT ALLOC_LIMIT = 0x1000000000000000;

// =========================================  forward decl section

#if defined(MG_DEBUG_ALLOCATOR)

void RegisterAlloc(void* ptr, size_t n MG_DEBUG_ALLOCATOR_SRC_ARG);
void RemoveAlloc(void* ptr, size_t n);

#endif //defined(MG_DEBUG_ALLOCATOR)

void PostReporting();
void ConsiderReporting();

template<typename Unsigned>
constexpr alloc_index_t highest_bit_rank(Unsigned value)
{
	return sizeof(Unsigned)*8 - std::countl_zero(value);
}

// =========================================  VirtualAlloc section

#if defined(WIN32)
#include <Windows.h>
#else
#include <sys/mman.h>
#endif

// ===== Instrumentation for the decommit-on-release experiment ==============================
// The question this answers is NOT "is it faster" but "does it reintroduce the serialisation the
// lock-free allocator exists to avoid". VirtualAlloc/VirtualFree take the process address-space
// lock, so if workers pile up inside them we are back to one-thread-at-a-time on the very path that
// was made lock-free. inFlightPeak is the direct evidence: it is the largest number of threads ever
// simultaneously inside a decommit/recommit syscall. 1-2 means no congestion; a number near the
// worker count means the syscall is serialising the pool.
static std::atomic<UInt64> s_DecommitCount = 0, s_RecommitCount = 0;
static std::atomic<UInt64> s_DecommitTicks = 0, s_RecommitTicks = 0;
// The pressure drain (§8.1.24) gets its own pair so its cost stays distinguishable from the
// >= 2 MB threshold path above; both share the in-flight congestion gauge below.
static std::atomic<UInt64> s_DrainCount = 0, s_DrainTicks = 0;
static std::atomic<UInt32> s_VmSysCallsInFlight = 0, s_VmSysCallsInFlightPeak = 0;

struct VmSysCallScope // RAII: gauge in, gauge out, accumulate elapsed ticks
{
	std::atomic<UInt64>& m_Count;
	std::atomic<UInt64>& m_Ticks;
	Int64 m_T0 = 0;

	VmSysCallScope(std::atomic<UInt64>& count, std::atomic<UInt64>& ticks)
		: m_Count(count), m_Ticks(ticks)
	{
		auto inFlight = s_VmSysCallsInFlight.fetch_add(1, std::memory_order_relaxed) + 1;
		auto prev = s_VmSysCallsInFlightPeak.load(std::memory_order_relaxed);
		while (inFlight > prev
			&& !s_VmSysCallsInFlightPeak.compare_exchange_weak(prev, inFlight, std::memory_order_relaxed))
			;
#if defined(WIN32)
		LARGE_INTEGER t; QueryPerformanceCounter(&t); m_T0 = t.QuadPart;
#endif
	}
	~VmSysCallScope()
	{
#if defined(WIN32)
		LARGE_INTEGER t; QueryPerformanceCounter(&t);
		m_Ticks.fetch_add(UInt64(t.QuadPart - m_T0), std::memory_order_relaxed);
#endif
		m_Count.fetch_add(1, std::memory_order_relaxed);
		s_VmSysCallsInFlight.fetch_sub(1, std::memory_order_relaxed);
	}
};

struct VirtualAllocChunk
{
	VirtualAllocChunk(SizeT chunkSize_)
		: chunkSize(chunkSize_)
	{
		if (s_BlockNewAllocations)
			throw MemoryAllocFailure();

		WaitForAvailableMemory(chunkSize_);
#if defined(WIN32)
		chunkPtr = reinterpret_cast<BYTE_PTR>(VirtualAlloc(nullptr, chunkSize_, MEM_RESERVE, PAGE_NOACCESS));
#else
		chunkPtr = reinterpret_cast<BYTE_PTR>(mmap(nullptr, chunkSize_, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
		if (chunkPtr == MAP_FAILED) chunkPtr = nullptr;
#endif
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
#if defined(WIN32)
			VirtualFree(chunkPtr, 0, MEM_RELEASE);
#else
			munmap(chunkPtr, chunkSize);
#endif
	}

	SizeT   ChunkSize() const { return chunkSize; }

	BYTE_PTR begin() const { return chunkPtr; }
	BYTE_PTR end  () const { return begin() + ChunkSize(); }

	// commit release and recommit are done without reference to the actual block in which the object memory will be (re)committed or was committed.
	static void commit(BYTE_PTR objectPtr, object_size_t objectSize, object_size_t objectStoreSize)
	{
		assert(std::popcount(objectStoreSize) == 1); // objectStoreSize is assumed to be a power of 2
#if defined(WIN32)
		VirtualAlloc(objectPtr, objectStoreSize, MEM_COMMIT, PAGE_READWRITE); // next occupant may use more of this store
#else
		mprotect(objectPtr, objectStoreSize, PROT_READ | PROT_WRITE);
#endif
	}

	// Only blocks at or above this are handed back to the OS. Below it, a freed store stays
	// committed and the free list serves it without any syscall -- which is the whole point of the
	// lock-free allocator.
	//
	// Why 2 MB. The size profile is tile-shaped: a default tile is 2^16 elements, so the classes
	// from 8 KB (Bool, 1/8 B) up to 1 MB (DPoint / pair<SizeT>, 16 B) are ORDINARY TILE BUFFERS,
	// with 4 KB appearing as a domain's smaller last tile. Those are recycled constantly -- a
	// decommit there is immediately undone by the matching recommit, pure syscall churn. Everything
	// at 2 MB and above is the sequence/string payload of a tile, which is where the retained volume
	// actually sits: on t641_2 the >= 2 MB classes account for ~223 GB of allocation.
	// Measured before this threshold existed: 34.1 M decommits costing 4 228 s of thread time with
	// all 33 workers inside the address-space lock at once (§8.1.14).
	static constexpr object_size_t DECOMMIT_MIN_SIZE = object_size_t(1) << 21; // 2 MB

	static void release(BYTE_PTR objectPtr, object_size_t objectSize)
	{
		if (objectSize < DECOMMIT_MIN_SIZE)
			return; // keep it committed; the free list will hand it out again without a syscall
#if defined(WIN32)
		VmSysCallScope scope(s_DecommitCount, s_DecommitTicks);
		VirtualFree(objectPtr, objectSize, MEM_DECOMMIT);
#else
//		madvise(objectPtr, objectSize, MADV_DONTNEED);
#endif
	}

	static void recommit(BYTE_PTR objectPtr, object_size_t objectSize)
	{
		if (objectSize < DECOMMIT_MIN_SIZE)
			return; // never decommitted, so still committed: nothing to undo
#if defined(WIN32)
		VmSysCallScope scope(s_RecommitCount, s_RecommitTicks);
		VirtualAlloc(objectPtr, objectSize, MEM_COMMIT, PAGE_READWRITE);
#endif
	}

	// Pressure drain (§8.1.24): unconditional decommit of a WHOLE free store. The
	// < DECOMMIT_MIN_SIZE early-out of release() is exactly what this bypasses -- it is called
	// only for stores release() left committed, taken from the cold end of a free stack while
	// the scheduler signals drainage. The store is free in full, so the full objectStoreSize
	// goes back, mirroring commit() which committed the full store; reuse goes through commit()
	// again (the pop hands a drained store out as if freshly reserved).
	static void decommit_free_store(BYTE_PTR objectPtr, object_size_t objectStoreSize)
	{
		assert(std::popcount(objectStoreSize) == 1);
#if defined(WIN32)
		VmSysCallScope scope(s_DrainCount, s_DrainTicks);
		VirtualFree(objectPtr, objectStoreSize, MEM_DECOMMIT);
#else
//		madvise(objectPtr, objectStoreSize, MADV_DONTNEED); // same posture as release(): not enabled on linux
#endif
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
		MakeMax<SizeT>(nextChunkSize, MIN_CHUNK_SIZE);
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
		assert(nrResevedButUncommitedObjectStores);
		auto currReservedButUncommittedObjectStoreIndex = --nrResevedButUncommitedObjectStores;
		return lastChunkPtr + (SizeT(currReservedButUncommittedObjectStoreIndex) << log2ObjectStoreSize);
	}
	void commit(BYTE_PTR ptr, object_size_t objectSize)
	{
		assert(objectSize > objectStoreSize /2 && objectSize <= objectStoreSize);
		VirtualAllocChunk::commit(ptr, objectSize, objectStoreSize);
	}

	void release(BYTE_PTR ptr, object_size_t objectSize)
	{
		assert(objectSize > objectStoreSize / 2 && objectSize <= objectStoreSize);
		VirtualAllocChunk::release(ptr, objectSize);
	}

	void recommit(BYTE_PTR ptr, object_size_t objectSize)
	{
		assert(objectSize > objectStoreSize / 2 && objectSize <= objectStoreSize);
		VirtualAllocChunk::recommit(ptr, objectSize);
	}
};

// =========================================  FreeStackAllocSummary

using FreeStackAllocSummary = std::tuple<SizeT, SizeT, SizeT, SizeT, SizeT>;

FreeStackAllocSummary operator +(FreeStackAllocSummary lhs, FreeStackAllocSummary rhs)
{
	return FreeStackAllocSummary(
		std::get<0>(lhs) + std::get<0>(rhs)
	,	std::get<1>(lhs) + std::get<1>(rhs)
	,	std::get<2>(lhs) + std::get<2>(rhs)
	,	std::get<3>(lhs) + std::get<3>(rhs)
	,	std::get<4>(lhs) + std::get<4>(rhs)
	);
}

// =========================================  FreeStackAllocator definition section

// Cross-check for PeakLiveLarge (user request 2026-08-01). Maintained at exactly the two points
// where objectCount moves, so it equals the sum of objectStoreSize*objectCount over all free-stack
// allocators by construction -- the same quantity ReportStatus computes, but continuously and
// without taking any lock. Comparing it against the AllocateFromStock live counter is the direct
// test: absent an MT defect the two must agree to within size-class rounding (< 2x), because every
// free-stack object store is handed out by exactly one AllocateFromStock call.
static std::atomic<SizeT> s_FreeStackLiveBytes = 0;
static std::atomic<SizeT> s_PeakFreeStackBytes = 0;
SizeT GetFreeStackLiveBytes() { return s_FreeStackLiveBytes.load(std::memory_order_relaxed); }
SizeT GetPeakFreeStackBytes() { return s_PeakFreeStackBytes.load(std::memory_order_relaxed); }

// Bytes of freed stores that are still COMMITTED: the reclaimable dead pool, measured at 53-112 GB
// on t641 (§8.1.23). The admission gate subtracts it from the observed process commit, because that
// memory is not held by anything -- drainage hands it back as soon as the gate is short of budget,
// so counting it as occupancy would throttle a run for memory it can have for the asking.
//
// Same population as s_DeallocatedButNotYetDecommitted counts, in bytes. Unlike that gate counter,
// this one pays the class-size test on the reuse-pop path: the gate counter may drift low and be
// recalibrated by the next sweep, but a drifting BYTE figure would silently bias every admission
// decision, and low drift here means under-subtracting, i.e. throttling for memory that is free.
// Every mutation is made under the shared allocSection; the atomic is for the lock-free reader.
static std::atomic<SizeT> s_FreeStackDeadBytes = 0;
SizeT GetFreeStackDeadBytes() { return s_FreeStackDeadBytes.load(std::memory_order_relaxed); }

// Drainage mode (§8.1.24): set by the admission ledger while a refused task's claim is pending
// (enforce mode only), cleared when the claim resolves. While it is on, every allocation from a
// free-stack class also decommits one cold freed store of that class -- the committed dead pool
// (measured at 53-112 GB on t641, §8.1.23) flows back to the OS at the pace of allocation
// traffic, exactly for as long as the scheduler is actually short of budget.
static std::atomic<bool> s_FreeStackDrainageMode = false; // effective: what the sweep gate reads
static std::atomic<bool> s_LedgerDrainageMode = false;    // the ledger's claim window (enforce only)
static std::atomic<bool> s_PressureDrainageMode = false;  // RAM use above MemoryFlushThreshold
static std::atomic<bool> s_DrainageIsStanding = false;    // pressure-driven: use the sustainable band
static std::atomic<SizeT> s_DrainedStackBytes = 0;        // lifetime bytes handed back by the drain

// -1 until the MemoryDrainage setting has been read; /CF and /SF write it directly, so the
// switch does not have to wait for the next pressure poll to be honoured.
static std::atomic<Int32> s_DrainageEnabled = -1;
static void UpdateEffectiveDrainage();

bool IsFreeStackDrainageEnabled()
{
	auto v = s_DrainageEnabled.load(std::memory_order_relaxed);
	if (v < 0)
	{
		v = (RTC_GetRegDWord(RegDWordEnum::MemoryDrainage) != 0) ? 1 : 0;
		s_DrainageEnabled.store(v, std::memory_order_relaxed);
	}
	return v != 0;
}

RTC_CALL void SetFreeStackDrainageEnabled(bool enabled)
{
	s_DrainageEnabled.store(enabled ? 1 : 0, std::memory_order_relaxed);
	UpdateEffectiveDrainage(); // take effect now, not at the next poll
}

static void UpdateEffectiveDrainage()
{
	bool ledger = s_LedgerDrainageMode.load(std::memory_order_relaxed);
	bool pressure = s_PressureDrainageMode.load(std::memory_order_relaxed);
	bool enabled = IsFreeStackDrainageEnabled();
	// A claim window is a short burst with a task waiting: drain hard. Standing RAM pressure is
	// open-ended, and §8.1.30 measured what draining hard for a whole run costs (1.7 TB
	// re-committed, +20 % wall), so that mode keeps a proportional reuse band instead.
	s_DrainageIsStanding.store(pressure && !ledger, std::memory_order_relaxed);
	s_FreeStackDrainageMode.store(enabled && (ledger || pressure), std::memory_order_relaxed);
}

void SetFreeStackDrainageMode(bool active)
{
	s_LedgerDrainageMode.store(active, std::memory_order_relaxed);
	UpdateEffectiveDrainage();
}
bool InFreeStackDrainageMode() { return s_FreeStackDrainageMode.load(std::memory_order_relaxed); }

// The 20.10.0 default trigger (§8.1.32): once the machine's RAM use passes
// MemoryFlushThreshold -- the same signal that already throttles operation activation via
// IsLowOnFreeRAM -- freed <2 MB stores start flowing back to the OS, with no scheduler
// involvement, so it works with the admission gate off. Polled at most once a second, and only
// on every 1024th large allocation, so the steady-state cost is one relaxed load.
static std::atomic<UInt32> s_DrainagePollCountdown = 0;
static std::atomic<Int64> s_DrainageNextPollNs = 0;

// The allocator must not touch the configuration system before the runtime is up.
// ConsiderDrainagePressure reads THREE registry settings -- MemoryFlushThreshold and
// MemoryRAM_MAX_GB via IsLowOnFreeRAM, and MemoryDrainage via UpdateEffectiveDrainage -- but
// allocation starts during module initialisation, long before any of that exists. On Linux
// RTC_GetRegDWord is served from an ini cache whose std::map is a dynamically initialised static
// in another translation unit, and static init order across TUs is unspecified: the very first
// large allocation (TokenComponent's string pool, from libDmRtc's own static init) walked an
// unconstructed red-black tree and segfaulted, so EVERY GeoDmsRun/GeoDmsGuiQt invocation died at
// startup on Linux -- 20.11.0 exit 139 on every unit test. Windows was unaffected only because
// RTC_GetRegDWord reads the real registry there rather than an ini file.
//
// So (user design 2026-08-06): until the process has allocated DRAINAGE_ARM_BYTES, behave as if
// MemoryFlushThreshold were 100% -- never low on RAM, hence no pressure drainage -- and read no
// configuration at all. The first large allocation past that mark arms the poll and reads the
// settings ONCE. A process that never allocates 8 GB never reads them from here, and any process
// that does has long since finished startup and config load, so the threshold is picked up from
// the real setting rather than assumed.
constexpr SizeT DRAINAGE_ARM_BYTES = SizeT(8) << 30; // 8 GB of large allocations
static std::atomic<SizeT> s_LargeAllocBytesTotal = 0; // cumulative; drives the arming test only
static std::atomic<bool>  s_DrainageArmed = false;

// false while the allocator must stay clear of the configuration system.
static bool ArmDrainageWhenWarm(size_t objectSize)
{
	if (s_DrainageArmed.load(std::memory_order_relaxed))
		return true;

	auto total = s_LargeAllocBytesTotal.fetch_add(objectSize, std::memory_order_relaxed) + objectSize;
	if (total < DRAINAGE_ARM_BYTES)
		return false;

	// Exactly one thread arms, and it is the one that pays for the single registry read.
	bool expected = false;
	if (!s_DrainageArmed.compare_exchange_strong(expected, true, std::memory_order_relaxed))
		return true; // another thread got there first

	IsFreeStackDrainageEnabled(); // read MemoryDrainage once and latch it into s_DrainageEnabled
	return true;
}

static void ConsiderDrainagePressure()
{
	if (s_DrainagePollCountdown.fetch_add(1, std::memory_order_relaxed) % 1024)
		return;
	auto nowNs = std::chrono::steady_clock::now().time_since_epoch().count();
	auto nextNs = s_DrainageNextPollNs.load(std::memory_order_relaxed);
	if (nowNs < nextNs)
		return;
	if (!s_DrainageNextPollNs.compare_exchange_strong(nextNs, nowNs + 1'000'000'000, std::memory_order_relaxed))
		return; // another thread is polling this second
	s_PressureDrainageMode.store(IsLowOnFreeRAM(), std::memory_order_relaxed);
	// Recompute unconditionally, not only when the pressure bit flips: the first poll can happen
	// before the command line is parsed (allocation starts during module init), so a latched
	// decision would ignore /CF. This way the setting is honoured within a second either way.
	UpdateEffectiveDrainage();
}

static void DrainAllFreeStacks_lockHeld(); // defined below GetFreeStackAllocatorArray()

// Sweep gate (user design 2026-08-04): stores pushed COMMITTED onto free stacks and not yet
// decommitted or recounted. Plain, not atomic: every touch point -- the push in
// add_to_freestack, the gate test in get_reserved_or_reset_objectstore, the recalibration in
// DrainAllFreeStacks_lockHeld -- runs under the shared allocSection. Once the pools are dry
// this is what makes a drainage-mode allocation cost one flag load and one compare instead of
// a 17-slot walk. Reuse-pops shrink the true backlog without decrementing (they would need a
// class-size test on the pop path); the sweep's recount corrects that within one low-yield
// walk.
static SizeT s_DeallocatedButNotYetDecommitted = 0;

// §8.1.27 budgeted-sweep tuning.
// DRAIN_SWEEP_BUDGET caps one sweep's decommits: ~64 x ~30 us ~= 2 ms worst-case hold of the
// shared allocSection, against the unbudgeted worst case of a whole release wave in one call
// (~80 K stores ~= seconds of stop-the-world). A backlog concentrated in one class drains up
// to 64x faster than the §8.1.25 one-per-class step, with the walk overhead divided by the
// batch size.
// KEEP_HOT_STORES spares the back-most stores of each stack as the reuse band: §8.1.24's
// per-class arm measured what draining stores the next allocation wants back costs -- 103 GiB
// re-zero-faulted, ~+5 % wall. Kept-hot stores become drainable only when newer deallocations
// push them deeper than KEEP_HOT_STORES from the back.
constexpr SizeT DRAIN_SWEEP_BUDGET = 64;
constexpr SizeT KEEP_HOT_STORES = 4;

// Classes at or above DECOMMIT_MIN_SIZE are decommitted by release() at free, so free-stack
// index i is drainable only while 2^(i + ALLOC_PAGESIZE_MIN_BITS) < DECOMMIT_MIN_SIZE: indices
// 0 (4 KB) .. NR_DRAINABLE_FSA-1 (1 MB), 9 of the 17 classes. The sweep walks exactly these --
// and walks them LARGEST FIRST (§8.1.28).
constexpr alloc_index_t NR_DRAINABLE_FSA =
	alloc_index_t(std::countr_zero(VirtualAllocChunk::DECOMMIT_MIN_SIZE)) - ALLOC_PAGESIZE_MIN_BITS;

// Sweep cost gauges (§8.1.28): sweeps run, and the worst single one. The max is the direct
// measure of how long a drain sweep can hold the shared allocSection -- the number that decides
// whether draining outside the lock is worth its complexity, rather than an assumption about it.
static std::atomic<UInt64> s_DrainSweepCount = 0, s_DrainSweepMaxTicks = 0;

struct FreeStackAllocator
{
	VirtualAllocChunkArray inner;
	std::vector<BYTE_PTR> freeStack;
	objectstore_count_t objectCount = 0;
	// Pressure-drain bookkeeping (§8.1.24), under the shared allocSection. INVARIANT: the
	// decommitted stores are exactly the cold PREFIX freeStack[0 .. nr_deallocated); everything
	// from nr_deallocated onward is committed. The three mutations each preserve it:
	// add_to_freestack pushes a committed store at the BACK; the pop takes the back, which lies
	// inside the prefix only when the prefix spans the whole stack (then the popped store is
	// handed out as-if-fresh and nr_deallocated shrinks with the stack); the drain step
	// decommits freeStack[nr_deallocated] itself and advances. Only meaningful for classes
	// below DECOMMIT_MIN_SIZE: larger stores are decommitted by release() and re-enter the
	// stack cold at the BACK, so for those classes nr_deallocated stays 0 and the existing
	// recommit() covers reuse.
	objectstore_count_t nr_deallocated = 0;
	// ONE mutex for all FreeStackAllocators (user ruling 2026-08-03): allocation traffic is
	// block-size correlated -- a workload hammers one or two classes at a time, so the per-class
	// striping this replaces mostly serialized the same hot class anyway -- and a single lock is
	// what lets the drain sweep ALL stacks from the allocation path without nesting or ordering
	// concerns (§8.1.25). std::mutex has a constexpr ctor, so constant-initialized.
	static inline std::mutex allocSection;

	void Init_log2(alloc_index_t log2ObjectStoreSize) 
	{ 
		inner.Init_log2(log2ObjectStoreSize); 
	}

	std::pair<BYTE_PTR, bool> get_reserved_or_reset_objectstore()
	{
		// critical section from here to result in thread-local ownership of to be committed or recommitted span of [ptr, ptr+objectSize]
		std::lock_guard guard(allocSection);

		objectCount++;
		auto fsLive = s_FreeStackLiveBytes.fetch_add(inner.objectStoreSize, std::memory_order_relaxed)
			+ inner.objectStoreSize;
		{	// CAS max, not load/store -- see the note on s_PeakLargeAllocBytes
			auto prev = s_PeakFreeStackBytes.load(std::memory_order_relaxed);
			while (fsLive > prev
				&& !s_PeakFreeStackBytes.compare_exchange_weak(prev, fsLive, std::memory_order_relaxed))
				;
		}

		std::pair<BYTE_PTR, bool> result;
		if (freeStack.empty())
			result = { inner.get_reserved_objectstore(), true };
		else
		{
			assert(nr_deallocated <= freeStack.size());
			// the back is committed unless the decommitted prefix spans the whole stack; a cold
			// back is handed out as-if-fresh, so the caller's commit() re-commits the full store
			bool poppedColdStore = (nr_deallocated == freeStack.size());
			result = { freeStack.back(), poppedColdStore };
			freeStack.pop_back();
			if (poppedColdStore)
				--nr_deallocated; // was decommitted: it held no commit charge, so the dead pool is unchanged
			else if (inner.objectStoreSize < VirtualAllocChunk::DECOMMIT_MIN_SIZE)
				s_FreeStackDeadBytes.fetch_sub(inner.objectStoreSize, std::memory_order_relaxed); // a committed dead store came back to life
		}

		// Pressure-triggered decommit (§8.1.24; array-wide sweep §8.1.25; budgeted §8.1.27):
		// while the scheduler drains, each allocation funds one budget's worth of decommits
		// across ALL classes -- coldest first, the FRONT of each stack, sparing each stack's
		// kept-hot reuse band. allocSection is shared by all FreeStackAllocators, so the sweep
		// runs under the lock already held here, and the size-correlation of allocation
		// traffic means an idle class's pool no longer shelters behind the busy one (the
		// §8.1.24 defect). The gate counter (user design 2026-08-04) spares the walk once
		// nothing is drainable: sweep only when more drainable stores are pending than a walk
		// has classes to visit. Cost and congestion stay visible in GetVmSysCallStats.
		if (s_FreeStackDrainageMode.load(std::memory_order_relaxed)
			&& s_DeallocatedButNotYetDecommitted > NR_FREE_STACK_ALLOCS)
			DrainAllFreeStacks_lockHeld();

		return result;
	}

	// Budgeted drain for this class (§8.1.27); assumes the shared allocSection is held by the
	// caller. Decommits cold stores front-first while the sweep budget lasts, sparing the
	// KEEP_HOT_STORES back-most stores as the reuse band. Returns the DRAINABLE remainder --
	// committed stores beyond the kept-hot band -- which the sweep sums into the recalibrated
	// gate counter; kept-hot stores are excluded so that a stack reduced to its reuse band
	// stops asking for sweeps.
	SizeT drain_cold_stores_lockHeld(SizeT& sweepBudget)
	{
		if (inner.objectStoreSize >= VirtualAllocChunk::DECOMMIT_MIN_SIZE)
			return 0; // release() decommits these at free: never gate-relevant
		// Standing (RAM-pressure) drainage keeps HALF of each stack committed, floored at
		// KEEP_HOT_STORES: the stack is LIFO, so the retained half is exactly the band reuse
		// comes from, and draining to the floor for a whole run is what turned §8.1.30 into
		// decommit-on-free (1.7 TB re-committed, +20 % wall). A claim window still drains to
		// the floor: it is short, and a task is waiting for the memory.
		SizeT keepHot = KEEP_HOT_STORES;
		if (s_DrainageIsStanding.load(std::memory_order_relaxed))
			MakeMax(keepHot, SizeT(freeStack.size() / 2));
		while (sweepBudget && nr_deallocated + keepHot < freeStack.size())
		{
			VirtualAllocChunk::decommit_free_store(freeStack[nr_deallocated], inner.objectStoreSize);
			++nr_deallocated;
			--sweepBudget;
			s_DrainedStackBytes.fetch_add(inner.objectStoreSize, std::memory_order_relaxed);
			s_FreeStackDeadBytes.fetch_sub(inner.objectStoreSize, std::memory_order_relaxed); // handed back to the OS: no longer committed
		}
		SizeT stillCommitted = freeStack.size() - nr_deallocated;
		return stillCommitted > keepHot ? stillCommitted - keepHot : 0;
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
		std::lock_guard guard(allocSection); // critical section here too

		objectCount--;
		s_FreeStackLiveBytes.fetch_sub(inner.objectStoreSize, std::memory_order_relaxed);
		freeStack.emplace_back(ptr);
		if (inner.objectStoreSize < VirtualAllocChunk::DECOMMIT_MIN_SIZE)
		{
			++s_DeallocatedButNotYetDecommitted; // arrives committed: gate-relevant (>= 2 MB stores arrive decommitted by release())
			s_FreeStackDeadBytes.fetch_add(inner.objectStoreSize, std::memory_order_relaxed);
		}
	}
	void deallocate(BYTE_PTR ptr, object_size_t objectSize)
	{
		assert(objectSize);
		assert(objectSize <= inner.objectStoreSize);
		assert(objectSize > inner.objectStoreSize / 2);

		inner.release(ptr, objectSize); // still thread-local ownership during release of object's memory
		add_to_freestack(ptr); // will sync with other threads
	}

	FreeStackAllocSummary ReportStatus() const
	{
		if (!inner.objectStoreSize)
			return FreeStackAllocSummary(0, 0, 0, 0, 0);

		SizeT pageCount, totalBytes = 0;
		SizeT nrAllocated;
		SizeT nrFreed;
		SizeT nrUncommitted;
		SizeT nrAllocatedBytes;
		{
			std::scoped_lock lock(allocSection);

			pageCount = inner.chunks.size();
			totalBytes = 0; for (const auto& page : inner.chunks) totalBytes += page.ChunkSize();
			nrAllocated = objectCount;
			// both populations hold no commit charge: the never-committed reserve tail plus the
			// drained cold prefix (§8.1.24) -- which keeps 'freed' meaning committed-on-stack,
			// i.e. the dead pool the §8.1.23 decomposition measures
			nrUncommitted = inner.nrResevedButUncommitedObjectStores + nr_deallocated;
			nrFreed = (totalBytes >> inner.log2ObjectStoreSize) - nrAllocated - nrUncommitted;
			nrAllocatedBytes = inner.objectStoreSize * nrAllocated;
		}

		#if defined(MG_DEBUG_ALLOCATOR)
		reportF(MsgCategory::memory, SeverityTypeID::ST_MinorTrace, "Block size: {}; pagecount: {}; alloc: {}; freed: {}; uncommitted: {}; total bytes: {}[MB] allocbytes = {}[MB]",
			inner.objectStoreSize, pageCount, nrAllocated, nrFreed, nrUncommitted, totalBytes >> 20, nrAllocatedBytes >> 20);
		#endif

		return FreeStackAllocSummary(totalBytes, nrAllocatedBytes, nrFreed << inner.log2ObjectStoreSize, nrUncommitted << inner.log2ObjectStoreSize, 0);
	}
};

// =========================================  FreeStackAllocator instance section

#if defined(MG_DEBUG)
struct FreeStackAllocatorArray* sd_FSA_ptr = nullptr;
#endif

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

	SizeT size() const { return NR_FREE_STACK_ALLOCS; }

	FreeStackAllocator& operator [](alloc_index_t i)
	{ 
		assert(i < NR_FREE_STACK_ALLOCS);
		return freeStackAllocators[i]; 
	}
	const FreeStackAllocator& operator [](alloc_index_t i) const
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

// The drain sweep (§8.1.25, budgeted per §8.1.27): walk the classes spending one shared budget
// of decommits, front-of-stack first within each. Callable only with
// FreeStackAllocator::allocSection held -- which, being shared by all instances, makes the
// cross-class walk as safe as a self-drain. The walk doubles as the gate counter's "reset"
// (user design 2026-08-04): a RECOUNT of the DRAINABLE remainder -- so a budget-exhausted sweep
// leaves the gate open and catch-up continues on the very next allocation, while a sweep that
// found nothing drainable (all pending inside kept-hot bands) resets the gate to zero, and the
// next walk then requires NR_FREE_STACK_ALLOCS+1 FRESH deallocations: the minimum-new-
// deallocations rule that keeps dry pools from being walked.
static void DrainAllFreeStacks_lockHeld()
{
#if defined(WIN32)
	LARGE_INTEGER t0; QueryPerformanceCounter(&t0);
#endif
	auto& fsaa = GetFreeStackAllocatorArray();
	SizeT sweepBudget = DRAIN_SWEEP_BUDGET;
	SizeT drainableRemainder = 0;

	// LARGEST DRAINABLE CLASS FIRST (user, 2026-08-05). A decommit costs one syscall whatever
	// the store size, so bytes returned per budget slot -- and per microsecond of held
	// allocSection -- is maximised by starting at 1 MB and walking down: a budget spent on 1 MB
	// stores returns 256x what the same budget spent on 4 KB stores would. Ascending order also
	// let the small classes, which hold almost nothing (the t641_2 profile is 256 K/512 K tile
	// buffers), consume a budget meant for the mass. The trade is deliberate: this is
	// bytes-first, not fair -- small classes are only reached once the big ones are down to
	// their keep-hot bands, which is exactly when their few megabytes start to matter.
	alloc_index_t i = NR_DRAINABLE_FSA;
	while (i)
	{
		drainableRemainder += fsaa[--i].drain_cold_stores_lockHeld(sweepBudget);
		if (!sweepBudget)
			break; // early exit: no point walking classes this sweep can no longer serve
	}

	// Recount only after a COMPLETE walk. A budget-exhausted sweep leaves the gate counter
	// untouched -- it was above threshold to get here, so it stays open and the next allocation
	// resumes the catch-up -- and only a full walk can know the true drainable remainder, which
	// is what arms the min-new-deallocations rule (§8.1.27) when it comes out zero.
	if (sweepBudget)
		s_DeallocatedButNotYetDecommitted = drainableRemainder;

	s_DrainSweepCount.fetch_add(1, std::memory_order_relaxed);
#if defined(WIN32)
	LARGE_INTEGER t1; QueryPerformanceCounter(&t1);
	auto ticks = UInt64(t1.QuadPart - t0.QuadPart);
	auto prevMax = s_DrainSweepMaxTicks.load(std::memory_order_relaxed);
	while (ticks > prevMax
		&& !s_DrainSweepMaxTicks.compare_exchange_weak(prevMax, ticks, std::memory_order_relaxed))
		;
#endif
}

// =========================================  FreeListAllocator definition section
#if defined(MG_CACHE_ALLOC_SMALL)

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

		// first try the free list - lock-free
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

		// then try reserved small objects - lock-free
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
			std::lock_guard guard(allocSection);
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

#endif //defined(MG_CACHE_ALLOC_SMALL)

//----------------------------------------------------------------------
// fixed_allocator arrays
//----------------------------------------------------------------------

constexpr alloc_index_t BlockListIndex(SizeT sz)
{
	SizeT org_sz = sz;
	assert(sz);
//	assert(sz <= ALLOC_OBJSSIZE_MAX);

	--sz;
	sz >>= ALLOC_OBJSSIZE_MIN_BITS;
	return highest_bit_rank(sz);
}


static UInt32 g_ElemAllocCounter = 0;
ElemAllocComponent s_Initialize;

#if defined(MG_CACHE_ALLOC_ONLY_SPECIALSIZE)

bool IsIntegralPowerOf2OrZero(SizeT sz)
{
	return !((sz - 1) & sz);
}

bool SpecialSize(SizeT sz)
{
//	return sz >= (1 << log2_default_segment_size) / 8 && IsIntegralPowerOf2OrZero(sz) && sz <= (1 << log2_default_segment_size) * sizeof(Float64) * 2;
	return sz >= (1 << log2_default_segment_size) / 8 && sz <= (1 << log2_default_segment_size) * sizeof(Float64) * 2;
}

#else

bool SpecialSize(SizeT sz) { return true;  }

#endif //defined(MG_CACHE_ALLOC_ONLY_SPECIALSIZE)

#endif defined(MG_CACHE_ALLOC)

//----------------------------------------------------------------------
// implement interface
//----------------------------------------------------------------------

std::allocator<UInt64> s_QWordArrayAllocator;

void* AllocateFromStock_impl(size_t objectSize)
{

#if defined(MG_CACHE_ALLOC)
	assert(objectSize);
	auto i = BlockListIndex(objectSize);
	if (i < FIRST_PAGE_INDEX)
	{
#if defined(MG_CACHE_ALLOC_SMALL)
		auto& freeStack = GetFreeListAllocator(i);
		return freeStack.allocate(objectSize);
#endif //defined(MG_CACHE_ALLOC_SMALL)
	}
	else
	{
		if (s_BlockNewAllocations)
			throw MemoryAllocFailure();

		if (SpecialSize(objectSize))
		{
			i -= FIRST_PAGE_INDEX;
			if (i < NR_FREE_STACK_ALLOCS)
			{
				auto& freeStack = GetFreeStackAllocator(i);
				return freeStack.allocate(objectSize);
			}
			assert(objectSize > ALLOC_OBJSSIZE_MAX);
		}
	}
#endif //defined(MG_CACHE_ALLOC)


	SizeT qWordCount = ((objectSize + (sizeof(UInt64) - 1)) & ~(sizeof(UInt64) - 1)) / sizeof(UInt64);

#if defined(MG_CACHE_ALLOC)
	if (i >= FIRST_PAGE_INDEX)
		WaitForAvailableMemory(qWordCount * sizeof(UInt64));
#endif //defined(MG_CACHE_ALLOC)

	auto result = s_QWordArrayAllocator.allocate(qWordCount);
	MG_CHECK(result);
	return result;
}

//----------------------------------------------------------------------
// Live large-allocation census, for the admission ledger (schedule-with-lookahead.md §8.1.8)
//----------------------------------------------------------------------
// Ground truth for "how much is actually resident", measured instead of modelled. Every attempt to
// attribute memory to a RESULT has failed against t641, because the bytes are owned by tile buffers
// whose lifetime is set by consumers: a lazy tile is owned solely by the consumer's lock, and a
// deferred tile_record is co-owned by whoever holds the future tile. Keying a booking on the item
// (released at ClearDataObject) or on the data object (released at ~AbstrDataObject) both unbook
// memory that is still there. This counter has no such problem: it moves when the bytes move.
//
// Only allocations >= LARGE_ALLOC_THRESHOLD are counted, for two reasons. The memory that matters
// is tile and array buffers, which are far above it; and a relaxed fetch_add on every small-object
// allocation would ping-pong one cacheline across ~30 worker threads for no diagnostic gain.
constexpr size_t LARGE_ALLOC_THRESHOLD = 4096;


// Size histogram of large allocations, bucketed by log2. Answers "what sizes is t641 actually
// asking for", and in particular how much lives above ALLOC_OBJSSIZE_MAX (2^28 = 256 MB), which is
// the cut-off above which requests bypass the free-stack allocators entirely and go to
// std::allocator -- i.e. the population that would come back under the lock-free allocator's control
// if ALLOC_OBJSSIZE_MAX_BITS and log2_max_chunk_size were raised.
static std::atomic<UInt64> s_AllocSizeHistogram[64] = {};

// Allocations at or above this are individually logged under PerformanceLogging (/SP), WITH the
// item context the reporting framework appends -- that is the attribution: which operation asked
// for it. The report happens after AllocateFromStock_impl has returned, so no allocator lock is
// held.
constexpr size_t HUGE_ALLOC_LOG_THRESHOLD = SizeT(1) << 28; // 256 MB == ALLOC_OBJSSIZE_MAX

static std::atomic<SizeT> s_LiveLargeAllocBytes = 0;
// True high-water mark, not a sample. The ledger sample is rate-limited to 30 s, which is far too
// coarse for a probe that runs in tens of seconds -- and the peak is exactly what a memory
// experiment is measuring. Maintained on the increment path only.
static std::atomic<SizeT> s_PeakLargeAllocBytes = 0;

SizeT GetLiveLargeAllocBytes()
{
	return s_LiveLargeAllocBytes.load(std::memory_order_relaxed);
}

SizeT GetPeakLargeAllocBytes()
{
	return s_PeakLargeAllocBytes.load(std::memory_order_relaxed);
}


void* AllocateFromStock(size_t objectSize MG_DEBUG_ALLOCATOR_SRC_ARG)
{
	if (!objectSize)
		return nullptr;

	auto result = AllocateFromStock_impl(objectSize);

	if (objectSize >= LARGE_ALLOC_THRESHOLD)
	{
		// §8.1.32: RAM use vs MemoryFlushThreshold, ~1/s -- but only once the process is warm
		// enough that reading configuration is safe; see ArmDrainageWhenWarm.
		bool warm = ArmDrainageWhenWarm(objectSize);
		if (warm)
			ConsiderDrainagePressure();
		s_AllocSizeHistogram[std::bit_width(objectSize) - 1].fetch_add(1, std::memory_order_relaxed);

		auto live = s_LiveLargeAllocBytes.fetch_add(objectSize, std::memory_order_relaxed) + objectSize;
		// Proper CAS max. The earlier load/store version was NOT a max but last-writer-wins: two
		// threads read the same old peak, the one computing the LOWER value could store last, and
		// the recorded peak went BACKWARDS. With ~30 workers allocating continuously that clobbers
		// the peak downward systematically -- it is what made PeakLiveLarge read 27.6 GB on t641
		// while the process held ~200 GB.
		auto prevPeak = s_PeakLargeAllocBytes.load(std::memory_order_relaxed);
		while (live > prevPeak
			&& !s_PeakLargeAllocBytes.compare_exchange_weak(prevPeak, live, std::memory_order_relaxed))
			;

		// Attribution for the >= 256 MB population: which operation asked for it. Reported directly
		// (not via PostMainThreadOper) precisely BECAUSE the reporting framework appends the calling
		// thread's current item context -- deferring it to the main thread would lose exactly the
		// information wanted. Safe here: AllocateFromStock_impl has returned, so no allocator lock is
		// held, and reportF's own allocations are far below this threshold so they cannot recurse
		// into this branch. Only under PerformanceLogging (/SP), and only once the process is warm:
		// IsPerformanceLogging's first call reads configuration, which is unsafe during static init.
		if (objectSize >= HUGE_ALLOC_LOG_THRESHOLD && warm && IsPerformanceLogging())
			reportF(MsgCategory::memory, SeverityTypeID::ST_MinorTrace
				, "huge alloc {}[MB]; live now {}[MB]", objectSize >> 20, live >> 20);

	}

#if defined(MG_CACHE_ALLOC)

#if defined(MG_DEBUG_ALLOCATOR)
	RegisterAlloc(result, objectSize MG_DEBUG_ALLOCATOR_SRC_PARAM);
#endif //defined(MG_DEBUG_ALLOCATOR)

#if defined(MG_CACHE_COLLECTDATA)
	// Beside RegisterAlloc, and for the same reason: this is the one point every allocation passes.
	if (objectSize >= LARGE_ALLOC_THRESHOLD)
		NoteAllocation(result, objectSize);
#endif //defined(MG_CACHE_COLLECTDATA)

	ConsiderReporting();

#endif //defined(MG_CACHE_ALLOC)

	return result;
}

void LeaveToStock(void* objectPtr, size_t objectSize) {
	if (!objectSize)
	{
		dms_assert(objectPtr == nullptr);
		return;
	}

	if (objectSize >= LARGE_ALLOC_THRESHOLD)
	{
		// Do not lie. The earlier version clamped at zero on underflow, which would have silently
		// absorbed exactly the alloc/free size mismatch this counter exists to detect. Subtract
		// plainly and assert instead: if the census ever goes negative we want to SEE it.
		auto prev = s_LiveLargeAllocBytes.fetch_sub(objectSize, std::memory_order_relaxed);
		assert(prev >= objectSize); // underflow wraps to a huge value rather than being hidden
	}
#if defined(MG_CACHE_ALLOC)

#if defined(MG_DEBUG_ALLOCATOR)
	RemoveAlloc(objectPtr, objectSize);
#endif //defined(MG_DEBUG_ALLOCATOR)

#if defined(MG_CACHE_COLLECTDATA)
	// Discharges the operation that ALLOCATED this block, which is why the owner has to be recorded
	// per pointer: the freeing thread is routinely working for a different operation by now.
	if (objectSize >= LARGE_ALLOC_THRESHOLD)
		NoteDeallocation(objectPtr, objectSize);
#endif //defined(MG_CACHE_COLLECTDATA)

	auto i = BlockListIndex(objectSize);
	if (i < FIRST_PAGE_INDEX)
	{
#if defined(MG_CACHE_ALLOC_SMALL)
		auto& alloc = GetFreeListAllocator(i);
		return alloc.deallocate(reinterpret_cast<BYTE_PTR>(objectPtr), objectSize);
#endif //defined(MG_CACHE_ALLOC_SMALL)
	}
	else
	{
		if (SpecialSize(objectSize))
		{
			i -= FIRST_PAGE_INDEX;
			if (i < NR_FREE_STACK_ALLOCS)
			{
				auto& alloc = GetFreeStackAllocator(i);
				return alloc.deallocate(reinterpret_cast<BYTE_PTR>(objectPtr), objectSize);
			}
		}
	}
#endif //defined(MG_CACHE_ALLOC)

	SizeT qWordCount = ((objectSize + (sizeof(UInt64) - 1)) & ~(sizeof(UInt64) - 1)) / sizeof(UInt64);
	s_QWordArrayAllocator.deallocate(reinterpret_cast<UInt64*>(objectPtr), qWordCount);
}

//----------------------------------------------------------------------
// Reporting
//----------------------------------------------------------------------

#if defined(MG_CACHE_ALLOC)

#include "utl/StrFormat.h"

#if defined(WIN32)
#include <Psapi.h>
#else
#include <unistd.h>
#include <fstream>
#include <string>
#endif

std::atomic<bool> s_ReportingRequestPending = false;
std::atomic<bool> s_BlockNewAllocations = false;

SizeT CommittedSize()
{
#if defined(WIN32)
	PROCESS_MEMORY_COUNTERS processInfo;
	GetProcessMemoryInfo(GetCurrentProcess(), &processInfo, sizeof(PROCESS_MEMORY_COUNTERS));
	return processInfo.PagefileUsage;
#else
	// Read VmRSS from /proc/self/status as a proxy for committed memory
	std::ifstream status("/proc/self/status");
	std::string line;
	while (std::getline(status, line))
	{
		if (line.compare(0, 6, "VmRSS:") == 0)
		{
			SizeT kb = 0;
			std::sscanf(line.c_str(), "VmRSS: %zu", &kb);
			return kb * 1024;
		}
	}
	return 0;
#endif
}

static FreeStackAllocSummary maxCumulBytes = FreeStackAllocSummary(0, 0, 0, 0, 0);

auto UpdateFixedAllocStatus() -> FreeStackAllocSummary
{
	s_ReportingRequestPending = false;

	FreeStackAllocSummary cumulBytes;
	const auto& fsaa = GetFreeStackAllocatorArray();
	for (SizeT i = 0; i != fsaa.size(); ++i)
		cumulBytes = cumulBytes + fsaa[i].ReportStatus();

	std::get<4>(cumulBytes) = CommittedSize();

	MakeMax(std::get<0>(maxCumulBytes), std::get<0>(cumulBytes));
	MakeMax(std::get<1>(maxCumulBytes), std::get<1>(cumulBytes));
	MakeMax(std::get<2>(maxCumulBytes), std::get<2>(cumulBytes));
	MakeMax(std::get<3>(maxCumulBytes), std::get<3>(cumulBytes));
	MakeMax(std::get<4>(maxCumulBytes), std::get<4>(cumulBytes));
	return cumulBytes;
}

// Periodic census line for diagnosing the PeakLiveLarge gap (user request 2026-08-01). Emitted
// beside the per-sample allocator status so the two populations can be followed over time instead of
// compared as two end-of-run peaks -- which is what left three explanations standing.
//   inUse  = sum(objectStoreSize * objectCount) over the free stacks, i.e. what the allocator says
//            is handed out RIGHT NOW (size-class rounded)
//   req    = bytes requested through AllocateFromStock and not yet returned
// Healthy relation: req <= inUse < 2*req. Anywhere those diverge is where the gap opens, and the
// running peaks beside them show whether it is a sustained level or a spike being missed.
auto GetLargeAllocCensus() -> SharedStr
{
	auto req = GetLiveLargeAllocBytes();
	auto inUse = GetFreeStackLiveBytes();
	return mySSPrintF("census: live req {}[MB] vs fs inUse {}[MB] (ratio {}); peaks req {}[MB] fs {}[MB]"
		, req >> 20, inUse >> 20
		, req ? Float64(inUse) / Float64(req) : 0.0
		, GetPeakLargeAllocBytes() >> 20, GetPeakFreeStackBytes() >> 20
	);
}

auto GetFixedAllocStatus(const FreeStackAllocSummary& cumulBytes) -> SharedStr
{
	return mySSPrintF("Reserved in Blocks {}[MB]; allocated: {}[MB]; freed: {}[MB]; uncommitted: {}[MB]; CommitCharge: {}[MB]"
		, std::get<0>(cumulBytes) >> 20
		, std::get<1>(cumulBytes) >> 20
		, std::get<2>(cumulBytes) >> 20
		, std::get<3>(cumulBytes) >> 20
		, std::get<4>(cumulBytes) >> 20
	);
}

// Decommit cost, and whether it serialises the workers. peak-concurrent is the number that matters:
// VirtualAlloc/VirtualFree take the process address-space lock, so if threads pile up inside them
// the lock-free allocator's whole purpose is defeated. 1-2 = no congestion; near the worker count =
// the pool has been re-serialised and the change should be reverted.
auto GetVmSysCallStats() -> SharedStr
{
#if defined(WIN32)
	LARGE_INTEGER f; QueryPerformanceFrequency(&f);
	auto perMs = Float64(f.QuadPart) / 1000.0;
#else
	auto perMs = 1.0;
#endif
	auto nrDrained = s_DrainCount.load(std::memory_order_relaxed);
	auto nrSweeps = s_DrainSweepCount.load(std::memory_order_relaxed);
	return mySSPrintF("vmcalls: decommit {}x/{}ms, recommit {}x/{}ms, drained {}x = {}[MB]/{}ms ({}[KB]/call over {} sweeps, worst sweep {}ms); peak concurrent {}"
		, s_DecommitCount.load(std::memory_order_relaxed)
		, Float64(s_DecommitTicks.load(std::memory_order_relaxed)) / perMs
		, s_RecommitCount.load(std::memory_order_relaxed)
		, Float64(s_RecommitTicks.load(std::memory_order_relaxed)) / perMs
		, nrDrained
		, s_DrainedStackBytes.load(std::memory_order_relaxed) >> 20
		, Float64(s_DrainTicks.load(std::memory_order_relaxed)) / perMs
		// bytes per syscall: the whole point of draining the largest classes first (§8.1.28)
		, nrDrained ? (s_DrainedStackBytes.load(std::memory_order_relaxed) / nrDrained) >> 10 : 0
		, nrSweeps
		// worst single sweep = worst-case allocSection hold from draining; decides whether
		// draining outside the lock would buy anything
		, Float64(s_DrainSweepMaxTicks.load(std::memory_order_relaxed)) / perMs
		, s_VmSysCallsInFlightPeak.load(std::memory_order_relaxed)
	);
}

// Which sizes are actually requested, and how much sits above ALLOC_OBJSSIZE_MAX (2^28 = 256 MB) --
// the population that bypasses the free stacks for std::allocator, and would return under the
// lock-free allocator's control if ALLOC_OBJSSIZE_MAX_BITS / log2_max_chunk_size were raised.
auto GetAllocHistogram() -> SharedStr
{
	SharedStr result;
	for (int i = 12; i < 64; ++i) // from 4 KB up
	{
		auto n = s_AllocSizeHistogram[i].load(std::memory_order_relaxed);
		if (!n)
			continue;
		SharedStr bucket = (i >= 30) ? mySSPrintF("{}G", SizeT(1) << (i - 30))
			: (i >= 20) ? mySSPrintF("{}M", SizeT(1) << (i - 20))
			: mySSPrintF("{}K", SizeT(1) << (i - 10));
		result = result + mySSPrintF(" {}:{}", bucket, n);
	}
	return mySSPrintF("alloc histogram (log2 buckets, >=4K):{}", result);
}

// The §8.1.23 pressure gauge as a number: what the ledger's commit-vs-budget trigger (§8.1.30)
// compares against the budget. Windows: commit charge; elsewhere the resident set is the
// nearest standing-pressure proxy.
SizeT GetProcessCommitBytes()
{
#if defined(WIN32)
	PROCESS_MEMORY_COUNTERS processInfo;
	GetProcessMemoryInfo(GetCurrentProcess(), &processInfo, sizeof(PROCESS_MEMORY_COUNTERS));
	return processInfo.PagefileUsage;
#else
	SizeT vmRSS = 0;
	std::ifstream status("/proc/self/status");
	std::string line;
	while (std::getline(status, line))
		if (line.compare(0, 6, "VmRSS:") == 0)
			std::sscanf(line.c_str(), "VmRSS: %zu", &vmRSS);
	return vmRSS * 1024;
#endif
}

RTC_CALL auto GetMemoryStatus() -> SharedStr
{
#if defined(WIN32)
	PROCESS_MEMORY_COUNTERS processInfo;
	GetProcessMemoryInfo(GetCurrentProcess(), &processInfo, sizeof(PROCESS_MEMORY_COUNTERS));
	return mySSPrintF("{}[MB] committed of peak {}[MB]"
		, processInfo.PagefileUsage >> 20
		, processInfo.PeakPagefileUsage >> 20
	);
#else
	SizeT vmRSS = 0, vmPeak = 0;
	std::ifstream status("/proc/self/status");
	std::string line;
	while (std::getline(status, line))
	{
		if (line.compare(0, 6, "VmRSS:") == 0)
			std::sscanf(line.c_str(), "VmRSS: %zu", &vmRSS);
		else if (line.compare(0, 7, "VmPeak:") == 0)
			std::sscanf(line.c_str(), "VmPeak: %zu", &vmPeak);
	}
	return mySSPrintF("{}[MB] committed of peak {}[MB]", vmRSS / 1024, vmPeak / 1024);
#endif
}

static UInt8 reportThrottler = 0;
void ReportFixedAllocStatus()
{
	auto cumulBytes = UpdateFixedAllocStatus();
	if (++reportThrottler > 17) // only report to log every 17th time
	{
		reportThrottler = 0;
		// The census line only under PerformanceLogging (/SP): it answered the PeakLiveLarge-gap
		// question (§8.1.23) and remains a calibration instrument, not product output. Sampled
		// here, at the same instant as the status line, so when both are on the allocator's view
		// and the request-side view stay directly comparable.
		auto censusStr = IsPerformanceLogging() ? GetLargeAllocCensus() : SharedStr();
		PostMainThreadOper([reportStr = GetFixedAllocStatus(cumulBytes), censusStr]
			{
				reportD(MsgCategory::memory, SeverityTypeID::ST_MajorTrace, reportStr.c_str());
				if (!censusStr.empty())
					reportD(MsgCategory::memory, SeverityTypeID::ST_MajorTrace, censusStr.c_str());
			}
		);
	}
}

auto UpdateAndGetFixedAllocFinalSummary() -> SharedStr
{
	auto cumulBytes = maxCumulBytes;

	// PeakLiveLarge is the one figure here that is LIVE data rather than a cumulative or reserved
	// total: the high-water mark of bytes simultaneously held in >= 4 KB allocations. On t641 it read
	// 27.6 GB against a 189 GB CommitCharge and 190 GB reserved -- i.e. it separates data an operation
	// actually holds from pool the allocator has taken and not returned. See §8.1.9.
	return mySSPrintF( "Highest Reserved in Blocks {}[MB]; Highest allocated: {}[MB]; Highest freed: {}[MB]; Highest uncommitted: {}[MB]; Highest CommitCharge: {}[MB]; PeakLiveLarge: {}[MB]; PeakFreeStack: {}[MB]; residual live: req {}[MB] vs fs {}[MB]"
		, std::get<0>(cumulBytes) >> 20
		, std::get<1>(cumulBytes) >> 20
		, std::get<2>(cumulBytes) >> 20
		, std::get<3>(cumulBytes) >> 20
		, std::get<4>(cumulBytes) >> 20
		, GetPeakLargeAllocBytes() >> 20
		, GetPeakFreeStackBytes() >> 20
		// Residual at shutdown: both should be near zero if every allocation was freed. A large
		// residual on either side localises a leak; a DIFFERENCE between them localises a counting
		// defect (alloc and free charged different sizes).
		, GetLiveLargeAllocBytes() >> 20
		, GetFreeStackLiveBytes() >> 20
	);
}

void ReportFixedAllocFinalSummary()
{
	// Decommit cost, once per run beside the memory summary. The size histogram only under
	// PerformanceLogging (/SP): its question -- what sizes a workload asks for, and how much lives
	// above ALLOC_OBJSSIZE_MAX -- is calibration, not product output. The underlying counters are
	// always maintained, so a run with /SP reports the complete profile.
	reportD(MsgCategory::memory, SeverityTypeID::ST_MajorTrace, GetVmSysCallStats().c_str());
	if (IsPerformanceLogging())
		reportD(MsgCategory::memory, SeverityTypeID::ST_MajorTrace, GetAllocHistogram().c_str());

	auto msgStr = UpdateAndGetFixedAllocFinalSummary();

	reportD(MsgCategory::memory, SeverityTypeID::ST_MajorTrace, msgStr.c_str());

	// Do NOT pump the main-thread operation queue here. This function runs from a
	// static destructor in libDmRtc, which on Linux fires AFTER the executable's
	// text segment has been unmapped by _dl_call_fini. Any leftover std::function
	// queued by PostAppOper (e.g. the dmsscript test driver) would then call into
	// freed code — observed as SIGSEGV during GeoDmsGuiQt teardown when running
	// with /T<dmsscript>. The queue is the responsibility of the active main loop
	// (Qt event loop / Win32 message pump) which drains it during normal shutdown.
}

#endif //defined(MG_CACHE_ALLOC)


#if defined(MG_DEBUG_ALLOCATOR)

#include <map>
#include "dbg/DebugReporter.h"


struct alloc_register_t
{
	using aspects_t = std::pair<std::string, size_t>; // source string and size
	std::map<void*, aspects_t> map;
	std::mutex mutex;
};

static alloc_register_t* s_AllocRegister = nullptr;

auto& GetAllocRegister()
{
	assert(s_AllocRegister);
	return *s_AllocRegister;
}


#endif

#if defined(MG_CACHE_COLLECTDATA)

// Which operation allocated each live block, so the matching free can discharge that SAME operation
// -- the freeing thread is routinely working for a different one by then, which is why a counter
// alone can only ever produce a gross total. Weak, so a context that ends while its memory is still
// out stops being charged rather than being kept alive by the allocator.
struct alloc_owner_register_t
{
	std::map<void*, std::weak_ptr<void>> map; // block -> owning OperationContext (type-erased)
	std::mutex mutex;
};

static alloc_owner_register_t* s_AllocOwnerRegister = nullptr;

auto& GetAllocOwnerRegister()
{
	assert(s_AllocOwnerRegister); // guaranteed by ElemAllocComponent; see RtcComponents.h
	return *s_AllocOwnerRegister;
}

void NoteAllocation(void* ptr, SizeT bytes)
{
	auto ocRef = CurrentOperationRef();
	if (!ocRef)
		return; // allocation outside any operation (meta thread, startup): nothing to charge

	auto& reg = GetAllocOwnerRegister();
	{
		std::lock_guard guard(reg.mutex);
		reg.map[ptr] = ocRef;
	}
	ChargeOperationAlloc(ocRef.get(), bytes);
}

void NoteDeallocation(void* ptr, SizeT bytes)
{
	auto& reg = GetAllocOwnerRegister();
	std::weak_ptr<void> owner;
	{
		std::lock_guard guard(reg.mutex);
		auto pos = reg.map.find(ptr);
		if (pos == reg.map.end())
			return; // never charged (allocated outside an operation)
		owner = std::move(pos->second);
		reg.map.erase(pos);
	}
	if (auto ocRef = owner.lock())
		ChargeOperationFree(ocRef.get(), bytes);
}

#endif //defined(MG_CACHE_COLLECTDATA)

//----------------------------------------------------------------------
// clean-up
//----------------------------------------------------------------------


ElemAllocComponent::ElemAllocComponent()
{
#if defined(MG_CACHE_ALLOC)
	if (g_ElemAllocCounter++)
		return;

	SetMainThreadID();


#if defined(MG_DEBUG_ALLOCATOR)
	assert(!s_AllocRegister);
	s_AllocRegister = new alloc_register_t;
#endif

#if defined(MG_CACHE_COLLECTDATA)
	assert(!s_AllocOwnerRegister);
	s_AllocOwnerRegister = new alloc_owner_register_t;
#endif

	GetFreeStackAllocatorArray();

#if defined(MG_CACHE_ALLOC_SMALL)
	GetFreeListAllocatorArray();
#endif //defined(MG_CACHE_ALLOC_SMALL)

	// Known leak: iconv-2.dll->relocatable.c->DllMain contains shared_library_fullname = strdup(location), which leaks 44 bytes.
#endif //defined(MG_CACHE_ALLOC)

}

ElemAllocComponent::~ElemAllocComponent()
{
#if defined(MG_CACHE_ALLOC)
	if (--g_ElemAllocCounter)
		return;
#endif //defined(MG_CACHE_ALLOC)

#if defined(MG_DEBUG_ALLOCATOR)
	assert(s_AllocRegister);
	delete s_AllocRegister;
	s_AllocRegister = nullptr;
#endif

#if defined(MG_CACHE_COLLECTDATA)
	assert(s_AllocOwnerRegister);
	delete s_AllocOwnerRegister;
	s_AllocOwnerRegister = nullptr;
#endif
}

#if defined(MG_CACHE_ALLOC)

#include "dbg/Timer.h"

void PostReporting()
{
	s_ReportingRequestPending = true;
	PostMainThreadOper([] 
		{
			if (s_ReportingRequestPending)
				ReportFixedAllocStatus();
		}
	);
}

static std::atomic<UInt32> s_ConsiderReportingReentranceCounter = 0;

void ConsiderReporting()
{
	static Timer t;
	
	if (s_ConsiderReportingReentranceCounter)
		return;

	StaticMtIncrementalLock<s_ConsiderReportingReentranceCounter> preventReentrance;
	if (t.PassedSecs())
		PostReporting();
}


#if defined(MG_DEBUG_ALLOCATOR)

void RegisterAlloc(void* ptr, size_t sz MG_DEBUG_ALLOCATOR_SRC_ARG)
{
	auto& reg = GetAllocRegister();
	std::lock_guard guard(reg.mutex);

	assert(reg.map.find(ptr) == reg.map.end()); // check that its not already assigned
	reg.map[ptr] = std::pair<CharPtr, size_t>{ srcStr, sz };
}

void RemoveAlloc(void* ptr, size_t sz)
{
	auto& reg = GetAllocRegister();
	std::lock_guard guard(reg.mutex);

	auto pos = reg.map.find(ptr);
	assert(pos != reg.map.end() && pos->first == ptr && pos->second.second == sz); // check that it was assigned as now assumed
	reg.map.erase(pos);
}

auto GetAllocRegisterCopy()
{
	auto& reg = GetAllocRegister();
	std::lock_guard guard(reg.mutex);
	return reg.map; // return a copy of the map, so that it can be used outside the lock
}

void ReportAllocs()
{
	auto map = GetAllocRegisterCopy();
	objectstore_count_t i = 0;

	std::map<alloc_register_t::aspects_t, SizeT> fequencyCounts;
	reportD(SeverityTypeID::ST_MinorTrace, "All Registered Memory Blocks");
	for (auto& registeredAlloc : map)
		fequencyCounts[registeredAlloc.second]++;

	SizeT cumulSize = 0, otherCount = 0, otherSize = 0;
	reportD(SeverityTypeID::ST_MinorTrace, "Frequency counts per size:");
	i = 0;
	reportF(SeverityTypeID::ST_MajorTrace, "volgnummer;size;count;totalsize;txt");
	for (auto& freq : fequencyCounts)
	{
		auto txt = freq.first.first;
		SizeT sz = freq.first.second;
		SizeT cnt = freq.second;
		SizeT totalSz = sz * cnt;
		if (totalSz > 1000000)
			reportF(SeverityTypeID::ST_MajorTrace, "#{:05};{};{};{};'{}'", i++, sz, cnt, totalSz, txt);
		else
		{
			otherCount++;
			otherSize += totalSz;				
		}
		cumulSize += totalSz;
	}
	reportF(SeverityTypeID::ST_MajorTrace, "Other count = {}", otherCount);
	reportF(SeverityTypeID::ST_MajorTrace, "Other size = {}", otherSize);
	reportF(SeverityTypeID::ST_MajorTrace, "Total Size = {}", cumulSize);

	ReportFixedAllocStatus();
//	PostReporting(); // Warning: allocSection can be locked, so don't call ReportFixedAllocStatus() here.
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

#endif // defined(MG_CACHE_ALLOC)
