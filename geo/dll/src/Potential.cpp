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

// Cached FFTW resources for a specific FFT length.
// Plans are created once and reused via fftw_execute_dft_r2c/c2r with new arrays.
struct FftwPlanSet {
	int fftLen = 0;
	fftw_plan planFwd = nullptr;  // Forward: real -> complex
	fftw_plan planInv = nullptr;  // Inverse: complex -> real

	// Scratch arrays used only for plan creation (FFTW_ESTIMATE doesn't actually use them)
	double* scratchReal = nullptr;
	fftw_complex* scratchComplex = nullptr;

	FftwPlanSet() = default;

	// Non-copyable, non-movable (plans are tied to this instance)
	FftwPlanSet(const FftwPlanSet&) = delete;
	FftwPlanSet& operator=(const FftwPlanSet&) = delete;

	~FftwPlanSet() {
		if (planFwd) fftw_destroy_plan(planFwd);
		if (planInv) fftw_destroy_plan(planInv);
		if (scratchReal) fftw_free(scratchReal);
		if (scratchComplex) fftw_free(scratchComplex);
	}

	bool initialize(int len) {
		fftLen = len;
		scratchReal = fftw_alloc_real(len);
		scratchComplex = fftw_alloc_complex(len / 2 + 1);
		if (!scratchReal || !scratchComplex) return false;

		planFwd = fftw_plan_dft_r2c_1d(len, scratchReal, scratchComplex, FFTW_ESTIMATE);
		planInv = fftw_plan_dft_c2r_1d(len, scratchComplex, scratchReal, FFTW_ESTIMATE);
		return planFwd && planInv;
	}
};

// Global cache of FFTW plans keyed by FFT length.
// Uses shared_mutex for read-heavy access pattern.
static std::shared_mutex g_planCacheMutex;
static std::unordered_map<int, std::unique_ptr<FftwPlanSet>> g_planCache;

// Get or create a plan set for the given FFT length.
// Returns nullptr on failure.
static FftwPlanSet* GetOrCreatePlanSet(int fftLen) {
	// Try read-only access first (common case)
	{
		std::shared_lock<std::shared_mutex> readLock(g_planCacheMutex);
		auto it = g_planCache.find(fftLen);
		if (it != g_planCache.end())
			return it->second.get();
	}

	// Need to create - acquire exclusive lock
	std::unique_lock<std::shared_mutex> writeLock(g_planCacheMutex);

	// Double-check after acquiring write lock
	auto it = g_planCache.find(fftLen);
	if (it != g_planCache.end())
		return it->second.get();

	// Create new plan set (plan creation itself needs the FFTW mutex)
	auto planSet = std::make_unique<FftwPlanSet>();
	{
		std::lock_guard<std::mutex> fftwLock(g_fftwPlanMutex);
		if (!planSet->initialize(fftLen))
			return nullptr;
	}

	auto* result = planSet.get();
	g_planCache[fftLen] = std::move(planSet);
	return result;
}

// Pre-computed kernel FFT for a specific (kernel, fftLen) combination.
// Stored in kernel_info via std::any for type erasure.
struct KernelFft {
	int fftLen = 0;
	int freqLen = 0;
	std::vector<double> freqData; // Interleaved [re0, im0, re1, im1, ...]
	double invScale = 0.0;        // 1.0 / fftLen for normalization

	KernelFft() = default;

	template <typename T>
	bool initialize(const T* kernelData, TileSize kernelLen, int totalFftLen) {
		fftLen = totalFftLen;
		freqLen = fftLen / 2 + 1;
		invScale = 1.0 / fftLen;
		freqData.resize(freqLen * 2); // [re, im] pairs

		auto* planSet = GetOrCreatePlanSet(fftLen);
		if (!planSet) return false;

		// Allocate temp buffers for kernel FFT computation
		double* tempReal = fftw_alloc_real(fftLen);
		fftw_complex* tempComplex = fftw_alloc_complex(freqLen);
		if (!tempReal || !tempComplex) {
			fftw_free(tempReal);
			fftw_free(tempComplex);
			return false;
		}

		// Zero-pad kernel to FFT length
		for (int i = 0; i < fftLen; ++i)
			tempReal[i] = (static_cast<TileSize>(i) < kernelLen) ? static_cast<double>(kernelData[i]) : 0.0;

		// Compute kernel FFT using new-array execute
		fftw_execute_dft_r2c(planSet->planFwd, tempReal, tempComplex);

		// Store frequency data
		for (int i = 0; i < freqLen; ++i) {
			freqData[i * 2]     = tempComplex[i][0]; // Real
			freqData[i * 2 + 1] = tempComplex[i][1]; // Imag
		}

		fftw_free(tempReal);
		fftw_free(tempComplex);
		return true;
	}
};

// Thread-local FFTW working buffers to avoid allocation per convolution call.
// Sized for the maximum FFT length needed.
struct FftwWorkBuffers {
	int capacity = 0;
	double* realIn = nullptr;
	double* realOut = nullptr;
	fftw_complex* freqData = nullptr;
	fftw_complex* freqProduct = nullptr;

	FftwWorkBuffers() = default;
	~FftwWorkBuffers() { cleanup(); }

	// Non-copyable
	FftwWorkBuffers(const FftwWorkBuffers&) = delete;
	FftwWorkBuffers& operator=(const FftwWorkBuffers&) = delete;

	// Move constructor for thread_local initialization
	FftwWorkBuffers(FftwWorkBuffers&& other) noexcept
		: capacity(other.capacity), realIn(other.realIn), realOut(other.realOut)
		, freqData(other.freqData), freqProduct(other.freqProduct)
	{
		other.capacity = 0;
		other.realIn = other.realOut = nullptr;
		other.freqData = other.freqProduct = nullptr;
	}

	void cleanup() {
		fftw_free(realIn); fftw_free(realOut);
		fftw_free(freqData); fftw_free(freqProduct);
		realIn = realOut = nullptr;
		freqData = freqProduct = nullptr;
		capacity = 0;
	}

	bool ensureCapacity(int fftLen) {
		if (fftLen <= capacity) return true;

		cleanup();
		int freqLen = fftLen / 2 + 1;
		realIn = fftw_alloc_real(fftLen);
		realOut = fftw_alloc_real(fftLen);
		freqData = fftw_alloc_complex(freqLen);
		freqProduct = fftw_alloc_complex(freqLen);

		if (!realIn || !realOut || !freqData || !freqProduct) {
			cleanup();
			return false;
		}
		capacity = fftLen;
		return true;
	}
};

// Thread-local work buffers - each thread gets its own set
static thread_local FftwWorkBuffers t_workBuffers;

// *****************************************************************************
//	Convolution status codes (replacing IPP types)
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
		"%s(%s,%d)\n"
		"function: %s\n",
		(status == ConvStatus::MemAllocErr) ? "Memory allocation failed" : "Convolution error",
		file, line,
		func ? func : "<unknown>"
	);
}

#define CheckConvolution(status, func) CheckConvResult(status, func, __FILE__, __LINE__)

// *****************************************************************************
//											impl
// *****************************************************************************

//================================================== IppsArray

// Cleans up aligned memory for IppsArray.
template <typename A>
void IppsArray<A>::clean()
{
	if (m_Data) //delete [] (m_Data);
		::operator delete(m_Data, std::align_val_t{ 64 });
}

// Reserves aligned memory for IppsArray.
template <typename A>
void IppsArray<A>::reserve(TileSize nrElem)
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
TileSize IppsArray_Init(IppsArray<A>* self, UPoint& zeroInfo, const UGrid<const T>& dataOrg, SideSize kernelWidth)
{
	DBG_START("IppsArray", "Init", MG_DEBUG_POTENTIAL);

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
void IppsArray_InitReversed(IppsArray<A>* self, const UGrid<const T>& dataOrg, SideSize tileDataWidth)
{
	DBG_START("IppsArray", "InitReversed", MG_DEBUG_POTENTIAL);

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
template <typename T>
inline ConvStatus FftwConvolveWithKernelFft(
	const T* pSrc1, TileSize lenSrc1,
	const KernelFft& kernelFft,
	T* pDst)
{
	int fftLen = kernelFft.fftLen;
	int freqLen = kernelFft.freqLen;

	// Get cached plan set
	auto* planSet = GetOrCreatePlanSet(fftLen);
	if (!planSet)
		return ConvStatus::Err;

	// Ensure thread-local buffers are large enough
	if (!t_workBuffers.ensureCapacity(fftLen))
		return ConvStatus::MemAllocErr;

	double* realIn = t_workBuffers.realIn;
	double* realOut = t_workBuffers.realOut;
	fftw_complex* freqData = t_workBuffers.freqData;
	fftw_complex* freqProduct = t_workBuffers.freqProduct;

	// Zero-pad input data to FFT length
	for (int i = 0; i < fftLen; ++i)
		realIn[i] = (static_cast<TileSize>(i) < lenSrc1) ? static_cast<double>(pSrc1[i]) : 0.0;

	// Forward FFT of data using new-array execute (thread-safe with different arrays)
	fftw_execute_dft_r2c(planSet->planFwd, realIn, freqData);

	// Multiply in frequency domain with pre-computed kernel FFT
	const double* kernelFreq = kernelFft.freqData.data();
	for (int i = 0; i < freqLen; ++i) {
		double re1 = freqData[i][0], im1 = freqData[i][1];
		double re2 = kernelFreq[i * 2], im2 = kernelFreq[i * 2 + 1];
		freqProduct[i][0] = re1 * re2 - im1 * im2; // Real
		freqProduct[i][1] = re1 * im2 + im1 * re2; // Imag
	}

	// Inverse FFT
	fftw_execute_dft_c2r(planSet->planInv, freqProduct, realOut);

	// Normalize and copy to output
	double scale = kernelFft.invScale;
	for (int i = 0; i < fftLen; ++i)
		pDst[i] = static_cast<T>(realOut[i] * scale);

	return ConvStatus::NoErr;
}

// Legacy FftwConvolve for Float32 - used when kernel FFT is not pre-computed.
// Computes linear convolution: conv(a, b) = IFFT(FFT(a) * FFT(b))
// Output length = lenSrc1 + lenSrc2 - 1
inline ConvStatus FftwConvolve(const Float32* pSrc1, TileSize lenSrc1, const Float32* pSrc2, TileSize lenSrc2, Float32* pDst)
{
	// FFTW uses int for sizes, so we need to validate the input lengths
	constexpr TileSize maxFftwSize = static_cast<TileSize>(std::numeric_limits<int>::max());
	TileSize outLen64 = lenSrc1 + lenSrc2 - 1;

	if (lenSrc1 > maxFftwSize || lenSrc2 > maxFftwSize || outLen64 > maxFftwSize)
		return ConvStatus::Err;

	int fftLen = static_cast<int>(outLen64);

	// Get cached plan set
	auto* planSet = GetOrCreatePlanSet(fftLen);
	if (!planSet)
		return ConvStatus::Err;

	// Ensure thread-local buffers are large enough
	if (!t_workBuffers.ensureCapacity(fftLen))
		return ConvStatus::MemAllocErr;

	// Allocate additional buffer for second input's frequency data
	fftw_complex* freq2 = fftw_alloc_complex(fftLen / 2 + 1);
	if (!freq2)
		return ConvStatus::MemAllocErr;

	double* realIn = t_workBuffers.realIn;
	double* realOut = t_workBuffers.realOut;
	fftw_complex* freq1 = t_workBuffers.freqData;
	fftw_complex* freqOut = t_workBuffers.freqProduct;

	// Forward FFT of first input
	for (int i = 0; i < fftLen; ++i)
		realIn[i] = (static_cast<TileSize>(i) < lenSrc1) ? static_cast<double>(pSrc1[i]) : 0.0;
	fftw_execute_dft_r2c(planSet->planFwd, realIn, freq1);

	// Forward FFT of second input (kernel)
	for (int i = 0; i < fftLen; ++i)
		realIn[i] = (static_cast<TileSize>(i) < lenSrc2) ? static_cast<double>(pSrc2[i]) : 0.0;
	fftw_execute_dft_r2c(planSet->planFwd, realIn, freq2);

	// Multiply in frequency domain
	int freqLen = fftLen / 2 + 1;
	for (int i = 0; i < freqLen; ++i) {
		double re1 = freq1[i][0], im1 = freq1[i][1];
		double re2 = freq2[i][0], im2 = freq2[i][1];
		freqOut[i][0] = re1 * re2 - im1 * im2;
		freqOut[i][1] = re1 * im2 + im1 * re2;
	}

	fftw_free(freq2);

	// Inverse FFT
	fftw_execute_dft_c2r(planSet->planInv, freqOut, realOut);

	// Normalize and copy to output
	double scale = 1.0 / fftLen;
	for (int i = 0; i < fftLen; ++i)
		pDst[i] = static_cast<Float32>(realOut[i] * scale);

	return ConvStatus::NoErr;
}

// Legacy FftwConvolve for Float64 - used when kernel FFT is not pre-computed.
inline ConvStatus FftwConvolve(const Float64* pSrc1, TileSize lenSrc1, const Float64* pSrc2, TileSize lenSrc2, Float64* pDst)
{
	constexpr TileSize maxFftwSize = static_cast<TileSize>(std::numeric_limits<int>::max());
	TileSize outLen64 = lenSrc1 + lenSrc2 - 1;

	if (lenSrc1 > maxFftwSize || lenSrc2 > maxFftwSize || outLen64 > maxFftwSize)
		return ConvStatus::Err;

	int fftLen = static_cast<int>(outLen64);

	auto* planSet = GetOrCreatePlanSet(fftLen);
	if (!planSet)
		return ConvStatus::Err;

	if (!t_workBuffers.ensureCapacity(fftLen))
		return ConvStatus::MemAllocErr;

	fftw_complex* freq2 = fftw_alloc_complex(fftLen / 2 + 1);
	if (!freq2)
		return ConvStatus::MemAllocErr;

	double* realIn = t_workBuffers.realIn;
	double* realOut = t_workBuffers.realOut;
	fftw_complex* freq1 = t_workBuffers.freqData;
	fftw_complex* freqOut = t_workBuffers.freqProduct;

	for (int i = 0; i < fftLen; ++i)
		realIn[i] = (static_cast<TileSize>(i) < lenSrc1) ? pSrc1[i] : 0.0;
	fftw_execute_dft_r2c(planSet->planFwd, realIn, freq1);

	for (int i = 0; i < fftLen; ++i)
		realIn[i] = (static_cast<TileSize>(i) < lenSrc2) ? pSrc2[i] : 0.0;
	fftw_execute_dft_r2c(planSet->planFwd, realIn, freq2);

	int freqLen = fftLen / 2 + 1;
	for (int i = 0; i < freqLen; ++i) {
		double re1 = freq1[i][0], im1 = freq1[i][1];
		double re2 = freq2[i][0], im2 = freq2[i][1];
		freqOut[i][0] = re1 * re2 - im1 * im2;
		freqOut[i][1] = re1 * im2 + im1 * re2;
	}

	fftw_free(freq2);

	fftw_execute_dft_c2r(planSet->planInv, freqOut, realOut);

	double scale = 1.0 / fftLen;
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

	auto dataBufferSize = IppsArray_Init(&context.paddedInput, zeroInfo, dataOrg, kernelInfo.orgWeightSize.Col()); // fill all in-between space with zeroes once

	TileSize outputSize = dataBufferSize + weightBuffer.capacity() - 1;
	dms_assert(outputSize == SizeT(nrCols) * nrRows ); // elementary math

	dms_assert(dataBufferSize <= context.paddedInput.capacity());
	dms_assert(outputSize <= context.overlappingOutput.capacity());

	ConvStatus result;

	// Try to use pre-computed kernel FFT for optimal performance
	if (kernelInfo.kernelFfts.has_value())
	{
		const auto& kernelFftMap = std::any_cast<const std::map<SideSize, KernelFft>&>(kernelInfo.kernelFfts);
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
		DmsException::throwMsgF("Convolution failed with code %d", static_cast<int>(result));

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

	auto errThreshold = sqrt(sumSqrData) / 1000000000.0;
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
		const kernel_info& kernelInfo, IppsArray<T>& output)
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

// Type alias for kernel FFT cache map (used with std::any in kernel_info)
using KernelFftMap = std::map<SideSize, KernelFft>;

// Helper to get or create kernel FFT cache from kernel_info
KernelFftMap& GetKernelFftMap(kernel_info& self)
{
	if (!self.kernelFfts.has_value())
		self.kernelFfts = KernelFftMap{};
	return std::any_cast<KernelFftMap&>(self.kernelFfts);
}

const KernelFftMap& GetKernelFftMap(const kernel_info& self)
{
	return std::any_cast<const KernelFftMap&>(self.kernelFfts);
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

	//	dms_assert(dataOrg.GetSize() == outputOrg.GetSize());
	const UGrid<const T>& weightOrg = *std::any_cast<UGrid<const T>>(&self.orgWeightGrid);
	SizeT weightBufferSize = weightOrg.size() + Cardinality(UPoint(nrDataCols - 1, weightOrg.GetSize().Row() - 1));

	// Initialize reversed weight buffer (for packed convolution layout)
	if (at == AnalysisType::PotentialIpps64 || at == AnalysisType::PotentialRawIpps64)
		potential::impl::IppsArray_InitReversed(self.weightBuffer<Float64>(nrDataCols), weightOrg, nrDataCols);
	else if (at == AnalysisType::PotentialIppsPacked || at == AnalysisType::PotentialRawIppsPacked)
		potential::impl::IppsArray_InitReversed(self.weightBuffer<T>(nrDataCols), weightOrg, nrDataCols);

	// Pre-compute kernel FFT for this column count if not already done
	auto& kernelFftMap = potential::impl::GetKernelFftMap(self);
	if (kernelFftMap.find(nrDataCols) == kernelFftMap.end())
	{
		// Get the weight buffer we just initialized
		const auto* weightBuffer = (at == AnalysisType::PotentialIpps64 || at == AnalysisType::PotentialRawIpps64)
			? self.weightBuffer<Float64>(nrDataCols)
			: reinterpret_cast<const IppsArray<Float64>*>(self.weightBuffer<T>(nrDataCols));

		// Compute FFT length based on max data size and kernel size
		// FFT length = dataBufferSize + weightBufferSize - 1
		// dataBufferSize = nx*ny + (kx-1)*(ny-1) where nx=nrDataCols, ny=maxDataRows
		// weightBufferSize = kx*ky + (nx-1)*(ky-1)
		SideSize maxDataRows = self.maxDataSize.Row();
		SideSize kx = self.orgWeightSize.Col();
		SideSize ky = self.orgWeightSize.Row();

		TileSize maxDataBufferSize = TileSize(nrDataCols) * maxDataRows + TileSize(kx - 1) * (maxDataRows - 1);
		TileSize kernelBufferSize = weightBuffer->capacity();
		TileSize fftLen = maxDataBufferSize + kernelBufferSize - 1;

		// Validate FFT size fits in int (FFTW limitation)
		constexpr TileSize maxFftwSize = static_cast<TileSize>(std::numeric_limits<int>::max());
		if (fftLen <= maxFftwSize)
		{
			KernelFft newKernelFft;
			if (newKernelFft.initialize(weightBuffer->begin(), kernelBufferSize, static_cast<int>(fftLen)))
			{
				kernelFftMap[nrDataCols] = std::move(newKernelFft);
			}
		}
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

		case AnalysisType::PotentialRawIppsPacked:
			return potential::impl::PotentialFftwRaw   <Float32>(context.F32, context.zeroInfo, kernelInfo, dataOrg);

		case AnalysisType::PotentialIppsPacked:
			return potential::impl::PotentialFftwSmooth<Float32>(context.F32, context.zeroInfo, kernelInfo, dataOrg);

		case AnalysisType::PotentialRawIpps64:
			return potential::impl::PotentialFftwRaw   <Float64>(context.F64, context.zeroInfo, kernelInfo, dataOrg);

		case AnalysisType::PotentialIpps64:
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

	if (at == AnalysisType::PotentialIppsPacked   ) at = AnalysisType::PotentialIpps64;
	if (at == AnalysisType::PotentialRawIppsPacked) at = AnalysisType::PotentialRawIpps64;

	switch (at) {
		case AnalysisType::PotentialSlow:
		case AnalysisType::Proximity:
			return potential::impl::CalculateClassic<Float64>(at,
				dataOrg, std::any_cast<UGrid<const Float64>>(kernelInfo.orgWeightGrid),
				kernelInfo, context.F64.overlappingOutput
			);

		case AnalysisType::PotentialRawIpps64:
			return potential::impl::PotentialFftwRaw   <Float64>(context.F64, context.zeroInfo, kernelInfo, dataOrg);

		case AnalysisType::PotentialIpps64:
			return potential::impl::PotentialFftwSmooth<Float64>(context.F64, context.zeroInfo, kernelInfo, dataOrg);

		default:
			throwIllegalAbstract(MG_POS, "Potential");
	}
		
	return true;
}

// Explicit template instantiations for IppsArray and AddConvolutionKernel.
template class IppsArray<Float32>;
template class IppsArray<Float64>;

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
