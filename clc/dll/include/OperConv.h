// Copyright (C) 1998-2024 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

// Shared header for OperConv split compilation units
// Contains all template definitions used by the conversion operators

#pragma once

#if !defined(__CLC_OPERCONV_H)
#define __CLC_OPERCONV_H

#include "ClcPch.h"

#include "geo/CheckedCalc.h"
#include "geo/Conversions.h"
#include "geo/StringArray.h"
#include "geo/PointOrder.h"
#include "geo/GeoSequence.h"
#include "mci/CompositeCast.h"
#include "ser/AsString.h"
#include "ser/StringStream.h"
#include "ser/SequenceArrayStream.h"
#include "ser/RangeStream.h"
#include "set/VectorFunc.h"
#include "stg/AbstrStorageManager.h"
#include "utl/Environment.h"
#include "utl/mySPrintF.h"
#include "LockLevels.h"

#include "Unit.h"
#include "Param.h"
#include "UnitClass.h"
#include "DataItemClass.h"
#include "TreeItemClass.h"
#include "LispTreeType.h"

#include "gdal/gdal_base.h"

#include "ConstOper.h"
#include "Lookup.h"
#include "OperUnit.h"
#include "CastedUnaryAttrOper.h"
#include "OperAttrUni.h"

#include "RtcTypeLists.h"
#include "utl/TypeListOper.h"

#include <functional>
#include <iterator>
#include <algorithm>

#include "ogr_spatialref.h"
#include "geo/Transform.h"
#include "Projection.h"
#include <gdal_priv.h>
#include <proj.h>

// *****************************************************************************
//			OGR helpers (defined in OperConv.cpp)
// *****************************************************************************

void OGRCheck(OGRSpatialReference* ref, OGRErr result, CharPtr format, const AbstrUnit* au);
const AbstrUnit* CompositeBase(const AbstrUnit* proj);
bool ProjectionIsColFirst(OGRSpatialReference& srs);

extern leveled_critical_section cs_SpatialRefBlockCreation;

// *****************************************************************************
//			SpatialRefBlock (defined in OperConv.cpp)
// *****************************************************************************

struct SpatialRefBlock : SharedBase, gdalComponent
{
	pj_ctx* m_ProjCtx = nullptr;
	OGRSpatialReference          m_Src, m_Dst;
	OGRCoordinateTransformation* m_Transformer = nullptr;

	SpatialRefBlock();
	~SpatialRefBlock();
	void CreateTransformer();
	void Release() const { delete this; }
};

// *****************************************************************************
//			Type Conversion Functors
// *****************************************************************************

template<bool mustRoundToNearest, typename TR, typename TA>
TR checked_multiply(TA v, Float64 factor)
{
	if (!IsDefined(v))
		return UNDEFINED_OR_ZERO(TR);
	if constexpr (mustRoundToNearest)
		return RoundedConvert<TR>(v * factor);
	else
		return Convert<TR>(v * factor);
}

template<bool mustRoundToNearest, typename TR, bit_size_t N>
TR checked_multiply(bit_value<N> v, Float64 factor)
{
	if constexpr (mustRoundToNearest)
		return RoundedConvert<TR>(v * factor);
	else
		return Convert<TR>(v * factor);
}

template<typename mustRoundToNearest>
struct Type0DConversionFunctor
{
	template<typename TR, typename TA>
	struct Type0DConversion : unary_func<TR, TA>
	{
		Type0DConversion(const AbstrUnit* resUnit, const AbstrUnit* srcUnit)
		{
			if (resUnit->GetUnitClass()->CreateDefault() != resUnit)
			{
				MG_CHECK(!srcUnit->GetMetric());
				MG_CHECK(!srcUnit->GetProjection());
			}
		};

		TR operator() (typename sequence_traits<TA>::container_type::const_reference v) const
		{
			if constexpr (mustRoundToNearest::value)
				return RoundedConvert<TR>(v);
			else
				return Convert<TR>(v);
		}

		using iterator = typename DataArrayBase<TR>::iterator;
		using const_iterator = typename DataArrayBase<TA>::const_iterator;

		void Dispatch(iterator ri, const_iterator ai, const_iterator ae)
		{
			for (; ai != ae; ++ai, ++ri)
				Assign(*ri, operator ()(*ai));
		}

		typedef typename sequence_traits<TR>::container_type TPR;
		typedef typename sequence_traits<TPR>::seq_t         pr_seq;
		typedef typename pr_seq::iterator                    seq_iterator;

		typedef typename sequence_traits<TA>::container_type TPA;
		typedef typename sequence_traits<TPA>::cseq_t  pa_cseq;
		typedef typename pa_cseq::const_iterator       cseq_iterator;

		void Dispatch(seq_iterator pri, cseq_iterator pai, cseq_iterator pae)
		{
			for (; pai != pae; ++pri, ++pai)
			{
				pri->resize(pai->size());
				Dispatch(pri->begin(), pai->begin(), pai->end());
			}
		}
	};
};

template<typename TR, typename TA>
void DispatchMapping(Type0DConversionFunctor<std::false_type>::template Type0DConversion<TR, TA>& functor
	, typename Type0DConversionFunctor<std::false_type>::template Type0DConversion<TR, TA>::iterator ri
	, typename Unit<TA>::range_t tileRange, SizeT n)
{
	for (SizeT i = 0; i != n; ++ri, ++i)
		Assign(*ri, functor(Range_GetValue_naked(tileRange, i)));
}

template<typename TR, typename TA>
void DispatchMapping(Type0DConversionFunctor<std::true_type>::template Type0DConversion<TR, TA>& functor
	, typename Type0DConversionFunctor<std::true_type>::template Type0DConversion<TR, TA>::iterator ri
	, typename Unit<TA>::range_t tileRange, SizeT n)
{
	for (SizeT i = 0; i != n; ++ri, ++i)
		Assign(*ri, functor(Range_GetValue_naked(tileRange, i)));
}

// *****************************************************************************
//			Type 1D Conversion Functor
// *****************************************************************************

template<typename mustRoundToNearest>
struct Type1DConversionFunctor
{
	template<typename TR, typename TA>
	struct Type1DConversion : unary_func<TR, TA>
	{
		Type1DConversion(const AbstrUnit* resUnit, const AbstrUnit* srcUnit)
		{
			if (!srcUnit->GetCurrMetric() || !resUnit->GetCurrMetric() || srcUnit->GetCurrMetric() == resUnit->GetCurrMetric())
			{
				m_Factor = 1.0;
				return;
			}
			if (!(srcUnit->GetCurrMetric()->m_BaseUnits == resUnit->GetCurrMetric()->m_BaseUnits))
			{
				srcUnit->UnifyValues(resUnit, "Base Units of first argument", "Base Units of cast target as specified by the second argument", UM_Throw);
				dms_assert(0);
			}
			m_Factor = srcUnit->GetCurrMetric()->m_Factor / resUnit->GetCurrMetric()->m_Factor;
		};

		TR ApplyDirect(typename sequence_traits<TA>::container_type::const_reference v) const
		{
			assert(m_Factor == 1.0);
			if constexpr (mustRoundToNearest::value)
				return RoundedConvert<TR>(v);
			else
				return Convert<TR>(v);
		}

		TR ApplyScaled(typename sequence_traits<TA>::container_type::const_reference v) const
		{
			assert(m_Factor != 1.0);
			if constexpr (has_undefines_v<TA>)
				if (!IsDefined(v))
					return UNDEFINED_OR_ZERO(TR);
			return checked_multiply<mustRoundToNearest::value, TR>(v, m_Factor);
		}

		using iterator = typename DataArrayBase<TR>::iterator;
		using const_iterator = typename DataArrayBase<TA>::const_iterator;

		void Dispatch(iterator ri, const_iterator ai, const_iterator ae)
		{
			if (m_Factor != 1.0)
				for (; ai != ae; ++ai, ++ri)
					Assign(*ri, ApplyScaled(*ai));
			else
				for (; ai != ae; ++ai, ++ri)
					Assign(*ri, ApplyDirect(*ai));
		}

		typedef typename sequence_traits<TR>::container_type TPR;
		typedef typename sequence_traits<TPR>::seq_t         pr_seq;
		typedef typename pr_seq::iterator                    seq_iterator;

		typedef typename sequence_traits<TA>::container_type TPA;
		typedef typename sequence_traits<TPA>::cseq_t  pa_cseq;
		typedef typename pa_cseq::const_iterator       cseq_iterator;

		void Dispatch(seq_iterator pri, cseq_iterator pai, cseq_iterator pae)
		{
			for (; pai != pae; ++pri, ++pai)
			{
				pri->resize_uninitialized(pai->size() MG_DEBUG_ALLOCATOR_SRC("ltrim_assign"));
				Dispatch(pri->begin(), pai->begin(), pai->end());
			}
		}

		Float64 m_Factor;
	};
};

template<typename TR, typename TA>
void DispatchMapping(Type1DConversionFunctor<std::false_type>::template Type1DConversion<TR, TA>& functor
	, typename Type1DConversionFunctor<std::false_type>::template Type1DConversion<TR, TA>::iterator ri
	, typename Unit<TA>::range_t tileRange, SizeT n)
{
	if (functor.m_Factor != 1.0)
		for (SizeT i = 0; i != n; ++ri, ++i)
			Assign(*ri, functor.ApplyScaled(Range_GetValue_naked(tileRange, i)));
	else
		for (SizeT i = 0; i != n; ++ri, ++i)
			Assign(*ri, functor.ApplyDirect(Range_GetValue_naked(tileRange, i)));
}

template<typename TR, typename TA>
void DispatchMapping(Type1DConversionFunctor<std::true_type>::template Type1DConversion<TR, TA>& functor
	, typename Type1DConversionFunctor<std::true_type>::template Type1DConversion<TR, TA>::iterator ri
	, typename Unit<TA>::range_t tileRange, SizeT n)
{
	if (functor.m_Factor != 1.0)
		for (SizeT i = 0; i != n; ++ri, ++i)
			Assign(*ri, functor.ApplyScaled(Range_GetValue_naked(tileRange, i)));
	else
		for (SizeT i = 0; i != n; ++ri, ++i)
			Assign(*ri, functor.ApplyDirect(Range_GetValue_naked(tileRange, i)));
}

// *****************************************************************************
//			Type 2D Conversion Functor
// *****************************************************************************

const int PROJ_BLOCK_SIZE = 1024;

template<typename TR, typename TA>
struct Type2DConversion : unary_func<TR, TA>
{
	Type2DConversion(const AbstrUnit* resUnit, const AbstrUnit* srcUnit)
		: m_PreRescaler(UnitProjection::GetCompositeTransform(srcUnit->GetCurrProjection()))
		, m_PostRescaler(UnitProjection::GetCompositeTransform(resUnit->GetCurrProjection()).Inverse())
	{
		TokenID srcFormat = CompositeBase(srcUnit)->GetCurrSpatialReference();
		if (srcFormat)
		{
			TokenID resFormat = CompositeBase(resUnit)->GetCurrSpatialReference();
			if (resFormat && srcFormat != resFormat)
			{
				leveled_critical_section::scoped_lock lock(cs_SpatialRefBlockCreation);
				m_OgrComponentHolder = new SpatialRefBlock;
				GDAL_ErrorFrame frame;
				SharedStr srcFormatStr = SharedStr(srcFormat);
				SharedStr resFormatStr = SharedStr(resFormat);

				if (!AuthorityCodeIsValidCrs(srcFormatStr.c_str()))
				{
					reportF(MsgCategory::progress, SeverityTypeID::ST_Error,
						"The 'src' unit in projection conversion from 'src' unit %s to res unit %s is not a valid projection.",
						srcUnit->GetFullName(), resUnit->GetFullName());
				}

				if (!AuthorityCodeIsValidCrs(resFormatStr.c_str()))
				{
					reportF(MsgCategory::progress, SeverityTypeID::ST_Error,
						"The 'res' unit in projection conversion from src unit %s to 'res' unit %s is not a valid projection.",
						srcUnit->GetFullName(), resUnit->GetFullName());
				}

				OGRCheck(&m_OgrComponentHolder->m_Src, m_OgrComponentHolder->m_Src.SetFromUserInput(srcFormatStr.c_str()), srcFormatStr.c_str(), srcUnit);
				OGRCheck(&m_OgrComponentHolder->m_Dst, m_OgrComponentHolder->m_Dst.SetFromUserInput(resFormatStr.c_str()), resFormatStr.c_str(), resUnit);
				m_OgrComponentHolder->CreateTransformer();
				m_Source_is_expected_to_be_col_first = ProjectionIsColFirst(m_OgrComponentHolder->m_Src);
				m_Projection_is_col_first = ProjectionIsColFirst(m_OgrComponentHolder->m_Dst);
				frame.ThrowUpWhateverCameUp();
				return;
			}
		}

		m_PreRescaler *= m_PostRescaler;
		m_PostRescaler = CrdTransformation();
	}

	TR ApplyDirect(const TA& p) const
	{
		assert(!m_OgrComponentHolder);
		assert(m_PreRescaler.IsIdentity());
		assert(m_PostRescaler.IsIdentity());
		return SignedIntGridConvert<TR>(p);
	}

	TR ApplyProjection(const TA& p) const
	{
		assert(m_OgrComponentHolder && m_OgrComponentHolder->m_Transformer);
		assert(m_PreRescaler.IsIdentity());
		assert(m_PostRescaler.IsIdentity());

		if (!IsDefined(p))
			return UNDEFINED_VALUE(TR);

		DPoint res = prj2dms_order(p.first, p.second, m_Source_is_expected_to_be_col_first);
		if (!m_OgrComponentHolder->m_Transformer->Transform(1, &res.first, &res.second, nullptr))
			return UNDEFINED_VALUE(TR);
		res = prj2dms_order(res.first, res.second, m_Projection_is_col_first);
		return SignedIntGridConvert<TR>(res);
	}

	TR ApplyScaled(const TA& p) const
	{
		assert(!m_OgrComponentHolder);
		assert(!m_PreRescaler.IsIdentity());
		assert(m_PostRescaler.IsIdentity());

		if (!IsDefined(p))
			return UNDEFINED_VALUE(TR);
		return SignedIntGridConvert<TR>(m_PreRescaler.Apply(DPoint(p)));
	}

	TR ApplyScaledProjection(const TA& p) const
	{
		assert(m_OgrComponentHolder && m_OgrComponentHolder->m_Transformer);
		assert(!m_PreRescaler.IsIdentity() || !m_PostRescaler.IsIdentity());

		if (!IsDefined(p))
			return UNDEFINED_VALUE(TR);

		DPoint res = m_PreRescaler.Apply(DPoint(p));
		res = prj2dms_order(res.first, res.second, m_Source_is_expected_to_be_col_first);
		if (!m_OgrComponentHolder->m_Transformer->Transform(1, &res.first, &res.second, nullptr))
			return UNDEFINED_VALUE(TR);
		res = prj2dms_order(res.first, res.second, m_Projection_is_col_first);
		return SignedIntGridConvert<TR>(m_PostRescaler.Apply(res));
	}

	using iterator = typename DataArrayBase<TR>::iterator;
	using const_iterator = typename DataArrayBase<TA>::const_iterator;

	void DispatchTransformation_impl(iterator ri, const_iterator ai, Float64* resX, Float64* resY, int* successFlags, UInt32 s)
	{
		assert(m_OgrComponentHolder);
		assert(s <= PROJ_BLOCK_SIZE);
		bool source_is_expected_to_be_col_first = this->m_Source_is_expected_to_be_col_first;
		bool projection_is_col_first = this->m_Projection_is_col_first;

		for (UInt32 i = 0; i != s; ++i)
		{
			DPoint rescaledA = m_PreRescaler.Apply(DPoint(ai[i]));
			rescaledA = prj2dms_order(rescaledA, source_is_expected_to_be_col_first);
			resX[i] = rescaledA.first;
			resY[i] = rescaledA.second;
		}
		if (!m_OgrComponentHolder->m_Transformer->Transform(s, resX, resY, nullptr, successFlags))
		{
			if (s > 1)
			{
				auto halfSize = s / 2;
				DispatchTransformation_impl(ri, ai, resX, resY, successFlags, halfSize);
				DispatchTransformation_impl(ri + halfSize, ai + halfSize, resX, resY, successFlags, s - halfSize);
			}
			else
				Assign(*ri, Undefined());
			return;
		}
		for (UInt32 i = 0; i != s; ++ri, ++i)
		{
			if (!successFlags[i])
			{
				DPoint rescaledA = m_PreRescaler.Apply(DPoint(ai[i]));
				rescaledA = prj2dms_order(rescaledA, source_is_expected_to_be_col_first);
				resX[i] = rescaledA.first;
				resY[i] = rescaledA.second;
				if (!m_OgrComponentHolder->m_Transformer->Transform(1, resX + i, resY + i, nullptr, successFlags + i))
				{
					Assign(*ri, Undefined());
					continue;
				}
			}
			auto reprojectedPoint = prj2dms_order(resX[i], resY[i], projection_is_col_first);
			auto rescaledPoint = m_PostRescaler.Apply(reprojectedPoint);
			Assign(*ri, SignedIntGridConvert<TR>(rescaledPoint));
		}
	}

	void Dispatch(iterator ri, const_iterator ai, const_iterator ae)
	{
		if (m_OgrComponentHolder)
		{
			Float64 resX[PROJ_BLOCK_SIZE];
			Float64 resY[PROJ_BLOCK_SIZE];
			int     successFlags[PROJ_BLOCK_SIZE];

			auto n = ae - ai;
			while (n)
			{
				auto s = n;
				MakeMin(s, PROJ_BLOCK_SIZE);
				auto ss = std::find_if_not(ai, ai + s, [](auto p) { return IsDefined(p); }) - ai;
				assert(ss == s || (ss < s && !IsDefined(ai[ss])));
				DispatchTransformation_impl(ri, ai, resX, resY, successFlags, ss);
				ri += ss;
				if (ss < s)
				{
					*ri++ = Undefined();
					ss++;
				}
				n -= ss;
				ai += ss;
			}
		}
		else
			if (m_PreRescaler.IsIdentity())
				for (; ai != ae; ++ai, ++ri)
					Assign(*ri, ApplyDirect(*ai));
			else
				for (; ai != ae; ++ai, ++ri)
					Assign(*ri, ApplyScaled(*ai));
	}

	typedef typename sequence_traits<TR>::container_type TPR;
	typedef typename sequence_traits<TPR>::seq_t         pr_seq;
	typedef typename pr_seq::iterator                    seq_iterator;

	typedef typename sequence_traits<TA>::container_type TPA;
	typedef typename sequence_traits<TPA>::cseq_t  pa_cseq;
	typedef typename pa_cseq::const_iterator       cseq_iterator;

	void Dispatch(seq_iterator pri, cseq_iterator pai, cseq_iterator pae)
	{
		for (; pai != pae; ++pri, ++pai)
		{
			pri->resize_uninitialized(pai->size() MG_DEBUG_ALLOCATOR_SRC("Type2DConversion"));
			Dispatch(pri->begin(), pai->begin(), pai->end());
		}
	}

	SharedPtr<SpatialRefBlock>   m_OgrComponentHolder;
	CrdTransformation            m_PreRescaler;
	CrdTransformation            m_PostRescaler;
	bool                         m_Source_is_expected_to_be_col_first = dms_order_tag::col_first;
	bool                         m_Projection_is_col_first = dms_order_tag::col_first;
};

template<typename TR, typename TA>
void DispatchMapping(Type2DConversion<TR, TA>& functor, typename Type2DConversion<TR, TA>::iterator ri, typename Unit<TA>::range_t tileRange, SizeT n)
{
	if (functor.m_OgrComponentHolder)
	{
		Float64 resX[PROJ_BLOCK_SIZE];
		Float64 resY[PROJ_BLOCK_SIZE];
		int     successFlags[PROJ_BLOCK_SIZE];
		bool    source_is_expected_to_be_col_first = functor.m_Source_is_expected_to_be_col_first;
		bool    projection_is_col_first = functor.m_Projection_is_col_first;

		while (n)
		{
			auto s = n;
			MakeMin(s, PROJ_BLOCK_SIZE);
			for (SizeT i = 0; i != s; ++i)
			{
				DPoint rescaledA = functor.m_PreRescaler.Apply(DPoint(Range_GetValue_naked(tileRange, i)));
				rescaledA = prj2dms_order(rescaledA, source_is_expected_to_be_col_first);
				resX[i] = rescaledA.first;
				resY[i] = rescaledA.second;
			}
			if (!functor.m_OgrComponentHolder->m_Transformer->Transform(s, resX, resY, nullptr, successFlags))
				fast_fill(successFlags, successFlags + s, 0);
			for (SizeT i = 0; i != s; ++ri, ++i)
			{
				if (successFlags[i])
				{
					auto reprojectedPoint = prj2dms_order(resX[i], resY[i], projection_is_col_first);
					auto rescaledPoint = functor.m_PostRescaler.Apply(reprojectedPoint);
					Assign(*ri, SignedIntGridConvert<TR>(rescaledPoint));
				}
				else
					Assign(*ri, Undefined());
			}
			n -= s;
		}
	}
	else
		if (functor.m_PreRescaler.IsIdentity())
			for (SizeT i = 0; i != n; ++ri, ++i)
				Assign(*ri, functor.ApplyDirect(Range_GetValue_naked(tileRange, i)));
		else
			for (SizeT i = 0; i != n; ++ri, ++i)
				Assign(*ri, functor.ApplyScaled(Range_GetValue_naked(tileRange, i)));
}

template<typename TR, typename TA, typename RI>
void DispatchMappingCount(Type2DConversion<TR, TA>& functor, RI ri, typename Unit<TA>::range_t srcTileRange, typename Unit<TR>::range_t dstRange, SizeT n)
{
	SizeT k = Cardinality(srcTileRange);
	if (functor.m_OgrComponentHolder)
		if (functor.m_PreRescaler.IsIdentity() && functor.m_PostRescaler.IsIdentity())
			for (SizeT i = 0; i != k; ++i)
			{
				auto j = Range_GetIndex_checked(dstRange, functor.ApplyProjection(Range_GetValue_naked(srcTileRange, i)));
				if (j < n)
					ri[j]++;
			}
		else
			for (SizeT i = 0; i != k; ++i)
			{
				auto j = Range_GetIndex_checked(dstRange, functor.ApplyScaledProjection(Range_GetValue_naked(srcTileRange, i)));
				if (j < n)
					ri[j]++;
			}
	else
		if (functor.m_PreRescaler.IsIdentity())
			for (SizeT i = 0; i != k; ++i)
			{
				auto j = Range_GetIndex_checked(dstRange, functor.ApplyDirect(Range_GetValue_naked(srcTileRange, i)));
				if (j < n)
					ri[j]++;
			}
		else
			for (SizeT i = 0; i != k; ++i)
			{
				auto j = Range_GetIndex_checked(dstRange, functor.ApplyScaled(Range_GetValue_naked(srcTileRange, i)));
				if (j < n)
					ri[j]++;
			}
}

// *****************************************************************************
//			Polymorphic Functor applicator
// *****************************************************************************

template <typename Functor>
void AssignFuncRes0(
	const Functor& func,
	typename sequence_traits<typename Functor::res_type >::container_type::reference       res,
	typename sequence_traits<typename Functor::arg1_type>::container_type::const_reference arg)
{
	Assign(res, func(arg));
}

template <typename Functor>
void AssignFuncRes0(
	const Functor& func,
	SA_Reference     <typename Functor::res_type > res,
	SA_ConstReference<typename Functor::arg1_type> arg)
{
	res.resize_uninitialized(arg.size() MG_DEBUG_ALLOCATOR_SRC("AssignFuncRes0"));
	dms_transform(arg.begin(), arg.end(), res.begin(), func);
}

// *****************************************************************************
//			Meta Function Class that returns a TypeConversion Functor
// *****************************************************************************

template <typename T> struct is_2d : std::integral_constant<bool, dimension_of<T>::value == 2> {};

template <typename mustRoundToNearest>
struct TypeConversionF
{
	template <typename TR, typename TA>
	using apply2 =
		std::conditional_t<(is_numeric_v<TR>&& is_numeric_v<TA>)
			, typename Type1DConversionFunctor<mustRoundToNearest>::template Type1DConversion<TR, TA>
			, std::conditional_t<(is_2d<TR>::value&& is_2d<TA>::value)
				, Type2DConversion<TR, TA>
				, typename Type0DConversionFunctor<mustRoundToNearest>::template Type0DConversion<TR, TA>
			>
		>;
};

template <typename MetaFunc, typename TR, typename TA>
struct ConversionGenerator
{
	static constexpr bool use_field = is_composite_v<TR> && is_composite_v<TA>;

	using R = std::conditional_t<use_field, field_of_t<TR>, TR >;
	using A = std::conditional_t<use_field, field_of_t<TA>, TA >;

	static_assert(tl::BinaryMeta<MetaFunc, TR, TA>);
	using type = MetaFunc::template apply2<R, A>;
};

template <typename TR, typename TA, typename TCF, typename CIT, typename RIT>
void do_transform(const AbstrUnit* dstUnit, const AbstrUnit* srcUnit, CIT srcBegin, CIT srcEnd, RIT dstBegin)
{
	using FunctorType = ConversionGenerator<TCF, TR, TA>::type;

	auto dstUnit2 = dstUnit;
	auto srcUnit2 = srcUnit;
	auto functor = FunctorType{ dstUnit2, srcUnit2 };

	parallel_for_if_separable<SizeT, TR>(0, srcEnd - srcBegin, [dstBegin, srcBegin, &functor](SizeT i)
		{
			AssignFuncRes0(functor, dstBegin[i], srcBegin[i]);
		}
	);
}

template <typename TR, typename TA, typename TCF, typename CIT, typename RIT>
void do_convert(const AbstrUnit* dstUnit, const AbstrUnit* srcUnit, CIT srcIter, CIT srcEnd, RIT dstIter)
{
	using FunctorType = typename ConversionGenerator<TCF, TR, TA>::type;

	auto functor = FunctorType{ dstUnit, srcUnit };
	functor.Dispatch(dstIter, srcIter, srcEnd);
}

// *****************************************************************************
//			Operator Classes
// *****************************************************************************

template <typename TR, typename TA, typename TCF>
class TransformAttrOperator : public AbstrCastedUnaryAttrOperator
{
	typedef DataArray<TR>    ResultType;
	typedef field_of_t<TR>   field_type;
	typedef DataArray<TA>    Arg1Type;
	typedef Unit<field_type> Arg2Type;

public:
	TransformAttrOperator(AbstrOperGroup* gr, bool reverseArgs = false)
		: AbstrCastedUnaryAttrOperator(gr
			, ResultType::GetStaticClass()
			, Arg1Type::GetStaticClass()
			, Arg2Type::GetStaticClass()
			, composition_of_v<TR>
			, reverseArgs
		)
	{}

	auto CreateFutureTileCaster(SharedPtr<AbstrDataItem> resultAdi, bool lazy, const AbstrUnit* valuesUnitA, const AbstrDataItem* arg1A, const AbstrUnit* argUnitA MG_DEBUG_ALLOCATOR_SRC(SharedStr srcStr)) const -> SharedPtr<const AbstrDataObject> override
	{
		auto tileRangeData = AsUnit(arg1A->GetAbstrDomainUnit()->GetCurrRangeItem())->GetTiledRangeData();
		auto valuesUnit = debug_cast<const Unit<field_of_t<TR>>*>(valuesUnitA);

		auto arg1 = MakeSharedFromBorrowedObjectPtr(const_array_cast<TA>(arg1A));
		auto arg2 = MakeSharedFromBorrowedObjectPtr(debug_cast<const Arg2Type*>(argUnitA));
		assert(arg1);

		using prepare_data = std::shared_ptr<typename Arg1Type::future_tile>;
		auto futureTileFunctor = make_unique_FutureTileFunctor<TR, prepare_data, false>(resultAdi, lazy, tileRangeData.get(), get_range_ptr_of_valuesunit(valuesUnit)
			, [arg1](tile_id t) { return arg1->GetFutureTile(t); }
			, [arg2, arg1A](typename sequence_traits<TR>::seq_t resData, prepare_data arg1FutureData)
			{
				auto argData = arg1FutureData->GetTile();
				do_transform<TR, TA, TCF>(arg2.get(), arg1A->GetAbstrValuesUnit(), argData.begin(), argData.end(), resData.begin());
			}
			MG_DEBUG_ALLOCATOR_SRC_PARAM
		);

		return futureTileFunctor.release();
	}

	void Calculate(AbstrDataObject* borrowedDataHandle, const AbstrDataItem* argDataA, const AbstrUnit* argUnit, tile_id t) const override
	{
		auto argData = const_array_cast<TA>(argDataA)->GetTile(t);
		auto resultData = mutable_array_cast<TR>(borrowedDataHandle)->GetWritableTile(t, dms_rw_mode::write_only_all);

		do_transform<TR, TA, TCF>(argUnit, argDataA->GetAbstrValuesUnit(), argData.begin(), argData.end(), resultData.begin());
	}
};

template <typename TR, typename TA, typename mustRoundToNearest>
class ConvertAttrOperator : public AbstrCastedUnaryAttrOperator
{
	typedef DataArray<TR>    ResultType;
	typedef field_of_t<TR>   field_type;
	typedef DataArray<TA>    Arg1Type;
	typedef Unit<field_type> Arg2Type;
	using ConversionFunctor = TypeConversionF<mustRoundToNearest>;

public:
	ConvertAttrOperator(AbstrOperGroup* gr, bool reverseArgs = false)
		: AbstrCastedUnaryAttrOperator(gr
			, ResultType::GetStaticClass()
			, Arg1Type::GetStaticClass()
			, Arg2Type::GetStaticClass()
			, composition_of_v<TR>
			, reverseArgs
		)
	{}

	auto CreateFutureTileCaster(SharedPtr<AbstrDataItem> resultAdi, bool lazy, const AbstrUnit* valuesUnitA, const AbstrDataItem* arg1A, const AbstrUnit* argUnitA MG_DEBUG_ALLOCATOR_SRC(SharedStr srcStr)) const -> SharedPtr<const AbstrDataObject> override
	{
		auto tileRangeData = AsUnit(arg1A->GetAbstrDomainUnit()->GetCurrRangeItem())->GetTiledRangeData();
		auto valuesUnit = debug_cast<const Unit<field_of_t<TR>>*>(valuesUnitA);

		auto arg1 = const_array_cast<TA>(arg1A);
		auto dstUnit = MakeSharedFromBorrowedObjectPtr(debug_cast<const Arg2Type*>(argUnitA));
		auto srcUnit = MakeSharedFromBorrowedObjectPtr(arg1A->GetAbstrValuesUnit());
		assert(arg1);

		using prepare_data = std::shared_ptr<typename Arg1Type::future_tile>;
		auto futureTileFunctor = make_unique_FutureTileFunctor<TR, prepare_data, false>(resultAdi.get(), lazy, tileRangeData.get(), get_range_ptr_of_valuesunit(valuesUnit)
			, [arg1](tile_id t) { return arg1->GetFutureTile(t); }
			, [srcUnit, dstUnit](typename sequence_traits<TR>::seq_t resData, prepare_data arg1FutureData)
			{
				auto argData = arg1FutureData->GetTile();
				do_convert<TR, TA, ConversionFunctor>(dstUnit.get(), srcUnit.get(), argData.begin(), argData.end(), resData.begin());
			}
			MG_DEBUG_ALLOCATOR_SRC_PARAM
		);

		return futureTileFunctor.release();
	}

	void Calculate(AbstrDataObject* borrowedDataHandle, const AbstrDataItem* argDataA, const AbstrUnit* argUnit, tile_id t) const override
	{
		auto argData = const_array_cast<TA>(argDataA)->GetDataRead(t);
		auto resultData = mutable_array_cast<TR>(borrowedDataHandle)->GetDataWrite(t, dms_rw_mode::write_only_all);

		do_convert<TR, TA, ConversionFunctor>(argUnit, argDataA->GetAbstrValuesUnit(), argData.begin(), argData.end(), resultData.begin());
	}
};

template <typename TR>
ConstUnitRef cast_unit_creator_field(const AbstrOperGroup* gr, const ArgSeqType& args)
{
	return cast_unit_creator<field_of_t<TR>>(args);
}

template <typename TR, typename TA>
struct CastAttrOperator : UnaryAttrOperator<TR, TA>
{
	CastAttrOperator(AbstrOperGroup* gr)
		: UnaryAttrOperator<TR, TA>(gr, ArgFlags(), cast_unit_creator_field<TR>, composition_of_v<TR>)
	{}

	void CalcTile(typename sequence_traits<TR>::seq_t resData, sequence_traits<TA>::cseq_t arg1Data, const AbstrUnit* argVU, ArgFlags af MG_DEBUG_ALLOCATOR_SRC_ARG) const override
	{
		assert(arg1Data.size() == resData.size());

		do_transform<TR, TA, tl::bind_placeholders<Type0DConversionFunctor<std::false_type>::Type0DConversion, ph::_1, ph::_2> >(
			Unit<field_of_t<TR>>::GetStaticClass()->CreateDefault()
			, Unit<field_of_t<TA>>::GetStaticClass()->CreateDefault()
			, arg1Data.begin(), arg1Data.end()
			, resData.begin()
		);
	}
};

// *****************************************************************************
//			UnitGroups helper
// *****************************************************************************

template <typename V>
CommonOperGroup* GetUnitGroup()
{
	static CommonOperGroup cog(ValueWrap<V>::GetStaticClass()->GetID());
	return &cog;
}

// *****************************************************************************
//			Instantiation helpers for convert operators (shared)
// *****************************************************************************

template <typename Res, typename Arg>
struct NamedCastAttrOper : CastAttrOperator<Res, Arg>
{
	NamedCastAttrOper() : CastAttrOperator<Res, Arg>(GetUnitGroup<Res>()) {}
};

extern CommonOperGroup cog_Convert;
extern CommonOperGroup cog_RoundedConvert;

template <typename TRL>
struct convertAndCastOpers
{
	template <typename TA>
	struct apply_TA
	{
		using ConvertOpers = tl_oper::inst_tuple<TRL, tl::bind_placeholders<ConvertAttrOperator, ph::_1, TA, std::false_type> >;
		using CastOpers = tl_oper::inst_tuple<TRL, tl::bind_placeholders<NamedCastAttrOper, ph::_1, TA> >;

		ConvertOpers m_convertOpers{ &cog_Convert, false };
		ConvertOpers m_lookupOpers { &cog_lookup , true };
		CastOpers    m_castOpers   {};
	};
};

template <typename TRL>
struct roundedConvertOpers
{
	template <typename TA>
	struct apply_TA
	{
		using rco = tl_oper::inst_tuple<TRL, tl::bind_placeholders<ConvertAttrOperator, ph::_1, TA, std::true_type>>;
		rco m_RoundedConvertOpers_{ &cog_RoundedConvert, false };
	};
};

#endif // !defined(__CLC_OPERCONV_H)
