// Copyright (C) 1998-2025 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////
// Potential.cpp
// Implements various convolution and potential analysis functions for grid data.
// Supports FFTW3-based FFT convolution and classic implementations.
// Handles memory alignment, buffer management, and kernel preparation for efficient computation.
// Main entry points: Potential(), AddConvolutionKernel(), and MDL_Potential32/64.
// See Potential.h for API and type definitions.

#include "GeoPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif

#include "Potential.h"

#include "dbg/debug.h"
#include "geo/Conversions.h"
#include "geo/PointOrder.h"
#include "xct/DmsException.h"

#include "AbstrUnit.h"

#include "aggrFuncNum.h"
#include "attrUniStructNum.h"

#include <fftw3.h>
#include <cstdlib> // std::atexit (fftw_cleanup at process exit)
#include <limits>
#include <map>
#include <mutex>
#include <unordered_map>
#include <shared_mutex>
#include <vector>

// FFTW requires serialized access to plan creation/destruction functions.
// The fftw_execute() calls with different plans can run concurrently,
// but fftw_plan_* and fftw_destroy_plan must be protected.
static std::mutex g_fftwPlanMutex;

// *****************************************************************************
//	FFTW Plan and Kernel FFT Caching
// *****************************************************************************

// FFTW ships one complete API per precision, distinguished only by prefix: fftw_ for double
// and fftwf_ for float (fftw3.h declares both, see its FFTW_DEFINE_API / FFTW_MANGLE_* lines).
// fftw_api<R> selects one of them, so that everything below -- plans, plan cache, pre-computed
// kernel FFT, per-thread work buffers -- exists once per precision. The Fft64 backends
// instantiate R = Float64, the Fft32 backends R = Float32; the latter therefore transform in
// single precision from end to end instead of widening their float32 arguments to double.
template <typename R> struct fftw_api;

template <> struct fftw_api<Float64>
{
	using real_type = double;
	using complex_type = fftw_complex;
	using plan_type = fftw_plan;

	static real_type*    alloc_real   (TileSize n)                                { return fftw_alloc_real(n); }
	static complex_type* alloc_complex(TileSize n)                                { return fftw_alloc_complex(n); }
	static void          release      (void* p)                                   { fftw_free(p); }
	static plan_type     plan_r2c     (int n, real_type* in, complex_type* out)   { return fftw_plan_dft_r2c_1d(n, in, out, FFTW_ESTIMATE); }
	static plan_type     plan_c2r     (int n, complex_type* in, real_type* out)   { return fftw_plan_dft_c2r_1d(n, in, out, FFTW_ESTIMATE); }
	static void          destroy_plan (plan_type p)                               { fftw_destroy_plan(p); }
	static void          execute_r2c  (plan_type p, real_type* i, complex_type* o){ fftw_execute_dft_r2c(p, i, o); }
	static void          execute_c2r  (plan_type p, complex_type* i, real_type* o){ fftw_execute_dft_c2r(p, i, o); }
	static void          cleanup      ()                                          { fftw_cleanup(); }
};

template <> struct fftw_api<Float32>
{
	using real_type = float;
	using complex_type = fftwf_complex;
	using plan_type = fftwf_plan;

	static real_type*    alloc_real   (TileSize n)                                { return fftwf_alloc_real(n); }
	static complex_type* alloc_complex(TileSize n)                                { return fftwf_alloc_complex(n); }
	static void          release      (void* p)                                   { fftwf_free(p); }
	static plan_type     plan_r2c     (int n, real_type* in, complex_type* out)   { return fftwf_plan_dft_r2c_1d(n, in, out, FFTW_ESTIMATE); }
	static plan_type     plan_c2r     (int n, complex_type* in, real_type* out)   { return fftwf_plan_dft_c2r_1d(n, in, out, FFTW_ESTIMATE); }
	static void          destroy_plan (plan_type p)                               { fftwf_destroy_plan(p); }
	static void          execute_r2c  (plan_type p, real_type* i, complex_type* o){ fftwf_execute_dft_r2c(p, i, o); }
	static void          execute_c2r  (plan_type p, complex_type* i, real_type* o){ fftwf_execute_dft_c2r(p, i, o); }
	static void          cleanup      ()                                          { fftwf_cleanup(); }
};

// Cached FFTW resources for a specific FFT length.
// Plans are created once and reused via execute_r2c/execute_c2r with new arrays.
template <typename R>
struct FftwPlanSet {
	using api = fftw_api<R>;

	int fftLen = 0;
	typename api::plan_type planFwd = nullptr;  // Forward: real -> complex
	typename api::plan_type planInv = nullptr;  // Inverse: complex -> real

	// Scratch arrays used only for plan creation (FFTW_ESTIMATE doesn't actually use them)
	typename api::real_type* scratchReal = nullptr;
	typename api::complex_type* scratchComplex = nullptr;

	FftwPlanSet() = default;

	// Non-copyable, non-movable (plans are tied to this instance)
	FftwPlanSet(const FftwPlanSet&) = delete;
	FftwPlanSet& operator=(const FftwPlanSet&) = delete;

	~FftwPlanSet() {
		if (planFwd) api::destroy_plan(planFwd);
		if (planInv) api::destroy_plan(planInv);
		if (scratchReal) api::release(scratchReal);
		if (scratchComplex) api::release(scratchComplex);
	}

	bool initialize(int len) {
		fftLen = len;
		scratchReal = api::alloc_real(len);
		scratchComplex = api::alloc_complex(len / 2 + 1);
		if (!scratchReal || !scratchComplex) return false;

		planFwd = api::plan_r2c(len, scratchReal, scratchComplex);
		planInv = api::plan_c2r(len, scratchComplex, scratchReal);
		return planFwd && planInv;
	}
};

// FFTW keeps an internal planner/solver registry that destroy_plan does NOT release; only
// cleanup() frees it (without it, every run that used a Fourier operator reports hundreds of
// fftw-internal blocks in the Debug CRT leak dump), and it exists per precision. cleanup()
// invalidates any still-existing plans, so it must run AFTER the plan caches are destroyed.
// This atexit is registered during static initialization, hence before either plan cache is
// constructed on first use; the exit stack is LIFO, so the caches (registered later) are
// destroyed first and the cleanups run last.
static int s_FftwCleanupRegistrar = (std::atexit([] { fftw_api<Float32>::cleanup(); fftw_api<Float64>::cleanup(); }), 0);

// Get or create a plan set for the given FFT length, from a cache per precision.
// Uses shared_mutex for the read-heavy access pattern. Returns nullptr on failure.
template <typename R>
static FftwPlanSet<R>* GetOrCreatePlanSet(int fftLen) {
	static std::shared_mutex planCacheMutex;
	static std::unordered_map<int, std::unique_ptr<FftwPlanSet<R>>> planCache;

	// Try read-only access first (common case)
	{
		std::shared_lock<std::shared_mutex> readLock(planCacheMutex);
		auto it = planCache.find(fftLen);
		if (it != planCache.end())
			return it->second.get();
	}

	// Need to create - acquire exclusive lock
	std::unique_lock<std::shared_mutex> writeLock(planCacheMutex);

	// Double-check after acquiring write lock
	auto it = planCache.find(fftLen);
	if (it != planCache.end())
		return it->second.get();

	// Create new plan set (plan creation itself needs the FFTW mutex)
	auto planSet = std::make_unique<FftwPlanSet<R>>();
	{
		std::lock_guard<std::mutex> fftwLock(g_fftwPlanMutex);
		if (!planSet->initialize(fftLen))
			return nullptr;
	}

	auto* result = planSet.get();
	planCache[fftLen] = std::move(planSet);
	return result;
}

// Pre-computed kernel FFT for a specific (kernel, fftLen) combination.
// Stored in kernel_info via std::any for type erasure.
// NB initialize() takes the kernel in the transform's own precision: the element type of the
// weight buffer and the precision of the FFT it feeds are one and the same choice, and making
// that a compile-time property is what rules out the mismatch of issue #1174.
template <typename R>
struct KernelFft {
	using api = fftw_api<R>;
	using real_type = typename api::real_type;

	int fftLen = 0;
	int freqLen = 0;
	std::vector<real_type> freqData; // Interleaved [re0, im0, re1, im1, ...]
	real_type invScale = 0;          // 1 / fftLen for normalization

	KernelFft() = default;

	bool initialize(const real_type* kernelData, TileSize kernelLen, int totalFftLen) {
		fftLen = totalFftLen;
		freqLen = fftLen / 2 + 1;
		invScale = real_type(1) / real_type(fftLen);
		freqData.resize(SizeT(freqLen) * 2); // [re, im] pairs

		auto* planSet = GetOrCreatePlanSet<R>(fftLen);
		if (!planSet) return false;

		// Allocate temp buffers for kernel FFT computation
		auto* tempReal = api::alloc_real(fftLen);
		auto* tempComplex = api::alloc_complex(freqLen);
		if (!tempReal || !tempComplex) {
			api::release(tempReal);
			api::release(tempComplex);
			return false;
		}

		// Zero-pad kernel to FFT length
		for (int i = 0; i < fftLen; ++i)
			tempReal[i] = (static_cast<TileSize>(i) < kernelLen) ? kernelData[i] : real_type(0);

		// Compute kernel FFT using new-array execute
		api::execute_r2c(planSet->planFwd, tempReal, tempComplex);

		// Store frequency data
		for (int i = 0; i < freqLen; ++i) {
			freqData[SizeT(i) * 2    ] = tempComplex[i][0]; // Real
			freqData[SizeT(i) * 2 + 1] = tempComplex[i][1]; // Imag
		}

		api::release(tempReal);
		api::release(tempComplex);
		return true;
	}
};

// Thread-local FFTW working buffers to avoid allocation per convolution call.
// Sized for the maximum FFT length needed.
template <typename R>
struct FftwWorkBuffers {
	using api = fftw_api<R>;

	int capacity = 0;
	typename api::real_type* realIn = nullptr;
	typename api::real_type* realOut = nullptr;
	typename api::complex_type* freqData = nullptr;
	typename api::complex_type* freqProduct = nullptr;

	FftwWorkBuffers() = default;
	~FftwWorkBuffers() { cleanup(); }

	// Non-copyable, non-movable: only ever reached as the thread_local below
	FftwWorkBuffers(const FftwWorkBuffers&) = delete;
	FftwWorkBuffers& operator=(const FftwWorkBuffers&) = delete;

	void cleanup() {
		api::release(realIn); api::release(realOut);
		api::release(freqData); api::release(freqProduct);
		realIn = realOut = nullptr;
		freqData = freqProduct = nullptr;
		capacity = 0;
	}

	bool ensureCapacity(int fftLen) {
		if (fftLen <= capacity) return true;

		cleanup();
		int freqLen = fftLen / 2 + 1;
		realIn = api::alloc_real(fftLen);
		realOut = api::alloc_real(fftLen);
		freqData = api::alloc_complex(freqLen);
		freqProduct = api::alloc_complex(freqLen);

		if (!realIn || !realOut || !freqData || !freqProduct) {
			cleanup();
			return false;
		}
		capacity = fftLen;
		return true;
	}
};

// Thread-local work buffers - each thread gets its own set, per precision
template <typename R>
static FftwWorkBuffers<R>& GetWorkBuffers() {
	static thread_local FftwWorkBuffers<R> workBuffers;
	return workBuffers;
}

// *****************************************************************************
//	Convolution status codes
// *****************************************************************************

enum class ConvStatus {
	NoErr = 0,
	MemAllocErr = -1,
	Err = -2
};

inline void CheckConvResult(ConvStatus status, CharPtr func, CharPtr file, int line)
{
	if (status == ConvStatus::NoErr)
		return;

	throwErrorF("FFTW Convolution Error",
		"{}({},{})\n"
		"function: {}\n",
		(status == ConvStatus::MemAllocErr) ? "Memory allocation failed" : "Convolution error",
		file, line,
		func ? func : "<unknown>"
	);
}

#define CheckConvolution(status, func) CheckConvResult(status, func, __FILE__, __LINE__)

// *****************************************************************************
//											impl
// *****************************************************************************

//================================================== AlignedArray

// Cleans up aligned memory for AlignedArray.
template <typename A>
void AlignedArray<A>::clean()
{
	if (m_Data) //delete [] (m_Data);
		::operator delete(m_Data, std::align_val_t{ 64 });
}

// Reserves aligned memory for AlignedArray.
template <typename A>
void AlignedArray<A>::reserve(TileSize nrElem)
{
	if (nrElem > m_Capacity)
	{
		clean();

		SizeT byteSize = nrElem * sizeof(A);
		m_Data = reinterpret_cast<A*>(::operator new(byteSize, std::align_val_t{ 64 }));
		m_Capacity = nrElem;
	}
}

namespace potential::impl {

// Initializes a padded array for convolution input.
template <typename A, typename T>
TileSize AlignedArray_Init(AlignedArray<A>* self, UPoint& zeroInfo, const UGrid<const T>& dataOrg, SideSize kernelWidth)
{
	DBG_START("AlignedArray", "Init", MG_DEBUG_POTENTIAL);

	dms_assert(kernelWidth);
	dms_assert(dataOrg.GetSize().Row());

	SideSize nrOrgRows = dataOrg.GetSize().Row();
	SideSize nrOrgCols = dataOrg.GetSize().Col();
	SideSize borderSize = kernelWidth - 1;
	SideSize bufferWidth = nrOrgCols + borderSize;
	TileSize bufferSize = TileSize(bufferWidth) * TileSize(nrOrgRows) - TileSize(borderSize);
	dms_assert(bufferSize <= self->capacity());

	if (zeroInfo.Col() != nrOrgCols)
	{
		zeroInfo.Col() = nrOrgCols;
		zeroInfo.Row() = 0;
	}

	const T* src = dataOrg.begin();
	A* bufferPtr = self->begin();
	SideSize nrRows = 0;
	while (true)
	{
		auto src1 = src + nrOrgCols;
		fast_copy(src, src1, bufferPtr);
		src = src1;
		if (++nrRows == nrOrgRows)
			break;
		if (zeroInfo.Row() < nrRows)
		{
			fast_zero(bufferPtr + nrOrgCols, bufferPtr + bufferWidth);
			zeroInfo.Row()++;
		}
#if defined(MG_DEBUG)
		else
		{
			for (auto ptr = bufferPtr + nrOrgCols; ptr != bufferPtr + bufferWidth; ++ptr)
				dms_assert(*ptr == 0); // must have been set in previous call
		}
#endif
		bufferPtr += bufferWidth; // start of next line in overlap area
	}
	dms_assert(self->begin() + bufferSize == bufferPtr + nrOrgCols);
	return bufferSize;
}

// Initializes a reversed (mirrored) kernel buffer for convolution.
template <typename A, typename T>
void AlignedArray_InitReversed(AlignedArray<A>* self, const UGrid<const T>& dataOrg, SideSize tileDataWidth)
{
	DBG_START("AlignedArray", "InitReversed", MG_DEBUG_POTENTIAL);

	dms_assert(self);

	SideSize nrOrgRows = dataOrg.GetSize().Row();
	SideSize nrOrgCols = dataOrg.GetSize().Col();
	SideSize paddingWidth = tileDataWidth -1;
	TileSize bufferSize = dataOrg.size() + SizeT(paddingWidth) * (nrOrgRows-1);
	if (self->WasInitialized())
	{
		dms_assert(self->capacity() == bufferSize);
		return;
	}
	self->reserve(bufferSize);

	const T* srckernelDataEnd = dataOrg.end();
	A* resultBufferPtr = self->begin();
	while (nrOrgRows)
	{
		const T* srcKernelDataRowBegin = srckernelDataEnd - nrOrgCols;
		resultBufferPtr = fast_copy(srcKernelDataRowBegin, srckernelDataEnd, resultBufferPtr);
		srckernelDataEnd = srcKernelDataRowBegin;
		if (--nrOrgRows)
		{
			A* bufferAfterPadding = resultBufferPtr + paddingWidth;
			fast_zero(resultBufferPtr, bufferAfterPadding);
			resultBufferPtr = bufferAfterPadding;
		}
	}
	dms_assert(self->begin() + self->capacity() == resultBufferPtr);
}

// Optimized FFTW convolution using pre-computed kernel FFT and cached plans.
// This version avoids:
//  - Redundant kernel FFT computation (uses kernelFft)
//  - Plan creation/destruction per call (uses cached plans)
//  - Memory allocation per call (uses thread-local buffers)
template <typename R>
inline ConvStatus FftwConvolveWithKernelFft(
	const R* pSrc1, TileSize lenSrc1,
	const KernelFft<R>& kernelFft,
	R* pDst)
{
	using api = fftw_api<R>;
	using real_type = typename api::real_type;

	int fftLen = kernelFft.fftLen;
	int freqLen = kernelFft.freqLen;

	// Get cached plan set
	auto* planSet = GetOrCreatePlanSet<R>(fftLen);
	if (!planSet)
		return ConvStatus::Err;

	// Ensure thread-local buffers are large enough
	auto& workBuffers = GetWorkBuffers<R>();
	if (!workBuffers.ensureCapacity(fftLen))
		return ConvStatus::MemAllocErr;

	auto* realIn = workBuffers.realIn;
	auto* realOut = workBuffers.realOut;
	auto* freqData = workBuffers.freqData;
	auto* freqProduct = workBuffers.freqProduct;

	// Zero-pad input data to FFT length
	for (int i = 0; i < fftLen; ++i)
		realIn[i] = (static_cast<TileSize>(i) < lenSrc1) ? pSrc1[i] : real_type(0);

	// Forward FFT of data using new-array execute (thread-safe with different arrays)
	api::execute_r2c(planSet->planFwd, realIn, freqData);

	// Multiply in frequency domain with pre-computed kernel FFT
	const real_type* kernelFreq = kernelFft.freqData.data();
	for (int i = 0; i < freqLen; ++i) {
		real_type re1 = freqData[i][0], im1 = freqData[i][1];
		real_type re2 = kernelFreq[i * 2], im2 = kernelFreq[i * 2 + 1];
		freqProduct[i][0] = re1 * re2 - im1 * im2; // Real
		freqProduct[i][1] = re1 * im2 + im1 * re2; // Imag
	}

	// Inverse FFT
	api::execute_c2r(planSet->planInv, freqProduct, realOut);

	// Normalize and copy to output
	real_type scale = kernelFft.invScale;
	for (int i = 0; i < fftLen; ++i)
		pDst[i] = realOut[i] * scale;

	return ConvStatus::NoErr;
}

// Fallback FftwConvolve - used when no kernel FFT was pre-computed for this column count.
// Computes linear convolution: conv(a, b) = IFFT(FFT(a) * FFT(b))
// Output length = lenSrc1 + lenSrc2 - 1
template <typename R>
inline ConvStatus FftwConvolve(const R* pSrc1, TileSize lenSrc1, const R* pSrc2, TileSize lenSrc2, R* pDst)
{
	using api = fftw_api<R>;
	using real_type = typename api::real_type;

	// FFTW uses int for sizes, so we need to validate the input lengths
	constexpr TileSize maxFftwSize = static_cast<TileSize>(std::numeric_limits<int>::max());
	TileSize outLen64 = lenSrc1 + lenSrc2 - 1;

	if (lenSrc1 > maxFftwSize || lenSrc2 > maxFftwSize || outLen64 > maxFftwSize)
		return ConvStatus::Err;

	int fftLen = static_cast<int>(outLen64);

	// Get cached plan set
	auto* planSet = GetOrCreatePlanSet<R>(fftLen);
	if (!planSet)
		return ConvStatus::Err;

	// Ensure thread-local buffers are large enough
	auto& workBuffers = GetWorkBuffers<R>();
	if (!workBuffers.ensureCapacity(fftLen))
		return ConvStatus::MemAllocErr;

	// Allocate additional buffer for second input's frequency data
	auto* freq2 = api::alloc_complex(fftLen / 2 + 1);
	if (!freq2)
		return ConvStatus::MemAllocErr;

	auto* realIn = workBuffers.realIn;
	auto* realOut = workBuffers.realOut;
	auto* freq1 = workBuffers.freqData;
	auto* freqOut = workBuffers.freqProduct;

	// Forward FFT of first input
	for (int i = 0; i < fftLen; ++i)
		realIn[i] = (static_cast<TileSize>(i) < lenSrc1) ? pSrc1[i] : real_type(0);
	api::execute_r2c(planSet->planFwd, realIn, freq1);

	// Forward FFT of second input (kernel)
	for (int i = 0; i < fftLen; ++i)
		realIn[i] = (static_cast<TileSize>(i) < lenSrc2) ? pSrc2[i] : real_type(0);
	api::execute_r2c(planSet->planFwd, realIn, freq2);

	// Multiply in frequency domain
	int freqLen = fftLen / 2 + 1;
	for (int i = 0; i < freqLen; ++i) {
		real_type re1 = freq1[i][0], im1 = freq1[i][1];
		real_type re2 = freq2[i][0], im2 = freq2[i][1];
		freqOut[i][0] = re1 * re2 - im1 * im2;
		freqOut[i][1] = re1 * im2 + im1 * re2;
	}

	api::release(freq2);

	// Inverse FFT
	api::execute_c2r(planSet->planInv, freqOut, realOut);

	// Normalize and copy to output
	real_type scale = real_type(1) / real_type(fftLen);
	for (int i = 0; i < fftLen; ++i)
		pDst[i] = realOut[i] * scale;

	return ConvStatus::NoErr;
}

// Performs raw convolution using FFTW, with buffer and kernel management.
// Uses pre-computed kernel FFT when available for optimal performance.
template <typename A, typename T>
TileSize PotentialFftwRaw(potential_context<A>& context, UPoint& zeroInfo, const kernel_info& kernelInfo, const UGrid<const T>& dataOrg)
{
	DBG_START("Potential", "Fftw", MG_DEBUG_POTENTIAL);

	if (!context.WasInitialized())
	{
		TileSize bufferSize = Cardinality(kernelInfo.maxDataSize) + Cardinality(UPoint(kernelInfo.orgWeightSize.Col()-1, kernelInfo.maxDataSize.Row() - 1));
		context.paddedInput.reserve(bufferSize);
		context.overlappingOutput.reserve(Cardinality(kernelInfo.maxColvolvedSize));
	}
	auto& weightBuffer = *kernelInfo.weightBuffer<A>(dataOrg.GetSize().Col());

	SideSize nrCols = dataOrg.GetSize().Col() + kernelInfo.orgWeightSize.Col() - 1;
	SideSize nrRows = dataOrg.GetSize().Row() + kernelInfo.orgWeightSize.Row() - 1;

	auto dataBufferSize = AlignedArray_Init(&context.paddedInput, zeroInfo, dataOrg, kernelInfo.orgWeightSize.Col()); // fill all in-between space with zeroes once

	TileSize outputSize = dataBufferSize + weightBuffer.capacity() - 1;
	dms_assert(outputSize == SizeT(nrCols) * nrRows ); // elementary math

	dms_assert(dataBufferSize <= context.paddedInput.capacity());
	dms_assert(outputSize <= context.overlappingOutput.capacity());

	ConvStatus result;

	// Try to use pre-computed kernel FFT for optimal performance
	if (kernelInfo.kernelFfts.has_value())
	{
		const auto& kernelFftMap = GetKernelFftMap<A>(kernelInfo);
		auto it = kernelFftMap.find(dataOrg.GetSize().Col());
		if (it != kernelFftMap.end())
		{
			// Use optimized path with pre-computed kernel FFT
			result = FftwConvolveWithKernelFft<A>(
				context.paddedInput.begin(), dataBufferSize,
				it->second,
				context.overlappingOutput.begin()
			);
		}
		else
		{
			// Fallback to legacy path (kernel FFT not pre-computed for this column count)
			result = FftwConvolve(
				context.paddedInput.begin(), dataBufferSize,
				weightBuffer.begin(), weightBuffer.capacity(),
				context.overlappingOutput.begin()
			);
		}
	}
	else
	{
		// Fallback to legacy path (no kernel FFT cache)
		result = FftwConvolve(
			context.paddedInput.begin(), dataBufferSize,
			weightBuffer.begin(), weightBuffer.capacity(),
			context.overlappingOutput.begin()
		);
	}

	CheckConvResult(result, "FftwConvolve", MG_POS);	
	if (result != ConvStatus::NoErr)
		DmsException::throwMsgF("Convolution failed with code {}", static_cast<int>(result));

	return outputSize;
}

// Helper for squaring Float64 values.
inline Float64 Sqr64(Float64 v) { return v*v; }

// Performs convolution and then smooths small values to zero.
template <typename A, typename T>
TileSize PotentialFftwSmooth(potential_context<A>& context, UPoint& zeroInfo, const kernel_info& kernelInfo, const UGrid<const T>& dataOrg)
{
	auto outputSize = PotentialFftwRaw<A, T>(context, zeroInfo, kernelInfo, dataOrg);

	Float64 sumSqrData = 0; 
	auto firstOutput = context.overlappingOutput.begin();
	auto lastOutput = context.overlappingOutput.begin() + outputSize;
	for (auto ptr = firstOutput, end = lastOutput; ptr !=end ; ++ptr)
		sumSqrData += Sqr64(*ptr);

	// The threshold has to sit above the transform's own noise floor, which scales with the
	// epsilon of the precision the transform ran in. 1e-9 of the L2 norm is some 4.5e6 epsilons
	// in float64, but two decades BELOW the float32 noise floor, where it would never fire.
	// 1e-6 is its float32 counterpart: roughly ten times that noise floor.
	constexpr Float64 relThreshold = std::is_same_v<A, Float64> ? 1e-9 : 1e-6;

	auto errThreshold = sqrt(sumSqrData) * relThreshold;
	auto errThresholdNeg = - errThreshold;
	for (auto ptr = firstOutput, end = lastOutput; ptr != end; ++ptr)
		if (*ptr < errThreshold && errThresholdNeg < *ptr)
			*ptr = 0.0;

	return outputSize;
}

// *****************************************************************************
//	CalculateClassic
// *****************************************************************************

// Classic (non-FFT) convolution and proximity calculation.
template <typename T>
bool CalculateClassic(AnalysisType at,
		const UGrid<const T>& dataGrid, const UGrid<const T>& kernelGrid,
		const kernel_info& kernelInfo, AlignedArray<T>& output)
{
	DBG_START("Potential", "CalculateClassic", true);

	auto dataSize = dataGrid.GetSize();
	auto kernelSize = kernelGrid.GetSize();

	if (!output.WasInitialized())
	{
		output.reserve(Cardinality(kernelInfo.maxColvolvedSize));
	}

	UPoint outputSize = dataSize + kernelSize - UPoint(1, 1);
	dms_assert(Cardinality(outputSize) <= output.capacity());

	UGrid<T> outputGrid(outputSize, output.begin());
	for (UCoordType outputRow = 0; outputRow != outputSize.Row(); ++outputRow)
	{
		auto firstDataRow = (outputRow + 1 > kernelSize.Row()) ? outputRow + 1 - kernelSize.Row() : 0;
		Range<UCoordType> dataRowRange(firstDataRow, Min<UCoordType>(outputRow + 1, dataSize.Row()));

		Range<UCoordType> weightRowRange((dataRowRange.first + kernelSize.Row()) - (outputRow + 1), (dataRowRange.second + kernelSize.Row()) - (outputRow + 1));

		dms_assert(dataRowRange.first   < dataRowRange.second);
		dms_assert(weightRowRange.first < weightRowRange.second);

		for (UCoordType outputCol = 0; outputCol != outputSize.Col(); ++outputCol)
		{
			auto firstDataCol = (outputCol + 1 > kernelSize.Col()) ? outputCol + 1 - kernelSize.Col() : 0;
			Range<UCoordType> dataColRange(firstDataCol, Min<UCoordType>(outputCol + 1, dataSize.Col()));

			Range<UCoordType> weightColRange(dataColRange.first  - (outputCol + 1 - kernelSize.Col()), dataColRange.second - (outputCol + 1 - kernelSize.Col()));

			UCoordType weightColDiff = weightColRange.second - weightColRange.first;

			dms_assert(dataColRange  .first < dataColRange  .second);
			dms_assert(weightColRange.first < weightColRange.second);

			T* outputPtr = outputGrid.elemptr(shp2dms_order(outputCol, outputRow));	
			*outputPtr = 0;
			
			for (UCoordType weightRow = weightRowRange.first, dataRowLocal = dataRowRange.first; weightRow != weightRowRange.second; ++weightRow, ++dataRowLocal)
			{
				const T* dataPtr   = dataGrid  .elemptr(shp2dms_order(dataColRange  .first, dataRowLocal));
				const T* weightPtr = kernelGrid.elemptr(shp2dms_order(weightColRange.first, weightRow   ));
				const T* weightEnd = weightPtr + weightColDiff;

				switch (at)
				{
					case AnalysisType::PotentialSlow:
						while (weightPtr != weightEnd)
							*outputPtr += *dataPtr++ * *weightPtr++;
						break;
					case AnalysisType::Proximity:
						while (weightPtr != weightEnd)
							MakeMax(*outputPtr, *dataPtr++ * *weightPtr++); 
						break;
				}
			} // next weightRow, dataRowLocal
		} // next outputCol, dataCol
	} // next outputRow, dataRow

	return true;
} // CalculateClassic

// Type alias for kernel FFT cache map (used with std::any in kernel_info).
// A kernel_info serves one operator, hence one backend, hence one precision R.
template <typename R> using KernelFftMap = std::map<SideSize, KernelFft<R>>;

// Helper to get or create kernel FFT cache from kernel_info
template <typename R>
KernelFftMap<R>& GetKernelFftMap(kernel_info& self)
{
	if (!self.kernelFfts.has_value())
		self.kernelFfts = KernelFftMap<R>{};
	return std::any_cast<KernelFftMap<R>&>(self.kernelFfts);
}

template <typename R>
const KernelFftMap<R>& GetKernelFftMap(const kernel_info& self)
{
	return std::any_cast<const KernelFftMap<R>&>(self.kernelFfts);
}

// Pre-compute (and cache) the kernel FFT for a given data column count. The element type of
// the weight buffer picks the transform precision, so the kernel always goes into the FFT as
// the type it was written as.
template <typename R>
void AddKernelFft(kernel_info& self, const AlignedArray<R>* weightBuffer, SideSize nrDataCols)
{
	auto& kernelFftMap = GetKernelFftMap<R>(self);
	if (kernelFftMap.find(nrDataCols) != kernelFftMap.end())
		return;

	// Compute FFT length based on max data size and kernel size
	// FFT length = dataBufferSize + weightBufferSize - 1
	// dataBufferSize = nx*ny + (kx-1)*(ny-1) where nx=nrDataCols, ny=maxDataRows
	// weightBufferSize = kx*ky + (nx-1)*(ky-1)
	SideSize maxDataRows = self.maxDataSize.Row();
	SideSize kx = self.orgWeightSize.Col();

	TileSize maxDataBufferSize = TileSize(nrDataCols) * maxDataRows + TileSize(kx - 1) * (maxDataRows - 1);
	TileSize kernelBufferSize = weightBuffer->capacity();
	TileSize fftLen = maxDataBufferSize + kernelBufferSize - 1;

	// Validate FFT size fits in int (FFTW limitation)
	constexpr TileSize maxFftwSize = static_cast<TileSize>(std::numeric_limits<int>::max());
	if (fftLen > maxFftwSize)
		return;

	KernelFft<R> newKernelFft;
	if (newKernelFft.initialize(weightBuffer->begin(), kernelBufferSize, static_cast<int>(fftLen)))
		kernelFftMap[nrDataCols] = std::move(newKernelFft);
}

} // namespace potential::impl

// Adds a convolution kernel to the kernel_info for a given data type and analysis type.
// Pre-computes and caches the kernel FFT for efficient convolution.
template < typename T>
MDL_CALL void AddConvolutionKernel(kernel_info& self, AnalysisType at, SideSize nrDataCols)
{
	DBG_START("AddConvolutionKernel", "Fftw", MG_DEBUG_POTENTIAL);

	if (!nrDataCols)
		return;

	// The Fft64 backends widen the kernel and their working buffers to Float64; the Fft32
	// backends keep everything in the element type of the arguments and run the transform in
	// single precision. Either way the weight buffer, the pre-computed kernel FFT and the
	// transform that consumes it share one element type -- letting those drift apart is what
	// issue #1174 was.
	bool isFloat64Backend = (at == AnalysisType::PotentialFft64 || at == AnalysisType::PotentialRawFft64);
	bool isFloat32Backend = (at == AnalysisType::PotentialFft32 || at == AnalysisType::PotentialRawFft32);
	if (!isFloat64Backend && !isFloat32Backend)
		return; // PotentialSlow and Proximity don't convolve; they need neither weight buffer nor kernel FFT.

	//	dms_assert(dataOrg.GetSize() == outputOrg.GetSize());
	const UGrid<const T>& weightOrg = *std::any_cast<UGrid<const T>>(&self.orgWeightGrid);

	// Initialize the reversed weight buffer, then the kernel FFT that reads it
	if (isFloat64Backend)
	{
		auto* weightBuffer = self.weightBuffer<Float64>(nrDataCols);
		potential::impl::AlignedArray_InitReversed(weightBuffer, weightOrg, nrDataCols);
		potential::impl::AddKernelFft(self, weightBuffer, nrDataCols);
	}
	else
	{
		auto* weightBuffer = self.weightBuffer<T>(nrDataCols);
		potential::impl::AlignedArray_InitReversed(weightBuffer, weightOrg, nrDataCols);
		potential::impl::AddKernelFft(self, weightBuffer, nrDataCols);
	}
}

// Main entry for Float32 potential calculation, dispatches to classic or FFTW.
bool Potential(AnalysisType at, potential_contexts& context, const kernel_info& kernelInfo, const UCFloat32Grid& dataOrg)
{
	DBG_START("Potential", "Float32", MG_DEBUG_POTENTIAL);

	assert(!dataOrg.empty());

	switch (at) {
		case AnalysisType::PotentialSlow:
		case AnalysisType::Proximity:
			return potential::impl::CalculateClassic<Float32>(at, 
				dataOrg, std::any_cast<UGrid<const Float32>>(kernelInfo.orgWeightGrid),
				kernelInfo, context.F32.overlappingOutput
			);

		case AnalysisType::PotentialRawFft32:
			return potential::impl::PotentialFftwRaw   <Float32>(context.F32, context.zeroInfo, kernelInfo, dataOrg);

		case AnalysisType::PotentialFft32:
			return potential::impl::PotentialFftwSmooth<Float32>(context.F32, context.zeroInfo, kernelInfo, dataOrg);

		case AnalysisType::PotentialRawFft64:
			return potential::impl::PotentialFftwRaw   <Float64>(context.F64, context.zeroInfo, kernelInfo, dataOrg);

		case AnalysisType::PotentialFft64:
			return potential::impl::PotentialFftwSmooth<Float64>(context.F64, context.zeroInfo, kernelInfo, dataOrg);

		default:
			throwIllegalAbstract(MG_POS, "Potential");
	}
		
	return true;
}

// Main entry for Float64 potential calculation, dispatches to classic or FFTW.
bool Potential(AnalysisType at, potential_contexts& context, const kernel_info& kernelInfo, const UCFloat64Grid& dataOrg)
{
	DBG_START("Potential", "Float64", MG_DEBUG_POTENTIAL);

	if (at == AnalysisType::PotentialFft32   ) at = AnalysisType::PotentialFft64;
	if (at == AnalysisType::PotentialRawFft32) at = AnalysisType::PotentialRawFft64;

	switch (at) {
		case AnalysisType::PotentialSlow:
		case AnalysisType::Proximity:
			return potential::impl::CalculateClassic<Float64>(at,
				dataOrg, std::any_cast<UGrid<const Float64>>(kernelInfo.orgWeightGrid),
				kernelInfo, context.F64.overlappingOutput
			);

		case AnalysisType::PotentialRawFft64:
			return potential::impl::PotentialFftwRaw   <Float64>(context.F64, context.zeroInfo, kernelInfo, dataOrg);

		case AnalysisType::PotentialFft64:
			return potential::impl::PotentialFftwSmooth<Float64>(context.F64, context.zeroInfo, kernelInfo, dataOrg);

		default:
			throwIllegalAbstract(MG_POS, "Potential");
	}
		
	return true;
}

// Explicit template instantiations for AlignedArray and AddConvolutionKernel.
template struct AlignedArray<Float32>;
template struct AlignedArray<Float64>;

template void AddConvolutionKernel<Float32>(kernel_info& self, AnalysisType at, SideSize nrDataCols);
template void AddConvolutionKernel<Float64>(kernel_info& self, AnalysisType at, SideSize nrDataCols);

// *****************************************************************************
// MDL Exported API
// *****************************************************************************

#if defined(MDL_EXPORTS)

// Exported API for Float32 potential calculation.
bool MDL_Potential32(
	AnalysisType at, 
	const TCFloat32Grid& data, 
	const TFloat32Grid&  output,
	const TCFloat32Grid& weight)
{
	return Potential(at, data, output, weight);
}

// Exported API for Float64 potential calculation.
bool MDL_Potential64(
	AnalysisType at, 
	const TCFloat64Grid& data, 
	const TFloat64Grid&  output,
	const TCFloat64Grid& weight)
{
	return Potential(at, data, output, weight);
}

#endif
