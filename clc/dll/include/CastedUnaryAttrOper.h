// Copyright (C) 1998-2024 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif


#if !defined(__CLC_CASTEDUNARYATTROPER_H)
#define __CLC_CASTEDUNARYATTROPER_H

#include "DataItemClass.h"
#include "OperSignature.h"
#include "ParallelTiles.h"
#include "UnitClass.h"
#include "TileFunctorImpl.h"
#include "stg/MemoryMappedDataStorageManager.h"

// *****************************************************************************
//									AbstrCastedUnaryAttrOperator
// *****************************************************************************

class AbstrCastedUnaryAttrOperator : public BinaryOperator
{
public:
	AbstrCastedUnaryAttrOperator(AbstrOperGroup* gr, const DataItemClass* resultType, const DataItemClass* argDataType, const UnitClass* argUnitType, ValueComposition vc, bool reverseArgs)
		:	BinaryOperator(gr
			,	resultType 
			,	reverseArgs ? typesafe_cast<const Class*>(argUnitType) : typesafe_cast<const Class*>(argDataType)
			,	reverseArgs ? typesafe_cast<const Class*>(argDataType) : typesafe_cast<const Class*>(argUnitType)
			)
		,	m_VC(vc)
		,	m_ReverseArgs(reverseArgs)
	{}

	// This is the one gated family whose pipelining test includes !GetKeepDataState() (see
	// CreateResult below), so it is the one that adds the term to the predicted regime. The base
	// predictor deliberately omits it, because the other six families do not test it.
	// doc/tile-data-retainment.md §4.7.
	auto EstimatePerformance(TreeItemDualRef& resultHolder, const ArgRefs& args) const -> PerformanceEstimationData override
	{
		auto result = BinaryOperator::EstimatePerformance(resultHolder, args);
		if (result.regime != materialization::meta)
			if (auto res = resultHolder.GetUlt(); res && res->GetKeepDataState())
			{
				result.regime = materialization::eager;
				result.residentMemory = result.resultingMemory; // a kept result holds its whole array
			}
		return result;
	}

	// Override Operator
	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		assert(args.size() == 2);

		const AbstrDataItem* argDataA = AsDataItem(args[m_ReverseArgs ? 1 : 0]);
		assert(argDataA);

		const AbstrUnit* argUnitA= AsUnit(args[m_ReverseArgs ? 0 : 1]);

		if (!resultHolder)
		{
			// A value/coordinate conversion preserves the geometric composition of its
			// argument (poly stays poly, arc stays arc, multipoint stays multipoint). It must
			// NOT default to the result value-type's composition (m_VC == composition_of_v<TR>,
			// which is Sequence/arc for coordinate types) -- that silently turned poly into arc,
			// and #1038 then surfaced the mismatch (e.g. t060 pand geometry -> LINESTRING).
			// A non-sequence result (e.g. a sequence->string convert) keeps m_VC: its value
			// type cannot carry the argument's composition.
			resultHolder = CreateCacheDataItem(argDataA->GetAbstrDomainUnit(), argUnitA,
			                                   CastResultingValueComposition(m_VC, argDataA->GetValueComposition()));
			if (argUnitA->GetTSF(TSF_Categorical))
				resultHolder->SetTSF(TSF_Categorical);
		}

		if (mustCalc)
		{
			AbstrDataItem* res = AsDataItem(resultHolder.GetNew());
			MG_PRECONDITION(res);

			DataReadLock arg1Lock(argDataA);

			tile_id nrTiles = argDataA->GetAbstrDomainUnit()->GetNrTiles();
			bool createPipelinedCaster = IsMultiThreaded3() && (nrTiles > 1) && !IsInMMD(res) && !res->GetKeepDataState();
			if (createPipelinedCaster)
			{
				auto valuesUnitA = AsUnit(res->GetValuesUnitOrThrow()->GetCurrRangeItem());
				MG_CHECK(valuesUnitA);
				AsDataItem(resultHolder.GetOld())->m_DataObject = CreateFutureTileCaster(make_shared_tree(res, existing_obj{}), res->GetLazyCalculatedState(), valuesUnitA.get(), argDataA, argUnitA MG_DEBUG_ALLOCATOR_SRC(res->md_FullName + " := "  + GetGroup()->GetNameStr()));
			}
			else
			{
				DataWriteLock resLock(res, dms_rw_mode::write_only_all);

				parallel_tileloop(nrTiles, [this, argDataA, argUnitA, &resLock, res](tile_id t)->void
					{
						try {
							this->Calculate(resLock.get(), argDataA, argUnitA, t);
						}
						catch (const DmsException& x)
						{
							res->Fail(x.GetAsText(), FailType::Data);
						}
					}
				);
				resLock.Commit();
			}
		}
		return true;
	}
	virtual void Calculate(AbstrDataObject* res, const AbstrDataItem* argDataA, const AbstrUnit* argUnit, tile_id t) const =0;
	virtual auto CreateFutureTileCaster(std::shared_ptr<AbstrDataItem> resultAdi, bool lazy, const AbstrUnit* valuesUnitA, const AbstrDataItem* arg1A, const AbstrUnit* argUnitA MG_DEBUG_ALLOCATOR_SRC(SharedStr srcStr)) const -> SharedPtr<const AbstrDataObject> = 0;

	// mirrors CreateResult above: one data argument and one unit argument; the
	// result ranges over the data argument's domain and takes the unit argument
	// as values unit — at class level: R's class equals U's class per member,
	// which the merged records' tuples carry (convert/value: the typed cast)
	bool DescribeSignature(AbstrSignatureBuilder& sb) const override
	{
		arg_index dataPos = m_ReverseArgs ? 1 : 0, unitPos = m_ReverseArgs ? 0 : 1;
		auto dataCls = dynamic_cast<const DataItemClass*>(GetArgClass(dataPos));
		auto unitCls = dynamic_cast<const UnitClass*>(GetArgClass(unitPos));
		auto resCls  = dynamic_cast<const DataItemClass*>(GetResultClass());
		if (!dataCls || !unitCls || !resCls)
			return false;
		sig_var D = sb.UnitVar("D"), V = sb.UnitVar("V"), U = sb.UnitVar("U"), R = sb.UnitVar("R");
		sb.MemberValueClass(V, dataCls->GetValuesType());
		sb.MemberValueClass(U, unitCls->GetValueType());
		sb.MemberValueClass(R, resCls->GetValuesType());
		sb.ArgAttr(dataPos, V, D, m_VC);
		sb.ArgUnit(unitPos, U);
		sb.ResultAttr(R, D, m_VC);
		return true;
	}

private:
	ValueComposition m_VC;
	bool             m_ReverseArgs;
};

// *****************************************************************************
//			AbstrMappingState: the per-invocation conversion state of a mapping
// *****************************************************************************
// Built ONCE in CreateResult and shared by every tile, instead of being rebuilt per tile.
// For a cross-CRS mapping that construction is two OGRSpatialReference::SetFromUserInput calls
// (proj.db lookups) plus an OGRCreateCoordinateTransformation, all under the global
// cs_SpatialRefBlockCreation -- measured at ~1.1 ms, which used to dominate the whole operator:
// a 12M-cell EPSG:4326 -> EPSG:3857 mapping spent ~220 ms of its ~230 ms there and only
// microseconds in the actual coordinate transformations.
//
// The concrete state (MappingState in SeparableMapping.h) also carries the whole-domain x/y
// cross when the transformation is coordinate separable, so tile production reads (x[c], y[r])
// and calls PROJ zero times per raster cell. See issue #298.
struct AbstrMappingState
{
	virtual ~AbstrMappingState() {}
};

class AbstrMappingOperator : public BinaryOperator
{
public:
	AbstrMappingOperator(AbstrOperGroup* gr, const DataItemClass* resultType, const UnitClass* argDomainUnitType, const UnitClass* argValuesUnitType)
		:	BinaryOperator(gr
			,	resultType
			,	argDomainUnitType
			,	argValuesUnitType
			)
	{}

	// Override Operator
	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		assert(args.size() == 2);

		const AbstrUnit* argDomainUnit = AsUnit(args[0]);
		const AbstrUnit* argValuesUnit = AsUnit(args[1]);
		assert(argDomainUnit);
		assert(argValuesUnit);

		if (!resultHolder)
			resultHolder = CreateCacheDataItem(argDomainUnit, argValuesUnit);
		
		if (mustCalc)
		{
			auto res = AsDataItem(resultHolder.GetNew());
			MG_PRECONDITION(res);

			assert(res->GetValueComposition() == ValueComposition::Single);

			// Once for the whole operator, not once per tile: see AbstrMappingState.
			auto mappingState = CreateMappingState(argDomainUnit, argValuesUnit);

			// tn > 1 conform the other pipelined channels: a single-tile result gains no
			// tile-wise streaming, and a LazyTileFunctor would re-run the mapping for the whole
			// attribute on every access that follows a release of its only tile.
			auto tn = argDomainUnit->GetNrTiles();
			if (IsMultiThreaded3() && (tn > 1) && !IsInMMD(res))
			{
				auto binaryOper = this;

				auto resDomainRange = res->GetDomainUnitOrThrow()->GetCurrRangeItem();
				MG_CHECK(resDomainRange);
				auto tileRangeData = AsUnit(resDomainRange)->GetTiledRangeData();
				auto valuesUnit = AsUnit(res->GetValuesUnitOrThrow()->GetCurrRangeItem());
				MG_CHECK(valuesUnit);
				visit<typelists::fields>(valuesUnit.get(), [binaryOper, res, argDomainUnit, argValuesUnit, tileRangeData, mappingState]<typename V>(const Unit<V>*valuesUnit) {
					SharedUnitInterestPtr retainedArgDomainUnit = argDomainUnit;
					SharedUnitInterestPtr retainedArgValuesUnit = argValuesUnit;
					// mappingState outlives every tile: the lambda owns a share of it, so a tile
					// recomputed long after CreateResult still finds the transformer and the
					// separable cross already built.
					auto lazyTileFunctor = make_unique_LazyTileFunctor<V>(make_shared_tree(res, existing_obj{}), tileRangeData.get(), valuesUnit->m_RangeDataPtr
						, [binaryOper, res, retainedArgDomainUnit, retainedArgValuesUnit, mappingState](AbstrDataObject* self, tile_id t) {
							binaryOper->Calculate(self, *mappingState, t); // write into the same tile.
						}
						MG_DEBUG_ALLOCATOR_SRC(res->md_FullName +  ": = MappingOperator()")
					);
					res->m_DataObject = lazyTileFunctor.release();
				}
				);
			}
			else
			{
				DataWriteLock resLock(res, dms_rw_mode::write_only_mustzero);

				parallel_tileloop(tn, [this, &mappingState, &resLock](tile_id t)->void
					{
						this->Calculate(resLock.get(), *mappingState, t);
					}
				);
				resLock.Commit();
			}
		}
		return true;
	}

	// Builds the shared conversion state. Called once per invocation, on the calling thread.
	virtual auto CreateMappingState(const AbstrUnit* argDomainUnit, const AbstrUnit* argValuesUnit) const -> std::shared_ptr<AbstrMappingState> = 0;
	virtual void Calculate(AbstrDataObject* res, const AbstrMappingState& state, tile_id t) const = 0;
};

class AbstrMappingCountOperator : public TernaryOperator
{
public:
	AbstrMappingCountOperator(AbstrOperGroup* gr, const DataItemClass* resultType, 
			const UnitClass* argDomainUnitType, 
			const UnitClass* argValuesUnitType, 
			const UnitClass* resultUnitType
)
		: TernaryOperator(gr
			, resultType
			, argDomainUnitType
			, argValuesUnitType
			, resultUnitType
		)
	{}

	// Override Operator
	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		assert(args.size() == 3);

		const AbstrUnit* argDomainUnit = AsUnit(args[0]);
		const AbstrUnit* argValuesUnit = AsUnit(args[1]);
		const AbstrUnit* resValuesUnit = AsUnit(args[2]);
		assert(argDomainUnit);
		assert(argValuesUnit);
		assert(resValuesUnit);

		if (!resultHolder)
			resultHolder = CreateCacheDataItem(argValuesUnit, resValuesUnit);

		if (mustCalc)
		{
			auto res = AsDataItem(resultHolder.GetNew());
			MG_PRECONDITION(res);

			// Once for the whole operator, not once per tile: see AbstrMappingState.
			auto mappingState = CreateMappingState(argDomainUnit, argValuesUnit);

			// With a coordinate-separable transformation the whole histogram is the OUTER PRODUCT
			// of two 1-D counts, so a destination cell's value depends on nothing but its own
			// (i, j). Result tiles are then independent -- no accumulation, no ordering -- and
			// this operator gets exactly the treatment AbstrMappingOperator gets: a
			// LazyTileFunctor when it pays, a parallel_tileloop otherwise.
			//
			// Without a cross the counts have to be scattered from the source, so the loop stays
			// over SOURCE tiles and stays serial. It also has to keep ONE destination write
			// handle for the whole loop: a histogram larger than a single tile makes
			// GetDataWrite(no_tile, ...) hand out a MutableShadowTile (DataArray.cpp:346) that
			// copies the entire destination in and out, which per source tile is
			// O(nrSourceTiles * destinationSize).
			if (HasIndependentResultTiles(*mappingState))
			{
				auto tn = argValuesUnit->GetNrTiles();
				if (IsMultiThreaded3() && (tn > 1) && !IsInMMD(res))
				{
					auto ternaryOper = this;

					auto resDomainRange = res->GetDomainUnitOrThrow()->GetCurrRangeItem();
					MG_CHECK(resDomainRange);
					auto tileRangeData = AsUnit(resDomainRange)->GetTiledRangeData();
					auto valuesUnit = AsUnit(res->GetValuesUnitOrThrow()->GetCurrRangeItem());
					MG_CHECK(valuesUnit);
					visit<typelists::fields>(valuesUnit.get(), [ternaryOper, res, argDomainUnit, argValuesUnit, tileRangeData, mappingState]<typename V>(const Unit<V>*valuesUnit) {
						SharedUnitInterestPtr retainedArgDomainUnit = argDomainUnit;
						SharedUnitInterestPtr retainedArgValuesUnit = argValuesUnit;
						auto lazyTileFunctor = make_unique_LazyTileFunctor<V>(make_shared_tree(res, existing_obj{}), tileRangeData.get(), valuesUnit->m_RangeDataPtr
							, [ternaryOper, retainedArgDomainUnit, retainedArgValuesUnit, mappingState](AbstrDataObject* self, tile_id t) {
								ternaryOper->Calculate(self, *mappingState, t); // write into the same tile.
							}
							MG_DEBUG_ALLOCATOR_SRC(res->md_FullName + ": = MappingCountOperator()")
						);
						res->m_DataObject = lazyTileFunctor.release();
					}
					);
				}
				else
				{
					// write_only_all, not mustzero: the outer product writes every destination
					// cell, including the zeros, so pre-zeroing the result is wasted work.
					DataWriteLock resLock(res, dms_rw_mode::write_only_all);

					parallel_tileloop(tn, [this, &mappingState, &resLock](tile_id t)->void
						{
							this->Calculate(resLock.get(), *mappingState, t);
						}
					);
					resLock.Commit();
				}
			}
			else
			{
				// mustzero: the fallback INCREMENTS, so it needs a zeroed result to start from.
				DataWriteLock resLock(res, dms_rw_mode::write_only_mustzero);
				AccumulateFromSourceTiles(resLock, *mappingState, argDomainUnit->GetNrTiles());
				resLock.Commit();
			}
		}
		return true;
	}

	virtual auto CreateMappingState(const AbstrUnit* argDomainUnit, const AbstrUnit* argValuesUnit) const -> std::shared_ptr<AbstrMappingState> = 0;
	// Whether the result is an outer product, i.e. whether each destination tile can be produced
	// on its own from the state alone.
	virtual bool HasIndependentResultTiles(const AbstrMappingState& state) const = 0;
	// Produces one RESULT tile. Only called when HasIndependentResultTiles().
	virtual void Calculate(AbstrDataObject* res, const AbstrMappingState& state, tile_id t) const = 0;
	// The fallback: scatters counts from every SOURCE tile into one destination write handle.
	virtual void AccumulateFromSourceTiles(DataWriteLock& res, const AbstrMappingState& state, tile_id nrSrcTiles) const = 0;
};

// *****************************************************************************
//									CastedUnaryAttrSpecialFuncOperator
// *****************************************************************************

template <typename TUniOper>
class CastedUnaryAttrSpecialFuncOperator : public AbstrCastedUnaryAttrOperator
{
	typedef typename TUniOper::arg1_type  Arg1Values;
	typedef DataArray<Arg1Values>         Arg1Type;
	typedef typename TUniOper::res_type   ResultValueType;
	typedef field_of_t<ResultValueType>   ResultFieldType;
	typedef DataArray<ResultValueType>    ResultType;
	typedef Unit     <ResultFieldType>    ValuesUnitType;
			
public:
	CastedUnaryAttrSpecialFuncOperator(AbstrOperGroup* gr)
		:	AbstrCastedUnaryAttrOperator(gr 
			,	ResultType::GetStaticClass() 
			,	Arg1Type::GetStaticClass()
			,	ValuesUnitType::GetStaticClass()
			,	composition_of<typename TUniOper::res_type>::value
			,	false
			)
	{}

	// Override Operator
	auto CreateFutureTileCaster(std::shared_ptr<AbstrDataItem> resultAdi, bool lazy, const AbstrUnit* valuesUnitA, const AbstrDataItem* arg1A, const AbstrUnit* argUnitA MG_DEBUG_ALLOCATOR_SRC(SharedStr srcStr)) const -> SharedPtr<const AbstrDataObject> override
	{
		auto tileRangeData = AsUnit(arg1A->GetAbstrDomainUnit()->GetCurrRangeItem())->GetTiledRangeData();
		auto valuesUnit = debug_cast<const Unit<field_of_t<ResultValueType>>*>(valuesUnitA);

		auto arg1 = MakeSharedFromBorrowedObjectPtr(const_array_cast<Arg1Values>(arg1A));
		assert(arg1);

		using prepare_data = std::shared_ptr<typename Arg1Type::future_tile>;

		auto futureTileFunctor = make_unique_FutureTileFunctor<ResultValueType, prepare_data, false>(resultAdi.get(), lazy, tileRangeData.get(), get_range_ptr_of_valuesunit(valuesUnit)
			, [arg1](tile_id t) { return arg1->GetFutureTile(t); }
			, [](typename sequence_traits<ResultValueType>::seq_t resData, prepare_data arg1FutureData)
			{
				auto arg1Data = arg1FutureData->GetTile();
				auto oper = TUniOper();
				std::transform(arg1Data.begin(), arg1Data.end(), resData.begin(), oper);
			}
			MG_DEBUG_ALLOCATOR_SRC_PARAM
		);

		return futureTileFunctor.release();
	}
	void Calculate(AbstrDataObject* res, const AbstrDataItem* arg1A, const AbstrUnit* argUnit, tile_id t) const override
	{
		assert(arg1A);
		const Arg1Type* arg1 = const_array_cast<Arg1Values>(arg1A);

		ResultType* result = mutable_array_cast<ResultValueType>(res);
		assert(result);

		auto arg1Data = arg1->GetTile(t);
		auto resData = result->GetWritableTile(t);

		MG_DEBUGCODE(dms_assert(arg1Data.size() == arg1A->GetAbstrDomainUnit()->GetTileCount(t)); )
		MG_DEBUGCODE(dms_assert(resData.size() == arg1A->GetAbstrDomainUnit()->GetTileCount(t)); )

		auto oper = TUniOper();
		std::transform(arg1Data.begin(), arg1Data.end(), resData.begin(), oper);
	}
};


#endif //!defined(__CLC_CASTEDUNARYATTROPER_H)
