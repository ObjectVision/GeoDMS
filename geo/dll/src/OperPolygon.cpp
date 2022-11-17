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

#include "GeoPCH.h"
#pragma hdrstop

#include "geo/Conversions.h"
#include "mci/CompositeCast.h"
#include "ptr/OwningPtrSizedArray.h"
#include "ptr/Resource.h"
#include "set/RangeFuncs.h"
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

#include "OperAttrUni.h"
#include "CastedUnaryAttrOper.h"
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

#include "geo/DynamicPoint.h"

template <class P>
struct ArcLengthFunc : Sequence2ScalarFunc<P>
{
	typename ArcLengthFunc::res_type operator() (typename ArcLengthFunc::arg1_cref arg1) const
	{
		return Convert<typename ArcLengthFunc::res_type>( ArcLength<Float64>(arg1) );
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
//									Area Operator
// *****************************************************************************


// *****************************************************************************
//									Points2SequenceOperator
// *****************************************************************************

class AbstrPoints2SequenceOperator : public VariadicOperator
{
public:
	typedef UInt32 PolygonIndex;
	typedef UInt32 OrdinalType;

	typedef DataArray<PolygonIndex> Arg2Type;
	typedef DataArray<OrdinalType>  Arg3Type;

	bool             m_HasSeqDomainArg : 1; 
	bool             m_HasSeqOrd       : 1;
	ValueComposition m_VC              : ValueComposition_BitCount;

	AbstrPoints2SequenceOperator(AbstrOperGroup* gr, const Class* resultType, const Class* arg1Type, bool hasSeqDomainArg, bool hasSeqOrd, ValueComposition vc)
		:	VariadicOperator(gr, resultType, 1 + (hasSeqDomainArg ? 1 : 0) + (hasSeqOrd ? 1 : 0) )
		,	m_HasSeqDomainArg(hasSeqDomainArg)
		,	m_HasSeqOrd(hasSeqOrd)
		,	m_VC(vc)
	{
		ClassCPtr* argClasses = m_ArgClasses.get();
		*argClasses++ = arg1Type;
		if (hasSeqDomainArg)
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

		const AbstrDataItem* arg2A = nullptr; if (m_HasSeqDomainArg) arg2A = AsDataItem(args[++argCount]);
		const AbstrDataItem* arg3A = nullptr; if (m_HasSeqOrd      ) arg3A = AsDataItem(args[++argCount]);

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

			parallel_tileloop(polyEntity->GetNrTiles(), [this, arg1A, arg2A, arg3A, resObj = resLock.get()](tile_id tr)->void
				{
					this->Calculate(resObj, tr, arg1A, arg2A, arg3A);
				}
			);
			resLock.Commit();
		}
		return true;
	}

	virtual void Calculate(AbstrDataObject* res, tile_id tr
	,	const AbstrDataItem* arg1A
	,	const AbstrDataItem* arg2A
	,	const AbstrDataItem* arg3A
	)	const =0;
};

template <class T>
class Points2SequenceOperator : public AbstrPoints2SequenceOperator
{
	typedef T                          PointType;
	typedef std::vector<PointType>     PolygonType;
	typedef Unit<PointType>            PointUnitType;
	typedef DataArray<PolygonType>     ResultType;	
	typedef DataArray<PointType>       Arg1Type;			
			
public:
	Points2SequenceOperator(AbstrOperGroup* gr, bool hasSeqDomainArg, bool hasSeqOrd, ValueComposition vc)
		:	AbstrPoints2SequenceOperator(gr 
			,	ResultType::GetStaticClass() 
			,	Arg1Type::GetStaticClass()
			,	hasSeqDomainArg, hasSeqOrd
			,	vc
			)
	{}

	void Calculate(AbstrDataObject* res, tile_id tr
	,	const AbstrDataItem* arg1A
	,	const AbstrDataItem* arg2A
	,	const AbstrDataItem* arg3A
	)	const override
	{
		const Arg1Type* arg1 = const_array_cast<PointType>(arg1A); // point array
		const Arg2Type* arg2 = nullptr; if (arg2A) arg2 = const_array_cast<PolygonIndex>(arg2A); // polygon ordinal; nullptr indicates sequence set has void domain (thus, one sequence)
		const Arg3Type* arg3 = nullptr; if (arg3A) arg3 = const_array_cast<OrdinalType> (arg3A); // ordinal within polygon; nullptr indicates ascending order
		dms_assert(arg1); 

		ResultType *result = mutable_array_cast<PolygonType>(res);
		dms_assert(result);

		auto resData = result->GetWritableTile(tr);

		Range<PolygonIndex> polyIndexRange = arg2 ? arg2->GetValueRangeData()->GetTileRange(tr) : Range<PolygonIndex>(0, 1);

		PolygonIndex nrPolys = Cardinality(polyIndexRange);
		tile_id tn = arg1A->GetAbstrDomainUnit()->GetNrTiles();
		std::vector<bool> hasPoly(tn);

		Arg2Type::locked_cseq_t arg2Data;
		std::vector<OrdinalType> nrPointsPerSeq (nrPolys, 0);

		// === first count the nrPointsPerSeq
		for (tile_id ta=0; ta!=tn; ++ta)
		{
			bool thisTileHasPoly = false;
			if (arg2)
			{
				arg2Data = arg2->GetLockedDataRead(ta);

				Arg2Type::const_iterator i2 = arg2Data.begin(), e2 = arg2Data.end();
				for(; i2 != e2; ++i2)
				{
	//				SizeT polyNr = Range_GetIndex_naked(polyIndexRange, *i2); 
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

		SizeT nrPoints = std::accumulate(nrPointsPerSeq.begin(), nrPointsPerSeq.end(), 0);
		resData.get_sa().data_reserve(nrPoints MG_DEBUG_ALLOCATOR_SRC(res->md_SrcStr));

		// ==== then resize each resulting polygon according to the nr of seen points.
		for (PolygonIndex i=0; i!=nrPolys; ++i)
		{
			SizeT polySize = nrPointsPerSeq[i];
			resData[i].resize_uninitialized(polySize);
			dbg_assert((cumulSize += polySize) == resData.get_sa().actual_data_size());
		}
		dbg_assert(cumulSize == nrPoints);


		dms_assert(resData.get_sa().actual_data_size() == nrPoints); // follows from previous asserts
		auto br = resData.begin();

		// ==== then proces all input-tiles again and collect to the resulting polygon tile
		OwningPtrSizedArray<SizeT> currOrdinals;
		if (!arg3) 
		{
			currOrdinals = OwningPtrSizedArray<SizeT>(nrPolys MG_DEBUG_ALLOCATOR_SRC_STR("OperPolygon: currOrdinals"));
			fast_zero(currOrdinals.begin(), currOrdinals.begin() + nrPolys);
		}
		for (tile_id ta=0; ta!=tn; ++ta) if (hasPoly[ta])
		{
			auto arg1Data = arg1->GetLockedDataRead(ta);
			Arg3Type::locked_cseq_t arg3Data;
			if (arg2) arg2Data = arg2->GetLockedDataRead(ta); // continuity of tile lock if tn = 1
			if (arg3) arg3Data = arg3->GetLockedDataRead(ta);

			auto b1 = arg1Data.begin();
			auto b2 = arg2Data.begin();
			auto b3 = arg3Data.begin();
			for (SizeT i=0, n = arg1Data.size(); i!=n; ++i)
			{
//				SizeT polyNr = Range_GetIndex_naked(polyIndexRange, b2[i]);
				SizeT polyNr = (arg2) ? b2[i] - polyIndexRange.first : 0;
				if (polyNr < nrPolys)
				{
					UInt32 orderNr = (arg3) ? b3[i] : currOrdinals[polyNr]++;

					if (orderNr >= nrPointsPerSeq[polyNr])
						GetGroup()->throwOperErrorF("unexpected orderNr %d for sequence %d which has %d elements", orderNr, polyNr, nrPointsPerSeq[polyNr]);
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
static TokenID s_nrOrgEntity = GetTokenID_st("SequenceNr");
static TokenID s_SequenceNr = GetTokenID_st("Ordinal");

namespace {
	enum TableCreateFlags
	{
		DoCloseLast           = 0x0001
	,	DoCreateNextPoint     = 0x0002
	,	DoCreateNrOrgEntity   = 0x0004
	,	DoCreateSequenceNr    = 0x0008
	,	DoCreateDistFromStart = 0x0010
	,	DoIncludeEndPoints    = 0x0020
	};
}	//	end of anonimous namespace

struct AbstrArcs2SegmentsOperator : public UnaryOperator
{
	typedef Unit<UInt32> ResultUnitType;
	AbstrArcs2SegmentsOperator(AbstrOperGroup* gr, TableCreateFlags createFlags, const Class* arg1Class)
		:	UnaryOperator(
				gr
			,	ResultUnitType::GetStaticClass()
			,	arg1Class
			)
		,	m_CreateFlags(createFlags)
	{
		dms_assert(createFlags & (DoCloseLast | DoCreateNextPoint));
		dms_assert(!(createFlags & DoCreateDistFromStart)); // NYI
	}

	// Override Operator
	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		dms_assert(args.size() == 1);

		const AbstrDataItem* arg1A = debug_cast<const AbstrDataItem*>(args[0]);
		dms_assert(arg1A);

		const AbstrUnit* polyEntity      = arg1A->GetAbstrDomainUnit();
		const AbstrUnit* pointValuesUnit = arg1A->GetAbstrValuesUnit();

		ResultUnitType* resDomain
			=	debug_cast<ResultUnitType*>(
					ResultUnitType::GetStaticClass()->CreateResultUnit(resultHolder)
				);
		dms_assert(resDomain);
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
			resSub3 = CreateDataItem(resDomain, s_nrOrgEntity, resDomain, polyEntity);
			MG_PRECONDITION(resSub3);
			resSub3->SetTSF(DSF_Categorical);
		}

		if (m_CreateFlags & DoCreateSequenceNr)
		{
			resSub4 = CreateDataItem(resDomain, s_SequenceNr, resDomain, Unit<UInt32>::GetStaticClass()->CreateDefault() );
			MG_PRECONDITION(resSub4);
		}
		
		if (mustCalc)
		{
			dms_assert(resultHolder->GetInterestCount()); // DEBUG

			DataReadLock arg1Lock(arg1A);

			Calculate(
				resDomain
			,	arg1A
			,	resSub1, resSub2, resSub3, resSub4
			);
		}
		return true;
	}

	virtual void Calculate(
		ResultUnitType* resDomain
	,	const AbstrDataItem* arg1A
	,	AbstrDataItem* resSub1
	,	AbstrDataItem* resSub2 
	,	AbstrDataItem* resSub3
	,	AbstrDataItem* resSub4
	)	const = 0;

	TableCreateFlags m_CreateFlags;
};

template <class T>
class Arcs2SegmentsOperator : public AbstrArcs2SegmentsOperator
{
	typedef T                         PointType;
	typedef std::vector<PointType>    PolygonType;
	typedef Unit<PointType>           PointUnitType;

	typedef DataArray<PolygonType>    Arg1Type;
	typedef DataArray<PointType>      ResultSub1Type; // Point
	typedef DataArray<PointType>      ResultSub2Type; // NextPoint
	typedef AbstrDataItem             ResultSub3Type; // nrOrgEntity
	typedef DataArray<UInt32>         ResultSub4Type; // SequenceId

public:
	Arcs2SegmentsOperator(AbstrOperGroup* gr, int createFlags)
		:	AbstrArcs2SegmentsOperator(gr, TableCreateFlags(createFlags), Arg1Type::GetStaticClass() )
	{}

	void Calculate(
		ResultUnitType* resDomain
	,	const AbstrDataItem* arg1A
	,	AbstrDataItem* resSub1
	,	AbstrDataItem* resSub2 
	,	AbstrDataItem* resSub3
	,	AbstrDataItem* resSub4
	)	const override
	{
		const Arg1Type* arg1 = const_array_cast<PolygonType>(arg1A);
		dms_assert(arg1);

		UInt32 nrPoints = 0;
		bool   bDoCloseLast = (m_CreateFlags & DoCloseLast);
		tile_id tn = arg1A->GetAbstrDomainUnit()->GetNrTiles();
		for (tile_id t=0; t!=tn; ++t)
		{
			auto arg1Data = arg1->GetLockedDataRead(t);

			// first, calc cardinality for resDomain
			for (auto i=arg1Data.begin(), e=arg1Data.end(); i !=e; ++i)
			{
				UInt32 sz = i->size();
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
	typedef Unit<UInt32> ResultUnitType;

	AbstrDynaPointOperator(AbstrOperGroup* gr, TableCreateFlags createFlags, const Class* pointDataClass, const Class* distDataClass)
		:	TernaryOperator(
				gr
			,	ResultUnitType::GetStaticClass()
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

		ResultUnitType* resDomain
			=	debug_cast<ResultUnitType*>(
					ResultUnitType::GetStaticClass()->CreateResultUnit(resultHolder)
				);
		dms_assert(resDomain);
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
			resSub3 = CreateDataItem(resDomain, s_nrOrgEntity, resDomain, pointEntity);
			resSub3->SetTSF(DSF_Categorical);
			MG_PRECONDITION(resSub3);
		}

		if (m_CreateFlags & DoCreateSequenceNr)
		{
			resSub4 = CreateDataItem(resDomain, s_SequenceNr, resDomain, Unit<UInt32>::GetStaticClass()->CreateDefault() );
			MG_PRECONDITION(resSub4);
		}
		
		if (mustCalc)
		{
			DataReadLock arg1Lock(arg1A);
			DataReadLock arg2Lock(arg2A);
			DataReadLock arg3Lock(arg3A);

			Calculate(
				resDomain
			,	arg1A, arg2A, arg3A
			,	resSub1, resSub2, resSub3, resSub4
			);
		}
		return true;
	}

	virtual void Calculate(
		ResultUnitType* resDomain
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
	typedef AbstrDataItem              ResultSub3Type; // SequenceNr (nr_OrgEntity)
	typedef DataArray<UInt32>          ResultSub4Type; // Ordinal

public:
	DynaPointOperator(AbstrOperGroup* gr, int createFlags)
		:	AbstrDynaPointOperator(gr, TableCreateFlags(createFlags), Arg1Type::GetStaticClass(), Arg3Type::GetStaticClass() )
	{}

	void Calculate(
		ResultUnitType* resDomain
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
			, i2 = b2;

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
			if (!isFirstPoint)
				if (i2[-1] != *i1)
					isFirstPoint = true;
			PointType segment = *i2 - *i1;
			Float64 segmLength = sqrt(Norm<Float64>(segment));
			if (isFirstPoint)
			{
				if (!resSub2)
					nrPoints++;
			}
			SizeT nrPointsHere = (segmLength+carry) / dist;
			dms_assert((segmLength+carry) >= dist * nrPointsHere); // assume division and float->int conversion round off towards zero.
			carry += (segmLength - dist * nrPointsHere);
			dms_assert(carry >= 0);

			if (withEnds && carry)
			{
				++nrPointsHere;
				carry = 0;
			}
			nrPoints += nrPointsHere;
			isFirstPoint = false;
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
			UInt32 ordinalID = 0;
			if (!isFirstPoint)
				if (i2[-1] != *i1)
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

CommonOperGroup cogPP("point_in_polygon", oper_policy::dynamic_result_class);

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
			resultHolder->SetTSF(DSF_Categorical);
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

			OwningPtrSizedArray<UInt32> polyTileCounters( te MG_DEBUG_ALLOCATOR_SRC_STR("OperPolygon: polyTileCounters"));

			fast_zero(polyTileCounters.begin(), polyTileCounters.begin()+te);

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
	dms_assert(count == pointData.size());
	dms_assert(polyData.size() == Cardinality(polyDomain));

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
					dms_assert(c < Cardinality(polyDomain));
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

CommonOperGroup cogPRP("point_in_ranked_polygon", oper_policy::dynamic_result_class);

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
			resultHolder->SetTSF(DSF_Categorical);
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

			OwningPtrSizedArray<UInt32> polyTileCounters(te MG_DEBUG_ALLOCATOR_SRC_STR("OperPolygon: polyTileCounters"));

			fast_zero(polyTileCounters.begin(), polyTileCounters.begin() + te);

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


static CommonOperGroup s_grPiaP("point_in_all_polygons", oper_policy::dynamic_result_class);

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
		if (res1) res1->SetTSF(DSF_Categorical);
		if (res2) res2->SetTSF(DSF_Categorical);

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

			for (tile_id u = 0, ue = domain2Unit->GetNrTiles(); u != ue; ++u)
			{
				ReadableTileLock readPolyLock(arg2A->GetCurrRefObj(), u);
				ResourceHandle polyInfoHandle;
				CreatePolyHandle(arg2A, u, polyInfoHandle);

				// 				for (tile_id t=0, te = domainUnit->GetNrTiles(); t != te; ++t)
				leveled_critical_section resInsertSection(item_level_type(0), ord_level_type::SpecificOperatorGroup, "PointInAllPolygons.InsertSection");

				std::atomic<tile_id> intersectCount = 0; // DEBUG

				parallel_tileloop(domain1Unit->GetNrTiles(), [this, &resInsertSection, arg1A, arg2A, u, &pointBoxDataHandle, &polyInfoHandle, &resData, &intersectCount](tile_id t)->void
					{
						if (this->IsIntersecting(t, u, pointBoxDataHandle, polyInfoHandle))
						{
							ReadableTileLock readPoly1Lock(arg1A->GetCurrRefObj(), t);

							Calculate(resData, resInsertSection, arg1A, arg2A, t, u, polyInfoHandle);

							++intersectCount; // DEBUG
						}
					}
				);
				reportF(SeverityTypeID::ST_MajorTrace, "Intersect count %d of %dx%d", intersectCount, domain1Unit->GetNrTiles(), ue); // DEBUG
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

	using ResDataElemType = Point<SizeT>;

	typedef std::vector<ResDataElemType> ResTileDataType;
	typedef std::map<Point<tile_id>, ResTileDataType> ResDataType;

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

		SizeT p1Offset = pointDataA->GetAbstrDomainUnit()->GetTileFirstIndex(t);
		SizeT p2Offset = polyDataA->GetAbstrDomainUnit()->GetTileFirstIndex(u);

		const SpatialIndexType* spIndexPtr = GetOptional<SpatialIndexType>(polyInfoHandle);

		typedef typename SpatialIndexType::template iterator<BoxType> box_iter_type;
		typedef typename scalar_of<P>::type coordinate_type;

		typedef ResDataElemType res_data_elem_type;

		ResTileDataType* resTileData = nullptr;
		if (!resTileData)
		{
			leveled_critical_section::scoped_lock resLock(resInsertSection); // free this lock after *resTileData has been created.

			if (!resDataHandle)
				resDataHandle = makeResource<ResDataType>();
			ResDataType& resData = GetAs<ResDataType>(resDataHandle);
			resTileData = &(resData[Point<tile_id>(t, u)]);
		}
		dms_assert(resTileData);

		leveled_critical_section resLocalAdditionSection(item_level_type(0), ord_level_type::SpecificOperator, "Polygon.LocalAdditionSection");

		parallel_for(SizeT(0), pointArray.size(), [p1Offset, p2Offset, &pointArray, &polyArray, spIndexPtr, resTileData, &resLocalAdditionSection](SizeT i)->void
			{
				auto pointPtr = pointArray.begin() + i;
				SizeT p1_rel = p1Offset + i;
//				BoxType bbox = RangeFromSequence(polyPtr->begin(), polyPtr->end());
				if (!::IsIntersecting(spIndexPtr->GetBoundingBox(), *pointPtr))
					return;

//				box_iter_type iter;
				for (auto iter = spIndexPtr->begin(*pointPtr); iter; ++iter)
				{
					if (IsInside(*((*iter)->get_ptr()), *pointPtr))
					{
						res_data_elem_type back;
						back.first = p1_rel;
						back.second = p2Offset + (((*iter)->get_ptr()) - polyArray.begin());

						leveled_critical_section::scoped_lock resLock(resLocalAdditionSection);
						resTileData->push_back(std::move(back));
					}
				}
			});
	}

	void StoreRes(AbstrUnit* res, AbstrDataItem* res1, AbstrDataItem* res2, ResourceHandle& resDataHandle) const override
	{
		ResDataType* resData = GetOptional<ResDataType>(resDataHandle);

		SizeT count = 0;
		if (resData)
			for (auto& resTiles : *resData) // ordered by (t, u)
				count += resTiles.second.size();
		res->SetCount(count);

		locked_tile_write_channel<UInt32> res1Writer(res1);
		locked_tile_write_channel<UInt32> res2Writer(res2);

		if (resData)
		{
			for (auto& resTiles : *resData) // ordered by (t, u)
			{
				ResTileDataType& resTileData = resTiles.second;
				std::sort(resTileData.begin(), resTileData.end()); // sort on (m_OrgRel)

				for (auto& resElemData : resTileData)
				{
					if (res1) res1Writer.Write(resElemData.first);
					if (res2) res2Writer.Write(resElemData.second);
				}
			}
		}
		auto tn = res->GetNrTiles();
		if (res1) { MG_CHECK(res1Writer.IsEndOfChannel()); res1Writer.Commit(); }
		if (res2) { MG_CHECK(res2Writer.IsEndOfChannel()); res2Writer.Commit(); }
	}
};

// *****************************************************************************
//											INSTANTIATION
// *****************************************************************************


namespace 
{
	CommonOperGroup cogLB("lower_bound");
	CommonOperGroup cogUB("upper_bound");
	CommonOperGroup cogCB("center_bound");

	Obsolete<CommonOperGroup> cogFN("use first_point instead", "first_node", oper_policy::obsolete);
	Obsolete<CommonOperGroup> cogLN("use last_point instead",  "last_node", oper_policy::obsolete);

	CommonOperGroup cogFP("first_point");
	CommonOperGroup cogLP("last_point");

	CommonOperGroup cogCentroid     ("centroid");
	CommonOperGroup cogMid          ("mid");
	CommonOperGroup cogCentroidOrMid("centroid_or_mid");

	CommonOperGroup cogAL("arc_length");
	CommonOperGroup cogAREA("area");

	CommonOperGroup cogP2S    ("points2sequence");
	CommonOperGroup cogP2S_p  ("points2sequence_p");
	CommonOperGroup cogP2S_ps ("points2sequence_ps");
	CommonOperGroup cogP2S_po ("points2sequence_po");
	CommonOperGroup cogP2S_pso("points2sequence_pso");

	CommonOperGroup cogP2P    ("points2polygon");
	CommonOperGroup cogP2P_p  ("points2polygon_p");
	CommonOperGroup cogP2P_ps ("points2polygon_ps");
	CommonOperGroup cogP2P_po ("points2polygon_po");
	CommonOperGroup cogP2P_pso("points2polygon_pso");

	CommonOperGroup cogS2P("sequence2points");

	CommonOperGroup cogArc2segm("arc2segm");
	CommonOperGroup cogDynaPoint("dyna_point");
	CommonOperGroup cogDynaPointWithEnds("dyna_point_with_ends");
	CommonOperGroup cogDynaSegment("dyna_segment");
	CommonOperGroup cogDynaSegmentWithEnds("dyna_segment_with_ends");

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

		Points2SequenceOperator<T> p2s1, p2s2, p2s3, p2s_p, p2s_ps, p2s_po, p2s_pso;
		Points2SequenceOperator<T> p2p1, p2p2, p2p3, p2p_p, p2p_ps, p2p_po, p2p_pso;

		Arcs2SegmentsOperator <T> arc2segm;
		Arcs2SegmentsOperator <T> seq2point;

		SequenceOperators()
			: lb(&cogLB)
			, ub(&cogUB)
			, cb(&cogCB)

			, firstN(&cogFN)
			, lastN(&cogLN)
			, firstP(&cogFP)
			, lastP(&cogLP)

			, p2s1(&cogP2S, false, false, ValueComposition::Sequence)
			, p2s2(&cogP2S, true, false, ValueComposition::Sequence)
			, p2s3(&cogP2S, true, true, ValueComposition::Sequence)
			, p2s_p(&cogP2S_p, false, false, ValueComposition::Sequence)
			, p2s_ps(&cogP2S_ps, true, false, ValueComposition::Sequence)
			, p2s_po(&cogP2S_po, false, true, ValueComposition::Sequence)
			, p2s_pso(&cogP2S_pso, true, true, ValueComposition::Sequence)

			, p2p1(&cogP2P, false, false, ValueComposition::Polygon)
			, p2p2(&cogP2P, true, false, ValueComposition::Polygon)
			, p2p3(&cogP2P, true, true, ValueComposition::Polygon)
			, p2p_p(&cogP2P_p, false, false, ValueComposition::Polygon)
			, p2p_ps(&cogP2P_ps, true, false, ValueComposition::Polygon)
			, p2p_po(&cogP2P_po, false, true, ValueComposition::Polygon)
			, p2p_pso(&cogP2P_pso, true, true, ValueComposition::Polygon)

			, arc2segm(&cogArc2segm, DoCreateNextPoint | DoCreateNrOrgEntity)
			, seq2point(&cogS2P, DoCloseLast | DoCreateNrOrgEntity| DoCreateSequenceNr)
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
		CastedUnaryAttrSpecialFuncOperator<AreaFunc     <P> > area;

		PointInPolygonOperator<P> pip;
		PointInRankedPolygonOperator<P, UInt8> pirpu8;
		PointInRankedPolygonOperator<P, UInt32> pirpu32;
		PointInRankedPolygonOperator<P, Int32>  pirpi32;
		PointInRankedPolygonOperator<P, Float32> pirpf32;
		PointInRankedPolygonOperator<P, Float64>  pirpf64;

		PointInAllPolygonsOperator<P> piap;
		DynaPointOperator<P>      dynaPoint1, dynaPoint2, dynaPoint3, dynaPoint4;

		GeometricOperators()
			:	centroid     (&cogCentroid)
			,	mid          (&cogMid)
			,	centroidOrMid(&cogCentroidOrMid)

			,	arcLength(&cogAL)
			,	area(&cogAREA)
			,	dynaPoint1(&cogDynaPoint,           DoCreateNrOrgEntity | DoCreateSequenceNr)
			,	dynaPoint2(&cogDynaPointWithEnds,   DoCreateNrOrgEntity | DoCreateSequenceNr | DoIncludeEndPoints)
			,	dynaPoint3(&cogDynaSegment,         DoCreateNextPoint | DoCreateNrOrgEntity | DoCreateSequenceNr)
			,	dynaPoint4(&cogDynaSegmentWithEnds, DoCreateNextPoint | DoCreateNrOrgEntity | DoCreateSequenceNr | DoIncludeEndPoints)
		{}

	};

	tl_oper::inst_tuple<typelists::seq_points , SequenceOperators <_>> seqOperPointInstances;
	tl_oper::inst_tuple<typelists::num_objects, SequenceOperators <_>> seqOperNumericInstances;
	tl_oper::inst_tuple<typelists::seq_points , GeometricOperators<_>> pointOperInstances;
}

/******************************************************************************/

