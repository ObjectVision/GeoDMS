// Copyright (C) 1998-2025 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////
// Potential.h
// Defines data structures and interfaces for various "Potential" / convolution / proximity
// style analyses over 2D grids. Abstractions provide:
//  - Kernel preparation (weight / mask pre-processing)
//  - Memory buffers for IPP(S) accelerated convolution variants (packed / raw)
//  - Context objects for float32 / float64 processing
//  - Support for slower fallback and proximity calculation
// The design separates kernel (immutable across tiles) from per-tile processing
// buffers (potential_context). Weight buffers are prepared per distinct
// column (padding) requirement to avoid recomputation.

#if defined(_MSC_VER)
#pragma once
#endif

#if !defined(__GEO_POTENTIAL_H)
#define  __GEO_POTENTIAL_H

#include "GeoBase.h"
#include "mem/Grid.h"
#include "IppBase.h"

//#define DMS_POTENTIAL_I16
const bool MG_DEBUG_POTENTIAL = false; // Enable extra debug logging when needed.

// *****************************************************************************
//											INTERFACE
// *****************************************************************************

using SideSize = UInt32;  // Represents a 1D size (e.g. number of columns) used as map key.
using TileSize = SizeT;   // Represents buffer element counts (potentially large).

// Enumeration of analysis implementation variants.
// Naming:
//  - Ipps64 / Packed -> IPP Signal Processing optimized versions for Float64 / packed layout
//  - Raw -> un-packed intermediate handling (explicit padding)
//  - Proximity -> distance/proximity specific computation (non-convolution path)
//  - PotentialSlow -> reference / fallback (no IPP)
//  - PotentialIppi -> (conditional) IPP Image Processing route
// PotentialDefault resolves to fastest available depending on build defines.
enum class AnalysisType {
	PotentialIpps64 = 0, 
	PotentialRawIpps64 = 1, 
	PotentialIppsPacked = 2, 
	PotentialRawIppsPacked = 3, 
	Proximity = 4, 
#if defined(DMS_USE_INTEL_IPPI)
	PotentialIppi = 5,
#endif //defined(DMS_USE_INTEL_IPPI)
	PotentialSlow = 6,

#if defined(DMS_USE_INTEL_IPPS)
	PotentialDefault = PotentialIpps64
#else
	AT_PotentialDefault = PotentialSlow
#endif
};

// *****************************************************************************
//	INTERFACE FUNCTIONS: Potential
// *****************************************************************************
// Exposed C-ABI wrappers (exported when building MDL) for external invocation.
// Each performs a "Potential" style convolution / accumulation using provided
// data (input), output (destination), and weight (kernel) grids. Returns success.
#if defined(MDL_EXPORTS)

extern "C" {

MDL_CALL bool DMS_CONV MDL_Potential32(AnalysisType at,
	const TCFloat32Grid& data, 
	const TFloat32Grid & output,
	const TCFloat32Grid& weight
);

MDL_CALL bool DMS_CONV MDL_Potential64(AnalysisType at,
	const TCFloat64Grid& data, 
	const TFloat64Grid & output,
	const TCFloat64Grid& weight
);

} // extern "C"

#endif

//================================================== IppsArray
// Small RAII wrapper for aligned (implementation-dependent) dynamically allocated
// linear buffers used by IPP code paths.
// Characteristics:
//  - Move-only semantic (copy ctor steals pointer; no explicit copy assignment).
//  - reserve() allocates (or reuses) capacity >= requested.
//  - clean() frees memory (implementation in .cpp).
//  - capacity() returns number of elements (not bytes).
// Thread-safety: Not thread-safe; intended for per-thread or single-owner use.
template <typename A>
struct IppsArray
{
	IppsArray() = default;

	IppsArray(TileSize nrElem)
	{
		reserve(nrElem);
	}
	~IppsArray()
	{
		clean();
	}

	// Move constructor
	IppsArray(IppsArray&& rhs) noexcept
		:	m_Data(rhs.m_Data)
		,	m_Capacity(rhs.m_Capacity)
	{
		rhs.m_Data = nullptr;
		rhs.m_Capacity = 0;
	}
	IppsArray(const IppsArray& rhs) = delete; // No copy ctor.

	// Allocate (or grow) buffer to hold at least nrElem elements.
	void reserve(TileSize nrElem);

	// Move assignment.
	void operator = (IppsArray&& rhs) noexcept
	{
		std::swap(m_Data, rhs.m_Data);
		std::swap(m_Capacity, rhs.m_Capacity);
	}

	A* begin() { return m_Data; }
	const A* begin() const { return m_Data; }

	bool WasInitialized() const { return m_Capacity != 0;  }
	TileSize capacity() const { return m_Capacity;  }

private:
	void clean(); // Implementation must handle aligned deallocation.

	A* m_Data = nullptr;      // Start of aligned element buffer (alignment e.g. 64B for SIMD).
	TileSize m_Capacity = 0;  // Number of elements allocated (not bytes).
};

//====================================================
// Optional Int16 specialized potential computation (disabled by default).
#if defined(DMS_POTENTIAL_I16)
bool Potential(AnalysisType at, 
	const TCInt16Grid& data, 
	const TInt16Grid & output,
	const TCInt16Grid& weight
);
#endif //defined(DMS_POTENTIAL_I16)

// kernel_info encapsulates all kernel-related data needed across tile computations:
//  - orgWeightSize: Original (kx, ky) kernel dimensions.
//  - maxDataSize:   Maximum data tile size encountered / planned.
//  - maxColvolvedSize: Derived size for overlapping output region
//        = maxDataSize + orgWeightSize - (1,1)
//  - weightBuffers32 / weightBuffers64: Pre-expanded weight buffers keyed by padding
//        (paddingSize == number of data columns); formula documented inline below.
//  - orgWeightGrid: Stored original weight grid for slow / proximity algorithms.
//  - weightShadowTile: Reference to weight tile metadata (tiling system).
// Buffer sizing formulas (comments from code):
//  For weight buffer (packed ipps):
//    size = (nx + kx - 1)*(ky - 1) + kx = kx*ky + (nx - 1)*(ky - 1)
//  For paddedInput (data):
//    size = (nx + kx - 1)*(ny - 1) + nx = nx*ny + (kx - 1)*(ny - 1)
//  For overlappingOutput:
//    size = (nx + kx - 1)*(ny + ky - 1)
struct kernel_info
{
	UPoint orgWeightSize, maxDataSize, maxColvolvedSize;

	// Weight buffers reused across tiles (avoid recomputation).
	std::map<SideSize, IppsArray<Float32>> weightBuffers32; // Float32 kernel expansions.
	std::map<SideSize, IppsArray<Float64>> weightBuffers64; // Float64 kernel expansions.

	std::any orgWeightGrid;            // Holds UGrid<const T> (erased); used by slow/proximity paths.
	TileCRef weightShadowTile;         // Original tile reference for weight grid.

	// Generic accessor (const) - specialization below.
	template <typename T> auto weightBuffer(SideSize paddingSize) const;
	template <> auto weightBuffer<Float32>(SideSize paddingSize) const { return &weightBuffers32.at(paddingSize); }
	template <> auto weightBuffer<Float64>(SideSize paddingSize) const { return &weightBuffers64.at(paddingSize); }

	// Generic accessor (mutable) - inserts on demand.
	template <typename T> auto weightBuffer(SideSize paddingSize);
	template <> auto weightBuffer<Float32>(SideSize paddingSize) { return &weightBuffers32[paddingSize]; }
	template <> auto weightBuffer<Float64>(SideSize paddingSize) { return &weightBuffers64[paddingSize]; }

#if defined(DMS_POTENTIAL_I16)
	IppsArray<Int16> weightBufferI16;      // Single Int16 kernel buffer (no map variant).
	template <> auto weightBuffer<Int16>() { return &weightBufferI16; }
	template <> auto weightBuffer<Int16>() const { return &weightBufferI16; }
#endif //defined(DMS_POTENTIAL_I16)
};

// PrepareConvolutionKernel:
//  - Captures original weight size & shadow tile
//  - Records maximum data size (for later buffer allocations)
//  - Pre-computes max overlapped convolution size (used by algorithms)
//  - Stores original weight grid into std::any for algorithms that need original layout
//  - Does NOT allocate expanded weight buffers (lazy: via AddConvolutionKernel).
template < typename T>
MDL_CALL kernel_info PrepareConvolutionKernel(AnalysisType at, TileCRef weightShadowTile, const UGrid<const T>& weightOrg, UPoint maxDataTileSize)
{
	DBG_START("PrepareConvolutionKernel", "Ipps", MG_DEBUG_POTENTIAL);

	//	dms_assert(dataOrg.GetSize() == outputOrg.GetSize());
	kernel_info result;
	result.maxDataSize = maxDataTileSize;
	result.orgWeightSize = weightOrg.GetSize();
	if (Cardinality(maxDataTileSize))
	{
		result.maxColvolvedSize = maxDataTileSize + result.orgWeightSize - UPoint(1, 1);

		dms_assert(maxDataTileSize.Col());
		dms_assert(weightOrg.GetSize().Row());
	}
	result.weightShadowTile = weightShadowTile;
	result.orgWeightGrid = weightOrg;
	return result;
}

// AddConvolutionKernel:
//  - Ensures (or creates) expanded weight buffer for a given data column count (padding).
//  - Implementation located in .cpp; templated on kernel scalar T.
template < typename T>
MDL_CALL void AddConvolutionKernel(kernel_info& self, AnalysisType at, SideSize nrDataCols);

// potential_context:
//  - Per-tile (or per-execution) buffers needed for convolution runtime.
//  - paddedInput: Input data transformed with horizontal padding & vertical replication scheme
//                 to match IPP packed convolution layout.
//  - overlappingOutput: Temporary buffer holding the full overlapped convolution result
//                       (before trimming to actual output region).
//  - ippsBuffer: Workspace required by specific IPP convolution kernels (library provided size).
//  - WasInitialized(): indicates that required buffers have been allocated.
template <typename A>
struct potential_context
{
	potential_context() = default;
	potential_context(potential_context&&) = default;

	IppsArray<A> paddedInput;       // size: (nx+kx-1)*(ny-1)+nx = nx*ny + (kx-1)*(ny-1); not used for PotentialSlow and Proximity
	IppsArray<A> overlappingOutput; // size: (nx+kx-1)*(ny+ky-1)
	IppsArray<UInt8> ippsBuffer;    // Auxiliary algorithm-specific scratch space.

	bool WasInitialized() const { return overlappingOutput.WasInitialized(); }
};

// potential_contexts:
//  - Holds both float32 and float64 contexts (lazily initialized).
//  - Copying a non-empty contexts object is explicitly disallowed (throws).
//  - zeroInfo: Tracks zero-value / normalization metadata (usage defined in .cpp).
struct potential_contexts
{
	potential_contexts() = default;
	potential_contexts(potential_contexts&&) = default;

	potential_contexts(potential_contexts const& rhs)
	{
		if (rhs.WasInitialized())
			throwIllegalAbstract(MG_POS, "potential_contexts::Copy Constructor with non-empty right hand side");
	}

	bool WasInitialized() const { return F32.WasInitialized() || F64.WasInitialized(); }

	potential_context<Float32> F32;
	potential_context<Float64> F64;

	// Type-based accessor (const) for active context.
	template <typename> auto F() const;
	template <> auto F<Float32>() const { return &F32; };
	template <> auto F<Float64>() const { return &F64; };

	UPoint zeroInfo;
};

// Potential (overloads):
//  - Execute potential analysis for provided data grid using prepared kernel info
//    and optionally reusing / allocating buffers in context.
//  - Returns success/failure.
bool Potential(AnalysisType at, potential_contexts& context, const kernel_info& weightBuffer, const UCFloat32Grid& data);
bool Potential(AnalysisType at, potential_contexts& context, const kernel_info& weightBuffer, const UCFloat64Grid& data);

// *****************************************************************************
// End of interface
// *****************************************************************************

#endif //!defined(__GEO_POTENTIAL_H)
