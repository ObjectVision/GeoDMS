// Copyright (C) 2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "ClcPCH.h"
#pragma hdrstop

#include "mci/CompositeCast.h"
#include "geo/CheckedCalc.h"

#include "AttrBinStruct.h"
#include "LispTreeType.h"
#include "OperAttrBin.h"
#include "OperUnit.h"
#include "UnitCreators.h"

// *****************************************************************************
//			StrConcat operator
// *****************************************************************************

using tile_future = std::function<TileCRef>;

struct StrConcatOperator : BinaryAttrOper<SharedStr, SharedStr, SharedStr>
{
	StrConcatOperator(AbstrOperGroup* gr) 
		: BinaryAttrOper(gr, compatible_simple_values_unit_creator, COMPOSITION(SharedStr), ArgFlags())
	{}

	// Override BinaryAttrOper
	void CalcTile(sequence_traits<SharedStr>::seq_t resData, sequence_traits<SharedStr>::cseq_t arg1Data, sequence_traits<SharedStr>::cseq_t arg2Data, ArgFlags af MG_DEBUG_ALLOCATOR_SRC_ARG) const override
	{
		auto cardinality = resData.size();

		Arg1Type::const_iterator a1i = arg1Data.begin();
		Arg2Type::const_iterator a2i = arg2Data.begin();

		bool e1Void = (af & AF1_ISPARAM);
		bool e2Void = (af & AF2_ISPARAM);

		dms_assert(arg1Data.size() == (e1Void ? 1 : cardinality));
		dms_assert(arg2Data.size() == (e2Void ? 1 : cardinality));

		using data_size_type = sequence_traits<SharedStr>::seq_t::data_size_type;
		data_size_type
			totalSize =
				CheckedAdd<data_size_type>(
					CheckedMul<data_size_type>(e1Void ? cardinality : 1, arg1Data.get_sa().actual_data_size())
				,	CheckedMul<data_size_type>(e2Void ? cardinality : 1, arg2Data.get_sa().actual_data_size())
				);
		if (e1Void && !a1i->IsDefined()) totalSize = 0;
		if (e2Void && !a2i->IsDefined()) totalSize = 0;

		resData.get_sa().data_reserve(totalSize MG_DEBUG_ALLOCATOR_SRC_PARAM);
		dms_assert(resData.size() == cardinality);

		ResultType::iterator
			ri = resData.begin(),
			re = resData.end();

		SizeT a1Size, a2Size;
		sequence_traits<char>::const_pointer a1Begin = nullptr, a1End = nullptr, a2Begin = nullptr, a2End = nullptr;
		bool a1Defined, a2Defined;
		if (e1Void && (a1Defined = a1i->IsDefined())) { a1Begin = a1i->begin(); a1End = a1i->end(); a1Size = a1End - a1Begin; }
		if (e2Void && (a2Defined = a2i->IsDefined())) { a2Begin = a2i->begin(); a2End = a2i->end(); a2Size = a2End - a2Begin; }

		for (;ri != re; ++ri)
		{
			if (!e1Void) { if (a1Defined = a1i->IsDefined()) { a1Begin = a1i->begin(); a1End = a1i->end(); a1Size = a1End - a1Begin; } ++a1i; }
			if (!e2Void) { if (a2Defined = a2i->IsDefined()) { a2Begin = a2i->begin(); a2End = a2i->end(); a2Size = a2End - a2Begin; } ++a2i; }

			if (a1Defined && a2Defined)
			{
				ri->resize_uninitialized(a1Size + a2Size);
				fast_copy(a2Begin, a2End, fast_copy(a1Begin, a1End, ri->begin()));
			}
			else
				Assign(*ri, Undefined());
		}
	}
};

// *****************************************************************************
//			Str2Operator operator
// *****************************************************************************

template <typename SubTerFunc>
struct Str2Operator : BinaryAttrOper<SharedStr, SharedStr, strpos_t>
{
	Str2Operator(AbstrOperGroup& aog, SubTerFunc&& sf)
		: BinaryAttrOper(&aog, arg1_values_unit, COMPOSITION(SharedStr), ArgFlags())
		,	m_SubFunc(std::move(sf))
	{}

	// Override BinaryAttrOper
	void CalcTile(sequence_traits<SharedStr>::seq_t resData, sequence_traits<SharedStr>::cseq_t arg1Data, sequence_traits<strpos_t>::cseq_t arg2Data, ArgFlags af MG_DEBUG_ALLOCATOR_SRC_ARG) const override
	{
		bool e1Void = af & AF1_ISPARAM;
		bool e2Void = af & AF2_ISPARAM;

		auto cardinality = resData.size();

		auto a1b = arg1Data.begin();
		auto a2b = arg2Data.begin();

		dms_assert(arg1Data.size() == (e1Void ? 1 : cardinality));
		dms_assert(arg2Data.size() == (e2Void ? 1 : cardinality));

		using data_size_type = sequence_traits<SharedStr>::seq_t::data_size_type;
		data_size_type totalSize = 0;
		for (auto i = 0; i != cardinality; ++i) {
			auto size = a1b[e1Void ? 0 : i].size();
			auto pos = a2b[e2Void ? 0 : i];
			if (pos < size)
				totalSize += size - pos;
		}
		resData.get_sa().data_reserve(totalSize MG_DEBUG_ALLOCATOR_SRC_PARAM);
		dms_assert(resData.size() == cardinality);

		ResultType::iterator
			ri = resData.begin(),
			re = resData.end();
		for (; ri != re; ++ri)
		{
			strpos_t pos = *a2b;

			auto range = m_SubFunc(a1b->begin(), a1b->end(), pos);
			ri->assign(range.first, range.second);

			if (!e1Void) ++a1b;
			if (!e2Void) ++a2b;
		}
	}
	SubTerFunc m_SubFunc;
};

template <typename SubTerFunc>
auto makeStr2Oper(AbstrOperGroup& aog, SubTerFunc&& sf)
{
	return std::make_unique<Str2Operator<SubTerFunc>>(aog, std::move(sf));
}

// *****************************************************************************
//							String2Operator
// *****************************************************************************

#include "geo/StringArray.h"

template <typename TA>
struct String2Operator : BinaryAttrOper<SharedStr, TA, decpos_t>
{

	String2Operator() 
		: BinaryAttrOper<SharedStr, TA, decpos_t>(GetUnitGroup<SharedStr>(), default_unit_creator<SharedStr>, ValueComposition::Single, ArgFlags())
	{}

	// Override BinaryAttrOper
	void CalcTile(sequence_traits<SharedStr>::seq_t resData, sequence_traits<TA>::cseq_t arg1Data, sequence_traits<decpos_t>::cseq_t arg2Data, ArgFlags af MG_DEBUG_ALLOCATOR_SRC_ARG) const override
	{
		bool e1Void = af & AF1_ISPARAM;
		bool e2Void = af & AF2_ISPARAM;

		auto cardinality = resData.size();

		auto a1i = arg1Data.begin();
		auto a2i = arg2Data.begin();

		dms_assert(e1Void || arg1Data.size() == cardinality);
		dms_assert(e2Void || arg2Data.size() == cardinality);

		auto ri = resData.begin();

		for (;cardinality--; ++ri)
		{
			AsString(*ri, *a1i, *a2i);
			if (!e1Void) ++a1i;
			if (!e2Void) ++a2i;
			
		}
	}
};

// *****************************************************************************
//							RepeatOperator
// *****************************************************************************

CommonOperGroup cog_repeat("repeat");

void Repeat(StringRef& res, const StringCRef& arg, strpos_t count)
{
	if (!IsDefined(count))
		MakeUndefined(res);
	else
	{
		res.resize_uninitialized(arg.size()*count);
		StringRef::iterator i = res.begin();
		for (; count; --count)
			i = fast_copy(arg.begin(), arg.end(), i);
	}
}

struct RepeatOperator : BinaryAttrOper<SharedStr, SharedStr, strpos_t>
{
	RepeatOperator() 
		: BinaryAttrOper(&cog_repeat, default_unit_creator<SharedStr>, ValueComposition::Single, ArgFlags())
	{}

	// Override BinaryAttrOper
	void CalcTile(sequence_traits<SharedStr>::seq_t resData, sequence_traits<SharedStr>::cseq_t arg1Data, sequence_traits<strpos_t>::cseq_t arg2Data, ArgFlags af MG_DEBUG_ALLOCATOR_SRC_ARG) const override
	{
		bool e1Void = af & AF1_ISPARAM;
		bool e2Void = af & AF2_ISPARAM;

		auto cardinality = resData.size();

		auto a1i = arg1Data.begin();
		auto a2i = arg2Data.begin();

		dms_assert(e1Void || arg1Data.size() == cardinality);
		dms_assert(e2Void || arg2Data.size() == cardinality);

		SizeT size = 0;
		for (;cardinality; --cardinality)
		{
			auto count = *a2i;
			if (IsDefined(count))
				size += a1i->size() * count;
			if (!e1Void) ++a1i;
			if (!e2Void) ++a2i;
			
		}
		resData.get_sa().data_reserve(size MG_DEBUG_ALLOCATOR_SRC_PARAM);

		cardinality = resData.size();
		a1i = arg1Data.begin();
		a2i = arg2Data.begin();

		dms_assert(e1Void || arg1Data.size() == cardinality);
		dms_assert(e2Void || arg2Data.size() == cardinality);

		ResultType::iterator ri = resData.begin();

		for (;cardinality; ++ri, --cardinality)
		{
			Repeat(*ri, *a1i, *a2i);
			if (!e1Void) ++a1i;
			if (!e2Void) ++a2i;
			
		}
	}
};

// *****************************************************************************
//											DEFINES
// *****************************************************************************

template <typename P> using norm_type_t = product_type_t<scalar_of_t<P>>;
template <typename P> using dist_type_t = div_type_t<norm_type_t<P>>;

template <typename P> struct sqrdist_func: binary_func<norm_type_t<P>, P, P>
{
	static ConstUnitRef unit_creator(const AbstrOperGroup* gr, const ArgSeqType& args) { return default_unit_creator<norm_type_t<P>>(); }

	auto operator ()(cref_t<P> a, cref_t<P> b) const
	{
		return Norm<norm_type_t<P>>(a - b);
	} 
};

template <typename P> struct dist_func: binary_func<dist_type_t<P>, P, P>
{
	static ConstUnitRef unit_creator(const AbstrOperGroup* gr, const ArgSeqType& args) { return default_unit_creator<typename dist_func::res_type>(); }

	dist_type_t<P> operator ()(cref_t<P> a, cref_t<P> b) const
	{
		return sqrt( Norm<Float64>(a - b ) );
	} 
};


// *****************************************************************************
//		BINARY FUNCTORS TAKEN FROM std
// *****************************************************************************

template <typename T> struct compare_func_base : binary_func<Bool, T, T> 
{
	static ConstUnitRef unit_creator(const AbstrOperGroup* gr, const ArgSeqType& args) { return compare_unit_creator(gr, args, true); }

};

template <typename T, typename Cmp> struct checked_compare_func : compare_func_base<T>
{
	Bool operator()(cref_t<T> a, cref_t<T> b) const
	{
		if constexpr (has_undefines_v<T>)
		{
			if (!IsDefined(a) || !IsDefined(b))
				return false;
//				throwDmsErrD("Invalid attempt to compare undefined values!");
		}
		return cmp(a, b);
	}
	Cmp cmp;
};

template <typename T> struct equal_to : compare_func_base<T>
{
	Bool operator ()(cref_t<T> a, cref_t<T> b) const
	{
		if constexpr (has_undefines_v<T>)
			if (!IsDefined(a) || !IsDefined(b))
				return false;
		return a == b;
	}
};

template <typename T> struct not_equal_to : compare_func_base<T>
{
	Bool operator ()(cref_t<T> a, cref_t<T> b) const
	{
		if constexpr (has_undefines_v<T>)
			if (!IsDefined(a) || !IsDefined(b))
				return false;
		return !(a == b);
	}
};

template <typename T> struct greater : checked_compare_func < T, decltype([](cref_t<T> a, cref_t<T> b) -> Bool { return b < a; }) > {};
template <typename T> struct greater_equal : checked_compare_func < T, decltype([](cref_t<T> a, cref_t<T> b) -> Bool { return !(a < b); }) > {};
template <typename T> struct less: checked_compare_func < T, decltype([](cref_t<T> a, cref_t<T> b) -> Bool { return a < b; }) > {};
template <typename T> struct less_equal: checked_compare_func < T, decltype([](cref_t<T> a, cref_t<T> b) -> Bool { return !(b < a); }) > {};

template <typename T> struct logical_and : compare_func_base<T> { Bool operator ()(cref_t<T> a, cref_t<T> b) const { return a && b; } };
template <typename T> struct logical_or  : compare_func_base<T> { Bool operator ()(cref_t<T> a, cref_t<T> b) const { return a || b; } };

template <typename T> struct binary_base: binary_func<T, T, T> {
	static ConstUnitRef unit_creator(const AbstrOperGroup* gr, const ArgSeqType& args) 
	{ 
		return compatible_values_unit_creator_func(0, gr, args, false); 
	}
};

template<typename T> struct is_safe_for_undefines<equal_to<T>> : std::true_type {};
template<typename T> struct is_safe_for_undefines<not_equal_to<T>> : std::true_type{};
template<typename T> struct is_safe_for_undefines<greater<T>> : std::true_type{};
template<typename T> struct is_safe_for_undefines<greater_equal<T>> : std::true_type{};
template<typename T> struct is_safe_for_undefines<less<T>> : std::true_type{};
template<typename T> struct is_safe_for_undefines<less_equal<T>> : std::true_type{};

template <typename T> struct binary_and : binary_base<T> { T operator()(cref_t<T> a, cref_t<T> b) const { return a&b; } };
template <typename T> struct binary_or  : binary_base<T> { T operator()(cref_t<T> a, cref_t<T> b) const { return a|b; } };
template <typename T> struct binary_eq  : binary_base<T> { T operator()(cref_t<T> a, cref_t<T> b) const { return ~(a^b); } };
template <typename T> struct binary_ne  : binary_base<T> { T operator()(cref_t<T> a, cref_t<T> b) const { return (a^b); } };


template <bit_size_t N> struct binary_and<bit_value<N> >   : binary_base<bit_value<N> >
{
	using block_func = binary_and<bit_block_t>;

	block_func m_BlockFunc;
};
template <bit_size_t N> struct binary_or<bit_value<N> >   : binary_base<bit_value<N> >
{
	using block_func = binary_or<bit_block_t>;

	block_func m_BlockFunc;
};
template <> struct logical_and<Bool> : binary_and<Bool> {};
template <> struct logical_or <Bool> : binary_or <Bool> {};

template <> struct equal_to<Bool> : binary_base<Bool>
{
	using block_func = binary_eq<bit_block_t>;

	block_func m_BlockFunc;
};

template <> struct not_equal_to<Bool>: binary_base<Bool>
{
	using block_func = binary_ne<bit_block_t>;

	block_func m_BlockFunc;
};

// *****************************************************************************
//			BinaryAttrFunc operators
// *****************************************************************************

template <typename BinOper>
struct BinaryAttrFuncOper : BinaryAttrOper<typename BinOper::res_type, typename BinOper::arg1_type, typename BinOper::arg2_type>
{
	BinaryAttrFuncOper(AbstrOperGroup* gr) 
		: BinaryAttrOper<typename BinOper::res_type, typename BinOper::arg1_type, typename BinOper::arg2_type>(gr, BinOper::unit_creator, composition_of<typename BinOper::res_type>::value, ArgFlags(AF1_HASUNDEFINED | AF2_HASUNDEFINED))
	{}

	void CalcTile(sequence_traits<typename BinOper::res_type>::seq_t resData, sequence_traits<typename BinOper::arg1_type>::cseq_t arg1Data, sequence_traits<typename BinOper::arg2_type>::cseq_t arg2Data, ArgFlags af MG_DEBUG_ALLOCATOR_SRC_ARG) const override
	{
		do_binary_func(resData, arg1Data, arg2Data, BinOper(), af & AF1_ISPARAM, af & AF2_ISPARAM, af & AF1_HASUNDEFINED, af & AF2_HASUNDEFINED);
	}
};

// *****************************************************************************
//											INSTANTIATION helpers
// *****************************************************************************

template <typename TL, template <typename T> class MetaFunc>
struct BinaryInstantiation{

	template <typename T> struct OperTemplate : BinaryAttrFuncOper<MetaFunc<T> > 
	{
		using BinaryAttrFuncOper<MetaFunc<T> >::BinaryAttrFuncOper; // <MetaFunc<T> >; // inherit constructors
	};

	tl_oper::inst_tuple_templ<TL, OperTemplate, AbstrOperGroup*> m_OperList;

	BinaryInstantiation(AbstrOperGroup* gr)
		: m_OperList(gr)
	{}

};

//#include "UnitGroup.h"

template <typename TL, template <typename T> class MetaFunc>
struct CogBinaryInstantiation : CommonOperGroup
{
	CogBinaryInstantiation(CharPtr cogName)
		:	CommonOperGroup(cogName)
		,	m_Instantiations(this)
	{}
	
	BinaryInstantiation<TL, MetaFunc> m_Instantiations;
};

// *****************************************************************************
//											INSTANTIATION
// *****************************************************************************

#include "RtcTypeLists.h"
using namespace typelists;

namespace {
	struct AbstrUnitMulOpers
	{
		AbstrUnitMulOpers(const UnitClass* resType, const UnitClass* arg1Type, const UnitClass* arg2Type)
			: unitMulOper(&cog_mul, resType, arg1Type, arg2Type)
			, unitDivOper(&cog_div, resType, arg1Type, arg2Type)
		{}
		BinUnitOperator<MulMetricFunctor> unitMulOper;
		BinUnitOperator<DivMetricFunctor> unitDivOper;
	};

	template <typename R, typename U, typename V>
	struct UnitMulOpers : AbstrUnitMulOpers
	{
		UnitMulOpers()
			: AbstrUnitMulOpers(Unit<R>::GetStaticClass()
			, Unit<U>::GetStaticClass()
			, Unit<V>::GetStaticClass()
			)
		{}
	};

	template <typename T>
	struct ParamUnitOpers : ParamMulUnitOperator, ParamDivUnitOperator, UnitPowerOperator
	{
		ParamUnitOpers()
			: ParamMulUnitOperator(&cog_mul, Unit<T>::GetStaticClass())
			, ParamDivUnitOperator(&cog_div, Unit<T>::GetStaticClass())
			, UnitPowerOperator(&cog_pow, Unit<T>::GetStaticClass())
		{}
	};
	CommonOperGroup
		cog_mul_or_null("mul_or_null"),
		cog_add_or_null("add_or_null"),
		cog_sub_or_null("sub_or_null");

	BinaryInstantiation<ranged_unit_objects, mul_func > sMul(&cog_mul);
	BinaryInstantiation<ranged_unit_objects, mul_or_null_func > sMulOrNull(&cog_mul_or_null);
	BinaryInstantiation<ranged_unit_objects, div_func > sDiv(&cog_div);
	BinaryInstantiation<ranged_unit_objects, plus_func> sAdd(&cog_add);
	BinaryInstantiation<ranged_unit_objects, plus_or_null_func> sAddOrNull(&cog_add_or_null);

	CogBinaryInstantiation<ranged_unit_objects, mod_func  > sMod("mod");
	BinaryInstantiation<ranged_unit_objects, minus_func> sSub(&cog_sub);
	BinaryInstantiation<ranged_unit_objects, minus_or_null_func> sSubOrNull(&cog_sub_or_null);

	CogBinaryInstantiation<points, dist_func   > sDist("dist");
	CogBinaryInstantiation<points, sqrdist_func> sSqrDist("sqrdist");

	BinaryInstantiation<fields, equal_to    > sEq(&cog_eq);
	BinaryInstantiation<fields, not_equal_to> sNe(&cog_ne);

	CogBinaryInstantiation<scalars, greater      > sGt("gt");
	CogBinaryInstantiation<scalars, greater_equal> sGe("ge");
	CogBinaryInstantiation<scalars, less         > sLt("lt");
	CogBinaryInstantiation<scalars, less_equal   > sLe("le");

	BinaryInstantiation<aints, binary_and>  sBitAnd(&cog_bitand);
	BinaryInstantiation<aints, binary_or >  sBitOr(&cog_bitor);

	CogBinaryInstantiation<aints, logical_and> sAnd("and");
	CogBinaryInstantiation<aints, logical_or > sOr("or");

//TODO: voor alle Floats beschikbaar maken.
	tl_oper::inst_tuple<num_objects, UnitMulOpers<_1, _1, _1> > g_UnitUnaryTypeMulOpers;

	tl_oper::inst_tuple<num_objects, ParamUnitOpers<_1> > g_ParamUnitOpers;

	#define INST(T,U,V) UnitMulOpers<T,U,V> g_UnitMul3##T##U##V;
	INST(Int32,   Int32,   UInt16)
	INST(Int32,   Int32,   Int16)
	INST(Int32,   Int32,   UInt8)
	INST(Int32,   Int32,   Int8)
//	INST(Int32,   Int32,   Bool)

	INST(Float32, Float32, UInt32)
	INST(Float32, Float32, Int32)
	INST(Float32, Float32, UInt16)
	INST(Float32, Float32, Int16)
	INST(Float32, Float32, UInt8)
	INST(Float32, Float32, Int8)
//	INST(Float32, Float32, Bool)

	INST(Float64, Float64, Float32)
	INST(Float64, Float64, UInt32)
	INST(Float64, Float64, Int32)
	INST(Float64, Float64, UInt16)
	INST(Float64, Float64, Int16)
	INST(Float64, Float64, UInt8)
	INST(Float64, Float64, Int8)
//	INST(Float64, Float64, Bool)

	INST(Int32, UInt16, Int32)
	INST(Int32, Int16,  Int32)
	INST(Int32, UInt8 , Int32)
	INST(Int32, Int8 ,  Int32)
	INST(Int32, Bool ,  Int32)

	INST(Float32, UInt32, Float32)
	INST(Float32, Int32,  Float32)
	INST(Float32, UInt16, Float32)
	INST(Float32, Int16,  Float32)
	INST(Float32, UInt8,  Float32)
	INST(Float32, Int8,   Float32)
	INST(Float32, Bool,   Float32)

	INST(Float64, Float32,Float64)
	INST(Float64, UInt32, Float64)
	INST(Float64, Int32,  Float64)
	INST(Float64, UInt16, Float64)
	INST(Float64, Int16,  Float64)
	INST(Float64, UInt8,  Float64)
	INST(Float64, Int8,   Float64)
	INST(Float64, Bool,   Float64)

	INST(WPoint, WPoint, WPoint)
	INST(SPoint, SPoint, SPoint)
	INST(UPoint, UPoint, UPoint)
	INST(IPoint, IPoint, IPoint)
	INST(FPoint, FPoint, FPoint)
	INST(DPoint, DPoint, DPoint)

	INST(WPoint, Bool, WPoint)
	INST(SPoint, Bool, SPoint)
	INST(UPoint, Bool, UPoint)
	INST(IPoint, Bool, IPoint)
	INST(FPoint, Bool, FPoint)
	INST(DPoint, Bool, DPoint)

	INST(WPoint, UInt16, WPoint)
	INST(SPoint, Int16,  SPoint)
	INST(UPoint, UInt32, UPoint)
	INST(IPoint, Int32, IPoint)
	INST(FPoint, Float32, FPoint)
	INST(DPoint, Float64, DPoint)

#undef INST

	StrConcatOperator strConcatC(&cog_add);

	CommonOperGroup
		cog_left(token::left),
		cog_right(token::right);

	auto substr = makeStr2Oper(cog_substr,
		[](CharPtr first, CharPtr last, strpos_t pos) { return std::make_pair((pos< last - first) ? first+pos : last, last); }
	);

	auto leftOper = makeStr2Oper(cog_left,
		[](CharPtr first, CharPtr last, strpos_t pos) { return std::make_pair(first, (pos< last - first) ? first+pos : last); }
	);

	auto rightOper = makeStr2Oper(cog_right,
		[](CharPtr first, CharPtr last, strpos_t pos) { return std::make_pair((pos< last - first) ? last - pos : first, last); }
	);


	RepeatOperator repOper;

	CommonOperGroup cogStrPos("strpos");
	CommonOperGroup cogStrRPos("strrpos");
	BinaryAttrFuncOper<strpos_func> g_StrPosU(&cogStrPos);
	BinaryAttrFuncOper<strrpos_func> g_StrRPosU(&cogStrRPos);

	CommonOperGroup cogStrCount("strcount");
	BinaryAttrFuncOper<strcount_func> g_StrCountU(&cogStrCount);

	tl_oper::inst_tuple_templ<typelists::floats, String2Operator > string2Opers;
} // namespace
