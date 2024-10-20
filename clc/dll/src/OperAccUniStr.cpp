// Copyright (C) 1998-2024 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "ClcPch.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif

#include "RtcTypeLists.h"

#include "geo/StringArray.h"
#include "geo/Conversions.h"
#include "mci/CompositeCast.h"
#include "set/VectorFunc.h"

#include "Unit.h"
#include "UnitClass.h"
#include "Param.h"
#include "DataItemClass.h"


#include "Prototypes.h"
#include "AggrFunc.h"
#include "AggrUniStructString.h"
#include "OperAccUni.h"
#include "OperRelUni.h"

// *****************************************************************************
//											Implementation of helper func
// *****************************************************************************

void InitOutput(sequence_traits<SharedStr>::seq_t& outputs, const length_finder_array& lengthFinderArray)
{
	assert(outputs.size() == lengthFinderArray.size());
	outputs.get_sa().data_reserve(lengthFinderArray.GetTotalLength() MG_DEBUG_ALLOCATOR_SRC("OperAccUniStr: outputs.get_sa().data_reserve()"));

	sequence_traits<SharedStr>::container_type::iterator
		outputsItr = outputs.begin(),
		outputsEnd = outputs.end();
	length_finder_array::const_iterator
		lengthFinderItr = lengthFinderArray.begin();

	while (outputsItr != outputsEnd)
		(outputsItr++)->resize_uninitialized(lengthFinderItr++->CurrPos()); // +1);
}



// *****************************************************************************
//											CLASSES
// *****************************************************************************

template <class TAcc1Func> 
struct OperAccTotUniStr : OperAccTotUni<TAcc1Func>
{
	OperAccTotUniStr(AbstrOperGroup* gr, TAcc1Func&& acc1Func = TAcc1Func()) 
		: OperAccTotUni<TAcc1Func>(gr, std::move(acc1Func))
	{}

	// Override Operator
	void Calculate(DataWriteHandle& res, const AbstrDataItem* arg1A, ArgRefs args, std::vector<ItemReadLock> readLocks) const override
	{
		auto arg1 = const_array_cast<typename OperAccTotUniStr::ValueType>(arg1A);
		assert(arg1);

		auto result = mutable_array_cast<typename OperAccTotUniStr::ResultValueType>(res);
		assert(result);

		assert(result->GetDataWrite().size() == 1);

		Assign(result->GetDataWrite()[0], this->m_Acc1Func.InitialValue());

		tile_id tn = arg1A->GetAbstrDomainUnit()->GetNrTiles();
		for (tile_id t = 0; t!=tn; ++t)
			this->m_Acc1Func(result->GetDataWrite()[0], arg1->GetTile(t));
	}
};

// *****************************************************************************
//											asList
// *****************************************************************************

CommonOperGroup cogAsList("asList");

class OperAsListTot : public BinaryOperator
{
	typedef aslist_total         TAcc2Func;
	
	typedef DataArray<SharedStr> Arg1Type;
	typedef DataArray<SharedStr> Arg2Type;
	typedef DataArray<SharedStr> ResultType;
			
public:
	OperAsListTot() 
		: BinaryOperator(&cogAsList, ResultType::GetStaticClass(),
				Arg1Type::GetStaticClass(), Arg2Type::GetStaticClass()
			) 
	{}

	// Override Operator
	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		assert(args.size() == 2);
		
		const AbstrDataItem* arg1A = AsDataItem(args[0]);
		const AbstrDataItem* arg2A = AsDataItem(args[1]);

		if (!resultHolder)
			resultHolder = CreateCacheDataItem(Unit<Void>::GetStaticClass()->CreateDefault(), arg1A->GetAbstrValuesUnit());

		if (mustCalc)
		{		
			DataReadLock arg1Lock(arg1A);
			DataReadLock arg2Lock(arg2A);

			const Arg1Type* arg1 = const_array_cast<SharedStr>(arg1A);
			const Arg2Type* arg2 = const_array_cast<SharedStr>(arg2A);

			AbstrDataItem* res = AsDataItem(resultHolder.GetNew());
			DataWriteLock resLock(res, dms_rw_mode::write_only_mustzero);

			ResultType* result = mutable_array_cast<SharedStr>(resLock);
			assert(result);

			auto arg2Data = arg2->GetDataRead();
			auto resData  = result->GetDataWrite();
			assert(arg2Data.size() == 1);
			assert(resData .size() == 1);

			Arg2Type::const_reference arg2Value = arg2Data[0];
			ResultType::reference     resValue  = resData[0];

			unary_ser_aslist m_SerFunc(arg2Data[0]);

			aslist_total::accumulation_ref output = resValue;

			tile_id te = arg1A->GetAbstrDomainUnit()->GetNrTiles();
			{
				InfiniteNullOutStreamBuff lengthFinderStreamBuff;
				for (tile_id t=0; t!=te; ++t)
				{
					auto arg1Data = arg1->GetLockedDataRead(t);

					aggr1_total<unary_ser_aslist>(lengthFinderStreamBuff, arg1Data.begin(), arg1Data.end(), m_SerFunc);
				}
				output.resize_uninitialized(lengthFinderStreamBuff.CurrPos());
			}
			ThrowingMemoOutStreamBuff outStr(ByteRange(begin_ptr( output ), end_ptr( output )));
			for (tile_id t=0; t!=te; ++t)
			{
				auto arg1Data = arg1->GetLockedDataRead(t);
				aggr1_total<unary_ser_aslist>(outStr, arg1Data.begin(), arg1Data.end(), m_SerFunc);
			}
			resLock.Commit();
		}
		return true;
	}
};

// *****************************************************************************
//											asList partitioned
// *****************************************************************************


class AbstrOperAsListPart : public TernaryOperator
{
public:
	typedef DataArray<SharedStr> Arg1Type; // value vector
	typedef DataArray<SharedStr> Arg2Type; // separator
	typedef DataArray<SharedStr> ResultType;

	AbstrOperAsListPart() 
		: TernaryOperator(&cogAsList,
				ResultType::GetStaticClass(), 
				Arg1Type::GetStaticClass(), Arg2Type::GetStaticClass(), AbstrDataItem::GetStaticClass()
			)
	{}

	// Override Operator
	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		assert(args.size() == 3);

		const AbstrDataItem* arg1A = AsDataItem(args[0]);
		const AbstrDataItem* arg2A = AsDataItem(args[1]);
		const AbstrDataItem* arg3A = AsDataItem(args[2]);
		assert(arg1A);
		assert(arg2A);
		assert(arg3A);

		const AbstrUnit* e1 = arg1A->GetAbstrDomainUnit();
		const AbstrUnit* e2 = arg2A->GetAbstrDomainUnit();
		const AbstrUnit* e3	= arg3A->GetAbstrDomainUnit();
		const AbstrUnit* p3	= arg3A->GetAbstrValuesUnit();

		e3->UnifyDomain(e1, "e3", "e1", UM_Throw);
		MG_PRECONDITION(p3);

		if (!resultHolder)
			resultHolder =
				CreateCacheDataItem(p3, 
					arg1A->GetAbstrValuesUnit(), 
					COMPOSITION(SharedStr)
				);
		if (mustCalc)
		{
			DataReadLock arg1Lock(arg1A);
			DataReadLock arg2Lock(arg2A);
			DataReadLock arg3Lock(arg3A);

			AbstrDataItem* res = AsDataItem(resultHolder.GetNew());
			DataWriteLock resLock(res, dms_rw_mode::write_only_mustzero);

			Calculate(resLock, arg1A, arg2A, arg3A);
			resLock.Commit();
		}
		return true;
	}
	virtual void Calculate(DataWriteHandle& res, const AbstrDataItem* arg1A, const AbstrDataItem* arg2A, const AbstrDataItem* arg3A) const = 0;
};

class OperAsListPart : public AbstrOperAsListPart
{
	typedef aslist_partial              TAcc2Func;
	typedef TAcc2Func::accumulation_seq AccumulationSeq;
	typedef TAcc2Func::dms_result_type  ResultValueType;
	typedef DataArray<ResultValueType>  ResultType;
			
public:
	OperAsListPart()
	{
		assert(ResultType::GetStaticClass() == GetResultClass());
	}

	void Calculate(DataWriteHandle& res, const AbstrDataItem* arg1A, const AbstrDataItem* arg2A, const AbstrDataItem* arg3A) const override
	{
		assert(arg3A);

		auto arg2Data = const_array_cast<SharedStr>(arg2A)->GetDataRead(); // no tiles, data locked by base class.

		MG_CHECK(arg2Data.size() == 1);

		CalcOperAccPartUniSer< aslist_partial>(res, arg1A, arg3A, aslist_partial(arg2Data[0]));
	}
};


// *****************************************************************************
//											INSTANTIATION
// *****************************************************************************

#include "AggrUniStructNum.h"

namespace 
{
	CommonOperGroup cogAsExprList("asExprList");
	CommonOperGroup cogAsItemList("asItemList");

	tl_oper::inst_tuple<typelists::num_objects, OperAccTotUniStr<asexprlist_total <_>>, AbstrOperGroup*> s_AsExprListT(&cogAsExprList);

	OperAccTotUniStr<asexprlist_total<SharedStr>> f1(&cogAsExprList);
	OperAccTotUniStr<asitemlist_total<SharedStr>> f2(&cogAsItemList);
	OperAccTotUniStr<min_total_best<SharedStr>> f3(&cog_Min);
	OperAccTotUniStr<max_total_best<SharedStr>> f4(&cog_Max);
	OperAccTotUniStr<first_total_best<SharedStr>> f5(&cog_First);
	OperAccTotUniStr<last_total_best<SharedStr>> f6(&cog_Last);

//	OPERPART_NUMB_INSTANTIATION  (OperAccPartUniSer, asexprlist_partial, cogAsExprList);
	tl_oper::inst_tuple<typelists::num_objects, OperAccPartUniSer<asexprlist_partial<_>>, AbstrOperGroup*> s_AsExprListPartNum(&cogAsExprList);

	OperAccPartUniSer<asexprlist_partial   <SharedStr> > s_AsExprListPart(&cogAsExprList);
	OperAccPartUniSer<asitemlist_partial   <SharedStr> > s_AsItemListPart(&cogAsItemList);
	OperAccPartUniDirect<min_partial_best  <SharedStr> > s_MinStrPart(&cog_Min);
	OperAccPartUniDirect<max_partial_best  <SharedStr> > s_MaxStrPart(&cog_Max);
	OperAccPartUniDirect<first_partial_best<SharedStr> > s_FirstStrPart(&cog_First);
	OperAccPartUniDirect<last_partial_best <SharedStr> > s_LastStrPart(&cog_Last);

	OperAsListTot alt;
//	OperAsListPart<UInt32> alpui32(cogAsList);
	
	OperAsListPart aoalp;
}
