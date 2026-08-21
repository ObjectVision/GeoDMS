// Copyright (C) 1998-2024 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "ClcPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif

#include "Invert.h"

#include "mci/CompositeCast.h"
#include "mci/ValueClass.h"

#include "DataArray.h"
#include "DataItemClass.h"
#include "Explain.h"
#include "OperSignature.h"
#include "ParallelTiles.h"
#include "Unit.h"
#include "UnitClass.h"
#include "TreeItemClass.h"
#include "UnitProcessor.h"

#include "RtcTypeLists.h"
#include "utl/TypeListOper.h"

// *****************************************************************************
//									Invert
// *****************************************************************************
namespace { // anonymous

CommonOperGroup cog_Invert   ("invert", oper_policy::dynamic_result_class);
CommonOperGroup cog_InvertAll("invertAll", oper_policy::dynamic_result_class);

// rlookup(D->V, E->V): D->E'
// invert(A->B) := B->A';
// => rlookup(D->D, E->D) == invert(E->D)
// rewrite: invert(X) -> rlookup(ID(ValuesUnit(X)), X)
// rewrite: invert(A->B) -> rlookup(B->B, A->B)

static StaticLateTokenID nextToken("Next");

struct AbstrInvertOperator : public UnaryOperator
{
	typedef AbstrDataItem     ResultType;
	typedef AbstrDataItem     ResultSubType;
	AbstrInvertOperator(bool all, const Class* arg1Cls)
		:	UnaryOperator(all ? &cog_InvertAll : &cog_Invert
			,	ResultType::GetStaticClass()
			,	arg1Cls
			)
		,	m_All(all)
	{
		// #612: this operator can say which argument row produced a result element, and -- more to the
		// point -- that there is no such row when the result element is null.
		(all ? &cog_InvertAll : &cog_Invert)->SetCanExplainValue();
	}

	// mirrors CreateResult below: invert(x: B[A]) -> A[B] -- DOUBLE cross-role:
	// x's VALUES unit is the result's DOMAIN and x's DOMAIN is the result's
	// VALUES (reduction-honest: domains are UnifyDomain-checked everywhere and
	// the result is flagged categorical, so declared-values conflicts also fail
	// CheckResultItem's UnifyDomain discharge). A carries no member class (its
	// class is the argument's domain class, dynamic); it flows through the
	// companion when A binds
	bool DescribeSignature(AbstrSignatureBuilder& sb) const override
	{
		auto argCls = dynamic_cast<const DataItemClass*>(GetArgClass(0));
		if (!argCls)
			return false;
		sig_var A = sb.UnitVar("A"), B = sb.UnitVar("B");
		sb.MemberValueClass(B, argCls->GetValuesType());
		sb.ArgAttr(0, B, A, argCls->GetValuesType()->GetValueComposition());
		sb.ResultAttr(A, B, ValueComposition::Single);
		return true;
	}

	// Overridden because SetCanExplainValue() makes the Operator base refuse the default caller;
	// the creation itself is unchanged.
	void CreateResultCaller(TreeItemDualRef& resultHolder, const ArgRefs& args, LispPtr) const override
	{
		if (resultHolder && !resultHolder.IsTmp())
			return;

		auto argSeq = GetItems(args);
		MG_CHECK(CreateResult(resultHolder, argSeq, false));
		assert(resultHolder);
	}

	// Idem, plus the #612 explanation: this runs a second time, with a context, when a value-info page
	// explains one element of an already calculated result.
	bool CalcResult(TreeItemDualRef& resultHolder, const ArgRefs& args, std::vector<ItemReadLock> readLocks, Explain::Context* context) const override
	{
		assert(resultHolder);
		assert(args.size() == 1);

		AbstrDataItem* res = AsDataItem(resultHolder.GetNew());
		assert(res);

		if (!res->m_DataObject)
		{
			auto argSeq = GetItems(args);
			if (!CreateResult(resultHolder, argSeq, true))
				return false;
		}
		if (context)
			ExplainResultElement(AsDataItem(args[0]), context);
		return true;
	}

	// Report which rows of arg1 map onto the result row being explained, so that they show up as
	// suppliers of that row, and say so when there are none, which is exactly when the result is null.
	static void ExplainResultElement(const AbstrDataItem* arg1A, Explain::Context* context)
	{
		assert(arg1A);
		assert(context && context->m_Coordinate);

		// result row f is an element of arg1's values unit; invert maps it back to the rows of arg1's
		// domain that hold value f.
		SizeT f = context->m_Coordinate->first;
		const AbstrUnit* entity = arg1A->GetAbstrDomainUnit();

		DataReadLock arg1Lock(arg1A);
		const AbstrDataObject* arg1Obj = arg1A->GetCurrRefObj().get();
		assert(arg1Obj);

		SizeT i = 0, k = 0, n = arg1Obj->GetTiledRangeData()->GetElemCount();
		while (i < n)
		{
			i = arg1Obj->FindPosOfSizeT(f, i);
			if (!IsDefined(i))
				break;
			Explain::AddQueueEntry(context->m_CalcExpl, entity, i);
			if (++k >= Explain::MaxNrEntries)
				break;
			++i;
		}
		if (!k)
		{
			// Name the argument when it has a name. Usually it has none: a configured attribute is
			// substituted by its definition, so what arrives here is an anonymous cache item (its domain
			// unit likewise). The page prints the expression right next to this note, which names it.
			auto subjectName = SharedStr(arg1A->GetFullName());
			Explain::SetValueReason(context, subjectName.empty()
				? SharedStr("no row of the inverted attribute refers to this row")
				: mySSPrintF("no row of {} refers to this row", subjectName));
		}
	}

	// Override Operator
	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		assert(args.size() == 1);

		const AbstrDataItem* arg1A= debug_cast<const AbstrDataItem*>(args[0]);
		assert(arg1A);

		const AbstrUnit* entity    = arg1A->GetAbstrDomainUnit();
		const AbstrUnit* newDomain = arg1A->GetAbstrValuesUnit();

		assert(entity);
		assert(newDomain);

		if (!resultHolder)
		{
			resultHolder = CreateCacheDataItem(newDomain, entity);
			resultHolder->SetTSF(TSF_Categorical);
		}
		resultHolder->m_StatusFlags.SetHasSortedValues(arg1A->m_StatusFlags.HasSortedValues());
		AbstrDataItem* res = AsDataItem(resultHolder.GetNew());

		AbstrDataItem* resSub = nullptr;
		if (m_All)
		{
			resSub = CreateDataItem(res, nextToken, entity, entity).get(); // owned by res
		}

		if (mustCalc)
		{
			DataReadLock arg1Lock(arg1A);

			DataWriteLock resLock1(res);
			DataWriteLock resLock2(resSub); // there's no mechanism yet to do invertAll with tiles

			Calculate(arg1A, resLock1, resLock2);

			if (m_All)
				resLock2.Commit();
			resLock1.Commit();
		}
		return true;
	}
	virtual void  Calculate(const AbstrDataItem* arg1A, DataWriteLock& res, DataWriteLock& resSub) const= 0;

protected:
	bool m_All;
};

template <class T>
class InvertOperator : public AbstrInvertOperator
{
	typedef DataArray<T>      Arg1Type;
			
public:
	InvertOperator(bool all)
		:	AbstrInvertOperator(all, Arg1Type::GetStaticClass())
	{}

	void  Calculate(const AbstrDataItem* arg1A, DataWriteLock& res, DataWriteLock& resSub) const override
	{
		const Arg1Type* arg1 = const_array_cast<T>(arg1A);
		assert(arg1);

		DataCheckMode dcm = (res->GetTiledRangeData()->GetNrTiles() > 1) ? DCM_CheckRange : arg1A->GetCheckMode();
		visit<typelists::domain_elements>(arg1A->GetAbstrDomainUnit(), 
			[&res, &resSub, arg1A, dcm] <typename E> (const Unit<E>* inviter)
			{
				DataArray<E>* resultObj = mutable_array_cast<E>(res);
				DataArray<E>* resSubData = resSub ? mutable_array_cast<E>(resSub) : nullptr;
				tile_id at_n = arg1A->GetAbstrDomainUnit()->GetNrTiles();
				tile_id rt_n = res->GetTiledRangeData()->GetNrTiles();

				typename Unit<E>::range_t entityRange = inviter->GetRange();

				if (!at_n)
					parallel_tileloop(rt_n, [arg1A, rt_n, &resultObj](tile_id rt)->void
						{
							auto resultDataPerV = resultObj->GetWritableTile(rt, dms_rw_mode::write_only_all);
							fast_fill(resultDataPerV.begin(), resultDataPerV.end(), UNDEFINED_OR_ZERO(E));
						}
					);
				else for (tile_id at = 0; at != at_n; ++at)  // iterate over tiles of E
				{
					auto arg1Data = const_array_cast<T>(arg1A)->GetTile(at);
					typename Unit<E>::range_t arg1TileRange = inviter->GetTileRange(at);
					typename Unit<T>::range_t valueRange;
					if constexpr (is_bitvalue_v<T>)
					{
						assert(rt_n == 1);
//						valueRange = Unit<T>::range_t(0, 1 << nrbits_of<T>);
					}
					else
					{
						if (rt_n > 1)
							valueRange = typename Unit<T>::range_t(arg1Data.begin(), arg1Data.end(), dcm != DCM_None, true);
					}
					DataCheckMode dcm2 = IsIncluding(arg1TileRange, entityRange) ? dcm: DCM_CheckRange;

					typename DataArray<E>::locked_seq_t resSubArray;
					if (resSubData)
					{
						resSubArray = resSubData->GetDataWrite(at, dms_rw_mode::write_only_all);
						if (rt_n > 1) // init separately (and not inline) only when multiple result tiles will be run with this resSubArray and it will be unknonw which is the first run, if any.
							fast_fill(resSubArray.begin(), resSubArray.end(), UNDEFINED_OR_ZERO(E));
					}

					//				for (tile_id rt = 0; rt!=rt_n; ++rt)  // iterate over tiles of V
					parallel_tileloop(rt_n, [arg1A, rt_n, &valueRange, &resultObj, &resSubArray, &arg1Data, arg1TileRange, dcm2, at](tile_id rt)->void
						{
							auto resultTileRange = const_array_cast<T>(arg1A)->GetValueRangeData()->GetTileRange(rt);
							auto resultDataPerV = resultObj->GetWritableTile(rt, at == 0 ? dms_rw_mode::write_only_all : dms_rw_mode::read_write);
							if (at == 0)
								fast_fill(resultDataPerV.begin(), resultDataPerV.end(), UNDEFINED_OR_ZERO(E));
							if (rt_n <= 1 || IsTouching(valueRange, resultTileRange))
								invertAll2values_array<T, E>(resultDataPerV
									, resSubArray ? &resSubArray : nullptr
									, arg1Data
									, resultTileRange
									, arg1TileRange
									, dcm2
									, (rt_n <= 1) // init resSubArray if this is the only run
								);
						}
					);
				}
			}
		);
	}
};

// *****************************************************************************
//                               INSTANTIATION
// *****************************************************************************

	template <typename T>
	struct InvertOperators
	{
		InvertOperator<T> i1, i2;
		InvertOperators()
			:	i1(true)
			,	i2(false)
		{}

	};

	tl_oper::inst_tuple_templ<typelists::domain_elements, InvertOperators > invertOperators;

} // end anonymous namespace
