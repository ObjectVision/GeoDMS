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

#include "OperAttrTer.h"

#include "mci/CompositeCast.h"

#include "Prototypes.h"
#include "AttrTerStruct.h"

// *****************************************************************************
//										ApplyTernaryFunc
// *****************************************************************************

template <typename TTerAssignOper, typename ResData, typename Arg1Data, typename Arg2Data, typename Arg3Data>
void ApplyTernaryAssign(ResData& resData, 
	const Arg1Data& arg1Data,
	const Arg2Data& arg2Data,
	const Arg3Data& arg3Data, 
	ArgFlags af,
	const TTerAssignOper& terAssignOper = TTerAssignOper()
)
{
	if (af & AF1_ISPARAM)
	{
		if (af & AF2_ISPARAM)
		{
			dms_assert(resData.size() == arg3Data.size());

			typename cref<typename Arg1Data::value_type>::type v1 = arg1Data[0];
			typename cref<typename Arg2Data::value_type>::type v2 = arg2Data[0];

			if constexpr (Ampable<TTerAssignOper>)
				transform_assign_amp(resData.begin(), arg3Data.begin(), arg3Data.end(), 
					[v1, v2, terAssignOper](typename ResData::reference rr, typename cref<typename Arg3Data::value_type>::type a3) restrict(cpu, amp) { terAssignOper(rr, v1, v2, a3);  }
				);
			else
				transform_assign_cpu(resData.begin(), arg3Data.begin(), arg3Data.end(),
					[v1, v2, terAssignOper](typename ResData::reference rr, typename cref<typename Arg3Data::value_type>::type a3) restrict(cpu) { terAssignOper(rr, v1, v2, a3);  }
				);
		}
		else 
		{
			dms_assert(resData.size() == arg2Data.size());
			if (af & AF3_ISPARAM)
			{
				transform_assign(resData.begin(), arg2Data.begin(), arg2Data.end(),
					[terAssignOper, v1 = arg1Data[0], v3 = arg3Data[0]](typename ResData::reference rr, typename cref<typename Arg2Data::value_type>::type a2)
				{
					terAssignOper(rr, v1, a2, v3);
				}
				);
			}
			else
			{
				dms_assert(resData.size() == arg3Data.size());

				transform_assign(resData.begin(), arg2Data.begin(), arg2Data.end(), arg3Data.begin(),
					[terAssignOper, v1 = arg1Data[0]](typename ResData::reference rr, typename cref<typename Arg2Data::value_type>::type a2, typename cref<typename Arg3Data::value_type>::type a3)
					{
						terAssignOper(rr, v1, a2, a3);
					}
				);
			}
		}
	}
	else // e1 not void
	{
		dms_assert(resData.size() == arg1Data.size());
		if (af & AF2_ISPARAM)
			if (af & AF3_ISPARAM)
			{
				transform_assign(resData.begin(), arg1Data.begin(), arg1Data.end(),
					[terAssignOper, v2 = arg2Data[0], v3 = arg3Data[0]](typename ResData::reference rr, typename cref<typename Arg1Data::value_type>::type a1)
					{
						terAssignOper(rr, a1, v2, v3);
					}
				);
			}
			else // e3 not void
			{
				dms_assert(resData.size() == arg3Data.size());
				transform_assign(resData.begin(), arg1Data.begin(), arg1Data.end(), arg3Data.begin(),
					[terAssignOper, v2 = arg2Data[0]](typename ResData::reference rr, typename cref<typename Arg1Data::value_type>::type a1, typename cref<typename Arg3Data::value_type>::type a3)
					{
						terAssignOper(rr, a1, v2, a3);
					}
				);
			}
		else // e2 not void
		{
			dms_assert(resData.size() == arg2Data.size());

			if (af & AF3_ISPARAM)
			{
				transform_assign(resData.begin(), arg1Data.begin(), arg1Data.end(), arg2Data.begin(), 
					[terAssignOper, v3 = arg3Data[0]](typename ResData::reference rr, typename cref<typename Arg1Data::value_type>::type a1, typename cref<typename Arg2Data::value_type>::type a2)
					{	
						terAssignOper(rr, a1, a2, v3); 
					}
				);
			}
			else // all non-void
			{
				dms_assert(resData.size() == arg3Data.size());

				transform_assign(resData.begin(), arg1Data.begin(), arg1Data.end(), arg2Data.begin(), arg3Data.begin(), 
					terAssignOper
				);
			}
		}
	}
};

// *****************************************************************************
//										TernaryAttrOperator
// *****************************************************************************

template <typename AttrOper>
struct MonalTernaryAttrOperator : TernaryAttrOper<typename AttrOper::assignee_type, typename AttrOper::arg1_type, typename AttrOper::arg2_type, typename AttrOper::arg3_type>
{
	MonalTernaryAttrOperator(AbstrOperGroup* gr) 
		: TernaryAttrOper<typename AttrOper::assignee_type, typename AttrOper::arg1_type, typename AttrOper::arg2_type, typename AttrOper::arg3_type>(gr
			, &AttrOper::unit_creator, COMPOSITION(typename AttrOper::assignee_type), false
			)
	{}

	// Override TernaryAttrOper
	void CalcTile(sequence_traits<typename AttrOper::assignee_type>::seq_t resData
		,	sequence_traits<typename AttrOper::arg1_type>::cseq_t arg1Data
		,	sequence_traits<typename AttrOper::arg2_type>::cseq_t arg2Data
		,	sequence_traits<typename AttrOper::arg3_type>::cseq_t arg3Data, ArgFlags af MG_DEBUG_ALLOCATOR_SRC_ARG) const override
	{
		ApplyTernaryAssign<AttrOper>(resData, arg1Data, arg2Data, arg3Data, af);
	}
};

template <typename AttrOper>
struct DualTernaryAttrOperator : TernaryAttrOper<typename AttrOper::assignee_type, typename AttrOper::arg1_type, typename AttrOper::arg2_type, typename AttrOper::arg3_type>
{
	typedef ternary_assign_checked<AttrOper> CheckedAttrOper;

	DualTernaryAttrOperator(AbstrOperGroup* gr) 
		: TernaryAttrOper<typename AttrOper::assignee_type, typename AttrOper::arg1_type, typename AttrOper::arg2_type, typename AttrOper::arg3_type>(gr
			, &AttrOper::unit_creator, COMPOSITION(typename AttrOper::assignee_type), true
			)
	{}

	// Override TernaryAttrOper
	void CalcTile(sequence_traits<typename AttrOper::assignee_type>::seq_t resData
		,	sequence_traits<typename AttrOper::arg1_type>::cseq_t arg1Data
		,	sequence_traits<typename AttrOper::arg2_type>::cseq_t arg2Data
		,	sequence_traits<typename AttrOper::arg3_type>::cseq_t arg3Data, ArgFlags af MG_DEBUG_ALLOCATOR_SRC_ARG) const override
	{
		if	( af & ( AF1_HASUNDEFINED | AF2_HASUNDEFINED | AF3_HASUNDEFINED))
			ApplyTernaryAssign<CheckedAttrOper>(resData, arg1Data, arg2Data, arg3Data, af);
		else
			ApplyTernaryAssign<      AttrOper>(resData, arg1Data, arg2Data, arg3Data, af);
	}
};

// *****************************************************************************
//								Specific Ternary functors
// *****************************************************************************

template<class T>	
struct iif_assign: ternary_assign<T, Bool, T, T>
{
	static ConstUnitRef unit_creator(const AbstrOperGroup* gr, const ArgSeqType& args)
	{
		return compatible_values_unit_creator_func(1, gr, args);
	}
	
	void operator()(typename iif_assign::assignee_ref res, typename iif_assign::arg1_cref b, typename iif_assign::arg2_cref t1, typename iif_assign::arg3_cref t2) const
	{ 
		Assign(res, (b ? t1 : t2));
	}
};

#include "geo/Color.h"

template<class T>	
struct rgb_assign_base : ternary_assign<UInt32, T, T, T>
{
	static ConstUnitRef unit_creator(const AbstrOperGroup* gr, const ArgSeqType& args) { return default_unit_creator<UInt32>(); }
};

//template<typename T> struct rgb_assign;

template<typename T> //requires (!is_ampable_v<T>)
struct rgb_assign_cpu : rgb_assign_base<T>
{
	void operator()(typename rgb_assign_cpu::assignee_ref res, typename rgb_assign_cpu::arg1_cref r, typename rgb_assign_cpu::arg2_cref g, typename rgb_assign_cpu::arg3_cref b) const
	{
		Assign(res, CombineRGB(r, g, b));
	}
};

template<typename T>
struct rgb_assign_amp : rgb_assign_base<T>
{
	void operator()(typename rgb_assign_amp::assignee_ref res, typename rgb_assign_amp::arg1_cref r, typename rgb_assign_amp::arg2_cref g, typename rgb_assign_amp::arg3_cref b) const
	restrict(amp, cpu)
	{
		res =	(((UInt32)b) << 16)
			|	(((UInt32)g) << 8)
			|	((UInt32)r)
		;
	}
};

struct rgb_amp_generator
{
	template <typename T>
	using apply = rgb_assign_amp < T>;
};

struct rgb_cpu_generator
{
	template <typename T>
	using apply = rgb_assign_cpu < T>;
};

template<typename T> //requires (!is_ampable_v<T>)
struct rgb_assign : std::conditional_t<is_ampable_v<T>, rgb_amp_generator, rgb_cpu_generator>::template apply<T>
{
};

template<typename T> constexpr bool is_ampable_v<rgb_assign<T>> = is_ampable_v<T>;

struct substr_assign3 : ternary_assign<SharedStr, SharedStr, UInt32, UInt32>
{
	static ConstUnitRef unit_creator(const AbstrOperGroup* gr, const ArgSeqType& args) { return arg1_values_unit(args); }

	void operator()(typename ternary_assign::assignee_ref res, typename ternary_assign::arg1_cref arg1, UInt32 arg2, UInt32 arg3) const
	{
		UInt32 orgSize = arg1.size();
		MakeMin(arg2, orgSize);
		MakeMin(arg3, orgSize-arg2);
		arg3 += arg2;
		res.assign(arg1.begin() + arg2, arg1.begin() + arg3);
	}
};

struct replace_assign : ternary_assign<SharedStr, SharedStr, SharedStr, SharedStr>
{
	static ConstUnitRef unit_creator(const AbstrOperGroup* gr, const ArgSeqType& args)
	{
		return arg1_values_unit(args);
	}


	void operator()(typename ternary_assign::assignee_ref res, typename ternary_assign::arg1_cref arg1, typename ternary_assign::arg2_cref arg2, typename ternary_assign::arg3_cref arg3) const
	{
		ReplaceAssign<typename ternary_assign::assignee_ref>(res, arg1, arg2, arg3);
	}
};


// *****************************************************************************
//											INSTANTIATION
// *****************************************************************************

#include "OperUnit.h"
#include "RtcTypeLists.h"
#include "utl/TypeListOper.h"

namespace 
{
	CommonOperGroup cog_Iif("iif"), cog_Rgb("rgb");

	template <typename X>
	struct iifOperator
	{
		iifOperator() : m_OperData(&cog_Iif) {}
		MonalTernaryAttrOperator<iif_assign<X> > m_OperData;
	};
	template <typename X>
	struct rgbOperator
	{
		rgbOperator() : m_OperData(&cog_Rgb) {}

		DualTernaryAttrOperator<rgb_assign<X> > m_OperData;
	};

	tl_oper::inst_tuple<typelists::value_elements, iifOperator<_>> s_iifOperators;
	tl_oper::inst_tuple<typelists::ints          , rgbOperator<_>> s_rgbOperators;


	DualTernaryAttrOperator<substr_assign3>	s_subStrOperatorU (&cog_substr);

	CommonOperGroup cogReplace("replace");

	DualTernaryAttrOperator<replace_assign>	s_ReplaceOperatorU(&cogReplace);
}
