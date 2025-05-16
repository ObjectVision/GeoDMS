// Copyright (C) 1998-2025 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

#if !defined(__CLC_OPERATTRUNI_H)
#define __CLC_OPERATTRUNI_H

#include "mci/CompositeCast.h"

#include "AbstrUnit.h"

#include "AttrUniStruct.h"
#include "ParallelTiles.h"
#include "TileFunctorImpl.h"
#include "UnitProcessor.h"

// *****************************************************************************
//											UnaryAttrOperator
// *****************************************************************************

struct AbstrUnaryAttrOperator: UnaryOperator
{
	AbstrUnaryAttrOperator(AbstrOperGroup* gr 
		,	const DataItemClass* resCls
		,	const DataItemClass* argCls 
		,	ArgFlags             possibleArgFlags
		,	UnitCreatorPtr       ucp
		,	ValueComposition     vc
		)	:	UnaryOperator(gr, resCls, argCls)
			,	m_UnitCreatorPtr(std::move(ucp))
			,	m_VC(vc)
			,	m_PossibleArgFlags(possibleArgFlags)
	{}

	auto EstimatePerformance(TreeItemDualRef& resultHolder, const ArgRefs& args) -> PerformanceEstimationData override
	{
		auto result = UnaryOperator::EstimatePerformance(resultHolder, args);
		result.expectedCalcTime *= GetGroup()->GetCalcFactor();
		return result;
	}

	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		MG_PRECONDITION(args.size() == 1);

		auto arg1A = AsDataItem(args[0]);
		assert(arg1A);

		const AbstrUnit* e = arg1A->GetAbstrDomainUnit();
		assert(e);

		if (!resultHolder)
			resultHolder = CreateCacheDataItem(e, (*m_UnitCreatorPtr)(GetGroup(), args), m_VC);

		if (mustCalc)
		{
			auto res = AsDataItem(resultHolder.GetNew());
			assert(res);

			DataReadLock arg1Lock(arg1A);

			ArgFlags af = ArgFlags();
			if ((m_PossibleArgFlags & AF1_HASUNDEFINED) && arg1A->HasUndefinedValues())
				af = ArgFlags(af | AF1_HASUNDEFINED);
			auto tn = e->GetNrTiles();

			auto valuesUnitA = AsUnit(res->GetAbstrValuesUnit()->GetCurrRangeItem());
			if (IsMultiThreaded3() && (tn > 1) && (LTF_ElementWeight(arg1A) <= LTF_ElementWeight(res)))
				AsDataItem(resultHolder.GetOld())->m_DataObject = CreateFutureTileFunctor(res, res->GetLazyCalculatedState(), valuesUnitA, arg1A, af MG_DEBUG_ALLOCATOR_SRC("res->md_FullName + GetGroup()->GetName().c_str()"));
			else
			{
				DataWriteLock resLock(res);

				parallel_tileloop(tn,
					[&resLock, this, arg1A, af](tile_id t)->void
					{
						Calculate(resLock, arg1A, af, t);
					}
				);

				resLock.Commit();
			}
		}
		return true;
	}

	virtual SharedPtr<const AbstrDataObject> CreateFutureTileFunctor(SharedPtr<AbstrDataItem> resultAdi, bool lazy, const AbstrUnit* valuesUnitA, const AbstrDataItem* arg1A, ArgFlags af MG_DEBUG_ALLOCATOR_SRC_ARG) const = 0;
	virtual void Calculate(AbstrDataObject* borrowedDataHandle, const AbstrDataItem* arg1A, ArgFlags af, tile_id t) const =0;

private:
	UnitCreatorPtr   m_UnitCreatorPtr;
	ValueComposition m_VC;
	ArgFlags         m_PossibleArgFlags;
};


template <typename ResultValueType, typename Arg1ValueType>
struct UnaryAttrOperator : AbstrUnaryAttrOperator
{
	using Arg1Type = DataArray<Arg1ValueType>;
	using ResultType = DataArray<ResultValueType>;

public:
	UnaryAttrOperator(AbstrOperGroup* gr, ArgFlags possibleArgFlags, UnitCreatorPtr ucp, ValueComposition vc)
		: AbstrUnaryAttrOperator(gr, ResultType::GetStaticClass(), Arg1Type::GetStaticClass(), possibleArgFlags, ucp, vc)
	{}

	auto CreateFutureTileFunctor(SharedPtr<AbstrDataItem> resultAdi, bool lazy, const AbstrUnit* valuesUnitA, const AbstrDataItem* arg1A, ArgFlags af MG_DEBUG_ALLOCATOR_SRC_ARG) const -> SharedPtr<const AbstrDataObject> override
	{
		auto tileRangeData = AsUnit(arg1A->GetAbstrDomainUnit()->GetCurrRangeItem())->GetTiledRangeData();
		auto valuesUnit = debug_cast<const Unit<field_of_t<ResultValueType>>*>(valuesUnitA);

		auto arg1 = MakeShared(const_array_cast<Arg1ValueType>(arg1A)); assert(arg1);
		auto arg1VU = MakeShared(arg1A->GetAbstrValuesUnit());

		using prepare_data = SharedPtr<typename Arg1Type::future_tile>;
		auto futureTileFunctor = make_unique_FutureTileFunctor<ResultValueType, prepare_data, false>(resultAdi, lazy, tileRangeData, get_range_ptr_of_valuesunit(valuesUnit)
			, [arg1, af](tile_id t) { return arg1->GetFutureTile(t); }
			, [this, arg1VU, af MG_DEBUG_ALLOCATOR_SRC_PARAM](sequence_traits<ResultValueType>::seq_t resData, prepare_data futureData)
			{
				this->CalcTile(resData, futureData->GetTile().get_view(), arg1VU, af MG_DEBUG_ALLOCATOR_SRC_PARAM);
			}
			MG_DEBUG_ALLOCATOR_SRC_PARAM
		);

		return futureTileFunctor.release();
	}

	void Calculate(AbstrDataObject* res, const AbstrDataItem* arg1A, ArgFlags af, tile_id t) const override
	{
		auto arg1Data = const_array_cast<Arg1ValueType>(arg1A)->GetTile(t);
		auto resData = mutable_array_cast<ResultValueType>(res)->GetWritableTile(t);

		CalcTile(resData, arg1Data, arg1A->GetAbstrValuesUnit(), af MG_DEBUG_ALLOCATOR_SRC("res->md_SrcStr"));
	}

	virtual void CalcTile(sequence_traits<ResultValueType>::seq_t resData, sequence_traits<Arg1ValueType>::cseq_t arg1Data, const AbstrUnit* argVU, ArgFlags af MG_DEBUG_ALLOCATOR_SRC_ARG) const = 0;
};


template <typename TUniAssign>
struct UnaryAttrAssignOperator : UnaryAttrOperator<typename TUniAssign::assignee_type, typename TUniAssign::arg1_type>
{
	UnaryAttrAssignOperator(AbstrOperGroup* gr) 
		: UnaryAttrOperator<typename TUniAssign::assignee_type, typename TUniAssign::arg1_type>(gr
			,	AF1_HASUNDEFINED
			,	&TUniAssign::unit_creator
			,	composition_of<typename TUniAssign::assignee_type>::value
			)
	{}

	void CalcTile(sequence_traits<typename TUniAssign::assignee_type>::seq_t resData, sequence_traits<typename TUniAssign::arg1_type>::cseq_t arg1Data, const AbstrUnit* argVU, ArgFlags af MG_DEBUG_ALLOCATOR_SRC_ARG) const override
	{
		assert(arg1Data.size() == resData.size());

		do_unary_assign(resData, arg1Data, TUniAssign(), af & AF1_HASUNDEFINED, TYPEID(TUniAssign));
	}
};

template <typename TUniOper>
struct UnaryAttrFuncOperator: UnaryAttrOperator<typename TUniOper::res_type, typename TUniOper::arg1_type>
{
	UnaryAttrFuncOperator(AbstrOperGroup* gr) 
		: UnaryAttrOperator<typename TUniOper::res_type, typename TUniOper::arg1_type>(gr
			,	ArgFlags(AF1_HASUNDEFINED)
			,	&TUniOper::unit_creator, composition_of<typename TUniOper::res_type>::value
			)
	{}

	void CalcTile(sequence_traits<typename TUniOper::res_type>::seq_t resData, sequence_traits<typename TUniOper::arg1_type>::cseq_t arg1Data, const AbstrUnit* argVU, ArgFlags af MG_DEBUG_ALLOCATOR_SRC_ARG) const override
	{
		assert(arg1Data.size() == resData.size());

		do_unary_func(resData, arg1Data, m_AttrOper, af & AF1_HASUNDEFINED);
	}
private:
	TUniOper m_AttrOper = TUniOper();
};

template <typename TUniOper>
struct UnaryAttrSpecialFuncOperator: UnaryAttrOperator<typename TUniOper::res_type, typename TUniOper::arg1_type>
{
	UnaryAttrSpecialFuncOperator(AbstrOperGroup* gr) 
		: UnaryAttrOperator<typename TUniOper::res_type, typename TUniOper::arg1_type>(gr
			,	ArgFlags()
			,	&TUniOper::unit_creator, composition_of<typename TUniOper::res_type>::value
			)
	{}

	void CalcTile(sequence_traits<typename TUniOper::res_type>::seq_t resData, sequence_traits<typename TUniOper::arg1_type>::cseq_t arg1Data, const AbstrUnit* argVU, ArgFlags af MG_DEBUG_ALLOCATOR_SRC_ARG) const override
	{
		assert(arg1Data.size() == resData.size());

		dms_transform(arg1Data.begin(), arg1Data.end(), resData.begin(), m_AttrOper);
	}
private:
	TUniOper m_AttrOper = TUniOper();
};

#include "Projection.h"

template <typename PointType, bool TAKE_THE_Y, typename ResCoordType = Float64>
struct UnaryAttr_XY_FuncOperator : UnaryAttrOperator<ResCoordType, PointType>
{
	UnaryAttr_XY_FuncOperator(AbstrOperGroup* gr)
		: UnaryAttrOperator<ResCoordType, PointType>(gr
			, ArgFlags()
			, &default_unit_creator<ResCoordType>, ValueComposition::Single
		)
	{
	}

	void CalcTile(sequence_traits<ResCoordType>::seq_t resData, sequence_traits<PointType>::cseq_t arg1Data, const AbstrUnit* argVU, ArgFlags af MG_DEBUG_ALLOCATOR_SRC_ARG) const override
	{
		assert(arg1Data.size() == resData.size());

		auto tr = UnitProjection::GetCompositeTransform(argVU->GetProjection());

		if constexpr (TAKE_THE_Y)
			dms_transform(arg1Data.begin(), arg1Data.end(), resData.begin(), [fy = tr.Factor().Y(), ty = tr.Offset().Y()](const auto& p) { return p.Y() * fy + ty;  });
		else
			dms_transform(arg1Data.begin(), arg1Data.end(), resData.begin(), [fx = tr.Factor().X(), tx = tr.Offset().X()](const auto& p) { return p.X() * fx + tx;  });
	}
};


#endif //!defined(__CLC_OPERATTRUNI_H)
