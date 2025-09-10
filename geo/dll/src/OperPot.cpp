// Copyright (C) 1998-2025 Object Vision b.v.
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////
//
// File: OperPot.cpp
// Purpose:
//   Implements tile-based "potential" (convolution-like) computations and
//   proximity analysis over gridded data. The implementation supports multiple
//   algorithm variants (IPP accelerated, packed, raw, slow fallback, proximity)
//   selected via AnalysisType. It performs kernel preparation once, then
//   processes each data tile in parallel, accumulating overlapping contributions
//   into result tiles with strict ordering to guarantee deterministic results.
//
// High-Level Algorithm (Pseudocode Plan):
// -----------------------------------------------------------------------------
// 1. Validate exactly 2 arguments: dataGrid (values), weightGrid (kernel).
// 2. Create / reuse a cache result data item with same domain as dataGrid.
// 3. Lock inputs for read, create write handle for result (zero-initialized).
// 4. Determine full domain rectangle and weight rectangle (kernel footprint).
// 5. Gather tile count (te) and max tile size for buffer planning.
// 6. Prepare kernel_info:
//      - Stores original kernel size
//      - Computes maximum convolution output size for overlap
//      - Prepares expanded kernel buffers (possibly one per padding width)
// 7. For every result tile, register its column count into kernel_info
//    (AddKernel) so kernel expansion can be specialized per width.
// 8. Launch parallel processing across data tiles:
//      For each data tile ti:
//        a. Get tile rectangle (dataTileRect).
//        b. Compute overlapTileRect = dataTileRect + weightRect - (1,1).
//           This is the bounding rectangle in result space that receives
//           contributions from this data tile's convolution.
//        c. Acquire read lock on tile ti from dataGrid.
//        d. Build a UGrid interface wrapper; if undefined values present,
//           duplicate tile and zero-out undefined cells.
//        e. Call Potential(...) which fills workingBuffer with
//           overlappedResult for (overlapTileRect).
//        f. For every result tile tr (in increasing order):
//             - Wait until resTileAddition[tr].m_NrAddedTiles >= ti
//               (serialized accumulation order).
//             - If tr intersects overlapTileRect: Store(...)
//               • If first contribution: copy / assign
//               • Else: additive accumulation (or max for Proximity)
//             - Increment m_NrAddedTiles and notify waiters.
// 9. Commit result write handle.
// 10. Instantiation block registers multiple DirectPotentialOperator<T>
//     variants in anonymous namespace to bind operator groups to backends.
//
// Concurrency / Synchronization:
// -----------------------------------------------------------------------------
// - parallel_tileloop: parallel driver over input (data) tiles.
// - Each worker thread owns a thread-local potential_contexts via
//   concurrency::combinable to avoid repeated allocations.
// - Accumulation into result tiles must be deterministic. Therefore,
//   although tiles are processed in parallel, writing into each result tile
//   is serialized per contributing data tile index (ti order) using
//   result_tile_protector (mutex + condition_variable + monotonic counter).
//
// Memory & Buffer Strategy (Kernel):
// -----------------------------------------------------------------------------
// - kernel_info precomputes size relationships and stores alternative weight
//   layouts (e.g., packed IPP format) keyed by data column padding size.
// - The overlapping output region for each data tile is computed so only
//   necessary result portions are generated and merged.
//
// Incremental Merge Semantics:
// -----------------------------------------------------------------------------
// - incremental = (result tile already had contributions).
// - Potential / Proximity modes differ:
//     • Potential modes: additive accumulation (sum).
//     • Proximity: max accumulation.
// - First writer copies result in directly without clearing the tile, because
//   DataWriteHandle was initialized with zero (write_only_mustzero).
//
// Edge Cases / Guards:
// -----------------------------------------------------------------------------
// - Empty weightRect -> all result tiles become zero-initialized (no work).
// - Empty dataTileRect -> still advance synchronization counters.
// - Undefined values replaced by zero before convolution (ensures neutrality).
//
// Performance Notes:
// -----------------------------------------------------------------------------
// - Avoids locking result tiles in inner loop (commented WritableTileLock)
//   because Store works with direct data handles (already write-managed).
// - Use of combinable reduces allocation churn for intermediate buffers.
// - Accumulation order fixed -> reproducible floating point sums across runs.
//
// Potential Improvements (Not Implemented):
// -----------------------------------------------------------------------------
// - Skip iterating all result tiles for non-overlapping overlapTileRect
//   by spatial indexing.
// - Replace per-tile condition_variable with per data tile barrier & per
//   result tile atomic stage counters (could lower contention).
// - Vectorize undefined value cleaning if large proportion appear.
//
// -----------------------------------------------------------------------------
// End of design / plan. Implementation follows.
// -----------------------------------------------------------------------------

#include "GeoPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif

#include "dbg/DebugContext.h"
#include "geo/Conversions.h"
#include "mci/CompositeCast.h"
#include "mem/RectCopy.h"
#include "ptr/OwningPtrReservedArray.h"
#include "ser/AsString.h"
#include "set/VectorFunc.h"
#include "xct/DmsException.h"

#include "LockLevels.h"

#include "DataCheckMode.h"
#include "UnitCreators.h"
#include "ParallelTiles.h"

#include "Potential.h"
#include "AggrFuncNum.h"

#include <minmax.h>

// *****************************************************************************
//                                    Potential
// *****************************************************************************

#include "Param.h"
#include "DataItemClass.h"

//------------------------------------------------------------------------------
// AbstrDirectPotentialOperator
// Core abstract base providing shared CreateResult logic, parallel orchestration
// and synchronization, while delegating numeric backend specifics to virtuals.
//------------------------------------------------------------------------------
struct AbstrDirectPotentialOperator : public BinaryOperator
{
    // Per result-tile synchronization state.
    // Ensures contributions are merged in deterministic order (ascending ti).
    struct result_tile_protector
    {
        std::mutex              m_AddingNow;        // Guards tile accumulation state
        std::condition_variable m_AddingProceeded;  // Signals next expected input tile done
        tile_id                 m_NrAddedTiles = 0; // Number of processed contributing data tiles
        bool                    m_WasInitialized = false; // True after first write into tile
    };

    AbstrDirectPotentialOperator(AbstrOperGroup* gr, AnalysisType at, const Class* arrayType)
        : BinaryOperator(gr, arrayType, arrayType, arrayType)
        , m_AnalysisType(at)
    {}

    // Override Operator
    bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
    {
        dms_assert(args.size() == 2);

        const AbstrDataItem* dataGridA = debug_cast<const AbstrDataItem*>(args[0]);
        dms_assert(dataGridA);

        const AbstrUnit* resDomainUnit = dataGridA->GetAbstrDomainUnit();

        // Lazy creation of result cache item (same domain, value unit derived via mul2_unit_creator)
        if (!resultHolder)
            resultHolder = CreateCacheDataItem(resDomainUnit, mul2_unit_creator(GetGroup(), args));

        if (mustCalc)
        {
            const AbstrDataItem* weightGridA = debug_cast<const AbstrDataItem*>(args[1]);
            dms_assert(weightGridA);
            const AbstrUnit* weightDomain = weightGridA->GetAbstrDomainUnit();

            AbstrDataItem* res = debug_cast<AbstrDataItem*>(resultHolder.GetNew());

            // Ensure input lifetime across parallel section
            DataReadLock arg1Lock(dataGridA);
            DataReadLock arg2Lock(weightGridA);

            IRect     dataRect   = resDomainUnit->GetRangeAsIRect();
            IRect     weightRect = weightDomain->GetRangeAsIRect();
            SharedStr strWeightRect = AsString(weightRect);

            reportF(SeverityTypeID::ST_MinorTrace, "%s %s", GetGroup()->GetNameStr(), strWeightRect.c_str());

            // Flags for undefined handling
            ArgFlags af = dataGridA->HasUndefinedValues() ? AF1_HASUNDEFINED : ArgFlags();

            // Sanity checks on rectangle ordering
            dms_assert(dataRect.first.first  <= dataRect.second.first);
            dms_assert(dataRect.first.second <= dataRect.second.second);
            dms_assert(weightRect.first.first <= weightRect.second.first);
            dms_assert(weightRect.first.second <= weightRect.second.second);

            // Acquire write handle; mode ensures zero-initialization
            DataWriteHandle resDataHandle(res, dms_rw_mode::write_only_mustzero);
            tile_id te = resDomainUnit->GetNrTiles();

            BitVector<1> wasInitialized(te, false MG_DEBUG_ALLOCATOR_SRC("BitVector for DirectPotential"));

            // Track maximum tile size for buffer preallocation / kernel planning
            UPoint maxDataTileSize{ 0, 0 };
            for (tile_id ti = 0; ti != te; ++ti)
                MakeUpperBound(maxDataTileSize, Convert<Point<UInt32>>(Size(resDomainUnit->GetTileRangeAsIRect(ti))));

            OwningPtrSizedArray<result_tile_protector> resTileAddition(te, value_construct MG_DEBUG_ALLOCATOR_SRC("OperPot: resTileAddition"));

            // Precompute kernel info once
            auto kernelInfo = CreateKernelInfo(weightGridA, Size(weightRect), maxDataTileSize);
            // Register per-column kernel expansions (if backend uses them)
            for (tile_id ti = 0; ti != te; ++ti)
                AddKernel(kernelInfo, Size(resDomainUnit->GetTileRangeAsIRect(ti)).Col());

            // Thread-local working buffers to avoid reallocation
            concurrency::combinable< potential_contexts > workingBuffers;

            if (weightRect.empty())
            {
                // No kernel: result stays zero (already zeroed by write_only_mustzero)
                parallel_tileloop(te, [&](tile_id ti)->void
                    {
                        WritableTileLock resTileLock(resDataHandle.get(), ti, dms_rw_mode::write_only_mustzero);
                    }
                );
            }
            else
            {
                // Main parallel convolution / accumulation loop
                parallel_tileloop(te, [&, resDataObj = resDataHandle.get()](tile_id ti)->void
                    {
                        potential_contexts& workingBuffer = workingBuffers.local();

                        IRect dataTileRect = resDomainUnit->GetTileRangeAsIRect(ti);
                        if (!dataTileRect.empty())
                        {
                            // Compute superset of result region influenced by this data tile
                            IRect overlapTileRect = dataTileRect + weightRect;
                            overlapTileRect.second -= IPoint(1, 1); // Adjust: (inclusive / exclusive reconciliation)

                            // Read data tile for computation
                            ReadableTileLock readLock(dataGridA->GetCurrRefObj(), ti);
                            Calculate(workingBuffer, kernelInfo, dataGridA, af, ti, Size(dataTileRect), overlapTileRect);

                            // Deterministic accumulation across all result tiles
                            for (tile_id tr = 0; tr != te; ++tr)
                            {
                                IRect resTileRect = resDomainUnit->GetTileRangeAsIRect(tr);
                                auto& resTileInfo = resTileAddition[tr];

                                std::unique_lock resTileAdditionLock(resTileInfo.m_AddingNow);
                                resTileInfo.m_AddingProceeded.wait(resTileAdditionLock, [tr, ti, &resTileInfo]() {
                                    return resTileInfo.m_NrAddedTiles >= ti;
                                });

                                if (IsIntersecting(resTileRect, overlapTileRect))
                                {
                                    // Accumulate or initialize tile portion
                                    // (WritableTileLock not strictly necessary if Store obtains correct handles)
                                    // WritableTileLock resTileLock(resDataHandle.get(), tr, resTileInfo.m_WasInitialized ? dms_rw_mode::read_write : dms_rw_mode::write_only_all);
                                    Store(resDataHandle.get(), tr, resTileRect, resTileInfo.m_WasInitialized, overlapTileRect, workingBuffer);
                                    resTileInfo.m_WasInitialized = true;
                                }
                                dms_assert(resTileInfo.m_NrAddedTiles == ti);
                                resTileInfo.m_NrAddedTiles++;
                                resTileInfo.m_AddingProceeded.notify_all();
                            }
                        }
                        else
                        {
                            // Empty data tile still advances counters for ordering
                            for (tile_id tr = 0; tr != te; ++tr)
                            {
                                auto& resTileInfo = resTileAddition[tr];

                                std::unique_lock resTileAdditionLock(resTileInfo.m_AddingNow);
                                resTileInfo.m_AddingProceeded.wait(resTileAdditionLock, [tr, ti, &resTileInfo]() {
                                    return resTileInfo.m_NrAddedTiles >= ti;
                                });

                                dms_assert(resTileInfo.m_NrAddedTiles == ti);
                                resTileInfo.m_NrAddedTiles++;
                                resTileInfo.m_AddingProceeded.notify_all();
                            }
                        }
                    }
                );
            }

            // Publish final result state
            resDataHandle.Commit();
        }
        return true;
    }

    // Backend hooks (implemented by templated subclass):
    virtual kernel_info CreateKernelInfo(const AbstrDataItem* weightGridA, UPoint weightSize, UPoint maxDataTileSize) const = 0;
    virtual void        AddKernel     (kernel_info& self, SideSize nrDataCols) const = 0;
    virtual void        Calculate     (potential_contexts& workingBuffer, const kernel_info& info, const AbstrDataItem* dataGridA,
                                       ArgFlags af, tile_id t, UPoint dataTileSize, IRect overlapTileRect) const = 0;
    virtual void        Store         (AbstrDataObject* res, tile_id tr, IRect resTileRect, bool doInit,
                                       IRect overlapTileRect, const potential_contexts& data) const = 0;
protected:
    AnalysisType m_AnalysisType;
};

//------------------------------------------------------------------------------
// DirectPotentialOperator<T>
// Concrete templated implementation binding numeric type and delegating
// to generic Potential() + kernel preparation / storage logic.
//------------------------------------------------------------------------------
template <class T>
class DirectPotentialOperator : public AbstrDirectPotentialOperator
{
    typedef DataArray<T> Arg1Type;   // Values grid (data)
    typedef DataArray<T> Arg2Type;   // Kernel / weight grid
    typedef DataArray<T> ResultType; // Output grid

public:
    DirectPotentialOperator(AbstrOperGroup* gr, AnalysisType at)
        : AbstrDirectPotentialOperator(gr, at, ResultType::GetStaticClass())
    {}

    // Optional scratch buffers (currently not extensively used)
    struct ResBuffer {
        std::vector<T>           resultRectData;
        potential_context<T>     convolutionBuffer;
    };

    // Create and initialize kernel_info (called once before parallel phase)
    kernel_info CreateKernelInfo(const AbstrDataItem* weightGridA, UPoint weightSize, UPoint maxDataTileSize) const override
    {
        const Arg2Type* weightGrid = const_array_cast<T>(weightGridA);
        dms_assert(weightGrid);

        auto weightShadowTile = weightGrid->GetDataRead(); // Single-tile assumption for kernel
        return PrepareConvolutionKernel<T>(m_AnalysisType,
                                           weightShadowTile.m_TileHolder,
                                           UGrid<const T>(weightSize, weightShadowTile.begin()),
                                           maxDataTileSize);
    }

    // Allow backend to append specialized kernel representations per data column width
    void AddKernel(kernel_info& self, SideSize nrDataCols) const override
    {
        AddConvolutionKernel<T>(self, m_AnalysisType, nrDataCols);
    }

    // Perform per-tile convolution / potential computation, producing overlapping output
    void Calculate(potential_contexts& workingBuffer, const kernel_info& info, const AbstrDataItem* dataGridA,
                   ArgFlags af, tile_id t, UPoint dataTileSize, IRect resultRect) const override
    {
        const Arg1Type* dataGrid = const_array_cast<T>(dataGridA);
        dms_assert(dataGrid);

        auto dataGridData = dataGrid->GetDataRead(t);

        SizeT dataSize = Cardinality(dataTileSize);
        dms_assert(dataGridData.size() == dataSize);

        // Construct UGrid view over raw data
        UGrid<const T> data(dataTileSize, &*dataGridData.begin());

        if (af & AF1_HASUNDEFINED)
        {
            // Duplicate and cleanse undefined values -> zero (neutral for sum)
            typename sequence_traits<T>::container_type
                definedDataGridCopy(dataGridData.begin(), dataGridData.end() MG_DEBUG_ALLOCATOR_SRC("DirectPotentialOperator.definedDataGridCopy"));
            auto
                ddgcI = begin_ptr(definedDataGridCopy),
                ddgcE = end_ptr(definedDataGridCopy);

            data.SetDataPtr(ddgcI);
            std::for_each(ddgcI, ddgcE, [](T& v) { if (!IsDefined(v)) v = T(); });

            // Execute potential using cleansed buffer
            Potential(m_AnalysisType, workingBuffer, info, data);
        }
        else
        {
            // Direct potential
            Potential(m_AnalysisType, workingBuffer, info, data);
        }
    }

    // Merge the overlapping output region into the target result tile
    void Store(AbstrDataObject* resObj, tile_id tr, IRect resTileRect, bool incremental,
               IRect overlapTileRect, const potential_contexts& workingBuffer) const override
    {
        switch (m_AnalysisType) {
            case AnalysisType::PotentialIpps64:
            case AnalysisType::PotentialRawIpps64:
                // These backends compute Float64 outputs even if T != Float64
                StoreImpl<Float64>(resObj, tr, resTileRect, incremental, overlapTileRect, workingBuffer.F64);
                break;
            case AnalysisType::PotentialIppsPacked:
            case AnalysisType::PotentialRawIppsPacked:
            case AnalysisType::PotentialSlow:
            case AnalysisType::Proximity:
                // Native type result accumulation
                StoreImpl<T>(resObj, tr, resTileRect, incremental, overlapTileRect, *workingBuffer.F<T>());
                break;
        }
    }

    // Generic storage merger: either assign / copy or accumulate / max depending on mode
    template <typename A>
    void StoreImpl(AbstrDataObject* resObj, tile_id tr, IRect resTileRect, bool incremental,
                   IRect overlapTileRect, const potential_context<A>& workingBuffer) const
    {
        ResultType* result = mutable_array_cast<T>(resObj);

        // Source: contiguous overlapping output region computed in Calculate
        UGrid<const A> overlappedResult(Size(overlapTileRect), workingBuffer.overlappingOutput.begin());

        // Destination: entire result tile
        UGrid<T> resultTile(Size(resTileRect), result->GetDataWrite(tr, dms_rw_mode::read_write).begin());

        MG_CHECK(resultTile.GetDataPtr()); // Guarantee by underlying tile system

        // Compute offset of target rectangle inside overlapped data
        if (incremental)
        {
            if (m_AnalysisType == AnalysisType::Proximity)
                // For Proximity analysis take maximum influence
                RectOper<T, A, SideSize, row_assigner<unary_assign_max<T, A>>>(resultTile, overlappedResult, resTileRect.first - overlapTileRect.first);
            else
                // Potential modes sum contributions
                RectOper<T, A, SideSize, row_assigner<unary_assign_add<T, A>>>(resultTile, overlappedResult, resTileRect.first - overlapTileRect.first);
        }
        else
        {
            // First writer: direct copy (tile already zeroed). Optionally we could
            // zero non-overlapping area if partial, but commented out for perf.
            // if (!IsIncluding(overlapTileRect, resTileRect))
            //     fast_zero(resultTile.begin(), resultTile.end());
            RectCopy(resultTile, overlappedResult, resTileRect.first - overlapTileRect.first);
        }
    }
};

// *****************************************************************************
//                                   INSTANTIATION
// Register concrete operator instances (anonymous namespace prevents ODR issues)
// *****************************************************************************

#include "DataArray.h"

namespace
{
    // Default group (auto-selects fastest feasible backend)
    CommonOperGroup potentialDefault("potential", oper_policy::better_not_in_meta_scripting);

#if defined(DMS_USE_INTEL_IPPS)
    CommonOperGroup potentialIpps64("potentialIpps64", oper_policy::better_not_in_meta_scripting);
#endif // defined(DMS_USE_INTEL_IPPS)

#if defined(DMS_USE_INTEL_IPPI)
    CommonOperGroup potentialIppi32("potentialIppi32", oper_policy::better_not_in_meta_scripting);
#endif // defined(DMS_USE_INTEL_IPPI)

    CommonOperGroup potentialRaw64    ("potentialRaw64",    oper_policy::better_not_in_meta_scripting);
    CommonOperGroup potentialSlow     ("potentialSlow",     oper_policy::better_not_in_meta_scripting);
    CommonOperGroup potentialPacked   ("potentialPacked",   oper_policy::better_not_in_meta_scripting);
    CommonOperGroup potentialRawPacked("potentialRawPacked",oper_policy::better_not_in_meta_scripting);

    // Float32 variants
    DirectPotentialOperator<Float32> potDF32Def  (&potentialDefault , AnalysisType::PotentialDefault);
    DirectPotentialOperator<Float32> potDF32Ipps (&potentialIpps64  , AnalysisType::PotentialIpps64);
    DirectPotentialOperator<Float32> potDF32IppsR(&potentialRaw64   , AnalysisType::PotentialRawIpps64);
    DirectPotentialOperator<Float32> potDF32Slow (&potentialSlow    , AnalysisType::PotentialSlow);
    DirectPotentialOperator<Float32> potDF32P    (&potentialPacked  , AnalysisType::PotentialIppsPacked);
    DirectPotentialOperator<Float32> potDF32RP   (&potentialRawPacked, AnalysisType::PotentialRawIppsPacked);

#if defined(DMS_USE_INTEL_IPPI)
    DirectPotentialOperator<Float32> potDF32Ippi (&potentialIppi32  , AnalysisType::PotentialIppi);
#endif // defined(DMS_USE_INTEL_IPPI)

    // Float64 variants
    DirectPotentialOperator<Float64> potDF64Def  (&potentialDefault , AnalysisType::PotentialDefault);
    DirectPotentialOperator<Float64> potDF64Ipps (&potentialIpps64  , AnalysisType::PotentialIpps64);
    DirectPotentialOperator<Float64> potDF64IppsR(&potentialRaw64   , AnalysisType::PotentialRawIpps64);
    DirectPotentialOperator<Float64> potDF64Slow (&potentialSlow    , AnalysisType::PotentialSlow);

#if defined(DMS_POTENTIAL_I16)
    // Int16 variants (mapped onto packed / raw / ipps64 backends)
    DirectPotentialOperator<Int16>   potDS16Def  (&potentialDefault , AnalysisType::PotentialRawIppsPacked);
    DirectPotentialOperator<Int16>   potDS16RP   (&potentialRawPacked, AnalysisType::PotentialRawIppsPacked);
    DirectPotentialOperator<Int16>   potDS16Ipps (&potentialIpps64  , AnalysisType::PotentialIpps64);
    DirectPotentialOperator<Int16>   potDS16IppsR(&potentialRaw64   , AnalysisType::PotentialRawIpps64);
    DirectPotentialOperator<Int16>   potDS16Slow (&potentialSlow    , AnalysisType::PotentialSlow);
#if defined(DMS_USE_INTEL_IPPI)
    DirectPotentialOperator<Int16>   potDS16Ippi (&potentialIppi32  , AnalysisType::PotentialIppi);
#endif // defined(DMS_USE_INTEL_IPPI)
#endif // defined(DMS_POTENTIAL_I16)

    // Proximity (max-based accumulation) group
    CommonOperGroup proximity("proximity", oper_policy::better_not_in_meta_scripting);
    DirectPotentialOperator<Float32> proxDF32(&proximity, AnalysisType::Proximity);
    DirectPotentialOperator<Float64> proxDF64(&proximity, AnalysisType::Proximity);
}

/******************************************************************************/
