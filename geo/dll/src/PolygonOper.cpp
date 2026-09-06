// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////

#include "GeoPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif

// dms_intersect, dms_union, dms_xor and dms_difference (issue #1214): the binary operators of
// the fault-tolerant sweep, the home-brewed fifth backend next to bp_ / bg_ / cgal_ / geos_.
// The sweep itself, and what it promises, is DMS_Traits.h; the rest of the family lives with
// the machinery it plugs into: dms_polygon, dms_union_polygon, their split_ variants,
// dms_overlay_polygon and dms_polygon_connectivity in BoostPolygon.cpp, and
// dms_minkowski_sum / dms_minkowski_difference in BoostGeometry_dms.cpp.
//
// Each operator has two forms: dms_xxx(a, b) derives its grid, and dms_xxx(a, b, grid) takes
// the grid size, the tolerance at which near-coincident vertices merge and slivers collapse.

#include "DMS_Traits.h"

#include "BoostGeometry.h"     // CheckGeometryArgComposition, and the operator headers behind it
#include "DataArrayValue.h"    // GetTheCurrValue: the grid parameter of the three-argument form
#include "UnitCreators.h"      // compatible_simple_values_unit_creator

namespace dms_overlay
{

// One tile: the element pairs, with either operand possibly a parameter (Void domain).
template <typename P, typename ResSeq, typename ArgSeq>
void CalcDmsOverlayTile(BoolOp op, CharPtr operName, Float64 grid
	, ResSeq& resData, const ArgSeq& arg1Data, const ArgSeq& arg2Data, bool e1IsVoid, bool e2IsVoid
	, CharPtr itemRef)
{
	SizeT n1 = arg1Data.size(), n2 = arg2Data.size();
	SizeT n = Max<SizeT>(n1, n2);
	assert(n1 == n || e1IsVoid);
	assert(n2 == n || e2IsVoid);
	assert(resData.size() == n);

	DmsOverlayEngine<P> engine(op, operName, grid);
	Timer processTimer;

	for (SizeT i = 0; i != n; ++i)
	{
		engine.Apply(resData[i], arg1Data[e1IsVoid ? 0 : i], arg2Data[e2IsVoid ? 0 : i]);

		if (processTimer.PassedSecs())
			reportF(SeverityTypeID::ST_MajorTrace, "{}{}: processed {} / {} sequences", itemRef, operName, AsString(i), AsString(n));
	}
	if (engine.NrCrossingPixels() || engine.NrExtraRounds() || engine.m_NrUndefined)
		reportF(SeverityTypeID::ST_MinorTrace, "{}{}: {} sequences; {} crossings snapped to the grid, {} extra noding rounds, {} undefined results"
			, itemRef, operName, AsString(n), AsString(engine.NrCrossingPixels()), AsString(engine.NrExtraRounds()), AsString(engine.m_NrUndefined));
}

// *****************************************************************************
//	the operators
// *****************************************************************************

template <typename P> using PolygonType_t = typename sequence_traits<P>::container_type;

// dms_xxx(a, b): BinaryAttrOper gives the domain unification, the Void broadcast and the tiling.
template <typename P, BoolOp Op>
struct DmsOverlayOperator : BinaryAttrOper<PolygonType_t<P>, PolygonType_t<P>, PolygonType_t<P>>
{
	using PolygonType = PolygonType_t<P>;
	using base_type = BinaryAttrOper<PolygonType, PolygonType, PolygonType>;
	using st = sequence_traits<PolygonType>;
	using seq_t = typename st::seq_t;
	using cseq_t = typename st::cseq_t;

	DmsOverlayOperator(AbstrOperGroup& gr)
		: base_type(&gr, compatible_simple_values_unit_creator, ValueComposition::Polygon)
	{}

	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		assert(args.size() == 2);
		CheckGeometryArgComposition(this->GetGroup(), AsDataItem(args[0]), ValueComposition::Polygon);
		CheckGeometryArgComposition(this->GetGroup(), AsDataItem(args[1]), ValueComposition::Polygon);
		return base_type::CreateResult(resultHolder, args, mustCalc);
	}

	void CalcTile(seq_t resData, cseq_t arg1Data, cseq_t arg2Data, ArgFlags af MG_DEBUG_ALLOCATOR_SRC_ARG) const override
	{
		bool e1IsVoid = (af & AF1_ISPARAM);
		bool e2IsVoid = (af & AF2_ISPARAM);
		CalcDmsOverlayTile<P>(Op, this->GetGroup()->GetNameStr(), 0.0, resData, arg1Data, arg2Data, e1IsVoid, e2IsVoid, "");
	}
};

// dms_xxx(a, b, grid): the same, with the grid size as a Float64 parameter.
class AbstrDmsOverlayGridOperator : public TernaryOperator
{
protected:
	AbstrDmsOverlayGridOperator(AbstrOperGroup& og, const DataItemClass* polyAttrClass)
		: TernaryOperator(&og, polyAttrClass
			, polyAttrClass
			, polyAttrClass
			, DataArray<Float64>::GetStaticClass()
		)
	{}

	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		assert(args.size() == 3);

		const AbstrDataItem* arg1A = AsDataItem(args[0]);
		const AbstrDataItem* arg2A = AsDataItem(args[1]);
		const AbstrDataItem* arg3A = AsDataItem(args[2]);
		assert(arg1A && arg2A && arg3A);

		CheckGeometryArgComposition(GetGroup(), arg1A, ValueComposition::Polygon);
		CheckGeometryArgComposition(GetGroup(), arg2A, ValueComposition::Polygon);

		const AbstrUnit* e1 = arg1A->GetAbstrDomainUnit(); bool e1IsVoid = e1->GetValueType() == ValueWrap<Void>::GetStaticClass();
		const AbstrUnit* e2 = arg2A->GetAbstrDomainUnit(); bool e2IsVoid = e2->GetValueType() == ValueWrap<Void>::GetStaticClass();
		const AbstrUnit* e3 = arg3A->GetAbstrDomainUnit(); bool e3IsVoid = e3->GetValueType() == ValueWrap<Void>::GetStaticClass();

		// One grid for the whole attribute: a tolerance that differs per element would make the
		// snapping of a shared boundary differ between neighbours.
		if (!e3IsVoid)
			arg3A->throwItemError("the grid size must be a parameter, not an attribute");

		const AbstrUnit* e = e1IsVoid ? e2 : e1;
		if (!e1IsVoid && !e2IsVoid)
			e1->UnifyDomain(e2, "e1", "e2", UM_Throw);

		const AbstrUnit* values1Unit = arg1A->GetAbstrValuesUnit();
		const AbstrUnit* values2Unit = arg2A->GetAbstrValuesUnit();
		values1Unit->UnifyValues(values2Unit, "v1", "v2", UM_Throw);

		if (!resultHolder)
			resultHolder = CreateCacheDataItem(e, values1Unit, ValueComposition::Polygon);

		if (mustCalc)
		{
			DataReadLock arg1Lock(arg1A);
			DataReadLock arg2Lock(arg2A);

			Float64 grid = GetTheCurrValue<Float64>(arg3A);
			if (!IsDefined(grid) || !(grid > 0.0) || !std::isfinite(grid))
				throwErrorF(GetGroup()->GetNameStr(), "the grid size must be a positive number, not {}", grid);

			auto itemRef = resultHolder.GetProgressPrefix(); // #795: names the config item

			auto resItem = AsDataItem(resultHolder.GetNew());
			DataWriteLock resLock(resItem, dms_rw_mode::write_only_mustzero);

			parallel_tileloop(e->GetNrTiles(), [=, this, resObj = resLock.get(), itemRefPtr = itemRef.c_str()](tile_id t)->void
				{
					this->Calculate(resObj, arg1A, arg2A, e1IsVoid, e2IsVoid, grid, t, itemRefPtr);
				}
			);

			resLock.Commit();
		}
		return true;
	}

	virtual void Calculate(AbstrDataObject* resObj, const AbstrDataItem* arg1A, const AbstrDataItem* arg2A
		, bool e1IsVoid, bool e2IsVoid, Float64 grid, tile_id t, CharPtr itemRef) const = 0;
};

template <typename P, BoolOp Op>
struct DmsOverlayGridOperator : AbstrDmsOverlayGridOperator
{
	using PolygonType = PolygonType_t<P>;
	using ArgType = DataArray<PolygonType>;

	DmsOverlayGridOperator(AbstrOperGroup& gr)
		: AbstrDmsOverlayGridOperator(gr, ArgType::GetStaticClass())
	{}

	void Calculate(AbstrDataObject* resObj, const AbstrDataItem* arg1A, const AbstrDataItem* arg2A
		, bool e1IsVoid, bool e2IsVoid, Float64 grid, tile_id t, CharPtr itemRef) const override
	{
		auto arg1Data = const_array_cast<PolygonType>(arg1A)->GetTile(e1IsVoid ? 0 : t);
		auto arg2Data = const_array_cast<PolygonType>(arg2A)->GetTile(e2IsVoid ? 0 : t);
		auto resData  = mutable_array_cast<PolygonType>(resObj)->GetWritableTile(t);

		CalcDmsOverlayTile<P>(Op, GetGroup()->GetNameStr(), grid, resData, arg1Data, arg2Data, e1IsVoid, e2IsVoid, itemRef);
	}
};

} // namespace dms_overlay

// *****************************************************************************
//	instantiation
// *****************************************************************************

static CommonOperGroup grDmsIntersect ("dms_intersect",  oper_policy::better_not_in_meta_scripting);
static CommonOperGroup grDmsUnion     ("dms_union",      oper_policy::better_not_in_meta_scripting);
static CommonOperGroup grDmsXOR       ("dms_xor",        oper_policy::better_not_in_meta_scripting);
static CommonOperGroup grDmsDifference("dms_difference", oper_policy::better_not_in_meta_scripting);

namespace
{
	using namespace dms_overlay;

	// two arguments: the grid is the integer grid, or derived for float coordinates
	template <typename P> using DmsIntersectOperator  = DmsOverlayOperator<P, BoolOp::Intersection>;
	template <typename P> using DmsUnionOperator      = DmsOverlayOperator<P, BoolOp::Union>;
	template <typename P> using DmsXorOperator        = DmsOverlayOperator<P, BoolOp::Xor>;
	template <typename P> using DmsDifferenceOperator = DmsOverlayOperator<P, BoolOp::Difference>;

	tl_oper::inst_tuple_templ<typelists::points, DmsIntersectOperator > dmsIntersectOperators (grDmsIntersect);
	tl_oper::inst_tuple_templ<typelists::points, DmsUnionOperator     > dmsUnionOperators     (grDmsUnion);
	tl_oper::inst_tuple_templ<typelists::points, DmsXorOperator       > dmsXorOperators       (grDmsXOR);
	tl_oper::inst_tuple_templ<typelists::points, DmsDifferenceOperator> dmsDifferenceOperators(grDmsDifference);

	// three arguments: an explicit grid size, the tolerance
	template <typename P> using DmsIntersectGridOperator  = DmsOverlayGridOperator<P, BoolOp::Intersection>;
	template <typename P> using DmsUnionGridOperator      = DmsOverlayGridOperator<P, BoolOp::Union>;
	template <typename P> using DmsXorGridOperator        = DmsOverlayGridOperator<P, BoolOp::Xor>;
	template <typename P> using DmsDifferenceGridOperator = DmsOverlayGridOperator<P, BoolOp::Difference>;

	tl_oper::inst_tuple_templ<typelists::points, DmsIntersectGridOperator > dmsIntersectGridOperators (grDmsIntersect);
	tl_oper::inst_tuple_templ<typelists::points, DmsUnionGridOperator     > dmsUnionGridOperators     (grDmsUnion);
	tl_oper::inst_tuple_templ<typelists::points, DmsXorGridOperator       > dmsXorGridOperators       (grDmsXOR);
	tl_oper::inst_tuple_templ<typelists::points, DmsDifferenceGridOperator> dmsDifferenceGridOperators(grDmsDifference);
}
