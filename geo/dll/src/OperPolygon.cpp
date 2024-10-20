// Copyright (C) 1998-2024 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "GeoPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif

#include "geo/DynamicPoint.h"
#include "geo/CheckedCalc.h"
#include "geo/Conversions.h"
#include "mci/CompositeCast.h"
#include "ptr/OwningPtrSizedArray.h"
#include "ptr/Resource.h"
#include "set/rangefuncs.h"
#include "LockLevels.h"

#include "DataArray.h"
#include "DataItemClass.h"
#include "LispTreeType.h"
#include "TiledRangeData.h"
#include "TileChannel.h"
#include "TreeItemClass.h"
#include "Unit.h"
#include "UnitClass.h"
#include "ValueFiller.h"

#include "CastedUnaryAttrOper.h"
#include "OperAttrUni.h"
#include "Prototypes.h"
#include "UnitCreators.h"


#include <numeric>

// *****************************************************************************
//								Sequence2Point Functors
// *****************************************************************************

template <typename P>
struct Sequence2PointFunc: unary_func<P, typename sequence_traits<P>::container_type>
{
	static ConstUnitRef unit_creator(const AbstrOperGroup* gr, const ArgSeqType& args) { return arg1_values_unit(args); }
};

template <typename P>
struct LowerBoundFunc : Sequence2PointFunc<P>
{
	P operator ()(typename LowerBoundFunc::arg1_cref t) const
	{ 
		return LowerBoundFromSequence(begin_ptr(t), end_ptr(t) );
	}
};

template <typename P>
struct UpperBoundFunc : Sequence2PointFunc<P>
{
	P operator ()(typename UpperBoundFunc::arg1_cref t) const
	{ 
		return UpperBoundFromSequence(begin_ptr(t), end_ptr(t) );
	}
};

template <class P>
struct CenterBoundFunc : Sequence2PointFunc<P>
{
	typename CenterBoundFunc::res_type operator() (typename CenterBoundFunc::arg1_cref arg1) const
	{
		return Convert<typename CenterBoundFunc::res_type>( Center(RangeFromSequence(begin_ptr(arg1), end_ptr(arg1) ) ) );
	}
};

template <typename P>
struct FirstFunc : Sequence2PointFunc<P>
{
	P operator ()(typename FirstFunc::arg1_cref t) const
	{ 
		return t.size()
			? t.front()
			: UNDEFINED_VALUE(P);
	}
};
template <typename P>
struct LastFunc : Sequence2PointFunc<P>
{
	P operator ()(typename LastFunc::arg1_cref t) const
	{ 
		return t.size()
			? t.back()
			: UNDEFINED_VALUE(P);
	}
};

#include "geo/CentroidOrMid.h"

template <class P>
struct CentroidFunc : Sequence2PointFunc<P>
{
	using typename Sequence2PointFunc<P>::res_type;
	res_type operator() (typename CentroidFunc::arg1_cref arg1) const
	{
		if (arg1.size() <= 2)
			return UNDEFINED_VALUE(res_type);
		return Convert<res_type>( Centroid<Float64>(arg1) );
	}
};

template <class P>
struct CentroidOrMidFunc : Sequence2PointFunc<P>
{
	mutable ScanPointCalcResource<Float64> m_CalcResource;
	typename CentroidOrMidFunc::res_type operator() (typename CentroidOrMidFunc::arg1_cref arg1) const
	{
		if (arg1.size() <= 2)
			return UNDEFINED_VALUE(typename CentroidOrMidFunc::res_type);
		return Convert<typename CentroidOrMidFunc::res_type>( CentroidOrMid<Float64, P>(arg1, m_CalcResource) );
	}
};

template <class P>
struct MidFunc : Sequence2PointFunc<P>
{
	mutable ScanPointCalcResource<Float64> m_CalcResource;
	typename MidFunc::res_type operator() (typename MidFunc::arg1_cref arg1) const
	{
		if (arg1.size() <= 2)
			return UNDEFINED_VALUE(typename MidFunc::res_type);
		return Convert<typename MidFunc::res_type>( Mid<Float64>(begin_ptr(arg1), end_ptr(arg1), m_CalcResource) );
	}
};

// *****************************************************************************
//								Sequence2Scalar Functors
// *****************************************************************************

template <class P>
struct Sequence2ScalarFunc
	:	unary_func<
			typename scalar_of<P>::type
		,	typename sequence_traits<P>::container_type
		>
{};

// *****************************************************************************
//									ArcLength Operator
// *****************************************************************************

template <class P>
struct ArcLengthFunc : Sequence2ScalarFunc<P>
{
	auto operator() (typename ArcLengthFunc::arg1_cref arg1) const -> typename ArcLengthFunc::res_type
	{
		return Convert<typename ArcLengthFunc::res_type>( ArcLength<Float64>(arg1) );
	}
};

template <class P>
struct MlsLengthFunc : Sequence2ScalarFunc<P>
{
	auto operator() (typename MlsLengthFunc::arg1_cref arg1) const -> typename MlsLengthFunc::res_type
	{
		return Convert<typename MlsLengthFunc::res_type>(MlsLength<Float64>(arg1));
	}
};

// *****************************************************************************
//									Area Operator
// *****************************************************************************

#include "geo/Area.h"

template <class P>
struct AreaFunc : Sequence2ScalarFunc<P>
{
	typename AreaFunc::res_type operator() (typename AreaFunc::arg1_cref arg1) const
	{
		return Convert<typename AreaFunc::res_type>( Area<Float64>(arg1) );
	}
};

// *****************************************************************************
//									Points2SequenceOperator
// *****************************************************************************

template<typename PolygonIndex>
class AbstrPoints2SequenceOperator : public VariadicOperator
{
public:
	using OrdinalType = UInt32;

	typedef DataArray<PolygonIndex> Arg2Type;
	typedef DataArray<OrdinalType>  Arg3Type;

	static const bool hasSeqDomainArg   = !std::is_same_v<PolygonIndex, Void>;
	bool              m_HasSeqOrd       : 1;
	ValueComposition  m_VC              : ValueComposition_BitCount;


	AbstrPoints2SequenceOperator(AbstrOperGroup* gr, const Class* resultType, const Class* arg1Type, bool hasSeqOrd, ValueComposition vc)
		:	VariadicOperator(gr, resultType, 1 + (hasSeqDomainArg ? 1 : 0) + (hasSeqOrd ? 1 : 0) )
		,	m_HasSeqOrd(hasSeqOrd)
		,	m_VC(vc)
	{
		ClassCPtr* argClasses = m_ArgClasses.get();
		*argClasses++ = arg1Type;
		if constexpr (hasSeqDomainArg)
			*argClasses++ = Arg2Type::GetStaticClass();
		if (hasSeqOrd)
			*argClasses++ = Arg3Type::GetStaticClass();
		dms_assert(argClasses == m_ArgClassesEnd);
	}

	// Override Operator
	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		dms_assert(args.size() == (m_ArgClassesEnd - m_ArgClassesBegin));

		const AbstrDataItem* arg1A = AsDataItem(args[0]); UInt32 argCount = 0;
		dms_assert(arg1A);

		const AbstrDataItem* arg2A = nullptr; if (hasSeqDomainArg) arg2A = AsDataItem(args[++argCount]);
		const AbstrDataItem* arg3A = nullptr; if (m_HasSeqOrd    ) arg3A = AsDataItem(args[++argCount]);

		const AbstrUnit* pointEntity = arg1A->GetAbstrDomainUnit();
		const AbstrUnit* valuesUnit  = arg1A->GetAbstrValuesUnit();
		const AbstrUnit* polyEntity  = arg2A ? arg2A->GetAbstrValuesUnit() : Unit<Void>::GetStaticClass()->CreateDefault();

		if (!resultHolder)
		{
			dms_assert(!mustCalc);
			if (arg2A) pointEntity->UnifyDomain(arg2A->GetAbstrDomainUnit(), "e1", "e2", UM_Throw);
			if (arg3A) pointEntity->UnifyDomain(arg3A->GetAbstrDomainUnit(), "e1", "e3", UM_Throw);
			resultHolder = CreateCacheDataItem(polyEntity, valuesUnit, m_VC);
		}
		if (mustCalc)
		{
			DataReadLock arg1Lock(arg1A);
			DataReadLock arg2Lock(arg2A);
			DataReadLock arg3Lock(arg3A);

			AbstrDataItem* res = AsDataItem(resultHolder.GetNew());
			DataWriteLock resLock(res);

			this->Calculate(resLock.get(), arg1A, arg2A, arg3A);
			resLock.Commit();
		}
		return true;
	}

	virtual void Calculate(AbstrDataObject* res, const AbstrDataItem* arg1A, const AbstrDataItem* arg2A, const AbstrDataItem* arg3A) const =0;
};

template <typename ArgType>
struct get_locked_cseq_t { using type = typename ArgType::locked_cseq_t; };

template <class T, typename PolygonIndex>
class Points2SequenceOperator : public AbstrPoints2SequenceOperator<PolygonIndex>
{
	typedef T                          PointType;
	typedef std::vector<PointType>     PolygonType;
	typedef Unit<PointType>            PointUnitType;
	typedef DataArray<PolygonType>     ResultType;	
	typedef DataArray<PointType>       Arg1Type;

	using typename AbstrPoints2SequenceOperator<PolygonIndex>::Arg2Type;
	using typename AbstrPoints2SequenceOperator<PolygonIndex>::Arg3Type;
	using typename AbstrPoints2SequenceOperator<PolygonIndex>::OrdinalType;

	using AbstrPoints2SequenceOperator<PolygonIndex>::hasSeqDomainArg;
public:
	Points2SequenceOperator(AbstrOperGroup* gr, bool hasSeqOrd, ValueComposition vc)
		: AbstrPoints2SequenceOperator<PolygonIndex>(gr, ResultType::GetStaticClass(), Arg1Type::GetStaticClass(), hasSeqOrd, vc)
	{}

	void Calculate(AbstrDataObject* res, const AbstrDataItem* arg1A, const AbstrDataItem* arg2A, const AbstrDataItem* arg3A) const override
	{
		const Arg1Type* arg1 = const_array_cast<PointType>(arg1A); // point array
		const Arg2Type* arg2 = nullptr; if constexpr (hasSeqDomainArg) arg2 = const_array_cast<PolygonIndex>(arg2A); // polygon ordinal; nullptr indicates sequence set has void domain (thus, one sequence)
		const Arg3Type* arg3 = nullptr; if (arg3A) arg3 = const_array_cast<OrdinalType> (arg3A); // ordinal within polygon; nullptr indicates ascending order
		assert(arg1); 

		ResultType *result = mutable_array_cast<PolygonType>(res);
		assert(result);

		auto resData = result->GetDataWrite(no_tile, dms_rw_mode::write_only_mustzero);

		using PolygonIndexOrUInt32 = std::conditional_t< hasSeqDomainArg, PolygonIndex, UInt32>;

		auto polyIndexRange = Range<PolygonIndexOrUInt32>(0, 1);
		if constexpr (hasSeqDomainArg)
			polyIndexRange = arg2->GetValueRangeData()->GetRange();

		PolygonIndexOrUInt32 nrPolys = Cardinality(polyIndexRange);
		tile_id tn = arg1A->GetAbstrDomainUnit()->GetNrTiles();
		std::vector<bool> hasPoly(tn);

		typename std::conditional_t < hasSeqDomainArg, get_locked_cseq_t<Arg2Type>, std::type_identity<Void>>::type arg2Data;
		std::vector<OrdinalType> nrPointsPerSeq (nrPolys, 0);

		// === first count the nrPointsPerSeq
		for (tile_id ta=0; ta!=tn; ++ta)
		{
			bool thisTileHasPoly = false;
			if constexpr (hasSeqDomainArg)
			{
				arg2Data = arg2->GetLockedDataRead(ta);

				auto i2 = arg2Data.begin(), e2 = arg2Data.end();
				for(; i2 != e2; ++i2)
				{
					SizeT polyNr = *i2;
					polyNr -= polyIndexRange.first;
					if (polyNr < nrPolys)
					{
						++nrPointsPerSeq[polyNr];
						thisTileHasPoly = true;
					}
				}
			}
			else
			{
				thisTileHasPoly = true;
				nrPointsPerSeq[0] += arg1A->GetAbstrDomainUnit()->GetTileCount(ta);
			}

			hasPoly[ta] = thisTileHasPoly; // flag each input-tile on incidence with the current sequence range.
		}
		MG_DEBUGCODE( size_t cumulSize = 0; )
		dbg_assert(cumulSize == resData.get_sa().actual_data_size());

		SizeT nrPoints = std::accumulate(nrPointsPerSeq.begin(), nrPointsPerSeq.end(), SizeT(0));
		resData.get_sa().data_reserve(nrPoints MG_DEBUG_ALLOCATOR_SRC("res->md_SrcStr"));

		// ==== then resize each resulting polygon according to the nr of seen points.
		for (PolygonIndexOrUInt32 i=0; i!=nrPolys; ++i)
		{
			SizeT polySize = nrPointsPerSeq[i];
			resData[i].resize_uninitialized(polySize);
			dbg_assert((cumulSize += polySize) == resData.get_sa().actual_data_size());
		}
		dbg_assert(cumulSize == nrPoints);


		assert(resData.get_sa().actual_data_size() == nrPoints); // follows from previous asserts
		auto br = resData.begin();

		// ==== then proces all input-tiles again and collect to the resulting polygon tile
		OwningPtrSizedArray<SizeT> currOrdinals;
		if (!arg3) 
			currOrdinals = OwningPtrSizedArray<SizeT>(nrPolys, value_construct MG_DEBUG_ALLOCATOR_SRC("OperPolygon: currOrdinals"));

		for (tile_id ta=0; ta!=tn; ++ta) if (hasPoly[ta])
		{
			auto arg1Data = arg1->GetLockedDataRead(ta);
			typename Arg3Type::locked_cseq_t arg3Data;
			if constexpr (hasSeqDomainArg)
				arg2Data = arg2->GetLockedDataRead(ta); // continuity of tile lock if tn = 1
			if (arg3) 
				arg3Data = arg3->GetLockedDataRead(ta);

			auto b1 = arg1Data.begin();
			auto b3 = arg3Data.begin();
			SizeT polyNr = 0;
			for (SizeT i=0, n = arg1Data.size(); i!=n; ++i)
			{
				if constexpr (hasSeqDomainArg)
					polyNr = arg2Data[i];
				if (polyNr < nrPolys)
				{
					UInt32 orderNr = (arg3) ? b3[i] : currOrdinals[polyNr]++;

					if (orderNr >= nrPointsPerSeq[polyNr])
						this->GetGroup()->throwOperErrorF("unexpected orderNr %d for sequence %d which has %d elements", orderNr, polyNr, nrPointsPerSeq[polyNr]);
					typename ResultType::reference polygon = *(br + polyNr);
					polygon[orderNr] = b1[i];
				}
			}
		}
	}
};

// *****************************************************************************
//									Arcs2SegmentsOperator
// *****************************************************************************

static TokenID s_Point = token::point;
static TokenID s_NextPoint = GetTokenID_st("NextPoint");
static TokenID s_DepreciatedSequenceNr = GetTokenID_st("SequenceNr");

namespace {
	enum TableCreateFlags
	{
		DoCloseLast           = 0x0001
	,	DoCreateNextPoint     = 0x0002
	,	DoCreateNrOrgEntity   = 0x0004
	,	DoCreateOrdinal       = 0x0008
	,	DoCreateDistFromStart = 0x0010
	,	DoIncludeEndPoints    = 0x0020
	,   CreateUInt64Domain    = 0x0040
	};

	const UnitClass* ResultDomainClass(TableCreateFlags tcf)
	{
		auto resultUnitClass = Unit<UInt32>::GetStaticClass();
		if (tcf & CreateUInt64Domain)
			resultUnitClass = Unit<UInt64>::GetStaticClass();
		return resultUnitClass;
	}

	AbstrUnit* CreateResultDomain(TreeItem* resultHolder, TableCreateFlags tcf)
	{
		auto resultUnitClass = ResultDomainClass(tcf);

		auto* resDomain = resultUnitClass->CreateResultUnit(resultHolder);
		assert(resDomain);
		resDomain->SetTSF(TSF_Categorical);
		return resDomain;
	}
}	//	end of anonimous namespace

struct AbstrArcs2SegmentsOperator : public UnaryOperator
{
//	typedef Unit<UInt32> ResultUnitType;
	AbstrArcs2SegmentsOperator(AbstrOperGroup* gr, TableCreateFlags createFlags, const Class* arg1Class)
		:	UnaryOperator(gr, ResultDomainClass(createFlags), arg1Class)
		,	m_CreateFlags(createFlags)
	{
		dms_assert(createFlags & (DoCloseLast | DoCreateNextPoint));
		dms_assert(!(createFlags & DoCreateDistFromStart)); // NYI
	}

	// Override Operator
	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		assert(args.size() == 1);

		auto arg1A = AsDataItem(args[0]);
		assert(arg1A);

		const AbstrUnit* polyEntity      = arg1A->GetAbstrDomainUnit();
		const AbstrUnit* pointValuesUnit = arg1A->GetAbstrValuesUnit();

		AbstrUnit* resDomain = CreateResultDomain(resultHolder, m_CreateFlags);
		resultHolder = resDomain;

		AbstrDataItem 
			*resSub1 = CreateDataItem(resDomain, s_Point, resDomain, pointValuesUnit),
			*resSub2 = nullptr, 
			*resSub3 = nullptr,
			*resSub4 = nullptr;

		MG_PRECONDITION(resSub1);

		if (m_CreateFlags & DoCreateNextPoint)
		{
			resSub2 = CreateDataItem(resDomain, s_NextPoint, resDomain, pointValuesUnit);
			MG_PRECONDITION(resSub2);
		}

		if (m_CreateFlags & DoCreateNrOrgEntity && polyEntity->GetUnitClass() != Unit<Void>::GetStaticClass() )
		{
			resSub3 = CreateDataItem(resDomain, token::sequence_rel, resDomain, polyEntity);
			MG_PRECONDITION(resSub3);
			resSub3->SetTSF(TSF_Categorical);
			if (!mustCalc)
			{
				auto depreciatedRes = CreateDataItem(resDomain, s_DepreciatedSequenceNr, resDomain, polyEntity);
				depreciatedRes->SetTSF(TSF_Categorical);
				depreciatedRes->SetTSF(TSF_Depreciated);
				depreciatedRes->SetReferredItem(resSub3);
			}
		}

		if (m_CreateFlags & DoCreateOrdinal)
		{
			resSub4 = CreateDataItem(resDomain, token::ordinal, resDomain, Unit<UInt32>::GetStaticClass()->CreateDefault() );
			MG_PRECONDITION(resSub4);
		}
		
		if (mustCalc)
		{
			dms_assert(resultHolder->GetInterestCount()); // DEBUG

			DataReadLock arg1Lock(arg1A);

			Calculate(resDomain, arg1A, resSub1, resSub2, resSub3, resSub4);
		}
		return true;
	}

	virtual void Calculate(AbstrUnit* resDomain
	,	const AbstrDataItem* arg1A
	,	AbstrDataItem* resSub1
	,	AbstrDataItem* resSub2 
	,	AbstrDataItem* resSub3
	,	AbstrDataItem* resSub4
	)	const = 0;

	TableCreateFlags m_CreateFlags;
};

template <typename T>
class Arcs2SegmentsOperator : public AbstrArcs2SegmentsOperator
{
	typedef T                         PointType;
	typedef std::vector<PointType>    PolygonType;
	typedef Unit<PointType>           PointUnitType;

	typedef DataArray<PolygonType>    Arg1Type;
	typedef DataArray<PointType>      ResultSub1Type; // Point
	typedef DataArray<PointType>      ResultSub2Type; // NextPoint
	typedef AbstrDataItem             ResultSub3Type; // nrOrgEntity
	typedef DataArray<UInt32>         ResultSub4Type; // Position in Sequence

public:
	Arcs2SegmentsOperator(AbstrOperGroup* gr, int createFlags)
		:	AbstrArcs2SegmentsOperator(gr, TableCreateFlags(createFlags), Arg1Type::GetStaticClass() )
	{}

	void Calculate(AbstrUnit* resDomain
	,	const AbstrDataItem* arg1A
	,	AbstrDataItem* resSub1
	,	AbstrDataItem* resSub2 
	,	AbstrDataItem* resSub3
	,	AbstrDataItem* resSub4
	)	const override
	{
		const Arg1Type* arg1 = const_array_cast<PolygonType>(arg1A);
		dms_assert(arg1);

		SizeT nrPoints = 0;
		bool   bDoCloseLast = (m_CreateFlags & DoCloseLast);
		tile_id tn = arg1A->GetAbstrDomainUnit()->GetNrTiles();
		for (tile_id t=0; t!=tn; ++t)
		{
			auto arg1Data = arg1->GetLockedDataRead(t);

			// first, calc cardinality for resDomain
			for (auto i=arg1Data.begin(), e=arg1Data.end(); i !=e; ++i)
			{
				auto sz = i->size();
				if (sz && !bDoCloseLast)
					--sz;
				nrPoints += sz;
			}
		}

		resDomain->SetCount(nrPoints);
		tile_id trn = resDomain->GetNrTiles();

		SizeT nrSkippedPoints = bDoCloseLast ? 0 : 1;
		dms_assert(resSub1);
		{
			locked_tile_write_channel<PointType> resWriter(resSub1);
			for (tile_id t=0; t!=tn; ++t)
			{
				auto arg1Data = arg1->GetLockedDataRead(t);
				for (auto i=arg1Data.begin(), e=arg1Data.end(); i !=e; ++i)
				{
					SizeT iSize = i->size();
					if (iSize)
						resWriter.Write(i->begin(), iSize - nrSkippedPoints);
				}
			}
			dms_assert(resWriter.IsEndOfChannel());
			resWriter.Commit();
		}

		if (resSub2)
		{
			locked_tile_write_channel<PointType> resWriter(resSub2);
			for (tile_id t=0; t!=tn; ++t)
			{
				auto arg1Data = arg1->GetLockedDataRead(t);
				for (auto i=arg1Data.begin(), e=arg1Data.end(); i !=e; ++i)
				{
					SizeT iSize = i->size();
					if (iSize)
					{
						resWriter.Write(i->begin()+1, i->size()-1);
						if (bDoCloseLast)
							resWriter.Write(*(i->begin())); // close ring
					}
				}
			}
			dms_assert(resWriter.IsEndOfChannel());
			resWriter.Commit();
		}

		if (resSub3)
		{
			DataWriteLock resLock(resSub3);
			if (resLock->GetTiledRangeData()->GetNrTiles())
			{
				AbstrDataObject* resDataObj = resLock.get();

				tile_id tr = 0;
				auto resTileLock = resLock->GetWritableTileLock(tr, dms_rw_mode::write_only_all);
				SizeT ri = 0, re = resLock->GetTiledRangeData()->GetTileSize(tr);
				SizeT c = 0;

				for (tile_id t = 0; t != tn; ++t)
				{
					auto arg1Data = arg1->GetTile(t);
					for (auto i = arg1Data.begin(), e = arg1Data.end(); i != e; ++c, ++i)
					{
						SizeT iSize = i->size();
						if (iSize)
						{
							iSize -= nrSkippedPoints;
							while (iSize)
							{
								SizeT mSize = Min<SizeT>(iSize, re - ri);
								if (mSize)
									resLock->FillWithInt32Values(tile_loc(tr, ri), mSize, c);
								iSize -= mSize;
								if (iSize)
								{
									resTileLock = resLock->GetWritableTileLock(++tr, dms_rw_mode::write_only_all);
									ri = 0; re = resLock->GetTiledRangeData()->GetTileSize(tr);
								}
								else
									ri += mSize;
							}
						}
					}
				}
				dms_assert(tr + 1 == trn);
				dms_assert(resLock->GetTiledRangeData()->GetTileSize(tr) == ri);
			}
			resLock.Commit();
		}
		if (resSub4)
		{
			locked_tile_write_channel<UInt32> resWriter(resSub4);
			for (tile_id t=0; t!=tn; ++t)
			{
				auto arg1Data = arg1->GetTile(t);
				for (auto i=arg1Data.begin(), e=arg1Data.end(); i !=e; ++i)
				{
					SizeT iSize = i->size();
					if (iSize)
						resWriter.WriteCounter(iSize - nrSkippedPoints);
				}
			}
			dms_assert(resWriter.IsEndOfChannel());
			resWriter.Commit();
		}
	}
};

// *****************************************************************************
//									DynaPointOperator
// *****************************************************************************

struct AbstrDynaPointOperator : public TernaryOperator
{
	AbstrDynaPointOperator(AbstrOperGroup* gr, TableCreateFlags createFlags, const Class* pointDataClass, const Class* distDataClass)
		:	TernaryOperator(
				gr
			,	ResultDomainClass(createFlags)
			,	pointDataClass
			,	pointDataClass
			,	distDataClass
			)
		,	m_CreateFlags(createFlags)

	{
		dms_assert(!(createFlags & DoCreateDistFromStart)); // NYI
	}

	// Override Operator
	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		dms_assert(args.size() == 3);

		const AbstrDataItem* arg1A = debug_cast<const AbstrDataItem*>(args[0]);
		const AbstrDataItem* arg2A = debug_cast<const AbstrDataItem*>(args[1]);
		const AbstrDataItem* arg3A = debug_cast<const AbstrDataItem*>(args[2]);
		dms_assert(arg1A);
		dms_assert(arg2A);
		dms_assert(arg3A);

		const AbstrUnit* pointEntity     = arg1A->GetAbstrDomainUnit();
		const AbstrUnit* pointValuesUnit = arg1A->GetAbstrValuesUnit();

		pointEntity->UnifyDomain(arg2A->GetAbstrDomainUnit(), "e1", "e2", UM_Throw);
		Unit<Void>::GetStaticClass()->CreateDefault()->UnifyDomain(arg3A->GetAbstrDomainUnit(), "Unit<Void>", "e3", UM_Throw);

		AbstrUnit* resDomain = CreateResultDomain(resultHolder, m_CreateFlags);
		resultHolder = resDomain;

		AbstrDataItem 
			*resSub1 = CreateDataItem(resDomain, s_Point, resDomain, pointValuesUnit),
			*resSub2 = nullptr,
			*resSub3 = nullptr,
			*resSub4 = nullptr;

		MG_PRECONDITION(resSub1);

		if (m_CreateFlags & DoCreateNextPoint)
		{
			resSub2 = CreateDataItem(resDomain, s_NextPoint, resDomain, pointValuesUnit);
			MG_PRECONDITION(resSub2);
		}

		if (m_CreateFlags & DoCreateNrOrgEntity && pointEntity->GetUnitClass() != Unit<Void>::GetStaticClass() )
		{
			resSub3 = CreateDataItem(resDomain, token::sequence_rel, resDomain, pointEntity);
			resSub3->SetTSF(TSF_Categorical);
			MG_PRECONDITION(resSub3);

			if (!mustCalc)
			{
				auto depreciatedRes = CreateDataItem(resDomain, s_DepreciatedSequenceNr, resDomain, pointEntity);
				depreciatedRes->SetTSF(TSF_Categorical);
				depreciatedRes->SetTSF(TSF_Depreciated);
				depreciatedRes->SetReferredItem(resSub3);
			}
		}

		if (m_CreateFlags & DoCreateOrdinal)
		{
			resSub4 = CreateDataItem(resDomain, token::ordinal, resDomain, Unit<UInt32>::GetStaticClass()->CreateDefault() );
			resSub4->SetTSF(TSF_Categorical);
			MG_PRECONDITION(resSub4);

		}
		
		if (mustCalc)
		{
			DataReadLock arg1Lock(arg1A);
			DataReadLock arg2Lock(arg2A);
			DataReadLock arg3Lock(arg3A);

			Calculate(resDomain
			,	arg1A, arg2A, arg3A
			,	resSub1, resSub2, resSub3, resSub4
			);
		}
		return true;
	}

	virtual void Calculate(AbstrUnit* resDomain
	,	const AbstrDataItem* arg1A
	,	const AbstrDataItem* arg2A
	,	const AbstrDataItem* arg3A
	,	AbstrDataItem* resSub1
	,	AbstrDataItem* resSub2 
	,	AbstrDataItem* resSub3
	,	AbstrDataItem* resSub4
	)	const = 0;

	TableCreateFlags m_CreateFlags;
};

template <class T>
class DynaPointOperator : public AbstrDynaPointOperator
{
	typedef T                                   PointType;
	typedef typename scalar_of<PointType>::type DistType;
	typedef Unit<PointType>                     PointUnitType;
	typedef Unit<DistType>                      DistUnitType;

	typedef DataArray<PointType>       Arg1Type;
	typedef DataArray<PointType>       Arg2Type;
	typedef DataArray<DistType>        Arg3Type;
	typedef DataArray<PointType>       ResultSub1Type; // Point
	typedef DataArray<PointType>       ResultSub2Type; // NextPoint
	typedef AbstrDataItem              ResultSub3Type; // sequence_rel
	typedef DataArray<UInt32>          ResultSub4Type; // Ordinal

public:
	DynaPointOperator(AbstrOperGroup* gr, int createFlags)
		:	AbstrDynaPointOperator(gr, TableCreateFlags(createFlags), Arg1Type::GetStaticClass(), Arg3Type::GetStaticClass() )
	{}

	void Calculate(AbstrUnit* resDomain
		, const AbstrDataItem* arg1A
		, const AbstrDataItem* arg2A
		, const AbstrDataItem* arg3A
		, AbstrDataItem* resSub1
		, AbstrDataItem* resSub2
		, AbstrDataItem* resSub3
		, AbstrDataItem* resSub4
	)	const override
	{
		auto arg1Data = const_array_cast<PointType>(arg1A)->GetDataRead();
		auto arg2Data = const_array_cast<PointType>(arg2A)->GetDataRead();

		dms_assert(arg1Data.size() == arg2Data.size());

		typename Arg1Type::const_iterator
			b1 = arg1Data.begin()
			, e1 = arg1Data.end()
			, i1 = b1
			, b2 = arg2Data.begin()
			, i2 = b2
			, last_i2 = {};

		Float64 dist = const_array_cast<DistType>(arg3A)->GetDataRead()[0];
		
		// first, calc cardinality for resDomain
		bool withEnds = (this->m_CreateFlags & TableCreateFlags::DoIncludeEndPoints);
		bool withNextPoints = (resSub2 != nullptr);
		bool extraStartPoint = (withEnds && !withNextPoints);

		SizeT nrPoints = 0;
		Float64 carry = 0;
		bool isFirstPoint = true;
		for (; i1 != e1; ++i2, ++i1)
		{
			if (!IsDefined(*i1))
				continue;
			if (!IsDefined(*i2))
				continue;

			if (!isFirstPoint)
				if (*last_i2 != *i1)
					isFirstPoint = true;
			PointType segment = *i2 - *i1;
			Float64 segmLength = sqrt(Norm<Float64>(segment));
			if (isFirstPoint)
			{
				if (!resSub2)
					nrPoints++;
			}
			SizeT nrPointsHere = (segmLength+carry) / dist;
			assert((segmLength+carry) >= dist * nrPointsHere); // assume division and float->int conversion round off towards zero.
			carry += (segmLength - dist * nrPointsHere);
			assert(carry >= 0);

			if (withEnds && carry)
			{
				++nrPointsHere;
				carry = 0;
			}
			nrPoints += nrPointsHere;
			isFirstPoint = false;
			last_i2 = i2;
		}
		resDomain->SetCount(nrPoints);

		// ==== calc point locations and segmentsId
		dms_assert(resSub1);
		locked_tile_write_channel<PointType> ri1(resSub1);
		locked_tile_write_channel<PointType> ri2(resSub2);
		locked_abstr_tile_write_channel      ri3(resSub3);
		locked_tile_write_channel<UInt32>    ri4(resSub4);

//		PointType* ri1 = resData1.begin();
		SizeT nrOrgEntity = 0, currPointIndex = 0;
//		UInt32 ri3 = 0;

		carry = 0;
		isFirstPoint = true;
		DPoint prevLoc;
		for (i1 = b1, i2 = b2; i1 != e1; ++nrOrgEntity, ++i2, ++i1)
		{
			if (!IsDefined(*i1))
				continue;
			if (!IsDefined(*i2))
				continue;

			UInt32 ordinalID = 0;
			if (!isFirstPoint)
				if (*last_i2 != *i1)
					isFirstPoint = true;
			DPoint segment = *i2 - *i1;
			Float64 segmLengthOrg = sqrt(Norm<Float64>(segment));
			Float64 segmLength = segmLengthOrg;
			if (isFirstPoint)
			{
				if (resSub2)
					prevLoc = *i1;
				else
				{
					ri1.Write(*i1);
					if (resSub3)
						ri3.WriteUInt32(nrOrgEntity);
					if (resSub4)
						ri4.Write(ordinalID++);
					currPointIndex++;
				}
			}
			SizeT nrPointsHere = (segmLength +carry) / dist;
			dms_assert(segmLength+carry >= dist * nrPointsHere); // assume division and float->int conversion round off towards zero.

			if (nrPointsHere)
			{
				segment /= segmLengthOrg; // norm
				DPoint currLoc = *i1;
				dms_assert(carry <= dist);
				currLoc += segment * (dist - carry);
				ri1.Write(currLoc);
				if (resSub2)
				{
					ri2.Write(prevLoc);
					prevLoc = currLoc;
				}
				if (resSub3)
					ri3.FillWithUInt32Values(nrPointsHere, nrOrgEntity);
				if (resSub4)
					ri4.Write(ordinalID++);

				if (nrPointsHere > 1)
				{
					segment *= dist;
					UInt32 nrRemainingPoints = nrPointsHere;
					while (--nrRemainingPoints)
					{
						currLoc += segment;
						ri1.Write(currLoc);
						if (resSub2)
						{
							ri2.Write(prevLoc);
							prevLoc = currLoc;
						}
						if (resSub4)
							ri4.Write(ordinalID++);
					}
				}
			}

			carry += (segmLength - dist * nrPointsHere);
			dms_assert(carry >= 0);

			if (withEnds)
			{
				if (carry)
				{
					ri1.Write(*i2);
					if (resSub2)
						ri2.Write(prevLoc);
					ri3.WriteUInt32(nrOrgEntity);
					ri4.Write(ordinalID++);
					++nrPointsHere;
					carry = 0;
				}
			}
			currPointIndex += nrPointsHere;
			isFirstPoint = false;
			last_i2 = i2;
		}
		dms_assert(currPointIndex == nrPoints);
		if (resSub1) { dms_assert(ri1.IsEndOfChannel());  ri1.Commit(); }
		if (resSub2) { dms_assert(ri2.IsEndOfChannel());  ri2.Commit(); }
		if (resSub3) { dms_assert(ri3.IsEndOfChannel());  ri3.Commit(); }
		if (resSub4) { dms_assert(ri4.IsEndOfChannel());  ri4.Commit(); }
	}
};

// *****************************************************************************
//									PointInPolygon
// *****************************************************************************

#include "geo/IsInside.h"

#include "UnitProcessor.h"
#include "geo/SpatialIndex.h"

template <typename E, typename ResSequence, typename PointArray, typename PolyArray, typename SpatialIndexType>
void point_in_polygon(
		      ResSequence resData,
		const PointArray& pointData, 
		const PolyArray&  polyData,
		const Range<E>&   polyDomain,
		SpatialIndexType* spIndexPtr,
		bool  isFirstPolyTile)
{
	typename PolyArray::const_iterator
		firstPolygon = polyData.begin(),
		lastPolygon  = polyData.end();

	typedef PointArray::value_type      PointType;
	typedef scalar_of<PointType>::type  ScalarType;
	typedef ResSequence::value_type     ResValueType;

	SizeT count = resData.size();
	dms_assert(count == pointData.size());
	dms_assert(polyData.size() == Cardinality(polyDomain));

	auto rb = resData.begin();
	auto pb = pointData.begin();
	auto polyb = polyData.begin();

	using spatial_iterator_type = typename SpatialIndexType::template iterator<PointType>;
	spatial_iterator_type iter;

	for (SizeT i = 0; i != count; ++i)
	{
		auto pi = pb + i;
		auto ri = rb + i;
		if (IsDefined(*pi) && IsIntersecting(spIndexPtr->GetBoundingBox(), *pi))
		{
			for (iter = spIndexPtr->begin(*pi); iter; ++iter)
				if (IsInside(*((*iter)->get_ptr()), *pi))
				{
					SizeT c = (*iter)->get_ptr() - polyb;
					dms_assert(c < Cardinality(polyDomain));
					*ri = Range_GetValue_naked(polyDomain, c);
					goto nextI;
				}
		}
		if (isFirstPolyTile) // don't overwrite earlier found polygon-ids
			*ri = UNDEFINED_OR_ZERO(ResValueType);
	nextI:
		;
	}
}

CommonOperGroup cogPP("point_in_polygon", oper_policy::dynamic_result_class | oper_policy::better_not_in_meta_scripting);

class AbstrPointInPolygonOperator : public BinaryOperator
{
protected:
	AbstrPointInPolygonOperator(const DataItemClass* pointAttrClass, const DataItemClass* polyAttrClass)
		:	BinaryOperator(&cogPP, AbstrDataItem::GetStaticClass()
			,	pointAttrClass
			,	polyAttrClass
			)
	{}

	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		dms_assert(args.size() == 2);

		const AbstrDataItem* arg1A = AsDataItem(args[0]);
		const AbstrDataItem* arg2A = AsDataItem(args[1]);
		dms_assert(arg1A);
		dms_assert(arg2A);

		const AbstrUnit* domainUnit = arg1A->GetAbstrDomainUnit();
		const AbstrUnit* valuesUnit = arg2A->GetAbstrDomainUnit();
		arg1A->GetAbstrValuesUnit()->UnifyValues(arg2A->GetAbstrValuesUnit(), "v1", "v2", UM_Throw);

		bool isOnePolygon = arg2A->HasVoidDomainGuarantee();
		if (isOnePolygon)
			valuesUnit = Unit<Bool>::GetStaticClass()->CreateDefault();

		if (!resultHolder)
		{
			resultHolder = CreateCacheDataItem(domainUnit, valuesUnit);
			resultHolder->SetTSF(TSF_Categorical);
		}
		if (mustCalc)
		{
			DataReadLock arg1Lock(arg1A);
			DataReadLock arg2Lock(arg2A);

			AbstrDataItem* res = AsDataItem(resultHolder.GetNew());
			DataWriteLock resLock(res);

			ResourceHandle pointBoxDataHandle;

			tile_id te = domainUnit->GetNrTiles();
			for (tile_id t=0; t != te; ++t)
			{
				ReadableTileLock readPointLock (arg1A->GetCurrRefObj(), t);
				CreatePointHandle(arg1A, t, pointBoxDataHandle);
			}

			OwningPtrSizedArray<UInt32> polyTileCounters(te, value_construct MG_DEBUG_ALLOCATOR_SRC("OperPolygon: polyTileCounters"));
//			fast_zero(polyTileCounters.begin(), polyTileCounters.begin()+te);

//			for (tile_id u=0, ue = valuesUnit->GetNrTiles(); u != ue; ++u) // each polygon tile
//			{
				auto u = no_tile;
//				ReadableTileLock readPolyLock (arg2A->GetCurrRefObj(), u);
				ResourceHandle spIndexHandle, polyTileHandle;
				CreatePolyHandle(arg2A, u, spIndexHandle, polyTileHandle);

				// each point tile
				parallel_tileloop(te, [this, resObj = resLock.get(), res, arg1A, arg2A, u, &pointBoxDataHandle, &polyTileCounters, &spIndexHandle, &polyTileHandle](tile_id t)->void
					{
						if (this->IsIntersecting(t, u, pointBoxDataHandle, spIndexHandle))
						{
							Calculate(resObj, res->GetAbstrValuesUnit(), arg1A, arg2A, t, u, !polyTileCounters[t]++, pointBoxDataHandle, spIndexHandle, polyTileHandle);
						}
					}
				);
//			}
			for (tile_id t=0; t != te; ++t)
			{
				if (!polyTileCounters[t])
				{
					FastUndefiner(res->GetAbstrValuesUnit(), resLock.get(), t);
				}
			}
			resLock.Commit();
		}
		return true;
	}
	virtual void CreatePolyHandle(const AbstrDataItem* polyDataA, tile_id u, ResourceHandle& spIndexHandle, ResourceHandle& polyTileHandle) const =0;
	virtual void CreatePointHandle(const AbstrDataItem* pointDataA, tile_id t, ResourceHandle& pointBoxDataHandle) const =0;
	virtual bool IsIntersecting(tile_id t, tile_id u, ResourceHandle& pointBoxDataHandle, ResourceHandle& polyInfoHandle) const=0;
	virtual void Calculate(AbstrDataObject* res, const AbstrUnit* resVU, const AbstrDataItem* pointDataA, const AbstrDataItem* polyDataA, tile_id t, tile_id u, 
		bool mustInitPointTile, const ResourceHandle& pointBoxDataHandle, const ResourceHandle& spIndexHandle, const ResourceHandle& polyTileHandle) const=0;
};

template <typename P>
class PointInPolygonOperator : public AbstrPointInPolygonOperator
{
	typedef P                      PointType;
	typedef std::vector<PointType>  PolygonType;
	typedef Unit<PointType>        PointUnitType;
	typedef Range<PointType>       BoxType;

	using Arg1Type = DataArray<PointType>;
	using Arg2Type = DataArray<PolygonType>;
	using BoxArrayType = std::vector<BoxType>;

	typedef typename scalar_of<PointType>::type  ScalarType;
	using SpatialIndexType = SpatialIndex<ScalarType, typename Arg2Type::const_iterator>;

	using ResourceType = std::pair<SpatialIndexType, typename Arg2Type::locked_cseq_t>;

	struct DispatcherData
	{
		DispatcherData(
			AbstrDataObject* resObj,
			const Arg1Type* pointData,
			const Arg2Type* polyData,
			tile_id t, tile_id u, bool isFirstPolyTile,
			const SpatialIndexType* spIndexPtr,
			const typename Arg2Type::locked_cseq_t& polyTileData
		)	:	m_ResObj(resObj)
			,	m_PointData(pointData->GetDataRead(t))
			,	m_PolyData(polyTileData)
			,	m_PointTileID(t)
			,	m_PolyTileID(u)
			,	m_SpIndexPtr(spIndexPtr)
			,	m_IsFirstPolyTile(isFirstPolyTile)
		{}
		AbstrDataObject*          m_ResObj;
		typename Arg1Type::locked_cseq_t m_PointData;
		typename Arg2Type::locked_cseq_t m_PolyData;
		tile_id                   m_PointTileID, m_PolyTileID;
		bool                      m_IsFirstPolyTile;
		const SpatialIndexType*   m_SpIndexPtr;
	};


	struct DispatcherBase : UnitProcessor
	{
		template <typename E>
		void VisitImpl(const Unit<E>* inviter) const
		{
			assert(m_Data);
			point_in_polygon(
				composite_cast<DataArray<E>*>(m_Data->m_ResObj)->GetDataWrite(m_Data->m_PointTileID),
				m_Data->m_PointData,
				m_Data->m_PolyData,
				(m_Data->m_PolyTileID == no_tile) ? inviter->GetRange() : inviter->GetTileRange(m_Data->m_PolyTileID),
				m_Data->m_SpIndexPtr,
				m_Data->m_IsFirstPolyTile
			);
		}
		template <>
		void VisitImpl<Bool>(const Unit<Bool>* inviter) const
		{
			assert(m_Data);
			Range<UInt32> resValueRange = inviter->GetTileRange(m_Data->m_PolyTileID);

			 // result has boolean values
			dms_assert(resValueRange.first == 0);
			dms_assert(resValueRange.second == 2);
			dms_assert(m_Data->m_PolyTileID == 0); // boolean and void units are never tiled.
			dms_assert(m_Data->m_IsFirstPolyTile); // boolean and void units are never tiled.

			UInt32 polyDataSize = m_Data->m_PolyData.size();
			dms_assert(polyDataSize <= 2); // domain of polyDataSize is either bool or void.
			if (polyDataSize < Cardinality(resValueRange))
			{
				dms_assert(polyDataSize == 1); // domain of polyDataSize was void.
				resValueRange.first =resValueRange.second - polyDataSize; // set resValue to 'true' when inside the one polygon (for points outside the polygon, the value will remain the initial UNDEFINED_OR_ZERO(Bool), which is 'false'.
			}

			point_in_polygon(
				composite_cast<DataArray<Bool>*>(m_Data->m_ResObj)->GetDataWrite(m_Data->m_PointTileID),
				m_Data->m_PointData,
				m_Data->m_PolyData,
				resValueRange,
				m_Data->m_SpIndexPtr,
				true
			);
		}
		DispatcherData* m_Data = nullptr;
	};

	struct Dispatcher : boost::mpl::fold<typelists::partition_elements, DispatcherBase, VisitorImpl<Unit<boost::mpl::_2>, boost::mpl::_1> >::type
	{};

public:
	PointInPolygonOperator()
		:	AbstrPointInPolygonOperator(Arg1Type::GetStaticClass(), Arg2Type::GetStaticClass())
	{}

	// Override Operator
	void CreatePolyHandle(const AbstrDataItem* polyDataA, tile_id u, ResourceHandle& spIndexHandle, ResourceHandle& polyTileHandle) const override
	{
		const Arg2Type* polyData  = const_array_cast<PolygonType>(polyDataA);
		dms_assert(polyData); 

		auto polyTile = polyData->GetDataRead(u);

		spIndexHandle  = makeResource<SpatialIndexType>(polyTile.begin(), polyTile.end(), 0);
		polyTileHandle = makeResource<typename Arg2Type::locked_cseq_t>(std::move(polyTile));
	}

	void CreatePointHandle(const AbstrDataItem* pointDataA, tile_id t, ResourceHandle& pointBoxDataHandle) const override
	{
		if (!pointBoxDataHandle)
			pointBoxDataHandle = makeResource<BoxArrayType>(pointDataA->GetAbstrDomainUnit()->GetNrTiles());
		BoxArrayType& boxArray = GetAs<BoxArrayType>(pointBoxDataHandle);

		const Arg1Type* pointData = const_array_cast<PointType>(pointDataA);
		dms_assert(pointData); 

		auto pointArray = pointData->GetDataRead(t);
		dms_assert(t < boxArray.size());
		boxArray[t] = RangeFromSequence(pointArray.begin(), pointArray.end());
	}

	bool IsIntersecting(tile_id t, tile_id u, ResourceHandle& pointBoxDataHandle, ResourceHandle& spIndexHandle) const override
	{
		BoxArrayType&     boxArray = GetAs<BoxArrayType>(pointBoxDataHandle);
		SpatialIndexType& spIndex  = GetAs<SpatialIndexType>(spIndexHandle);

		return ::IsIntersecting(spIndex.GetBoundingBox(), boxArray[t]);
	}

	void Calculate(AbstrDataObject* res, const AbstrUnit* resVU, const AbstrDataItem* pointDataA, const AbstrDataItem* polyDataA, tile_id t, tile_id u, 
		bool mustInitPointTile, const ResourceHandle& pointBoxDataHandle, const ResourceHandle& spIndexHandle, const ResourceHandle& polyTileHandle) const override
	{
		const Arg1Type* pointData = const_array_cast<PointType  >(pointDataA);
		const Arg2Type* polyData  = const_array_cast<PolygonType>(polyDataA);
		dms_assert(pointData); 
		dms_assert(polyData); 

		const BoxArrayType    & boxArray    = GetAs<BoxArrayType>(pointBoxDataHandle);
		const SpatialIndexType* spIndexPtr  = GetOptional<SpatialIndexType>(spIndexHandle);

		DispatcherData data(res, pointData, polyData, t, u, mustInitPointTile, spIndexPtr, GetAs<typename Arg2Type::locked_cseq_t>(polyTileHandle));
		
		Dispatcher disp; disp.m_Data = &data;
		resVU->InviteUnitProcessor(disp);
	}
};


// *****************************************************************************
//	point_in_ranked_polygon
// *****************************************************************************

template <typename E, typename ResSequence, typename PointArray, typename PolyArray, typename RankArray, typename SpatialIndexType>
void point_in_ranked_polygon(
	ResSequence resData,
	const PointArray& pointData,
	const PolyArray& polyData,
	const Range<E>& polyDomain,
	SpatialIndexType* spIndexPtr,
	const RankArray& rankData,
	bool  isFirstPolyTile

)
{
	typename PolyArray::const_iterator
		firstPolygon = polyData.begin(),
		lastPolygon = polyData.end();

	typedef PointArray::value_type      PointType;
	typedef scalar_of<PointType>::type  ScalarType;
	typedef ResSequence::value_type     ResValueType;

	SizeT count = resData.size();
	assert(count == pointData.size());
	assert(polyData.size() == Cardinality(polyDomain));

	auto rb = resData.begin();
	auto pb = pointData.begin();
	auto polyb = polyData.begin();

	using spatial_iterator_type = typename SpatialIndexType::template iterator<PointType>;
	spatial_iterator_type iter;

	using RankType = typename RankArray::value_type;

	for (SizeT i = 0; i != count; ++i)
	{
		auto pi = pb + i;
		auto ri = rb + i;
		auto foundRank = UNDEFINED_VALUE(RankType);

		if (IsDefined(*pi) && IsIntersecting(spIndexPtr->GetBoundingBox(), *pi))
		{
			for (iter = spIndexPtr->begin(*pi); iter; ++iter)
				if (IsInside(*((*iter)->get_ptr()), *pi))
				{
					SizeT c = (*iter)->get_ptr() - polyb;
					assert(c < Cardinality(polyDomain));
					RankType thisRank = rankData[c];
					if (!IsDefined(foundRank) || thisRank > foundRank)
					{
						*ri = Range_GetValue_naked(polyDomain, c);
						foundRank = thisRank;
					}
//					goto nextI;
				}
		}
		if (isFirstPolyTile && !IsDefined(foundRank)) // don't overwrite earlier found polygon-ids
			*ri = UNDEFINED_OR_ZERO(ResValueType);
//	nextI:
		;
	}
}

CommonOperGroup cogPRP("point_in_ranked_polygon", oper_policy::dynamic_result_class | oper_policy::better_not_in_meta_scripting);

class AbstrPointInRankedPolygonOperator : public TernaryOperator
{
protected:
	AbstrPointInRankedPolygonOperator(const DataItemClass* pointAttrClass, const DataItemClass* polyAttrClass, const DataItemClass* rankAttrClass)
		: TernaryOperator(&cogPRP, AbstrDataItem::GetStaticClass()
			, pointAttrClass
			, polyAttrClass
			, rankAttrClass
		)
	{}

	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		dms_assert(args.size() == 3);

		const AbstrDataItem* arg1A = AsDataItem(args[0]);
		const AbstrDataItem* arg2A = AsDataItem(args[1]);
		const AbstrDataItem* arg3A = AsDataItem(args[2]);
		dms_assert(arg1A);
		dms_assert(arg2A);
		dms_assert(arg3A);

		const AbstrUnit* domainUnit = arg1A->GetAbstrDomainUnit();
		const AbstrUnit* valuesUnit = arg2A->GetAbstrDomainUnit();
		arg1A->GetAbstrValuesUnit()->UnifyValues(arg2A->GetAbstrValuesUnit(), "v1", "v2", UM_Throw);
		arg2A->GetAbstrDomainUnit()->UnifyDomain(arg3A->GetAbstrDomainUnit(), "e2", "e3", UM_Throw);

		bool isOnePolygon = arg2A->HasVoidDomainGuarantee();
		MG_CHECK(!isOnePolygon);

		if (!resultHolder)
		{
			resultHolder = CreateCacheDataItem(domainUnit, valuesUnit);
			resultHolder->SetTSF(TSF_Categorical);
		}
		if (mustCalc)
		{
			DataReadLock arg1Lock(arg1A);
			DataReadLock arg2Lock(arg2A);
			DataReadLock arg3Lock(arg3A);

			AbstrDataItem* res = AsDataItem(resultHolder.GetNew());
			DataWriteLock resLock(res);

			ResourceHandle pointBoxDataHandle;

			tile_id te = domainUnit->GetNrTiles();
			for (tile_id t = 0; t != te; ++t)
			{
				ReadableTileLock readPointLock(arg1A->GetCurrRefObj(), t);
				CreatePointHandle(arg1A, t, pointBoxDataHandle);
			}

			OwningPtrSizedArray<UInt32> polyTileCounters(te, value_construct MG_DEBUG_ALLOCATOR_SRC("OperPolygon: polyTileCounters"));
//			fast_zero(polyTileCounters.begin(), polyTileCounters.begin() + te);

			//			for (tile_id u=0, ue = valuesUnit->GetNrTiles(); u != ue; ++u) // each polygon tile
			//			{
			auto u = no_tile;
			//				ReadableTileLock readPolyLock (arg2A->GetCurrRefObj(), u);
			ResourceHandle spIndexHandle, polyTileHandle;
			CreatePolyHandle(arg2A, u, spIndexHandle, polyTileHandle);

			// each point tile
			parallel_tileloop(te, [this, resObj = resLock.get(), res, arg1A, arg2A, arg3A, u, &pointBoxDataHandle, &polyTileCounters, &spIndexHandle, &polyTileHandle](tile_id t)->void
				{
					if (this->IsIntersecting(t, u, pointBoxDataHandle, spIndexHandle))
					{
						Calculate(resObj, res->GetAbstrValuesUnit(), arg1A, arg2A, arg3A, t, u, !polyTileCounters[t]++, pointBoxDataHandle, spIndexHandle, polyTileHandle);
					}
				}
			);
			//			}
			for (tile_id t = 0; t != te; ++t)
			{
				if (!polyTileCounters[t])
				{
					FastUndefiner(res->GetAbstrValuesUnit(), resLock.get(), t);
				}
			}
			resLock.Commit();
		}
		return true;
	}
	virtual void CreatePolyHandle(const AbstrDataItem* polyDataA, tile_id u, ResourceHandle& spIndexHandle, ResourceHandle& polyTileHandle) const = 0;
	virtual void CreatePointHandle(const AbstrDataItem* pointDataA, tile_id t, ResourceHandle& pointBoxDataHandle) const = 0;
	virtual bool IsIntersecting(tile_id t, tile_id u, ResourceHandle& pointBoxDataHandle, ResourceHandle& polyInfoHandle) const = 0;
	virtual void Calculate(AbstrDataObject* res, const AbstrUnit* resVU, const AbstrDataItem* pointDataA, const AbstrDataItem* polyDataA, const AbstrDataItem* rankDataA, tile_id t, tile_id u,
		bool mustInitPointTile, const ResourceHandle& pointBoxDataHandle, const ResourceHandle& spIndexHandle, const ResourceHandle& polyTileHandle) const = 0;
};

template <typename P, typename RankingType>
class PointInRankedPolygonOperator : public AbstrPointInRankedPolygonOperator
{
	typedef P                      PointType;
	typedef std::vector<PointType>  PolygonType;
	typedef Unit<PointType>        PointUnitType;
	typedef Range<PointType>       BoxType;

	using Arg1Type = DataArray<PointType>;
	using Arg2Type = DataArray<PolygonType>;
	using Arg3Type = DataArray<RankingType>;

	using BoxArrayType = std::vector<BoxType>;

	typedef typename scalar_of<PointType>::type  ScalarType;
	typedef SpatialIndex<ScalarType, typename Arg2Type::const_iterator> SpatialIndexType;

	using ResourceType = std::pair<SpatialIndexType, typename Arg2Type::locked_cseq_t>;

	struct DispatcherData
	{
		DispatcherData(
			AbstrDataObject* resObj,
			const Arg1Type* pointData,
			const Arg2Type* polyData,
			tile_id t, tile_id u, bool isFirstPolyTile,
			const SpatialIndexType* spIndexPtr,
			const typename Arg2Type::locked_cseq_t& polyTileData,
			const typename Arg3Type::locked_cseq_t& rankTileData
		) : m_ResObj(resObj)
			, m_PointData(pointData->GetDataRead(t))
			, m_PolyTileData(polyTileData)
			, m_RankTileData(rankTileData)
			, m_PointTileID(t)
			, m_PolyTileID(u)
			, m_SpIndexPtr(spIndexPtr)
			, m_IsFirstPolyTile(isFirstPolyTile)
		{}
		AbstrDataObject* m_ResObj;
		typename Arg1Type::locked_cseq_t m_PointData;
		typename Arg2Type::locked_cseq_t m_PolyTileData;
		typename Arg3Type::locked_cseq_t m_RankTileData;
		tile_id                   m_PointTileID, m_PolyTileID;
		bool                      m_IsFirstPolyTile;
		const SpatialIndexType* m_SpIndexPtr;
	};


	struct DispatcherBase : UnitProcessor
	{
		template <typename E>
		void VisitImpl(const Unit<E>* inviter) const
		{
			dms_assert(m_Data);
			point_in_ranked_polygon(
				composite_cast<DataArray<E>*>(m_Data->m_ResObj)->GetDataWrite(m_Data->m_PointTileID),
				m_Data->m_PointData,
				m_Data->m_PolyTileData,
				(m_Data->m_PolyTileID == no_tile) ? inviter->GetRange() : inviter->GetTileRange(m_Data->m_PolyTileID),
				m_Data->m_SpIndexPtr,
				m_Data->m_RankTileData,
				m_Data->m_IsFirstPolyTile
			);
		}
		DispatcherData* m_Data = nullptr;
	};

	struct Dispatcher : boost::mpl::fold<typelists::partition_elements, DispatcherBase, VisitorImpl<Unit<boost::mpl::_2>, boost::mpl::_1> >::type
	{};

public:
	PointInRankedPolygonOperator()
		: AbstrPointInRankedPolygonOperator(Arg1Type::GetStaticClass(), Arg2Type::GetStaticClass(), Arg3Type::GetStaticClass())
	{}

	// Override Operator
	void CreatePolyHandle(const AbstrDataItem* polyDataA, tile_id u, ResourceHandle& spIndexHandle, ResourceHandle& polyTileHandle) const override
	{
		const Arg2Type* polyData = const_array_cast<PolygonType>(polyDataA);
		dms_assert(polyData);

		auto polyTile = polyData->GetDataRead(u);

		spIndexHandle = makeResource<SpatialIndexType>(polyTile.begin(), polyTile.end(), 0);
		polyTileHandle = makeResource<typename Arg2Type::locked_cseq_t>(std::move(polyTile));
	}

	void CreatePointHandle(const AbstrDataItem* pointDataA, tile_id t, ResourceHandle& pointBoxDataHandle) const override
	{
		if (!pointBoxDataHandle)
			pointBoxDataHandle = makeResource<BoxArrayType>(pointDataA->GetAbstrDomainUnit()->GetNrTiles());
		BoxArrayType& boxArray = GetAs<BoxArrayType>(pointBoxDataHandle);

		const Arg1Type* pointData = const_array_cast<PointType>(pointDataA);
		dms_assert(pointData);

		auto pointArray = pointData->GetDataRead(t);
		dms_assert(t < boxArray.size());
		boxArray[t] = RangeFromSequence(pointArray.begin(), pointArray.end());
	}

	bool IsIntersecting(tile_id t, tile_id u, ResourceHandle& pointBoxDataHandle, ResourceHandle& spIndexHandle) const override
	{
		BoxArrayType& boxArray = GetAs<BoxArrayType>(pointBoxDataHandle);
		SpatialIndexType& spIndex = GetAs<SpatialIndexType>(spIndexHandle);

		return ::IsIntersecting(spIndex.GetBoundingBox(), boxArray[t]);
	}

	void Calculate(AbstrDataObject* res, const AbstrUnit* resVU, const AbstrDataItem* pointDataA, const AbstrDataItem* polyDataA, const AbstrDataItem* rankDataA, tile_id t, tile_id u,
		bool mustInitPointTile, const ResourceHandle& pointBoxDataHandle, const ResourceHandle& spIndexHandle, const ResourceHandle& polyTileHandle) const override
	{
		const Arg1Type* pointData = const_array_cast<PointType>(pointDataA);
		const Arg2Type* polyData  = const_array_cast<PolygonType>(polyDataA);
		const Arg3Type* rankData  = const_array_cast<RankingType>(rankDataA);
		dms_assert(pointData);
		dms_assert(polyData);

		const BoxArrayType& boxArray = GetAs<BoxArrayType>(pointBoxDataHandle);
		const SpatialIndexType* spIndexPtr = GetOptional<SpatialIndexType>(spIndexHandle);

		DispatcherData data(res, pointData, polyData, t, u, mustInitPointTile, spIndexPtr, GetAs<typename Arg2Type::locked_cseq_t>(polyTileHandle), rankData->GetDataRead(u));

		Dispatcher disp; disp.m_Data = &data;
		resVU->InviteUnitProcessor(disp);
	}
};


// *****************************************************************************
//	point_in_all_polygons
// *****************************************************************************


static CommonOperGroup s_grPiaP("point_in_all_polygons", oper_policy::dynamic_result_class | oper_policy::better_not_in_meta_scripting);

static TokenID
	s_tGM = token::geometry,
	s_tFR = token::first_rel,
	s_tSR = token::second_rel;

class AbstrPointInAllPolygonsOperator : public BinaryOperator
{
protected:
	AbstrPointInAllPolygonsOperator(const DataItemClass* pointAttrClass, const DataItemClass* polyAttrClass)
		: BinaryOperator(&s_grPiaP, Unit<UInt32>::GetStaticClass()
			, pointAttrClass
			, polyAttrClass
		)
	{}

	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		dms_assert(args.size() == 2);

		const AbstrDataItem* arg1A = AsDataItem(args[0]);
		const AbstrDataItem* arg2A = AsDataItem(args[1]);
		dms_assert(arg1A);
		dms_assert(arg2A);

		const AbstrUnit* domain1Unit = arg1A->GetAbstrDomainUnit(); bool e1IsVoid = domain1Unit->GetValueType() == ValueWrap<Void>::GetStaticClass();
		const AbstrUnit* values1Unit = arg1A->GetAbstrValuesUnit();

		const AbstrUnit* domain2Unit = arg2A->GetAbstrDomainUnit(); bool e2IsVoid = domain2Unit->GetValueType() == ValueWrap<Void>::GetStaticClass();
		const AbstrUnit* values2Unit = arg2A->GetAbstrValuesUnit();

		values1Unit->UnifyValues(values2Unit, "v1", "v2", UM_Throw);

		AbstrUnit* res = Unit<UInt32>::GetStaticClass()->CreateResultUnit(resultHolder);
		resultHolder = res;

		AbstrDataItem* res1 = e1IsVoid ? nullptr : CreateDataItem(res, s_tFR, res, domain1Unit);
		AbstrDataItem* res2 = e2IsVoid ? nullptr : CreateDataItem(res, s_tSR, res, domain2Unit);
		if (res1) res1->SetTSF(TSF_Categorical);
		if (res2) res2->SetTSF(TSF_Categorical);

		if (mustCalc)
		{
			DataReadLock arg1Lock(arg1A);
			DataReadLock arg2Lock(arg2A);

			ResourceHandle resData;
			ResourceHandle pointBoxDataHandle;

			for (tile_id t = 0, te = domain1Unit->GetNrTiles(); t != te; ++t)
			{
				ReadableTileLock readPointLock(arg1A->GetCurrRefObj(), t);
				CreatePointHandle(arg1A, t, pointBoxDataHandle);
			}

			std::atomic<tile_id> intersectCount = 0;

			for (tile_id u = 0, ue = domain2Unit->GetNrTiles(); u != ue; ++u)
			{
				ReadableTileLock readPolyLock(arg2A->GetCurrRefObj(), u);
				ResourceHandle polyInfoHandle;
				CreatePolyHandle(arg2A, u, polyInfoHandle);

				// 				for (tile_id t=0, te = domainUnit->GetNrTiles(); t != te; ++t)
				leveled_critical_section resInsertSection(item_level_type(0), ord_level_type::SpecificOperatorGroup, "PointInAllPolygons.InsertSection");


				parallel_tileloop(domain1Unit->GetNrTiles(), [this, &resInsertSection, arg1A, arg2A, u, &pointBoxDataHandle, &polyInfoHandle, &resData, &intersectCount](tile_id t)->void
					{
						if (this->IsIntersecting(t, u, pointBoxDataHandle, polyInfoHandle))
						{
							ReadableTileLock readPoly1Lock(arg1A->GetCurrRefObj(), t);

							Calculate(resData, resInsertSection, arg1A, arg2A, t, u, polyInfoHandle);

							++intersectCount;
						}
					}
				);

				reportF(SeverityTypeID::ST_MajorTrace, "point_in_all_polygons with %d point tiles after processing %d/%d polygon tiles resulted in %d matches"
					, domain1Unit->GetNrTiles()
					, u+1, ue
					, intersectCount
				);
			}

			StoreRes(res, res1, res2, resData);
		}
		return true;
	}
	virtual void CreatePolyHandle(const AbstrDataItem* polyDataA, tile_id u, ResourceHandle& polyInfoHandle) const = 0;
	virtual void CreatePointHandle(const AbstrDataItem* pointDataA, tile_id t, ResourceHandle& pointBoxDataHandle) const = 0;
	virtual bool IsIntersecting(tile_id t, tile_id u, ResourceHandle& pointBoxDataHandle, ResourceHandle& polyInfoHandle) const = 0;
	virtual void Calculate(ResourceHandle& resData, leveled_critical_section& resInsertSection, const AbstrDataItem* poly1DataA, const AbstrDataItem* poly2DataA, tile_id t, tile_id u, const ResourceHandle& polyInfoHandle) const = 0;
	virtual void StoreRes(AbstrUnit* res, AbstrDataItem* res1, AbstrDataItem* res2, ResourceHandle& resData) const = 0;
};

template <typename P>
class PointInAllPolygonsOperator : public AbstrPointInAllPolygonsOperator
{
	typedef P                      PointType;
	typedef std::vector<PointType> PolygonType;
	typedef Unit<PointType>        PointUnitType;
	typedef Range<PointType>       BoxType;
	typedef std::vector<BoxType>   BoxArrayType;

	typedef DataArray<PointType>   Arg1Type;
	typedef DataArray<PolygonType> Arg2Type;

	using ResDataElemType = std::pair<tile_offset, tile_offset>;
	using ResTileDataType = std::vector<ResDataElemType>;
	using DualTileKey = std::pair<tile_id, tile_id>;
	using ResDataType     = std::map<DualTileKey, ResTileDataType>;

	typedef typename scalar_of<PointType>::type  ScalarType;
	typedef SpatialIndex<ScalarType, typename Arg2Type::const_iterator> SpatialIndexType;
	typedef std::vector<SpatialIndexType> SpatialIndexArrayType;

public:
	PointInAllPolygonsOperator()
		: AbstrPointInAllPolygonsOperator(Arg1Type::GetStaticClass(), Arg2Type::GetStaticClass())
	{}

	// Override Operator
	void CreatePolyHandle(const AbstrDataItem* polyDataA, tile_id u, ResourceHandle& polyInfoHandle) const override
	{
		const Arg2Type* polyData = const_array_cast<PolygonType>(polyDataA);
		dms_assert(polyData);

		auto polyArray = polyData->GetTile(u);
		polyInfoHandle = makeResource<SpatialIndexType>(polyArray.begin(), polyArray.end(), 0);
	}

	void CreatePointHandle(const AbstrDataItem* pointDataA, tile_id t, ResourceHandle& pointBoxDataHandle) const override
	{
		if (!pointBoxDataHandle)
			pointBoxDataHandle = makeResource<BoxArrayType>(pointDataA->GetAbstrDomainUnit()->GetNrTiles());
		BoxArrayType& boxArray = GetAs<BoxArrayType>(pointBoxDataHandle);

		const Arg1Type* pointData = const_array_cast<PointType>(pointDataA);
		dms_assert(pointData);

		auto pointArray = pointData->GetTile(t);
		dms_assert(t < boxArray.size());
		boxArray[t] = RangeFromSequence(pointArray.begin(), pointArray.end());
	}

	bool IsIntersecting(tile_id t, tile_id u, ResourceHandle& pointBoxDataHandle, ResourceHandle& polyInfoHandle) const override
	{
		BoxArrayType& boxArray = GetAs<BoxArrayType>(pointBoxDataHandle);
		SpatialIndexType* spIndexPtr = GetOptional<SpatialIndexType>(polyInfoHandle);

		return ::IsIntersecting(spIndexPtr->GetBoundingBox(), boxArray[t]);
	}

	void Calculate(ResourceHandle& resDataHandle, leveled_critical_section& resInsertSection, const AbstrDataItem* pointDataA, const AbstrDataItem* polyDataA, tile_id t, tile_id u, const ResourceHandle& polyInfoHandle) const override
	{
		const Arg1Type* pointData = const_array_cast<PointType>(pointDataA);
		const Arg2Type* polyData  = const_array_cast<PolygonType>(polyDataA);

		dms_assert(pointData);
		dms_assert(polyData);

		auto pointArray = pointData->GetTile(t);
		auto polyArray = polyData->GetTile(u);

		const SpatialIndexType* spIndexPtr = GetOptional<SpatialIndexType>(polyInfoHandle);

		using box_iter_type   = typename SpatialIndexType::template iterator<BoxType> ;
		using coordinate_type = typename scalar_of<P>::type ;
		using res_data_elem_type = ResDataElemType;

		ResTileDataType* resTileData = nullptr;
		if (!resTileData)
		{
			leveled_critical_section::scoped_lock resLock(resInsertSection); // free this lock after *resTileData has been created.

			if (!resDataHandle)
				resDataHandle = makeResource<ResDataType>();
			ResDataType& resData = GetAs<ResDataType>(resDataHandle);
			resTileData = &(resData[DualTileKey(t, u)]);
		}
		dms_assert(resTileData);

		leveled_critical_section resLocalAdditionSection(item_level_type(0), ord_level_type::SpecificOperator, "Polygon.LocalAdditionSection");

		parallel_for(tile_offset(0), tile_offset(pointArray.size()),
			[&pointArray, &polyArray, spIndexPtr, resTileData, &resLocalAdditionSection]
			(tile_offset pointTileOffset)->void
			{
				auto pointPtr = pointArray.begin() + pointTileOffset;
				if (!::IsIntersecting(spIndexPtr->GetBoundingBox(), *pointPtr))
					return;

				for (auto iter = spIndexPtr->begin(*pointPtr); iter; ++iter)
				{
					if (IsInside(*((*iter)->get_ptr()), *pointPtr))
					{
						leveled_critical_section::scoped_lock resLock(resLocalAdditionSection);
						resTileData->emplace_back(pointTileOffset, (*iter)->get_ptr() - polyArray.begin());
					}
				}
			}
		);
	}

	void StoreRes(AbstrUnit* res, AbstrDataItem* res1, AbstrDataItem* res2, ResourceHandle& resDataHandle) const override
	{
		ResDataType* resData = GetOptional<ResDataType>(resDataHandle);

		SizeT count = 0;
		if (resData)
			for (auto& resTiles : *resData) // ordered by (t, u)
				count += resTiles.second.size();
		res->SetCount(count);

		if (resData)
		{
			std::vector<ResDataType::value_type> resTileDataArray; resTileDataArray.reserve(resData->size());
			for (auto& resTileData : *resData)
				resTileDataArray.emplace_back(std::move(resTileData));

			parallel_for<SizeT>(0, resTileDataArray.size()
			,	[&resTileDataArray](SizeT t)
				{
					auto& resTileData = resTileDataArray[t];
					std::sort(resTileData.second.begin(), resTileData.second.end()); // sort on (m_OrgRel)
				});
			if (res1)
				visit<typelists::domain_elements>(res1->GetAbstrValuesUnit()
				,	[res1, &resTileDataArray]<typename P>(const Unit<P>* resValuesUnit)
					{
						locked_tile_write_channel<P> resWriter(res1);
						auto tileRangeDataPtr = resValuesUnit->GetCurrSegmInfo();
						MG_CHECK(tileRangeDataPtr);

						for (auto& resTiles : resTileDataArray) // ordered by (t, u)
						{
							ResTileDataType& resTileData = resTiles.second;

							for (auto& resElemData : resTileData)
								resWriter.Write(tileRangeDataPtr->GetTileValue(resTiles.first.first, resElemData.first));
						}
						MG_CHECK(resWriter.IsEndOfChannel()); 
						resWriter.Commit();
					}
				);
			if (res2)
				visit<typelists::domain_elements>(res2->GetAbstrValuesUnit()
				,	[res2, &resTileDataArray]<typename P>(const Unit<P>*resValuesUnit)
					{
						locked_tile_write_channel<P> resWriter(res2);
						auto tileRangeDataPtr = resValuesUnit->GetCurrSegmInfo();
						MG_CHECK(tileRangeDataPtr);

						for (auto& resTiles : resTileDataArray) // ordered by (t, u)
						{
							ResTileDataType& resTileData = resTiles.second;

							for (auto& resElemData : resTileData)
								resWriter.Write(tileRangeDataPtr->GetTileValue(resTiles.first.second, resElemData.second));
						}
						MG_CHECK(resWriter.IsEndOfChannel());
						resWriter.Commit();
					}
				);
		}
		else
		{
			if (res1) { DataWriteLock wr1(res1); wr1.Commit(); }
			if (res2) { DataWriteLock wr2(res2); wr2.Commit(); }
		}
	}
};

// *****************************************************************************
//											INSTANTIATION
// *****************************************************************************


namespace 
{
	CommonOperGroup cogLB("lower_bound", oper_policy::better_not_in_meta_scripting);
	CommonOperGroup cogUB("upper_bound", oper_policy::better_not_in_meta_scripting);
	CommonOperGroup cogCB("center_bound");

	Obsolete<CommonOperGroup> cogFN("use first_point instead", "first_node", oper_policy::depreciated);
	Obsolete<CommonOperGroup> cogLN("use last_point instead",  "last_node", oper_policy::depreciated);

	CommonOperGroup cogFP("first_point", oper_policy::better_not_in_meta_scripting);
	CommonOperGroup cogLP("last_point", oper_policy::better_not_in_meta_scripting);

	CommonOperGroup cogCentroid     ("centroid", oper_policy::better_not_in_meta_scripting);
	CommonOperGroup cogMid          ("mid", oper_policy::better_not_in_meta_scripting);
	CommonOperGroup cogCentroidOrMid("centroid_or_mid", oper_policy::better_not_in_meta_scripting);

	CommonOperGroup cogAL("arc_length", oper_policy::better_not_in_meta_scripting);
	CommonOperGroup cogAREA("area", oper_policy::better_not_in_meta_scripting);
	CommonOperGroup cogMLSL("mls_length", oper_policy::better_not_in_meta_scripting);

	CommonOperGroup cogP2S    ("points2sequence", oper_policy::better_not_in_meta_scripting);
	CommonOperGroup cogP2S_p  ("points2sequence_p", oper_policy::better_not_in_meta_scripting);
	CommonOperGroup cogP2S_ps ("points2sequence_ps", oper_policy::better_not_in_meta_scripting);
	CommonOperGroup cogP2S_po ("points2sequence_po", oper_policy::better_not_in_meta_scripting);
	CommonOperGroup cogP2S_pso("points2sequence_pso", oper_policy::better_not_in_meta_scripting);

	CommonOperGroup cogP2P    ("points2polygon", oper_policy::better_not_in_meta_scripting);
	CommonOperGroup cogP2P_p  ("points2polygon_p", oper_policy::better_not_in_meta_scripting);
	CommonOperGroup cogP2P_ps ("points2polygon_ps", oper_policy::better_not_in_meta_scripting);
	CommonOperGroup cogP2P_po ("points2polygon_po", oper_policy::better_not_in_meta_scripting);
	CommonOperGroup cogP2P_pso("points2polygon_pso", oper_policy::better_not_in_meta_scripting);

	CommonOperGroup cogS2P("sequence2points", oper_policy::better_not_in_meta_scripting);
	CommonOperGroup cogS2P_uint64("sequence2points_uint64", oper_policy::better_not_in_meta_scripting);

	CommonOperGroup cogArc2segm("arc2segm", oper_policy::better_not_in_meta_scripting);
	CommonOperGroup cogArc2segm_uint64("arc2segm_uint64", oper_policy::better_not_in_meta_scripting);

	CommonOperGroup cogDynaPoint("dyna_point", oper_policy::better_not_in_meta_scripting);
	CommonOperGroup cogDynaPointWithEnds("dyna_point_with_ends", oper_policy::better_not_in_meta_scripting);
	CommonOperGroup cogDynaSegment("dyna_segment", oper_policy::better_not_in_meta_scripting);
	CommonOperGroup cogDynaSegmentWithEnds("dyna_segment_with_ends", oper_policy::better_not_in_meta_scripting);

	CommonOperGroup cogDynaPoint_uint64("dyna_point_uint64", oper_policy::better_not_in_meta_scripting);
	CommonOperGroup cogDynaPointWithEnds_uint64("dyna_point_with_ends_uint64", oper_policy::better_not_in_meta_scripting);
	CommonOperGroup cogDynaSegment_uint64("dyna_segment_uint64", oper_policy::better_not_in_meta_scripting);
	CommonOperGroup cogDynaSegmentWithEnds_uint64("dyna_segment_with_ends_uint64", oper_policy::better_not_in_meta_scripting);

	template <typename T>
	struct SequenceOperators
	{
		//	Unary functions that have point results
		// Functors that return extreme value for empty and null sequences; checked version returns undefined for null sequecnes
		UnaryAttrFuncOperator<LowerBoundFunc <T> > lb;
		UnaryAttrFuncOperator<UpperBoundFunc <T> > ub;
		UnaryAttrFuncOperator<CenterBoundFunc<T> > cb;

		// Functors that return Undefined for empty and null sequences
		UnaryAttrSpecialFuncOperator<FirstFunc<T> > firstN, firstP;
		UnaryAttrSpecialFuncOperator<LastFunc <T> > lastN, lastP;

		// points2sequence
		Points2SequenceOperator<T, Void> p2s1, p2s_p, p2s_po;
		Points2SequenceOperator<T, UInt32> p2s2_ui32, p2s3_ui32, p2s_ps_ui32, p2s_pso_ui32;
		Points2SequenceOperator<T, UInt64> p2p2_ui64, p2p3_ui64, p2p_ps_ui64, p2p_pso_ui64;

		// points2polygon
		Points2SequenceOperator<T, Void> p2p1, p2p_p, p2p_po;
		Points2SequenceOperator<T, UInt32> p2p2_ui32, p2p3_ui32, p2p_ps_ui32, p2p_pso_ui32;
		Points2SequenceOperator<T, UInt64> p2s2_ui64, p2s3_ui64, p2s_ps_ui64, p2s_pso_ui64;

		Arcs2SegmentsOperator <T> arc2segm, arc2segm_uint64, seq2point, seq2point_uint64;

		SequenceOperators()
			: lb(&cogLB)
			, ub(&cogUB)
			, cb(&cogCB)

			, firstN(&cogFN)
			, lastN(&cogLN)
			, firstP(&cogFP)
			, lastP(&cogLP)

			// points2sequence
			, p2s1(&cogP2S, false, ValueComposition::Sequence)
			, p2s2_ui32(&cogP2S, false, ValueComposition::Sequence)
			, p2s3_ui32(&cogP2S, true, ValueComposition::Sequence)
			, p2s2_ui64(&cogP2S, false, ValueComposition::Sequence)
			, p2s3_ui64(&cogP2S, true, ValueComposition::Sequence)

			, p2s_p(&cogP2S_p, false, ValueComposition::Sequence)
			, p2s_ps_ui32(&cogP2S_ps, false, ValueComposition::Sequence)
			, p2s_ps_ui64(&cogP2S_ps, false, ValueComposition::Sequence)

			, p2s_po(&cogP2S_po, true, ValueComposition::Sequence)
			, p2s_pso_ui32(&cogP2S_pso, true, ValueComposition::Sequence)
			, p2s_pso_ui64(&cogP2S_pso, true, ValueComposition::Sequence)

			// points2polygon
			, p2p1(&cogP2P, false, ValueComposition::Polygon)
			, p2p2_ui32(&cogP2P, false, ValueComposition::Polygon)
			, p2p3_ui32(&cogP2P, true, ValueComposition::Polygon)
			, p2p2_ui64(&cogP2P, false, ValueComposition::Polygon)
			, p2p3_ui64(&cogP2P, true, ValueComposition::Polygon)

			, p2p_p(&cogP2P_p, false, ValueComposition::Polygon)
			, p2p_ps_ui32(&cogP2P_ps, false, ValueComposition::Polygon)
			, p2p_pso_ui32(&cogP2P_pso, true, ValueComposition::Polygon)

			, p2p_po(&cogP2P_po, true, ValueComposition::Polygon)
			, p2p_ps_ui64(&cogP2P_ps, false, ValueComposition::Polygon)
			, p2p_pso_ui64(&cogP2P_pso, true, ValueComposition::Polygon)

			, arc2segm(&cogArc2segm, DoCreateNextPoint | DoCreateNrOrgEntity)
			, arc2segm_uint64(&cogArc2segm_uint64, DoCreateNextPoint | DoCreateNrOrgEntity| CreateUInt64Domain)
			, seq2point(&cogS2P, DoCloseLast | DoCreateNrOrgEntity| DoCreateOrdinal)
			, seq2point_uint64(&cogS2P_uint64, DoCloseLast | DoCreateNrOrgEntity | DoCreateOrdinal | CreateUInt64Domain)
		{}

	};

	template <typename P>
	struct GeometricOperators
	{

		// Functors that return Undefined for empty and null sequences
		UnaryAttrSpecialFuncOperator<CentroidFunc     <P> > centroid;
		UnaryAttrSpecialFuncOperator<MidFunc          <P> > mid;
		UnaryAttrSpecialFuncOperator<CentroidOrMidFunc<P> > centroidOrMid;

	//Casted functions that result in scalars
		// Functors that return 0 for both empty and null sequences
		CastedUnaryAttrSpecialFuncOperator<ArcLengthFunc<P> > arcLength;
		CastedUnaryAttrSpecialFuncOperator<MlsLengthFunc<P> > mlsLength;
		CastedUnaryAttrSpecialFuncOperator<AreaFunc     <P> > area;

		PointInPolygonOperator<P> pip;
		PointInRankedPolygonOperator<P, UInt8> pirpu8;
		PointInRankedPolygonOperator<P, UInt32> pirpu32;
		PointInRankedPolygonOperator<P, Int32>  pirpi32;
		PointInRankedPolygonOperator<P, Float32> pirpf32;
		PointInRankedPolygonOperator<P, Float64>  pirpf64;

		PointInAllPolygonsOperator<P> piap;
		DynaPointOperator<P>      dynaPoint1, dynaPoint2, dynaPoint3, dynaPoint4;
		DynaPointOperator<P>      dynaPoint1_64, dynaPoint2_64, dynaPoint3_64, dynaPoint4_64;

		GeometricOperators()
			:	centroid     (&cogCentroid)
			,	mid          (&cogMid)
			,	centroidOrMid(&cogCentroidOrMid)

			,	arcLength(&cogAL)
			,	mlsLength(&cogMLSL)
			,	area(&cogAREA)
			,	dynaPoint1(&cogDynaPoint,           DoCreateNrOrgEntity | DoCreateOrdinal)
			,	dynaPoint2(&cogDynaPointWithEnds,   DoCreateNrOrgEntity | DoCreateOrdinal | DoIncludeEndPoints)
			,	dynaPoint3(&cogDynaSegment,         DoCreateNextPoint | DoCreateNrOrgEntity | DoCreateOrdinal)
			,	dynaPoint4(&cogDynaSegmentWithEnds, DoCreateNextPoint | DoCreateNrOrgEntity | DoCreateOrdinal | DoIncludeEndPoints)
			,	dynaPoint1_64(&cogDynaPoint_uint64, DoCreateNrOrgEntity | DoCreateOrdinal | CreateUInt64Domain)
			,	dynaPoint2_64(&cogDynaPointWithEnds_uint64, DoCreateNrOrgEntity | DoCreateOrdinal | DoIncludeEndPoints | CreateUInt64Domain)
			,	dynaPoint3_64(&cogDynaSegment_uint64, DoCreateNextPoint | DoCreateNrOrgEntity | DoCreateOrdinal | CreateUInt64Domain)
			,	dynaPoint4_64(&cogDynaSegmentWithEnds_uint64, DoCreateNextPoint | DoCreateNrOrgEntity | DoCreateOrdinal | DoIncludeEndPoints | CreateUInt64Domain)
		{}

	};

	tl_oper::inst_tuple_templ<typelists::seq_points , SequenceOperators > seqOperPointInstances;
	tl_oper::inst_tuple_templ<typelists::num_objects, SequenceOperators > seqOperNumericInstances;
	tl_oper::inst_tuple_templ<typelists::seq_points , GeometricOperators> pointOperInstances;
}

/******************************************************************************/

