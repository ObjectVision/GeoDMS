//<HEADER> 
/*
Data & Model Server (DMS) is a server written in H++ for DSS applications. 
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

#pragma once

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
struct poly_oper_base: binary_assign_groupoid<typename sequence_traits<PointType>::container_type> 
{
	static ConstUnitRef unit_creator(const AbstrOperGroup* gr, const ArgSeqType& args) { return arg1_values_unit(args); }

	typedef typename union_poly_traits<PointType>::polygon_set_data_type polygon_set_data_type;
	mutable typename polygon_set_data_type::clean_resources cleanResources;
	mutable polygon_set_data_type tmp_ps;
	static void PrepareTile(typename sequence_traits<typename poly_oper_base::assignee_type>::seq_t res, typename sequence_traits<typename poly_oper_base::arg1_type>::cseq_t a1)
	{
		// XXX
	}

};

template <typename PointType> 
struct poly_and: poly_oper_base<PointType> 
{
	void Apply(typename poly_and::assignee_ref res, const typename poly_and::polygon_set_data_type& pa, const typename poly_and::polygon_set_data_type& pb) const
	{
		gtl::assign(this->tmp_ps, pa & pb, this->cleanResources);
		dms_assign(res, this->tmp_ps, this->cleanResources);
	}
};

template <typename PointType> 
struct poly_or: poly_oper_base<PointType> 
{
	void Apply(typename poly_or::assignee_ref res, const typename poly_or::polygon_set_data_type& pa, const typename poly_or::polygon_set_data_type& pb) const
	{
		gtl::assign(this->tmp_ps, pa | pb, this->cleanResources);
		dms_assign(res, this->tmp_ps, this->cleanResources);
	}
};

template <typename PointType> 
struct poly_xor: poly_oper_base<PointType> 
{
	void Apply(typename typename poly_xor::assignee_ref res, const typename poly_xor::polygon_set_data_type& pa, const typename poly_xor::polygon_set_data_type& pb) const
	{
		gtl::assign(this->tmp_ps, pa ^ pb, this->cleanResources);
		dms_assign(res, this->tmp_ps, this->cleanResources);
	}
};

template <typename PointType> 
struct poly_sub: poly_oper_base<PointType> 
{
	void Apply(typename poly_sub::assignee_ref res,
		const typename poly_sub::polygon_set_data_type& pa,
		const typename poly_sub::polygon_set_data_type& pb) const
	{
		gtl::assign(this->tmp_ps, pa - pb, this->cleanResources);
		dms_assign(res, this->tmp_ps, this->cleanResources);
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
	typedef typename Arg1Sequence::value_type Poly;
	typedef typename cref<Poly>::type arg1_cref;
	typedef typename cref<Poly>::type arg2_cref;
	typedef typename v_ref<Poly>::type res_ref;

	typedef field_of_t<Poly> PointType;
	typedef typename scalar_of<Poly>::type CrdType;
	typedef typename union_poly_traits<PointType>::polygon_set_data_type polygon_set_data_type;

	if (e1IsVoid)
	{
		dms_assert(arg1Data.size() == 1);

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

	dms_assert(arg1Data.size() == resData.size());

	if (e2IsVoid)
	{
		dms_assert(arg2Data.size() == 1);

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
			,	&PolyAttrOper::unit_creator, composition_of<typename PolyAttrOper::assignee_type>::value, ArgFlags()
			)
	{}

	void CalcTile(sequence_traits<typename PolyAttrOper::assignee_type>::seq_t resData, sequence_traits<typename PolyAttrOper::arg1_type>::cseq_t arg1Data, sequence_traits<typename PolyAttrOper::arg2_type>::cseq_t arg2Data, ArgFlags af MG_DEBUG_ALLOCATOR_SRC_ARG) const override
	{
		do_binary_poly_assign(resData, arg1Data, arg2Data, PolyAttrOper(), af & AF1_ISPARAM, af & AF2_ISPARAM);
	}
};


#endif //!defined(DMS_GEO_POLYOPER_H)
