// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "GeoPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

#include "async.h"

#include "mci/CompositeCast.h"
#include "mth/Mathlib.h"
#include "geo/PointOrder.h"
#include "ser/AsString.h"
#include "ser/RangeStream.h"
#include "set/VectorFunc.h"
#include "utl/Swap.h"

#include "Param.h"
#include "UnitClass.h"
#include "DataItemClass.h"

// *****************************************************************************
//									RELATIONAL FUNCTIONS
// *****************************************************************************

// *****************************************************************************
//									ConvertAttrToPointOperator
// *****************************************************************************

template <class T>
class ConvertAttrToPointOperator : public TernaryOperator
{
	typedef Point<T>                       PointType;
	typedef DataArray<T>                   Arg1Type;	
	typedef DataArray<T>                   Arg2Type;	
	typedef Unit<PointType>	               Arg3Type;
	typedef DataArray<PointType>           ResultType;

	bool m_IsTraditionalGisOrder = false;

public:
	ConvertAttrToPointOperator(AbstrOperGroup* gr, arg_index nrArgs, bool isTraditionalGisOrder)
		:	TernaryOperator(gr, 
				ResultType::GetStaticClass(), 
				Arg1Type::GetStaticClass(), Arg2Type::GetStaticClass(), Arg3Type::GetStaticClass()
			) 
		,	m_IsTraditionalGisOrder(isTraditionalGisOrder)
	{
		if (nrArgs == 2)
			--m_ArgClassesEnd;
	}

	// Override Operator
	void CreateResultCaller(TreeItemDualRef& resultHolder, const ArgRefs& args, OperationContext*, LispPtr) const override
	{
		if (resultHolder)
			return;

		const AbstrDataItem* arg1A = AsDataItem(args[0]); assert(arg1A);
		const AbstrDataItem* arg2A = AsDataItem(args[1]); assert(arg2A);

		auto arg3A = (args.size() > 2)
			? GetItem(args[2])
			: Arg3Type::GetStaticClass()->CreateDefault();
		assert(arg3A);

		const Arg3Type* arg3 = debug_cast<const Arg3Type*>(arg3A); assert(arg3);

		const AbstrUnit* entity1 = arg1A->GetAbstrDomainUnit();
		const AbstrUnit* entity2 = arg2A->GetAbstrDomainUnit();
		entity1->UnifyDomain(entity2, "e1", "e2", UM_Throw);

		resultHolder = CreateCacheDataItem(entity1, arg3);
		assert(resultHolder);
	}

	bool CalcResult(TreeItemDualRef& resultHolder, ArgRefs args, std::vector<ItemReadLock> readLocks, OperationContext*, Explain::Context* context = nullptr) const override
	{
		dms_assert(resultHolder);

		const AbstrDataItem* arg1A = AsDataItem(args[0]);
		const AbstrDataItem* arg2A = AsDataItem(args[1]);
		assert(arg1A); 
		assert(arg2A); 

		const AbstrUnit* entity1 = arg1A->GetAbstrDomainUnit();

		// GROTE WISSELTRUUK
		bool colFirst = m_IsTraditionalGisOrder;
		if (!colFirst)
			colFirst = g_cfgColFirst;

		if (colFirst != dms_order_tag::col_first)
		{
			omni::swap(arg1A, arg2A);
		}

		DataReadLock arg1Lock(arg1A);
		DataReadLock arg2Lock(arg2A);

		AbstrDataItem* res = AsDataItem(resultHolder.GetNew());
		auto tn = entity1->GetNrTiles();

		if (IsMultiThreaded3() && (tn > 1) && (LTF_ElementWeight(arg1A) + LTF_ElementWeight(arg2A) <= LTF_ElementWeight(res)))
			res->m_DataObject = CreateFutureTileFunctor(res, res->GetLazyCalculatedState(), res->GetAbstrValuesUnit(), arg1A, arg2A);
		else
		{
			DataWriteLock resLock(res);
			auto resTileFunctor = mutable_array_cast<PointType>(resLock);
			auto arg1 = const_array_cast<T>(arg1A);
			auto arg2 = const_array_cast<T>(arg2A);

			parallel_tileloop(tn, [this, &resLock, arg1, arg2, resTileFunctor](tile_id t)
				{
					auto arg1Data = arg1->GetTile(t);
					auto arg2Data = arg2->GetTile(t);

					auto resData = resTileFunctor->GetWritableTile(t);
					this->CalcTile(resData, arg1Data, arg2Data);
				}
			);
			resLock.Commit();
		}
		return true;
	}

	SharedPtr<const AbstrDataObject> CreateFutureTileFunctor(SharedPtr<AbstrDataItem> resultAdi, bool lazy, const AbstrUnit* valuesUnitA, const AbstrDataItem* arg1A, const AbstrDataItem* arg2A) const
	{
		auto tileRangeData = AsUnit(arg1A->GetAbstrDomainUnit()->GetCurrRangeItem())->GetTiledRangeData();
		auto valuesUnit = debug_cast<const Unit<PointType>*>(valuesUnitA);
		auto arg1 = MakeShared(const_array_cast<T>(arg1A)); assert(arg1);
		auto arg2 = MakeShared(const_array_cast<T>(arg2A)); assert(arg2);

		using prepare_data = std::pair<SharedPtr<typename Arg1Type::future_tile>, SharedPtr<typename Arg2Type::future_tile>>;
		auto futureTileFunctor = make_unique_FutureTileFunctor<PointType, prepare_data, false>(resultAdi, lazy, tileRangeData, get_range_ptr_of_valuesunit(valuesUnit)
			, [arg1, arg2](tile_id t) { return prepare_data{ arg1->GetFutureTile(t), arg2->GetFutureTile(t) }; }
			, [this](sequence_traits<PointType>::seq_t resData, prepare_data futureData)
			{
				auto futureTileA = throttled_async([&futureData] { return futureData.first->GetTile();  });
				auto tileB = futureData.second->GetTile();
				this->CalcTile(resData, futureTileA.get().get_view(), tileB.get_view());
			}
			MG_DEBUG_ALLOCATOR_SRC("Point tile functor")
			);

		return futureTileFunctor.release();
	}

	// conform BinaryAttrOper
	void CalcTile(sequence_traits<PointType>::seq_t resData, sequence_traits<T>::cseq_t arg1Data, sequence_traits<T>::cseq_t arg2Data) const
	{
		auto cardinality = arg1Data.size();
		assert(arg2Data.size() == cardinality);
		assert(resData.size() == cardinality);

		auto
			b1 = arg1Data.begin(),
			e1 = arg1Data.end();
		auto b2 = arg2Data.begin();
		auto br = resData.begin();


		for (; b1 != e1; ++br, ++b1, ++b2)
			if (!IsDefined(*b1) || !IsDefined(*b2))
				MakeUndefined(*br);
			else
				*br = PointType(*b1, *b2);
	}
};

// *****************************************************************************
//									Distance. 
// *****************************************************************************
// Distance. 
//
// The function distance : 
//		POINTS X RECTSET -> S<POINTS, n> X V<n, R> 
// is defined by distance(p, set)[i] = ((set[i], i), 
// distance(p, set[i])), 1 <= i <= | set |.
//#include "Param.h"
#include "DataArray.h"

using square_dist_type = UInt32;

// TODO G8: generalize DistOper for domains with other Point types (IPoint, UPoint, WPoint) and resulting square_dist_types

class Dist2Operator : public BinaryOperator
{
	typedef DataArray<SPoint>           Arg1Type; // domain of dist matrix
	typedef Unit<square_dist_type>      Arg2Type; // resulting values unit for resulting dist2 attr of newly created entity
	typedef DataArray<square_dist_type> ResultType;
			
public:
	Dist2Operator(AbstrOperGroup* gr)
		:	BinaryOperator(gr, ResultType::GetStaticClass(), 
				Arg1Type::GetStaticClass(), Arg2Type::GetStaticClass()
			)
	{}

	// Override Operator
	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		dms_assert(args.size() == 2);

		const AbstrDataItem* arg1A = AsDataItem(args[0]);
		const Arg2Type* arg2 = const_unit_cast<square_dist_type>(args[1]);
		dms_assert(arg1A);
		dms_assert(arg2);

		checked_domain<Void>(arg1A, "a1");
		const Unit<SPoint>* domain = const_unit_cast<SPoint>(arg1A->GetAbstrValuesUnit());
		dms_assert(domain);

		if (!resultHolder)
			resultHolder = CreateCacheDataItem(domain, arg2);

		if (mustCalc)
		{
			const Arg1Type* arg1 = const_array_cast<SPoint>(arg1A);
			dms_assert(arg1);

			SPoint mid     = GetCurrValue<SPoint>(arg1A, 0);
			SRect  rect    = domain->GetRange();
			UInt32 size    = Cardinality(Size(rect));
			UInt32 nrCol   = Size(rect).Col();

			assert(rect.first.first  <= rect.second.first);
			assert(rect.first.second <= rect.second.second);

			TreeItem* res = resultHolder;
			DataWriteLock resLock(debug_cast<AbstrDataItem*>(res));
			auto resultData =  mutable_array_cast<UInt32>(resLock)->GetDataWrite();
			dms_assert(resultData.size() == size);

			ResultType::iterator dai = resultData.begin();
			for (Int16 row=rect.first.Row();  row!=rect.second.Row();  ++row)
			{
				UInt32 sumSqr = sqr(Int32(mid.Row()-row));
				Int32  colV   =  rect.first.Col()- mid.Col();
				sumSqr += sqr(colV);
				for (ResultType::iterator dae = dai + nrCol; dai != dae; ++dai)
				{
					*dai = sumSqr;
					sumSqr += colV; ++colV; sumSqr += colV;
				}
			}
			resLock.Commit();
		}
		return true;
	}
};

// *****************************************************************************
//											FUNCTORS
// *****************************************************************************

#include "Prototypes.h"
#include "UnitCreators.h"

template <typename Scalar>
struct point2rowFunc : unary_func<Scalar, Point<Scalar> >
{
	static ConstUnitRef unit_creator(const AbstrOperGroup* gr, const ArgSeqType& args) { return default_unit_creator<Scalar>(); }

	typename point2rowFunc::res_type operator()(typename point2rowFunc::arg1_cref x) const
	{
		return x.Row();
	}
};

template <typename Scalar>
struct point2colFunc : unary_func<Scalar, Point<Scalar> >
{
	static ConstUnitRef unit_creator(const AbstrOperGroup* gr, const ArgSeqType& args) { return default_unit_creator<Scalar>(); }

	typename point2colFunc::res_type operator()(typename point2colFunc::arg1_cref x) const
	{
		return x.Col();
	}
};


// *****************************************************************************
//											INSTANTIATION
// *****************************************************************************


#include "RtcTypeLists.h"
#include "utl/TypeListOper.h"

#include "LispTreeType.h"

#include "OperAttrUni.h"

namespace 
{
	Obsolete< CommonOperGroup> cog_Point("obsolete function point called.\n"
		"Use the point_xy operation to unambiguously define points.\n"
		"Exchange the first and second argument unless ColRowOrder was set in Config.ini"
		, token::point);

	CommonOperGroup cog_PointRow("pointrow"), cog_PointCol("pointcol");

	CommonOperGroup
		cog_PointXY("point_xy"), cog_GetX("get_x"), cog_GetY("get_y");

	template <typename P>
	struct PointOpers
	{
		typedef typename scalar_of<P>::type S;

		PointOpers()
			: ca2Point(&cog_Point, 2, false), ca2PointXY(&cog_Point, 2, true)
			, ca3Point(&cog_Point, 3, false), ca3PointXY(&cog_Point, 3, true)
			, ca2Row(&cog_PointRow)
			, ca2Col(&cog_PointCol)
			, ca2X(&cog_GetX)
			, ca2Y(&cog_GetY)
		{}

		ConvertAttrToPointOperator<S> ca2Point, ca3Point;
		ConvertAttrToPointOperator<S> ca2PointXY, ca3PointXY;

		UnaryAttrSpecialFuncOperator<point2rowFunc<S> > ca2Row, ca2X;
		UnaryAttrSpecialFuncOperator<point2colFunc<S> > ca2Col, ca2Y;
	};
	//	oper_arg_policy oap_point[3] = { oper_arg_policy::calc_as_result, oper_arg_policy::calc_as_result, oper_arg_policy::calc_never };
	tl_oper::inst_tuple_templ<typelists::points, PointOpers > pointOpers;

	CommonOperGroup dist2("dist2");       
	Dist2Operator dist2Oper(&dist2);
}
