// Copyright (C) 1998-2024 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "ClcPch.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif

#include "geo/GeoSequence.h"
#include "geo/StringBounds.h"
#include "mci/CompositeCast.h"
#include "ser/AsString.h"
#include "dbg/SeverityType.h"
#include "set/VectorFunc.h"
#include "xct/DmsException.h"

#include "CheckedDomain.h"
#include "DataArray.h"
#include "DataItemClass.h"
#include "LispTreeType.h"
#include "Metric.h"
#include "ParallelTiles.h"
#include "TileChannel.h"
#include "TreeItemContextHandle.h"
#include "Unit.h"
#include "UnitClass.h"

#include "OperRelUni.h"
#include "lookup.h"
#include "UnitProcessor.h"

CommonOperGroup cog_union      ("union",      oper_policy::allow_extra_args);
CommonOperGroup cog_unionUnit  ("union_unit", oper_policy::allow_extra_args);
CommonOperGroup cog_unionUnit08("union_unit_uint8", oper_policy::allow_extra_args);
CommonOperGroup cog_unionUnit16("union_unit_uint16", oper_policy::allow_extra_args);
CommonOperGroup cog_unionUnit32("union_unit_uint32", oper_policy::allow_extra_args);
CommonOperGroup cog_unionUnit64("union_unit_uint64", oper_policy::allow_extra_args);
Annotated<CommonOperGroup> cog_unionData (
	"Note that the first argument indicates the domain of the result and subsequent arguments (at least one) determine the unit and type of the resulting values."
,	token::union_data, oper_policy::allow_extra_args|oper_policy::can_explain_value
);

// *****************************************************************************
//                         UnionOperator
// *****************************************************************************
static TokenID s_UnionData = GetTokenID_st("UnionData");

class AbstrUnionOperator : public UnaryOperator // extra args are allowed
{
	ValueComposition m_VC;
public:
	AbstrUnionOperator(const Class* resCls, const DataItemClass* arg1Cls)
		: UnaryOperator(&cog_union, resCls, arg1Cls)
		,	m_VC(arg1Cls->GetValuesType()->GetValueComposition())
	{}

	// Override Operator
	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		SizeT n = args.size();                              assert(n>=1);

		auto arg1A = AsDataItem(args[0]);                   assert(arg1A);
		auto arg1_ValuesUnit = arg1A->GetAbstrValuesUnit(); assert(arg1_ValuesUnit);

		auto vc = m_VC;
		Unify(vc, arg1A->GetValueComposition());
		for (arg_index i=1; i!=n; ++i)
		{
			const AbstrDataItem* argA = AsCertainDataItem(args[i]);
			assert(argA);

			const AbstrUnit* currArg_ValuesUnit = argA->GetAbstrValuesUnit();
			Unify(vc, argA->GetValueComposition());

			assert(currArg_ValuesUnit);
			if (arg1_ValuesUnit->IsDefaultUnit())
				arg1_ValuesUnit = currArg_ValuesUnit;
			else
				currArg_ValuesUnit->UnifyValues(arg1_ValuesUnit, "Values of a next attribute wiht known values unit", "Values of the first attribute with known values unit", UnifyMode(UM_Throw | UM_AllowDefault));
		}

		AbstrUnit* resultDomain = debug_cast<const UnitClass*>(GetResultClass())->CreateResultUnit(resultHolder);
		assert(resultDomain);
		resultHolder = resultDomain;

		AbstrDataItem* resSub = CreateDataItem(resultDomain, s_UnionData, resultDomain, arg1_ValuesUnit, vc );
		MG_PRECONDITION(resSub);

		if (mustCalc)
		{
			SizeT count = 0;
			for (SizeT i=0; i!=n; ++i)
				count += debug_cast<const AbstrDataItem*>(args[i])->GetAbstrDomainUnit()->GetCount();

			resultDomain->SetCount(count);

			DataWriteLock resSubLock(resSub); 

			SizeT nrWritten = 0;

			for (arg_index i=0; i!=n; ++i)
			{
				const AbstrDataItem* argA = AsDataItem(args[i]);
				dms_assert(argA);
				DataReadLock argLock(argA);

				nrWritten = UnionCopy(resSubLock, argA, nrWritten);
			}
			dms_assert(nrWritten == count);

			resSubLock.Commit();
		}
		return true;
	}
	virtual SizeT UnionCopy(DataWriteLock& resSub, const AbstrDataItem* argA, SizeT nrCopied) const =0;
};

template <class V>
class UnionOperator : public AbstrUnionOperator // extra args are allowed
{
   typedef DataArray<V>     ArgType;
   typedef Unit<UInt32>     ResultType;
   typedef DataArray<V>     ResultSubType;

public:
	UnionOperator() : AbstrUnionOperator(ResultType::GetStaticClass(), ArgType::GetStaticClass()) 
	{}

	// Override Operator
	SizeT UnionCopy(DataWriteLock& resSub, const AbstrDataItem* argA, SizeT nrCopied) const  override
	{
		// don't use tileWriteChannel as it doesn't support parallel copying.
		ResultSubType* resultSub = mutable_array_cast<V>(resSub);
		auto resultSubData = resultSub->GetDataWrite(no_tile, dms_rw_mode::read_write); // TODO G8: copy tile by tile if possible; this will break non covering tilings and non-sequential tilings
		auto resultSubDataEnd = resultSubData.end();
		auto xx = resultSubData.begin() + nrCopied;
		dms_assert(!(resultSubDataEnd < xx));

		const ArgType* arg = const_array_cast<V>(argA);
		auto adu = argA->GetAbstrDomainUnit();
		tile_id tn = adu->GetNrTiles();

		std::atomic<SizeT> atomicNrCopied = nrCopied;
		parallel_tileloop_if_separable<V>(tn, [=, &atomicNrCopied](tile_id t)
			{
				auto x = xx + adu->GetTileFirstIndex(t);
				auto argData = arg->GetLockedDataRead(t);
				dms_assert(!(argData.size() && resultSubDataEnd < x + argData.size()));

				for (auto di = argData.begin(), de = argData.end(); di != de; ++x, ++di)
					Assign(*x, *di);

				atomicNrCopied += argData.size();
			}
		);
		return atomicNrCopied;
	}
};

// *****************************************************************************
//                         UnionUnitOperator
// *****************************************************************************
bool UnionUnit_impl(TreeItemDualRef& resultHolder, AbstrUnit* result, const ArgSeqType& args, bool mustCalc)
{
	dms_assert(args.size() >= 1);

	const AbstrUnit* arg1 = debug_cast<const AbstrUnit*>(args[0]);
	dms_assert(arg1);
	dms_assert(result);

	resultHolder = result;

	if (mustCalc)
	{
		row_id count = 0;
		for (arg_index i = 0, n = args.size(); i < n; ++i)
		{
			const AbstrUnit* adu = dynamic_cast<const AbstrUnit*>(args[i]);
			if (!adu)
				throwErrorF(cog_unionUnit.GetName().c_str(), "argument %d is not a Unit", i+1);
			for (tile_id t = 0, tn = adu->GetNrTiles(); t != tn; ++t)
			{
				auto tileCount = adu->GetTileCount(t);
				row_id newCount = count + tileCount;
				MG_CHECK2(newCount >= count,
					"Error in union_unit operator: the cumulation of the cardinalities of the arguments exceeds Max(SizeT)");
				count = newCount;
			}
		}
		result->SetCount(count);
	}
	return true;
}

template <typename ResultValueType>
class UnionUnitOperator : public UnaryOperator // extra args are allowed
{
   typedef AbstrUnit             ArgType;
   typedef Unit<ResultValueType> ResultType;

public:
   UnionUnitOperator(AbstrOperGroup& cog)
	   : UnaryOperator(&cog, ResultType::GetStaticClass(), ArgType::GetStaticClass())
	{}

   // Override Operator
	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		auto resultUnit = ResultType::GetStaticClass()->CreateResultUnit(resultHolder);
		resultUnit->SetTSF(TSF_Categorical);
		return UnionUnit_impl(resultHolder, resultUnit, args, mustCalc);
	}
};

// *****************************************************************************
//                         UnionDataOperator
// *****************************************************************************

// REMOVE TODO AbstrOperator
template <typename V>
class UnionDataOperator : public BinaryOperator // extra args are allowed
{
	typedef AbstrUnit            Arg1Type;
	typedef DataArray<V>         ArgType;
	typedef DataArray<V>         ResultType;

public:
	UnionDataOperator()
		:	BinaryOperator(&cog_unionData, ResultType::GetStaticClass(), Arg1Type::GetStaticClass(), ArgType::GetStaticClass())
	{}

   // Override Operator
	void CreateResultCaller(TreeItemDualRef& resultHolder, const ArgRefs& args, OperationContext*, LispPtr) const override
	{
		arg_index n = args.size() - 1;
		dms_assert(n >= 1);

		const AbstrUnit* resultDomain = AsUnit(GetItem(args[0]));

		ConstUnitRef constUnitRef;
		bool hadToTryWithoutCategoricalCheck = false;
		try {
			constUnitRef = compatible_values_unit_creator_func(1, &cog_unionData, GetItems(args), true);
		}
		catch (const DmsException& x)
		{
			constUnitRef = compatible_values_unit_creator_func(1, &cog_unionData, GetItems(args), false);
			reportF(SeverityTypeID::ST_Warning, "Depreciated usage of Union_data: %s"
				, x.AsErrMsg()->Why().c_str()
			);
			hadToTryWithoutCategoricalCheck = true;
		}

		auto vc = COMPOSITION(V);
		bool isCategorical = false;
		if (!hadToTryWithoutCategoricalCheck)
			for (arg_index i = 1; i <= n; ++i)
			{
				auto adi = AsDataItem(args[i]); assert(adi);
				if (adi->GetTSF(TSF_Categorical))
					isCategorical = true;
				Unify(vc, AsDataItem(args[i])->GetValueComposition());
			}

		if (resultHolder)
			return;

		resultHolder = CreateCacheDataItem(resultDomain, constUnitRef, vc );
		if (isCategorical)
			resultHolder->SetTSF(TSF_Categorical);
	}

	bool CalcResult(TreeItemDualRef& resultHolder, ArgRefs args, std::vector<ItemReadLock> readLocks, OperationContext* fc, Explain::Context* context) const override
	{
		dms_assert(resultHolder);

		const AbstrUnit* resultDomain = AsUnit(GetItem(args[0]));
		// aggregate resulting cardinality
		SizeT count = 0;
		arg_index n = args.size() - 1;
		for (arg_index i=1; i<=n; ++i)
		{
			const AbstrUnit* adu = AsDataItem(args[i])->GetAbstrDomainUnit();
			for (tile_id t = 0, tn= adu->GetNrTiles(); t!=tn; ++t)
				count += adu->GetTileCount(t);
		}

		resultDomain->ValidateCount(count);

		AbstrDataItem* res = AsDataItem(resultHolder.GetNew());

		dms_assert(!context || context->m_Domain && resultDomain->UnifyDomain(context->m_Domain, "r1", "e2"));
		dms_assert(!context || context->m_Coordinate);
		SizeT coordOffset = context ? context->m_Coordinate->first : 0;

		dms_assert(!res->m_DataObject || context);
		bool dontRecalc = IsDataReady(res);
		dms_assert(!dontRecalc || context);
		if (n == 1)
		{
			const AbstrDataItem* argA = AsDataItem(args[1]);

			DataReadLock argLock(argA);
			if (argLock->GetTiledRangeData() == resultDomain->GetTiledRangeData())
			{
				res->m_DataObject = argLock;
				return true;
			}
		}

		locked_tile_write_channel<V> resultDataChannel(dontRecalc ? nullptr : res);
		for (arg_index i = 1; i <= n; ++i)
		{
			const AbstrDataItem* argA = AsDataItem(args[i]);
			dms_assert(argA);
			const AbstrUnit* argDU = argA->GetAbstrDomainUnit();

			DataReadLock argLock(argA);
			const ArgType* arg = debug_cast<const ArgType*>(argA->GetCurrRefObj());
			for (tile_id t = 0, tn = argDU->GetNrTiles(); t != tn; ++t)
			{
				auto argData = arg->GetLockedDataRead(t);

				if (!dontRecalc)
					resultDataChannel.Write(argData.begin(), argData.end());
				if (context)
				{
					SizeT sz = argData.size();
					if (coordOffset < sz)
					{
						Explain::AddQueueEntry(context->m_CalcExpl, argDU, argDU->GetTileIndex(t, coordOffset));
						context = nullptr;
					}
					else
						coordOffset -= sz;
				}
			}
		}
		if (!dontRecalc)
		{
			dms_assert(resultDataChannel.IsEndOfChannel());
			resultDataChannel.Commit();
		}
		return true;
	}
};

// *****************************************************************************
//                               INSTANTIATION
// *****************************************************************************

namespace 
{

	template <typename X>
	struct UnionOpers
	{
		UnionOperator<X>     unionOrg;
		UnionDataOperator<X> unionData;
	};

	tl_oper::inst_tuple_templ<typelists::value_elements, UnionOpers > instUnionOpers;
	UnionUnitOperator<UInt32> unionUnit(cog_unionUnit);
	UnionUnitOperator<UInt8 > unionUnit08(cog_unionUnit08);
	UnionUnitOperator<UInt16> unionUnit16(cog_unionUnit16);
	UnionUnitOperator<UInt32> unionUnit32(cog_unionUnit32);
	UnionUnitOperator<UInt64> unionUnit64(cog_unionUnit64);

} // end anonymous namespace



