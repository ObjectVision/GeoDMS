// Copyright (C) 1998-2024 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

#if !defined(__CLC_OPERACCUNI_H)
#define __CLC_OPERACCUNI_H

#include "UnitClass.h"

#include <future>

#include "AggrUniStruct.h"
#include "AggrUniStructString.h"
#include "FutureTileArray.h"
#include "Explain.h"
#include "OperAcc.h"
#include "TreeItemClass.h"
#include "IndexGetterCreator.h"


// *****************************************************************************
//											AbstrOperAccTotUni
// *****************************************************************************

struct AbstrOperAccTotUni: UnaryOperator
{
	AbstrOperAccTotUni(AbstrOperGroup* gr, ClassCPtr resultCls, ClassCPtr arg1Cls, UnitCreatorPtr ucp, ValueComposition vc)
		:	UnaryOperator(gr, resultCls, arg1Cls)
		,	m_UnitCreatorPtr(std::move(ucp))
		,	m_ValueComposition(vc)
	{
		gr->SetCanExplainValue();
	}

	void CreateResultCaller(TreeItemDualRef& resultHolder, const ArgRefs& args, OperationContext*, LispPtr) const override
	{
		assert(args.size() == 1);

		if (resultHolder)
			return;

		const AbstrDataItem* arg1A = AsDataItem(args[0]);
		assert(arg1A);

		auto argSeq = GetItems(args);
		resultHolder = CreateCacheDataItem(
			Unit<Void>::GetStaticClass()->CreateDefault(),
			(*m_UnitCreatorPtr)(GetGroup(), argSeq),
			m_ValueComposition
		);
	}

	bool CalcResult(TreeItemDualRef& resultHolder, ArgRefs args, std::vector<ItemReadLock> readLocks, OperationContext*, Explain::Context* context) const override
	{
		dms_assert(resultHolder);
		AbstrDataItem* res = AsDataItem(resultHolder.GetNew());
		dms_assert(res);

		dms_assert(args.size() == 1);
		const AbstrDataItem* arg1A = AsDataItem(args[0]);
		dms_assert(arg1A);

		bool dontRecalc = res->m_DataObject;
//		dms_assert(dontRecalc || !context);
		if (!dontRecalc)
		{
//			DataReadLock arg1Lock(arg1A);
			DataWriteLock resLock(res, dms_rw_mode::write_only_mustzero); 
			Calculate(resLock, arg1A, std::move(args), std::move(readLocks));
			resLock.Commit();
		}
		if (context)
		{
			const AbstrUnit* adu = arg1A->GetAbstrDomainUnit();
			SizeT n = adu->GetCount();
			MakeMin<SizeT>(n, Explain::MaxNrEntries);
			for (SizeT i=0; i!=n; ++i)
				Explain::AddQueueEntry(context->m_CalcExpl, adu, i);
		}
		return true;
	}
	virtual void Calculate(DataWriteLock& res, const AbstrDataItem* arg1A, ArgRefs args, std::vector<ItemReadLock> readLocks) const =0;

private:
	UnitCreatorPtr m_UnitCreatorPtr;
	ValueComposition m_ValueComposition;
};


template <class TAcc1Func> 
struct OperAccTotUni : AbstrOperAccTotUni
{
	typedef typename TAcc1Func::value_type1     ValueType;
	typedef typename TAcc1Func::assignee_type   AccumulationType;
	using ResultValueType = dms_result_type_t<TAcc1Func>;

	typedef DataArray<ResultValueType>          ResultType;
	typedef DataArray<ValueType>                ArgType;
			
	OperAccTotUni(AbstrOperGroup* gr, TAcc1Func&& acc1Func = TAcc1Func())
		:	AbstrOperAccTotUni(gr
			,	ResultType::GetStaticClass(), ArgType::GetStaticClass()
			,	&TAcc1Func::template unit_creator<ResultValueType>, COMPOSITION(ResultValueType)
			)
		,	m_Acc1Func(acc1Func)
	{}

protected:
	TAcc1Func m_Acc1Func;
};

// *****************************************************************************
//											AbstrOperAccPartUni
// *****************************************************************************

struct AbstrOperAccPartUni: BinaryOperator
{
	AbstrOperAccPartUni(AbstrOperGroup* gr, 
			ClassCPtr resultCls, ClassCPtr arg1Cls, ClassCPtr arg2Cls, 
			UnitCreatorPtr ucp, ValueComposition vc
		)
		:	BinaryOperator(gr, resultCls, arg1Cls, arg2Cls)
		,	m_UnitCreatorPtr(std::move(ucp))
		,	m_ValueComposition(vc)
	{
		assert(gr);
		gr->SetCanExplainValue();

	}

	void CreateResultCaller(TreeItemDualRef& resultHolder, const ArgRefs& args, OperationContext*, LispPtr) const override
	{
		MG_PRECONDITION(args.size() == 2);

		if (resultHolder)
			return;

		const AbstrDataItem* arg1A = AsDataItem(args[0]);
		const AbstrDataItem* arg2A = AsDataItem(args[1]);
		dms_assert(arg1A);
		dms_assert(arg2A);

		const AbstrUnit* e1 = arg1A->GetAbstrDomainUnit();
		const AbstrUnit* e2 = arg2A->GetAbstrDomainUnit();
		e2->UnifyDomain(e1, "e2", "e1", UM_Throw);

		const AbstrUnit* p2 = arg2A->GetAbstrValuesUnit();
		MG_PRECONDITION(p2);

		auto argSeq = GetItems(args);
		resultHolder = CreateCacheDataItem(p2, (*m_UnitCreatorPtr)(GetGroup(), argSeq), m_ValueComposition);
	}

	bool CalcResult(TreeItemDualRef& resultHolder, ArgRefs args, std::vector<ItemReadLock> readLocks, OperationContext* oc, Explain::Context* context) const override
	{
		const AbstrDataItem* arg1A = AsDataItem(args[0]);
		const AbstrDataItem* arg2A = AsDataItem(args[1]);
		dms_assert(arg1A);
		dms_assert(arg2A);
		const AbstrUnit* e2 = arg2A->GetAbstrDomainUnit();

		dms_assert(resultHolder);

		AbstrDataItem* res = AsDataItem(resultHolder.GetNew());
		dms_assert(res);
		bool doRecalc = !res->m_DataObject;
		dms_assert(context || doRecalc);
		if (doRecalc)
		{
			DataWriteLock resLock(res);

			Calculate(resLock, arg1A, arg2A, std::move(args), std::move(readLocks));

			resLock.Commit();
		}
		if (context)
		{
			DataReadLock arg2Lock(arg2A);
			SizeT f = context->m_Coordinate->first;

			// search in partitioning for values with index f.
			const AbstrDataObject* ado = arg2A->GetCurrRefObj();
			SizeT i = 0, k = 0, n = ado->GetTiledRangeData()->GetElemCount();
			while (i < n) {
				i = ado->FindPosOfSizeT(f, i);
				if (!IsDefined(i))
					break;
				Explain::AddQueueEntry(context->m_CalcExpl, e2, i);
				if (++k >= Explain::MaxNrEntries)
					break;
				++i;
			}
		}
		return true;
	}
	virtual void Calculate(DataWriteLock& res, const AbstrDataItem* arg1A,  const AbstrDataItem* arg2A, ArgRefs args, std::vector<ItemReadLock> readLocks) const =0;

private:
	UnitCreatorPtr   m_UnitCreatorPtr;
	ValueComposition m_ValueComposition;
};

// *****************************************************************************
//											OperAccPartUni
// *****************************************************************************

template <typename V, typename R> 
struct OperAccPartUni: AbstrOperAccPartUni
{
	using Arg1Type = DataArray<V>; // value vector
	using Arg2Type = AbstrDataItem; // index vector, must be partition type
	using ResultValueType = R;
	using ResultType = DataArray<ResultValueType>;
	/*
	using ValueType = typename TAcc1Func::value_type1;
	using AccumulationSeq = typename TAcc1Func::accumulation_seq;
	*/
	OperAccPartUni(AbstrOperGroup* gr, UnitCreatorPtr ucp)
	:	AbstrOperAccPartUni(gr
		,   ResultType::GetStaticClass(), Arg1Type::GetStaticClass(), Arg2Type::GetStaticClass()
		,	ucp, COMPOSITION(ResultValueType)
		)
	{}
};

//template <typename TAcc1Func, typename ResultValueType = typename dms_result_type_of<TAcc1Func>::type> 
template <typename V, typename R>
struct OperAccPartUniWithCFTA : OperAccPartUni<V, R> // with consumable tile array
	// CFTA = Consumable Future Tile Array
{
	using OperAccPartUni<V, R>::OperAccPartUni;
	using ValueType = V;
	using ResultType = DataArray<R>;

	struct ProcessDataInfo {
		const AbstrDataItem* arg2A;
		future_tile_array<V> values_fta;
		abstr_future_tile_array part_fta;
		SizeT n;
		tile_id nrTiles;
		value_range_data<V> valuesRangeData;
		SizeT resCount;
	};

	void Calculate(DataWriteLock& res, const AbstrDataItem* arg1A, const AbstrDataItem* arg2A, ArgRefs args, std::vector<ItemReadLock> readLocks) const override
	{
		assert(arg2A);
		auto arg1 = (DataReadLock(arg1A), const_array_cast<V>(arg1A));
		assert(arg1);
		auto pdi = ProcessDataInfo{
			.arg2A = arg2A,
			.values_fta = GetFutureTileArray(arg1),
			.part_fta = (DataReadLock(arg2A), GetAbstrFutureTileArray(arg2A->GetCurrRefObj())),
			.n = arg1A->GetAbstrDomainUnit()->GetCount(),
			.nrTiles = arg1A->GetAbstrDomainUnit()->GetNrTiles(),
			.valuesRangeData = arg1->GetValueRangeData(),
			.resCount = arg2A->GetAbstrValuesUnit()->GetCount()
		};
		arg1 = nullptr;
		args.clear();
		readLocks.clear();

//		if (IsMultiThreaded3())
		ProcessData(mutable_array_cast<R>(res), pdi);
	}
	virtual void ProcessData(ResultType* result, ProcessDataInfo& pdi) const = 0;
};


template <typename TAcc1Func, typename Base>
struct FuncOperAccPartUni : Base
{
	FuncOperAccPartUni(AbstrOperGroup* gr, TAcc1Func&& acc1Func)
		: Base(gr, &TAcc1Func::template unit_creator<field_of_t<typename Base::ResultValueType>>)
		, m_Acc1Func(std::move(acc1Func))
	{}

protected:
	TAcc1Func m_Acc1Func;
};



template <class TAcc1Func>
struct OperAccPartUniBuffered : FuncOperAccPartUni<TAcc1Func, OperAccPartUniWithCFTA<typename TAcc1Func::value_type1, typename TAcc1Func::dms_result_type> >
{
	using ResultValueType = typename TAcc1Func::dms_result_type;
	using ValueType = typename TAcc1Func::value_type1;
	using ResultType = DataArray<ResultValueType>;
	using base_type = FuncOperAccPartUni<TAcc1Func, OperAccPartUniWithCFTA<ValueType, ResultValueType> >;
	using ProcessDataInfo = base_type::ProcessDataInfo;
	using AccumulationSeq = typename TAcc1Func::accumulation_seq;

	using res_buffer_type = typename sequence_traits<typename TAcc1Func::accumulation_type>::container_type;

	OperAccPartUniBuffered(AbstrOperGroup* gr, TAcc1Func&& acc1Func = TAcc1Func())
		:	base_type(gr, std::move(acc1Func))
	{}

	auto AggregateTiles(ProcessDataInfo& pdi, tile_id t, tile_id te, SizeT availableThreads) const
		-> res_buffer_type
	{
		if (te - t > 1)
		{
			auto m = t + (te - t) / 2;
			auto mt = availableThreads / 2;
			auto futureFirstHalfBuffer = std::async(mt ? std::launch::async : std::launch::deferred, [this, &pdi, t, m, mt]()
				{
					return AggregateTiles(pdi, t, m, mt);
				});
			t = m;
			availableThreads -= mt;
			auto secondHalfBuffer = AggregateTiles(pdi, t, te, availableThreads);
			auto secondHalfBufferIterator = secondHalfBuffer.begin();
			auto firstHalfBuffer = futureFirstHalfBuffer.get();
			auto firstHalfBufferEnd = firstHalfBuffer.end();
			for (auto firstHalfBufferIterator = firstHalfBuffer.begin(); firstHalfBufferIterator != firstHalfBufferEnd; ++secondHalfBufferIterator, ++firstHalfBufferIterator)
			{
				this->m_Acc1Func.CombineRefs(*firstHalfBufferIterator, *secondHalfBufferIterator);
			}
			return firstHalfBuffer;
		}

		res_buffer_type resBuffer(pdi.resCount);
		this->m_Acc1Func.Init(AccumulationSeq(&resBuffer));
		if (t < te)
		{
			auto arg1Data = pdi.values_fta[t]->GetTile(); pdi.values_fta[t] = nullptr;
			OwningPtr<IndexGetter> indexGetter = IndexGetterCreator::Create(pdi.arg2A, pdi.part_fta[t]); pdi.part_fta[t] = nullptr;

			this->m_Acc1Func(AccumulationSeq(&resBuffer), arg1Data, indexGetter);
		}
		return resBuffer;
	}

	void ProcessData(ResultType* result, ProcessDataInfo& pdi) const override
	{
		res_buffer_type resBuffer;
		if (pdi.resCount)
		{
			SizeT maxNrThreads = MaxConcurrentTreads();
			MakeMin(maxNrThreads, pdi.n / pdi.resCount);

			resBuffer = AggregateTiles(pdi, 0, pdi.nrTiles, maxNrThreads);
		}
		this->m_Acc1Func.AssignOutput(result->GetDataWrite(no_tile, dms_rw_mode::write_only_all), resBuffer);
	}
};

template <class TAcc1Func> 
struct OperAccPartUniDirect : FuncOperAccPartUni<TAcc1Func, OperAccPartUniWithCFTA<typename TAcc1Func::value_type1, typename TAcc1Func::result_type> >
{
	using ResultValueType = typename TAcc1Func::result_type;
	using ValueType = typename TAcc1Func::value_type1;
	using ResultType = DataArray<ResultValueType>;
	using base_type = FuncOperAccPartUni<TAcc1Func, OperAccPartUniWithCFTA<ValueType, ResultValueType> >;
	using ProcessDataInfo = base_type::ProcessDataInfo;

	OperAccPartUniDirect(AbstrOperGroup* gr, TAcc1Func&& acc1Func = TAcc1Func())
		: base_type(gr, std::move(acc1Func))
	{}

	using result_seq_t = typename sequence_traits<ResultValueType>::seq_t;
	using result_container_t = typename sequence_traits<ResultValueType>::container_type;
	void AggregateTiles(result_seq_t resData, ProcessDataInfo& pdi, tile_id t, tile_id te, SizeT availableThreads) const
	{
		if (te - t > 1)
		{
			auto m = t + (te - t) / 2;
			auto mt = availableThreads / 2;
			auto futureFirstHalf = std::async(mt ? std::launch::async : std::launch::deferred, [this, resData, &pdi, t, m, mt]()
				{
					AggregateTiles(resData, pdi, t, m, mt);
				});
			t = m;
			availableThreads -= mt;

			result_container_t secondHalf(resData.size());
			auto secondHalfRef = result_seq_t(&secondHalf);
			m_Acc1Func.Init(secondHalfRef);
			AggregateTiles(secondHalfRef, pdi, t, te, availableThreads);
			auto secondHalfBufferIterator = secondHalf.begin();

			futureFirstHalf.wait();
			auto firstHalfBufferEnd = resData.end();
			for (auto firstHalfBufferIterator = resData.begin(); firstHalfBufferIterator != firstHalfBufferEnd; ++secondHalfBufferIterator, ++firstHalfBufferIterator)
			{
				this->m_Acc1Func.CombineRefs(*firstHalfBufferIterator, *secondHalfBufferIterator);
			}
			return;
		}

		if (t < te)
		{
			auto arg1Data = pdi.values_fta[t]->GetTile(); pdi.values_fta[t] = nullptr;
			OwningPtr<IndexGetter> indexGetter = IndexGetterCreator::Create(pdi.arg2A, pdi.part_fta[t]); pdi.part_fta[t] = nullptr;

			m_Acc1Func(resData, arg1Data, indexGetter);
		}
	}

	void ProcessData(ResultType* result, ProcessDataInfo& pdi) const override
	{
		SizeT maxNrThreads = MaxConcurrentTreads();
		if (pdi.resCount)
			MakeMin(maxNrThreads, pdi.n / pdi.resCount);

		auto resData = result->GetDataWrite(no_tile, dms_rw_mode::write_only_all);
		m_Acc1Func.Init(resData);
		AggregateTiles(resData, pdi, 0, pdi.nrTiles, maxNrThreads);
	}

private:
	TAcc1Func m_Acc1Func;
};

struct make_direct   
{ 
	template <class TAcc1Func> struct apply {
		using type = OperAccPartUniDirect<TAcc1Func>;
	};
};

struct make_buffered
{
	template <class TAcc1Func> struct apply {
		using type = OperAccPartUniBuffered<TAcc1Func>;
	};
};

template <class TAcc1Func> const bool must_buffer_result = !std::is_same<dms_result_type_t<TAcc1Func>, typename TAcc1Func::assignee_type>::value;
template <class TAcc1Func> using base_of_functor = std::conditional_t<must_buffer_result<TAcc1Func>, make_buffered, make_direct>;
template <class TAcc1Func> using OperAccPartUniBest = typename base_of_functor<TAcc1Func>::template apply<TAcc1Func>::type;

template <typename TAcc1Func>
void CalcOperAccPartUniSer(DataWriteLock& res, const AbstrDataItem* arg1A, const AbstrDataItem* arg2A, TAcc1Func acc1Func = TAcc1Func())
{
	using ValueType = typename TAcc1Func::value_type1;
//	using AccumulationSeq = typename TAcc1Func::accumulation_seq;

	DataReadLock arg1Lock(arg1A);
	DataReadLock arg2Lock(arg2A);

	auto nrTiles = arg1A->GetAbstrDomainUnit()->GetNrTiles();
	auto arg1 = const_array_cast<ValueType>(arg1A);
	auto resData = mutable_array_cast<SharedStr>(res)->GetDataWrite(no_tile, dms_rw_mode::write_only_all);
	//		m_Acc1Func.Init(resData);
	{

		length_finder_array lengthFinderArray(resData.size());
		for (tile_id t = 0; t != nrTiles; ++t)
		{
			typename DataArray<ValueType>::locked_cseq_t arg1Data = arg1->GetTile(t);
			OwningPtr<IndexGetter> indexGetter = IndexGetterCreator::Create(arg2A, t);

			acc1Func.InspectData(lengthFinderArray, arg1Data, indexGetter);
		}
		// allocate sufficent space in outputs
		InitOutput(resData, lengthFinderArray);
		// can destruct lengthFinderArray now
	}

	// build memo_out_stream_array that refers to each of the allocated output cells.
	memo_out_stream_array outStreamArray;
	acc1Func.ReserveData(outStreamArray, resData);

	for (tile_id t = 0; t != nrTiles; ++t)
	{
		auto arg1Data = arg1->GetTile(t);
		OwningPtr<IndexGetter> indexGetter = IndexGetterCreator::Create(arg2A, t);

		acc1Func.ProcessTileData(outStreamArray, arg1Data, indexGetter);
	}
}


template <typename TAcc1Func> 
struct OperAccPartUniSer : FuncOperAccPartUni<TAcc1Func, OperAccPartUni<typename TAcc1Func::value_type1, SharedStr> >
{
	using ValueType = typename TAcc1Func::value_type1;
	using base_type = FuncOperAccPartUni<TAcc1Func, OperAccPartUni<ValueType, SharedStr> >;

	OperAccPartUniSer(AbstrOperGroup* gr, TAcc1Func&& acc1Func = TAcc1Func())
		: base_type(gr, std::move(acc1Func))
	{}

	void Calculate(DataWriteLock& res, const AbstrDataItem* arg1A, const AbstrDataItem* arg2A, ArgRefs args, std::vector<ItemReadLock> readLocks) const override
	{
		CalcOperAccPartUniSer<TAcc1Func>(res, arg1A, arg2A, this->m_Acc1Func);
	}
};

#endif

