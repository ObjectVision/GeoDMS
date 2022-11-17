//<HEADER> 
/*
Data & Model Server (DMS) is a server written in C++ for DSS applications. 
Version: see srv/dms/rtc/dll/src/RtcVersion.h for version info.

Copyright (C) 1998-2004  YUSE GSO Object Vision BV. 

Documentation on using the Data & Model Server software can be found at:
http://www.ObjectVision.nl/DMS/

See additional guidelines and notes in srv/dms/Readme-srv.txt 

This library is free software; you can use, redistribute, and/or
modify it under the terms of the GNU General Public License version 2 
(the License) as published by the Free Software Foundation,
provided that this entire header notice and readme-srv.txt is preserved.

See LICENSE.TXT for terms of distribution or look at our web site:
http://www.objectvision.nl/DMS/License.txt
or alternatively at: http://www.gnu.org/copyleft/gpl.html

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
General Public License for more details. However, specific warranties might be
granted by an additional written contract for support, assistance and/or development
*/
//</HEADER>

#include "ClcPCH.h"
#pragma hdrstop

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
	dms_assert(outputs.size() == lengthFinderArray.size());
	outputs.get_sa().data_reserve(lengthFinderArray.GetTotalLength() MG_DEBUG_ALLOCATOR_SRC_STR("OperAccUniStr: outputs.get_sa().data_reserve()"));

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
	OperAccTotUniStr(AbstrOperGroup* gr, const TAcc1Func& acc1Func = TAcc1Func()) 
		: OperAccTotUni<TAcc1Func>(gr, acc1Func)
	{}

	// Override Operator
	void Calculate(DataWriteHandle& res, const AbstrDataItem* arg1A) const override
	{
		auto arg1 = const_array_cast<typename OperAccTotUniStr::ValueType>(arg1A);
		dms_assert(arg1);

		auto result = mutable_array_cast<typename OperAccTotUniStr::ResultValueType>(res);
		dms_assert(result);

		dms_assert(result->GetDataWrite().size() == 1);

		m_Acc1Func.Init(result->GetDataWrite()[0]);

		tile_id tn = arg1A->GetAbstrDomainUnit()->GetNrTiles();
		for (tile_id t = 0; t!=tn; ++t)
		{
			m_Acc1Func(
				result->GetDataWrite()[0],
				arg1->GetLockedDataRead(t), 
				arg1A->HasUndefinedValues()
			);
		}
	}
private:
	TAcc1Func m_Acc1Func;
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
		dms_assert(args.size() == 2);
		
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
			dms_assert(result);

			auto arg2Data = arg2->GetDataRead();
			auto resData  = result->GetDataWrite();
			dms_assert(arg2Data.size() == 1);
			dms_assert(resData .size() == 1);

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

					aggr1_total_best<unary_ser_aslist>(lengthFinderStreamBuff, arg1Data.begin(), arg1Data.end(), arg1A->HasUndefinedValues(), m_SerFunc);
				}
				output.resize_uninitialized(lengthFinderStreamBuff.CurrPos());
			}
			ThrowingMemoOutStreamBuff outStr(begin_ptr( output ), end_ptr( output ));
			for (tile_id t=0; t!=te; ++t)
			{
				auto arg1Data = arg1->GetLockedDataRead(t);
				aggr1_total_best<unary_ser_aslist>(outStr, arg1Data.begin(), arg1Data.end(), arg1A->HasUndefinedValues(), m_SerFunc);
			}
/////////////////////////////////////////////////////////////////////
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

	AbstrOperAsListPart(const Class* arg3Cls) 
		: TernaryOperator(&cogAsList,
				ResultType::GetStaticClass(), 
				Arg1Type::GetStaticClass(), Arg2Type::GetStaticClass(), arg3Cls
			)
	{}

	// Override Operator
	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		dms_assert(args.size() == 3);

		const AbstrDataItem* arg1A = AsDataItem(args[0]);
		const AbstrDataItem* arg2A = AsDataItem(args[1]);
		const AbstrDataItem* arg3A = AsDataItem(args[2]);
		dms_assert(arg1A);
		dms_assert(arg2A);
		dms_assert(arg3A);

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
	typedef AbstrDataItem               Arg3Type; // index vector
	typedef DataArray<ResultValueType>  ResultType;
			
public:
	OperAsListPart() : AbstrOperAsListPart(Arg3Type::GetStaticClass())
	{
		dms_assert(ResultType::GetStaticClass() == GetResultClass());
	}

	void Calculate(DataWriteHandle& res, const AbstrDataItem* arg1A, const AbstrDataItem* arg2A, const AbstrDataItem* arg3A) const override
	{
		const Arg1Type* arg1 = const_array_cast<SharedStr>(arg1A);
		const Arg2Type* arg2 = const_array_cast<SharedStr>(arg2A);
		dms_assert(arg1);
		dms_assert(arg2);
		dms_assert(arg3A);

		ResultType* result = mutable_array_cast<ResultValueType>(res);
		dms_assert(result);
		
		tile_id tn  = arg1A->GetAbstrDomainUnit()->GetNrTiles();
		auto arg2Data = arg2->GetDataRead(); // no tiles, data locked by base class.

		dms_assert(arg2Data.size() == 1);

//		sequence_traits<AccumulationSeq::value_type>::container_type
//			resBuffer(Cardinality(indexValueRange));
		
		TAcc2Func asListOper(arg2Data[0]);
		OperAccPartUniSer_impl<TAcc2Func> impl(asListOper);

		impl(
			result
		,	arg1
		,	arg3A
		,	tn
		,	arg1A->HasUndefinedValues()
		);
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
