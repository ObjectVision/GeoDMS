// Copyright (C) 1998-2024 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

#if !defined(DMS_GEO_POLYOPER_H)
#define DMS_GEO_POLYOPER_H

#include "GeoBase.h"

#include "AttrBinStruct.h"
#include "OperAttrBin.h"
#include "Prototypes.h"
#include "UnitCreators.h"

#include "geo/BoostPolygon.h"

template <typename G> 
struct binary_assign_groupoid: binary_assign<G, G, G> {};

template <typename PointType> 
struct bp_poly_oper_base: binary_assign_groupoid<typename sequence_traits<PointType>::container_type> 
{
	static ConstUnitRef unit_creator(const AbstrOperGroup* gr, const ArgSeqType& args) { return arg1_values_unit(args); }
	using traits = bp_union_poly_traits<PointType>;
	using polygon_set_data_type = typename traits::polygon_set_data_type;
	mutable typename polygon_set_data_type::clean_resources cleanResources;
	mutable polygon_set_data_type tmp_ps;

/* REMOVE
	static void PrepareTile(typename sequence_traits<typename bp_poly_oper_base::assignee_type>::seq_t res, typename sequence_traits<typename bp_poly_oper_base::arg1_type>::cseq_t a1)
	{
		// XXX
	}
	*/
};

template <typename PointType> 
struct poly_and: bp_poly_oper_base<PointType> 
{
	void Apply(typename poly_and::assignee_ref res, const typename poly_and::polygon_set_data_type& pa, const typename poly_and::polygon_set_data_type& pb) const
	{
		gtl::assign(this->tmp_ps, pa & pb, this->cleanResources);
		bp_assign(res, this->tmp_ps, this->cleanResources);
	}
};

template <typename PointType> 
struct poly_or: bp_poly_oper_base<PointType>
{
	void Apply(typename poly_or::assignee_ref res, const typename poly_or::polygon_set_data_type& pa, const typename poly_or::polygon_set_data_type& pb) const
	{
		gtl::assign(this->tmp_ps, pa | pb, this->cleanResources);
		bp_assign(res, this->tmp_ps, this->cleanResources);
	}
};

template <typename PointType> 
struct poly_xor: bp_poly_oper_base<PointType>
{
	void Apply(typename typename poly_xor::assignee_ref res, const typename poly_xor::polygon_set_data_type& pa, const typename poly_xor::polygon_set_data_type& pb) const
	{
		gtl::assign(this->tmp_ps, pa ^ pb, this->cleanResources);
		bp_assign(res, this->tmp_ps, this->cleanResources);
	}
};

template <typename PointType> 
struct poly_sub: bp_poly_oper_base<PointType>
{
	void Apply(typename poly_sub::assignee_ref res,
		const typename poly_sub::polygon_set_data_type& pa,
		const typename poly_sub::polygon_set_data_type& pb) const
	{
		gtl::assign(this->tmp_ps, pa - pb, this->cleanResources);
		bp_assign(res, this->tmp_ps, this->cleanResources);
	}
};

// *****************************************************************************
//			BinaryAttrFunc operators
// *****************************************************************************

template<typename ResSequence, typename Arg1Sequence, typename Arg2Sequence, typename AttrOper>
void do_binary_poly_assign(
	ResSequence resData,
	const Arg1Sequence& arg1Data,
	const Arg2Sequence& arg2Data,
	const AttrOper&      oper,
	bool e1IsVoid, bool e2IsVoid)
{
	using Poly = typename Arg1Sequence::value_type;
	using arg1_cref =cref_t<Poly>;
	using arg2_cref = cref_t<Poly>;
	using res_ref = vref_t<Poly> ;

	using PointType = field_of_t<Poly> ;
	using CrdType = scalar_of_t<Poly>;
	using traits = typename AttrOper::traits;
	using polygon_set_data_type = typename traits::polygon_set_data_type;

	if (e1IsVoid)
	{
		assert(arg1Data.size() == 1);

		if (!IsDefined(arg1Data[0]))
			fast_undefine(resData.begin(), resData.end());
		else
		{
			arg1_cref arg1Value = arg1Data[0];
			polygon_set_data_type pa;
			gtl::assign(pa, arg1Value, oper.cleanResources);

			do_unary_assign(resData, arg2Data
			,	[pa, &oper](res_ref res, arg2_cref arg2)
				{
					polygon_set_data_type pb;
					gtl::assign(pb, arg2, oper.cleanResources);
					oper.Apply(res, pa, pb);
				}
			,	true
			,	TYPEID(AttrOper)
			);
		}
		return;
	}

	assert(arg1Data.size() == resData.size());

	if (e2IsVoid)
	{
		assert(arg2Data.size() == 1);

		if (!IsDefined(arg2Data[0]))
			fast_undefine(resData.begin(), resData.end());
		else
		{
			arg2_cref arg2Value = arg2Data[0];
			polygon_set_data_type pb;
			gtl::assign(pb, arg2Value, oper.cleanResources);
			do_unary_assign(
				resData
			,	arg1Data
//			,	composition_2_p_v<AttrOper>(oper, arg2Data[0])
			,	[pb, &oper](res_ref res, arg1_cref arg1)  {
					polygon_set_data_type pa; gtl::assign(pa, arg1, oper.cleanResources); 
					oper.Apply(res, pa, pb); }
			,	true
			,	TYPEID(AttrOper)
			);
		}
		return;
	}

	dms_assert(arg2Data.size() == resData.size());

	transform_assign(
		resData.begin(), 
		arg1Data.begin(), arg1Data.end(),
		arg2Data.begin(), 
		[&oper](res_ref res, arg1_cref arg1, arg1_cref arg2)
		{
			if (IsDefined(arg1) && IsDefined(arg2))
			{
				polygon_set_data_type pa; gtl::assign(pa, arg1, oper.cleanResources); 
				polygon_set_data_type pb; gtl::assign(pb, arg2, oper.cleanResources);
				oper.Apply(res, pa, pb);
			}
			else
				MakeUndefined(res);
		}
	);
}

// *****************************************************************************
//			BinaryAttrFunc operators
// *****************************************************************************

template <typename PolyAttrOper>
struct BinaryPolyAttrAssignOper : BinaryAttrOper< typename PolyAttrOper::assignee_type, typename PolyAttrOper::arg1_type, typename PolyAttrOper::arg2_type>
{
	BinaryPolyAttrAssignOper(AbstrOperGroup* gr) 
		: BinaryAttrOper<typename PolyAttrOper::assignee_type, typename PolyAttrOper::arg1_type, typename PolyAttrOper::arg2_type>(gr
			,	&PolyAttrOper::unit_creator, composition_of_v<typename PolyAttrOper::assignee_type>
			)
	{}

	void CalcTile(sequence_traits<typename PolyAttrOper::assignee_type>::seq_t resData, sequence_traits<typename PolyAttrOper::arg1_type>::cseq_t arg1Data, sequence_traits<typename PolyAttrOper::arg2_type>::cseq_t arg2Data, ArgFlags af MG_DEBUG_ALLOCATOR_SRC_ARG) const override
	{
		do_binary_poly_assign(resData, arg1Data, arg2Data, PolyAttrOper(), af & AF1_ISPARAM, af & AF2_ISPARAM);
	}
};


#endif //!defined(DMS_GEO_POLYOPER_H)
