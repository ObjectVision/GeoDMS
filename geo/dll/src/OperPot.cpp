#include "GeoPCH.h"
#pragma hdrstop

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
//									Potential
// *****************************************************************************

#include "Param.h"
#include "DataItemClass.h"

struct AbstrDirectPotentialOperator : public BinaryOperator
{
	struct result_tile_protector
	{
		std::mutex m_AddingNow;
		std::condition_variable m_AddingProceeded;
		tile_id    m_NrAddedTiles = 0;
		bool       m_WasInitialized = false;
	};

	AbstrDirectPotentialOperator(AbstrOperGroup* gr, AnalysisType at, const Class* arrayType)
		:	BinaryOperator(gr, arrayType, arrayType, arrayType) 
		, 	m_AnalysisType(at) 
	{}
//	// Override Operator
	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		dms_assert(args.size() == 2);

		const AbstrDataItem* dataGridA  = debug_cast<const AbstrDataItem*>(args[0]);
		dms_assert(dataGridA);

		const AbstrUnit* resDomainUnit = dataGridA->GetAbstrDomainUnit();

		if (!resultHolder)
			resultHolder = CreateCacheDataItem(resDomainUnit, mul2_unit_creator(GetGroup(), args) );

		if (mustCalc)
		{
			const AbstrDataItem* weightGridA = debug_cast<const AbstrDataItem*>(args[1]);
			dms_assert(weightGridA);
			const AbstrUnit* weightDomain = weightGridA->GetAbstrDomainUnit();

			AbstrDataItem* res = debug_cast<AbstrDataItem*>(resultHolder.GetNew());

			DataReadLock arg1Lock(dataGridA);
			DataReadLock arg2Lock(weightGridA);

			IRect         dataRect = resDomainUnit->GetRangeAsIRect();
			IRect         weightRect = weightDomain->GetRangeAsIRect();
			SharedStr     strWeightRect = AsString(weightRect);
			CDebugContextHandle dch1("OperPot", strWeightRect.c_str(), true);

			ArgFlags af = dataGridA->HasUndefinedValues() ? AF1_HASUNDEFINED : ArgFlags();

			dms_assert(dataRect.first.first  <= dataRect.second.first);
			dms_assert(dataRect.first.second <= dataRect.second.second);
			dms_assert(weightRect.first.first <= weightRect.second.first);
			dms_assert(weightRect.first.second <= weightRect.second.second);

			DataWriteHandle resDataHandle(res, dms_rw_mode::write_only_mustzero);
			tile_id te = resDomainUnit->GetNrTiles();

			BitVector<1> wasInitialized(te, false);

			UPoint maxDataTileSize{ 0, 0 };

			for (tile_id ti = 0; ti != te; ++ti)
				MakeUpperBound(maxDataTileSize, Convert<Point<UInt32>>(Size(resDomainUnit->GetTileRangeAsIRect(ti))));

			OwningPtrSizedArray<result_tile_protector> resTileAddition(te MG_DEBUG_ALLOCATOR_SRC("OperPot: resTileAddition"));

			auto kernelInfo = CreateKernelInfo(weightGridA, Size(weightRect), maxDataTileSize);
			for (tile_id ti = 0; ti != te; ++ti)
				AddKernel(kernelInfo, Size(resDomainUnit->GetTileRangeAsIRect(ti)).Col());

			concurrency::combinable< potential_contexts > workingBuffers;

			if (weightRect.empty())
				parallel_tileloop(te, [&](tile_id ti)->void
					{
						WritableTileLock resTileLock(resDataHandle.get(), ti, dms_rw_mode::write_only_mustzero);
					}
				);
			else
			{
				parallel_tileloop(te, [&, resDataObj = resDataHandle.get()](tile_id ti)->void
					{
						potential_contexts& workingBuffer = workingBuffers.local();

						IRect dataTileRect = resDomainUnit->GetTileRangeAsIRect(ti);
						if (!dataTileRect.empty())
						{
							IRect overlapTileRect = dataTileRect + weightRect; overlapTileRect.second -= IPoint(1, 1); // encloses tile rect ti

							ReadableTileLock readLock(dataGridA->GetCurrRefObj(), ti);
							Calculate(workingBuffer, kernelInfo, dataGridA, af, ti, Size(dataTileRect), overlapTileRect);

							// do this in synchronized order
							for (tile_id tr = 0; tr != te; ++tr)
							{
								IRect resTileRect = resDomainUnit->GetTileRangeAsIRect(tr);
								auto& resTileInfo = resTileAddition[tr];

								std::unique_lock resTileAdditionLock(resTileInfo.m_AddingNow);
								resTileInfo.m_AddingProceeded.wait(resTileAdditionLock, [tr, ti, &resTileInfo]() { return resTileInfo.m_NrAddedTiles >= ti;  });

								if (IsIntersecting(resTileRect, overlapTileRect))
								{
//									WritableTileLock resTileLock(resDataHandle.get(), tr, resTileInfo.m_WasInitialized ? dms_rw_mode::read_write : dms_rw_mode::write_only_all);
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
							for (tile_id tr = 0; tr != te; ++tr)
							{
								auto& resTileInfo = resTileAddition[tr];

								std::unique_lock resTileAdditionLock(resTileInfo.m_AddingNow);
								resTileInfo.m_AddingProceeded.wait(resTileAdditionLock, [tr, ti, &resTileInfo]() { return resTileInfo.m_NrAddedTiles >= ti;  });

								dms_assert(resTileInfo.m_NrAddedTiles == ti);
								resTileInfo.m_NrAddedTiles++;
								resTileInfo.m_AddingProceeded.notify_all();
							}
						}
					}
				);
			}

			resDataHandle.Commit();
		}
		return true;
	}
	virtual kernel_info CreateKernelInfo(const AbstrDataItem* weightGridA, UPoint weightSize, UPoint maxDataTileSize) const = 0;
	virtual void AddKernel(kernel_info& self, SideSize nrDataCols) const = 0;
	virtual void Calculate(potential_contexts& workingBuffer, const kernel_info& info, const AbstrDataItem* dataGridA, ArgFlags af, tile_id t, UPoint dataTileSize, IRect overlapTileRect) const =0;
	virtual void Store(AbstrDataObject* res, tile_id tr, IRect resTileRect, bool doInit, IRect overlapTileRect, const potential_contexts& data) const =0;
protected:
	AnalysisType m_AnalysisType;
};

template <class T>
class DirectPotentialOperator : public AbstrDirectPotentialOperator
{
	typedef DataArray<T> Arg1Type; // ValuesGrid
	typedef DataArray<T> Arg2Type; // Focal point matrix
	typedef DataArray<T> ResultType;    // BasisGrid related VAT-attribute with potential to values T
			
public:
	DirectPotentialOperator(AbstrOperGroup* gr, AnalysisType at)
		:	AbstrDirectPotentialOperator(gr, at, ResultType::GetStaticClass())
	{}

	struct ResBuffer {
		std::vector<T> resultRectData;
		potential_context<T> convolutionBuffer;
	};

	kernel_info CreateKernelInfo(const AbstrDataItem* weightGridA, UPoint weightSize, UPoint maxDataTileSize) const override
	{
		const Arg2Type* weightGrid = const_array_cast<T>(weightGridA);
		dms_assert(weightGrid);

		auto weightShadowTile = weightGrid->GetDataRead();
		return PrepareConvolutionKernel<T>(m_AnalysisType, weightShadowTile.m_TileHolder, UGrid<const T>(weightSize, weightShadowTile.begin()), maxDataTileSize);
	}
	void AddKernel(kernel_info& self, SideSize nrDataCols) const override
	{
		AddConvolutionKernel<T>(self, m_AnalysisType, nrDataCols);
	}
	void Calculate(potential_contexts& workingBuffer, const kernel_info& info, const AbstrDataItem* dataGridA, ArgFlags af, tile_id t, UPoint dataTileSize, IRect resultRect) const override
	{
		const Arg1Type* dataGrid   = const_array_cast<T>(dataGridA  );
		dms_assert(dataGrid);

		auto dataGridData = dataGrid->GetDataRead(t);

		SizeT dataSize = Cardinality(dataTileSize);
		dms_assert(dataGridData.size() == dataSize);

		// prepare interface parameters and compute potential 
		UGrid<const T> data  (dataTileSize, &*dataGridData.begin  ());

		if (af & AF1_HASUNDEFINED) 
		{
			// make copy of dataGrid with Undefined replaced by 0.
			typename sequence_traits<T>::container_type
				definedDataGridCopy(dataGridData.begin(), dataGridData.end());
			auto
				ddgcI = begin_ptr( definedDataGridCopy),
				ddgcE = end_ptr  ( definedDataGridCopy);

			data.SetDataPtr( ddgcI );
			std::for_each(ddgcI, ddgcE, [](T& v){ if(!IsDefined(v)) v = T(); });
	
			// don't remove from if since data points at internals of definedDataGridCopy
			Potential(m_AnalysisType, workingBuffer, info, data);
		}
		else
			Potential(m_AnalysisType, workingBuffer, info, data);
	}

	void Store(AbstrDataObject* resObj, tile_id tr, IRect resTileRect, bool incremental, IRect overlapTileRect, const potential_contexts& workingBuffer) const override
	{
		switch (m_AnalysisType) {
			case AnalysisType::PotentialIpps64:
			case AnalysisType::PotentialRawIpps64:
				StoreImpl<Float64>(resObj, tr, resTileRect, incremental, overlapTileRect, workingBuffer.F64); break;
			case AnalysisType::PotentialIppsPacked:
			case AnalysisType::PotentialRawIppsPacked:
			case AnalysisType::PotentialSlow:
			case AnalysisType::Proximity:
				StoreImpl<T>(resObj, tr, resTileRect, incremental, overlapTileRect, *workingBuffer.F<T>()); break;
		}
	}
	template <typename A>
	void StoreImpl(AbstrDataObject* resObj, tile_id tr, IRect resTileRect, bool incremental, IRect overlapTileRect, const potential_context<A>& workingBuffer) const
	{
		ResultType* result = mutable_array_cast<T>(resObj);
		UGrid<const A> overlappedResult(Size(overlapTileRect), workingBuffer.overlappingOutput.begin());
		UGrid<T> resultTile(Size(resTileRect), result->GetDataWrite(tr, dms_rw_mode::read_write).begin());

		MG_CHECK(resultTile.GetDataPtr()); // DEBUG, guaranteed by WritableTileLock ?

		if (incremental)
		{
			if (m_AnalysisType ==AnalysisType::Proximity)
				RectOper<T, A, SideSize, row_assigner<unary_assign_max<T, A> > >(resultTile, overlappedResult, resTileRect.first - overlapTileRect.first);
			else
				RectOper<T, A, SideSize, row_assigner<unary_assign_add<T, A> > >(resultTile, overlappedResult, resTileRect.first - overlapTileRect.first);
		}
		else
		{
//			if (!IsIncluding(overlapTileRect, resTileRect))
//				fast_zero(resultTile.begin(), resultTile.end());
			RectCopy(resultTile, overlappedResult, resTileRect.first - overlapTileRect.first);
		}
	}
};

// *****************************************************************************
//											INSTANTIATION
// *****************************************************************************

#include "DataArray.h"

namespace
{
	CommonOperGroup potentialDefault  ("potential"         );
#if defined(DMS_USE_INTEL_IPPS)
	CommonOperGroup potentialIpps64("potentialIpps64");
#endif //defined(DMS_USE_INTEL_IPPS)
#if defined(DMS_USE_INTEL_IPPI)
	CommonOperGroup potentialIppi32("potentialIppi32");
#endif //defined(DMS_USE_INTEL_IPPI)
	CommonOperGroup potentialRaw64("potentialRaw64");
	CommonOperGroup potentialSlow("potentialSlow");
	CommonOperGroup potentialPacked("potentialPacked");
	CommonOperGroup potentialRawPacked("potentialRawPacked");

	DirectPotentialOperator<Float32> potDF32Def(&potentialDefault, AnalysisType::PotentialDefault);
	DirectPotentialOperator<Float32> potDF32Ipps(&potentialIpps64, AnalysisType::PotentialIpps64);
	DirectPotentialOperator<Float32> potDF32IppsR(&potentialRaw64, AnalysisType::PotentialRawIpps64);
	DirectPotentialOperator<Float32> potDF32Slow(&potentialSlow, AnalysisType::PotentialSlow);
	DirectPotentialOperator<Float32> potDF32P(&potentialPacked, AnalysisType::PotentialIppsPacked);
	DirectPotentialOperator<Float32> potDF32RP(&potentialRawPacked, AnalysisType::PotentialRawIppsPacked);

#if defined(DMS_USE_INTEL_IPPI)
	DirectPotentialOperator<Float32> potDF32Ippi(&potentialIppi32, AnalysisType::PotentialIppi);
#endif //defined(DMS_USE_INTEL_IPPI)


	DirectPotentialOperator<Float64> potDF64Ipps(&potentialIpps64, AnalysisType::PotentialIpps64);
	DirectPotentialOperator<Float64> potDF64IppsR(&potentialRaw64, AnalysisType::PotentialRawIpps64);
	DirectPotentialOperator<Float64> potDF64Slow(&potentialSlow, AnalysisType::PotentialSlow);

#if defined(DMS_POTENTIAL_I16)

	DirectPotentialOperator<Int16>   potDS16Def(&potentialDefault, AnalysisType::PotentialRawIppsPacked);
	DirectPotentialOperator<Int16>   potDS16RP(&potentialRawPacked, AnalysisType::PotentialRawIppsPacked);
	DirectPotentialOperator<Int16>   potDS16Ipps(&potentialIpps64, AnalysisType::PotentialIpps64);
	DirectPotentialOperator<Int16>   potDS16IppsR(&potentialRaw64, AnalysisType::PotentialRawIpps64);
	DirectPotentialOperator<Int16>   potDS16Slow(&potentialSlow, AnalysisType::PotentialSlow);
#if defined(DMS_USE_INTEL_IPPI)
	DirectPotentialOperator<Int16>   potDS16Ippi(&potentialIppi32, AnalysisType::PotentialIppi);
#endif //defined(DMS_USE_INTEL_IPPI)

#endif //defined(DMS_POTENTIAL_I16)

	CommonOperGroup proximity("proximity");

	DirectPotentialOperator<Float32> proxDF32(&proximity, AnalysisType::Proximity);
	DirectPotentialOperator<Float64> proxDF64(&proximity, AnalysisType::Proximity);
}

/******************************************************************************/

