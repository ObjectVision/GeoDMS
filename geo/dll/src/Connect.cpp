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

#include "dbg/SeverityType.h"
#include "dbg/Timer.h"
#include "mci/CompositeCast.h"
#include "mci/ValueClass.h"
#include "mci/ValueClassID.h"
#include "geo/GeoDist.h"
#include "geo/SpatialIndex.h"
#include "geo/NeighbourIter.h"
#include "ptr/Resource.h"
#include "set/DataCompare.h"

#include "DataArray.h"
#include "DataItemClass.h"
#include "IndexAssigner.h"
#include "ParallelTiles.h"
#include "TreeItemClass.h"
#include "Unit.h"
#include "UnitClass.h"

#include "IndexGetterCreator.h"

CommonOperGroup cogCONNEIGH("connect_neighbour",   oper_policy::dynamic_result_class);
CommonOperGroup cogCON     ("connect",             oper_policy::dynamic_result_class);
CommonOperGroup cogCCON    ("capacitated_connect", oper_policy::dynamic_result_class);
CommonOperGroup cogCONINFO ("connect_info");
CommonOperGroup cogDISTINFO("dist_info");
CommonOperGroup cogIndex   ("spatialIndex");

CommonOperGroup cogCON_EQ("connect_eq", oper_policy::dynamic_result_class);
CommonOperGroup cogCON_NE("connect_ne", oper_policy::dynamic_result_class);
CommonOperGroup cogCONINFO_EQ("connect_info_eq");
CommonOperGroup cogCONINFO_NE("connect_info_ne");
CommonOperGroup cogDISTINFO_EQ("dist_info_eq");
CommonOperGroup cogDISTINFO_NE("dist_info_ne");

typedef UInt32 seq_index_type;

enum class compare_type {
	none, eq, ne, count
};

template<typename PointType, typename SpatialIndexType>
std::any CreateSpatialIndex(const AbstrOperGroup* og, const AbstrDataItem* arg1A, ResourceHandle& spi)
{
	auto arg1Data = const_array_cast<PointType>(arg1A)->GetDataRead();

	if (!arg1Data.size())
		return {};

	typename DataArray<PointType>::const_iterator
		destBegin= arg1Data.begin(),
		destEnd  = arg1Data.end();

	// test that arg1Data has only unique values
	{
		std::vector<PointType> sortedArg1Data(destBegin, destEnd);
		std::sort(sortedArg1Data.begin(), sortedArg1Data.end(), DataCompare<PointType>());
		if (std::adjacent_find(sortedArg1Data.begin(), sortedArg1Data.end()) != sortedArg1Data.end() )
			og->throwOperError("Multiple destinations with the same location found");
	}

	spi = makeResource<SpatialIndexType>(destBegin, destEnd, 0);
	return std::any(std::move(arg1Data));
}

// *****************************************************************************
//							ConnectNeighbourPointOperator
// *****************************************************************************

struct AbstrConnectNeighbourPointOperator : VariadicOperator
{
	AbstrConnectNeighbourPointOperator(const DataItemClass* argCls, bool withPartitioning)
		:	VariadicOperator(&cogCONNEIGH, AbstrDataItem::GetStaticClass(), withPartitioning ? 2 : 1)
	{
		ClassCPtr* argClsIter = m_ArgClasses.get();
		*argClsIter++ = argCls;
		if (withPartitioning)
			*argClsIter++ = AbstrDataItem::GetStaticClass();
		dms_assert(m_ArgClassesEnd == argClsIter);
	}

	// Override Operator
	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		dms_assert(args.size() >= 1);

		const AbstrDataItem* arg1A = AsDataItem(args[0]);
		const AbstrDataItem* arg2A = (args.size() >= 2) ? AsDataItem(args[1]) : nullptr;

		const AbstrUnit* pointUnit   = arg1A->GetAbstrValuesUnit();
		const AbstrUnit* pointDomain = arg1A->GetAbstrDomainUnit();
		const AbstrUnit* neighbourEntity = arg2A ? arg2A->GetAbstrValuesUnit() : nullptr;
		if (arg2A)
			pointDomain->UnifyDomain(arg2A->GetAbstrDomainUnit(), "v1", "e2", UM_Throw);

		if (!resultHolder)
		{
			resultHolder = CreateCacheDataItem(pointDomain, pointDomain);
			resultHolder->SetTSF(DSF_Categorical);
		}
		if (mustCalc)
		{
			DataReadLock arg1Lock(arg1A);
			DataReadLock arg2Lock(arg2A);

			AbstrDataItem* res = AsDataItem(resultHolder.GetNew());
			DataWriteLock resLock(res);

			ResourceHandle spi;
			auto arg1DataHolder = CreateIndex(arg1A, spi);

			if (spi)
			{
				SizeT count = res->GetAbstrDomainUnit()->GetCount();
				if (count)
				{
					OwningPtr<const IndexGetter> indexGetter;
					if (arg2A)
						indexGetter = IndexGetterCreator::Create(arg2A, no_tile);

					IndexAssigner32 indexAssigner(res, resLock.get(), no_tile, 0, count);

					Calculate(spi, indexAssigner.m_Indices, count, arg1A, no_tile, indexGetter);

					indexAssigner.Store();
				}
			}
			resLock.Commit();
		}
		return true;
	}

	virtual std::any CreateIndex(const AbstrDataItem* arg1, ResourceHandle& spi) const=0;
	virtual void Calculate(ResourceHandle& spi, UInt32* resBuffer, SizeT resSize, const AbstrDataItem* arg1A, tile_id t, const IndexGetter*) const=0;
};

template <typename T>
struct ConnectNeighbourPointOperator : AbstrConnectNeighbourPointOperator
{
	typedef T                              PointType;
	typedef Range<T>                       RangeType;
	typedef typename PointType::field_type CoordType;
	typedef Float64                        SqrDistType;
	typedef Float64                        SqrtDistType;
	typedef Unit<PointType>                PointUnitType;

	typedef DataArray<PointType>           ArgType;

	typedef SpatialIndex<CoordType, typename ArgType::const_iterator> SpatialIndexType;

	ConnectNeighbourPointOperator(bool withPartitioning)
		:	AbstrConnectNeighbourPointOperator(ArgType::GetStaticClass(), withPartitioning)
	{}

	std::any CreateIndex(const AbstrDataItem* arg1A, ResourceHandle& spi) const override
	{
		return CreateSpatialIndex<PointType, SpatialIndexType>(GetGroup(), arg1A, spi);
	}

	// Override Operator
	void Calculate(ResourceHandle& spi, UInt32* resBuffer, SizeT resSize, const AbstrDataItem* arg1A, tile_id t, const IndexGetter* indexGetter) const override
	{
		const ArgType* argPoints = const_array_cast<PointType>(arg1A);

		dms_assert(argPoints);
		dms_assert(resSize);

		SpatialIndexType& spIndex = GetAs<SpatialIndexType>(spi);
				
		auto pointData = argPoints->GetLockedDataRead(t);
		dms_assert(pointData.size() == resSize);

		auto destBegin = spIndex.first_leaf();
		dms_assert(pointData.begin() == destBegin);

		neighbour_iter<SpatialIndexType> iter(&spIndex);

		for (SizeT i=0; i!=resSize; ++i)
		{
			SizeT clusterID = i;
			if (indexGetter)
				clusterID = indexGetter->Get(i);

			const PointType& point = pointData[i];

			if (!IsDefined(point))
			{
				resBuffer[i] = UNDEFINED_VALUE(UInt32);
				continue;
			}
			typename DataArray<PointType>::const_iterator foundDestPointPtr = nullptr;

			iter.Reset(point);

			SizeT foundDestIndex = UNDEFINED_VALUE(SizeT);
			for (; iter; ++iter)
			{
				typename ArgType::const_iterator othPointPtr = iter.CurrLeaf().Value();
				SizeT thatIndex = othPointPtr - destBegin;

				SizeT thatClusterID = thatIndex;
				if (indexGetter)
					thatClusterID = indexGetter->Get(thatClusterID);

				if (clusterID != thatClusterID)
				{
					foundDestIndex = thatIndex;
					break;
				}
			}

			// store results 
			resBuffer[i] = foundDestIndex;
		}
	}
};

// *****************************************************************************
//									ConnectPointInfoOperator
// *****************************************************************************

struct AbstrConnectPointOperator : VariadicOperator
{
	bool isCapacitated;

	AbstrConnectPointOperator(const DataItemClass* argCls, bool isCapacitated_, compare_type CT)
		:	VariadicOperator(
				isCapacitated_ ? &cogCCON : &cogCON
			,	AbstrDataItem::GetStaticClass()
			,	2 + ( isCapacitated_ ? 2 : 0 ) + ( CT!=compare_type::none ? 2 : 0 )
			)
		,	isCapacitated(isCapacitated_)
	{
		ClassCPtr* argClsIter = m_ArgClasses.get();
		*argClsIter++ = argCls;
		if (isCapacitated) *argClsIter++ = AbstrDataItem::GetStaticClass();
		if (CT != compare_type::none) *argClsIter++ = AbstrDataItem::GetStaticClass();

		*argClsIter++ = argCls;
		if (isCapacitated) *argClsIter++ = AbstrDataItem::GetStaticClass();
		if (CT != compare_type::none) *argClsIter++ = AbstrDataItem::GetStaticClass();
		dms_assert(m_ArgClassesEnd == argClsIter);
	}

	// Override Operator
	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		bool isCapacitated = args.size() == 4;
		dms_assert(args.size() == 2 || isCapacitated);

		const AbstrDataItem* arg1A = AsDataItem(args[0]);
		const AbstrDataItem* arg1W = isCapacitated ? AsDataItem(args[1]) : nullptr;
		const AbstrDataItem* arg2A = AsDataItem(args[isCapacitated ? 2 : 1]);
		const AbstrDataItem* arg2W = isCapacitated ? AsDataItem(args[3]) : nullptr;

		const AbstrUnit* point1Unit   = arg1A->GetAbstrValuesUnit();
		const AbstrUnit* point2Unit   = arg2A->GetAbstrValuesUnit();
		const AbstrUnit* point1Entity = arg1A->GetAbstrDomainUnit();
		const AbstrUnit* point2Entity = arg2A->GetAbstrDomainUnit();

		arg1A->GetAbstrValuesUnit()->UnifyValues(arg2A->GetAbstrValuesUnit(), "v1", isCapacitated ? "v3" : "v2", UM_Throw);
		if (isCapacitated)
		{
			point1Entity->UnifyDomain(arg1W->GetAbstrDomainUnit(), "e1", "e2", UM_Throw);
			point2Entity->UnifyDomain(arg2W->GetAbstrDomainUnit(), "e3", "e4", UM_Throw);

			arg1W->GetAbstrValuesUnit()->UnifyValues(arg2W->GetAbstrValuesUnit(), "v2", "v4", UM_Throw);
		}

		if (!resultHolder)
		{
			resultHolder = CreateCacheDataItem(point2Entity, point1Entity);
			resultHolder->SetTSF(DSF_Categorical);
		}

		if (mustCalc)
		{
			DataReadLock arg1Lock(arg1A);
			DataReadLock arg2Lock(arg2A);

			AbstrDataItem* res = AsDataItem(resultHolder.GetNew());
			DataWriteLock resLock(res);

			ResourceHandle spi;
			auto arg1DataHolder = CreateIndex(arg1A, spi);

			OwningPtr<WeightGetter> weights1Getter(arg1W ? WeightGetterCreator::Create(arg1W) : nullptr);

			if (spi)
			{
				parallel_tileloop(point2Entity->GetNrTiles(), [res, resObj = resLock.get(), arg2A, arg2W, &spi, &weights1Getter, this](tile_id t)->void
				{
					SizeT tileSize = resObj->GetTiledRangeData()->GetTileSize(t);
					if (!tileSize)
						return;

					IndexAssigner32 indexAssigner(res, resObj, t, 0, tileSize);

					OwningPtr<WeightGetter> weights2Getter(arg2W ? WeightGetterCreator::Create(arg2W, t) : nullptr);

					Calculate(spi, indexAssigner.m_Indices, tileSize, weights1Getter, arg2A, t, weights2Getter);

					indexAssigner.Store();
				});
			}
			resLock.Commit();
		}
		return true;
	}

	virtual std::any CreateIndex(const AbstrDataItem* arg1, ResourceHandle& spi) const=0;
	virtual void Calculate(ResourceHandle& spi, UInt32* resBuffer, SizeT resSize, const WeightGetter* weights1, const AbstrDataItem* arg2A, tile_id t, const WeightGetter* weights2) const=0;
};

template <typename T, typename E = UInt32>
struct ConnectPointOperator : AbstrConnectPointOperator
{
	typedef T                              PointType;
	typedef Range<T>                       RangeType;
	typedef typename PointType::field_type CoordType;
	typedef Float64                        SqrDistType;
	typedef Float64                        SqrtDistType;
	typedef Unit<PointType>                PointUnitType;

	typedef DataArray<PointType>           ArgType;

	typedef SpatialIndex<CoordType, typename ArgType::const_iterator> SpatialIndexType;

	ConnectPointOperator(bool isCapacitated, compare_type ct)
		:	AbstrConnectPointOperator(ArgType::GetStaticClass(), isCapacitated, ct)
	{}

	std::any CreateIndex(const AbstrDataItem* arg1A, ResourceHandle& spi) const override
	{
		return CreateSpatialIndex<PointType, SpatialIndexType>(GetGroup(), arg1A, spi);
	}

	// Override Operator
	void Calculate(ResourceHandle& spi, UInt32* resBuffer, SizeT resSize, const WeightGetter* weights1, const AbstrDataItem* arg2A, tile_id t, const WeightGetter* weights2) const override
	{
		Timer processTimer;

		const ArgType* arg2 = const_array_cast<PointType>(arg2A);

		dms_assert(arg2);
		dms_assert(resSize);

		SpatialIndexType& spIndex = GetAs<SpatialIndexType>(spi);
				
		auto arg2Data = arg2->GetLockedDataRead(t);
		dms_assert(arg2Data.size() == resSize);

		auto
			point2Begin = arg2Data.begin(),
			point2End   = arg2Data.end();

		auto
			destBegin = spIndex.first_leaf();

		auto reporter = [&processTimer, point2Begin, point2End, t, tn = arg2A->GetAbstrDomainUnit()->GetNrTiles()](auto i) {
			if (processTimer.PassedSecs(5))
				reportF(SeverityTypeID::ST_MajorTrace, "Connect %s/%s points of tile %s/%s done", i, point2End - point2Begin, t, tn);
		};

		parallel_for_if_separable<SizeT, UInt32>(0, point2End - point2Begin, [point2Begin, weights1, weights2, destBegin, resBuffer, &spIndex, &reporter](auto i)
		{
			if (IsDefined(point2Begin[i]))
			{
				Float64 minWeight;
				if (weights1)
				{
					dms_assert(weights2);
					minWeight = weights2->Get(i);
				}
				neighbour_iter<SpatialIndexType> iter(&spIndex);
				iter.Reset(point2Begin[i]);
				for (; iter; ++iter)
				{
					SizeT pointIndex = (*iter) - destBegin;
					if (!weights1 || weights1->Get(pointIndex) >= minWeight)
					{
						resBuffer[i] = pointIndex;
						goto nextPoint;
					}
				}
			}
			resBuffer[i] = UNDEFINED_VALUE(UInt32);

		nextPoint:
			reporter(i);
		}
		);
	}
};

// *****************************************************************************
//									IndexedArcProjectionHandle
// *****************************************************************************

template <typename R, typename T, typename ResObjectPtr>
struct IndexedArcProjectionHandle : ArcProjectionHandleWithDist<R, T>
{
	template <typename SpatialIndexType>
	IndexedArcProjectionHandle(const Point<T>* p, const SpatialIndexType& spIndex, const R* optionalMaxSqrDistPtr)
		: ArcProjectionHandle(p, spIndex.GetSqrProximityUpperBound<R>(*p, 0xFFFFFFFF, optionalMaxSqrDistPtr))
	{
		dms_assert(spIndex.size());		
		for (auto iter = spIndex.begin(Inflate(*p, Point<T>(this->m_Dist, this->m_Dist))); iter; ++iter)
		{
			ResObjectPtr streetPtr = (*iter)->get_ptr();
			if (Project2Arc(begin_ptr(*streetPtr), end_ptr(*streetPtr)))
			{
				m_ArcPtr = streetPtr;
				iter.m_SearchObj = Inflate(*p, Point<T>(this->m_Dist, this->m_Dist));
			}
		}

		dms_assert(this->m_FoundAny);
	}

	template <typename SpatialIndexType, typename Filter>
	IndexedArcProjectionHandle(const Point<T>* p, const SpatialIndexType& spIndex,  const Filter& filter, const R* optionalMaxSqrDistPtr)
	{
		UInt32 maxDepth = 0xFFFFFFFF;
		while (true) {
	
			ArcProjectionHandleWithDist<R, T> aph(p, spIndex.GetSqrProximityUpperBound<R>(*p, maxDepth, optionalMaxSqrDistPtr));
			for (auto iter = spIndex.begin(Inflate(*p, Point<T>(aph.m_Dist, aph.m_Dist))); iter; ++iter)
			{
				ResObjectPtr streetPtr = (*iter)->get_ptr();
				if (!filter(streetPtr))
					continue;
				if (aph.Project2Arc(begin_ptr(*streetPtr), end_ptr(*streetPtr)))
				{
					m_ArcPtr = streetPtr;
					iter.RefineSearch( Inflate(*p, Point<T>(aph.m_Dist, aph.m_Dist)) );
				}
			}
			if (aph.m_FoundAny || !maxDepth)
			{
				ArcProjectionHandleWithDist<R, T>::operator =(aph);
				break;
			}
		}
	}

	ResObjectPtr m_ArcPtr;
};


// *****************************************************************************
//									ConnectInfoOperator
// *****************************************************************************

static TokenID s_Dist = GetTokenID_st("dist");
static TokenID s_ArcID = GetTokenID_st("ArcID");
static TokenID s_CutPoint = GetTokenID_st("CutPoint");
static TokenID s_InArc = GetTokenID_st("InArc");
static TokenID s_InSegm = GetTokenID_st("InSegm");
static TokenID s_SegmID = GetTokenID_st("SegmID");

template <typename P, typename E = UInt32, compare_type CT = compare_type::none, typename SegmID = UInt32, typename SqrtDistType = Float64, bool HasMaxDist = false, bool HasMinDist = false, bool OnlyDistResult = false>
//class ConnectInfoOperator : public boost::mpl::if_c<CT== compare_type::none, BinaryOperator, QuaternaryOperator>::type
class ConnectInfoOperator : std::conditional_t<CT == compare_type::none, 
		std::conditional_t<HasMaxDist, std::conditional_t<HasMinDist, QuaternaryOperator, TernaryOperator>, BinaryOperator>
	,	std::conditional_t<HasMaxDist, std::conditional_t<HasMinDist, SexenaryOperator, QuinaryOperator>, QuaternaryOperator>
	>
{
	typedef P                              PointType;
	typedef Range<P>                       RangeType;
	typedef typename PointType::field_type CoordType;
	typedef SqrtDistType                   SqrDistType;
	typedef std::vector<PointType>         PolygonType;
	typedef Unit<PointType>                PointUnitType;
	typedef Unit<SqrtDistType>             DistUnitType;
	typedef Unit<Bool>                     BoolUnitType;
	typedef Unit<UInt32>                   SegmUnitType;

	typedef DataArray<PolygonType>         Arg1Type;
	typedef DataArray<PointType>           Arg2Type;

	typedef DataArray<SqrtDistType>        ResSubType1; // distance
//	typedef DataArray<E>                   ResSubType2; // arc-id
	typedef DataArray<PointType>           ResSubType3; // cut-point
	typedef DataArray<Bool>                ResSubType4; // in-arc
	typedef DataArray<Bool>                ResSubType5; // in-semg
	typedef DataArray<SegmID>              ResSubType6; // segm-id

	using SpatialIndexType = SpatialIndex<CoordType, typename Arg1Type::const_iterator>;

	static auto cogInfo() { return OnlyDistResult ? &cogDISTINFO : &cogCONINFO; }
	static const Class* ResultCls()
	{
		if (OnlyDistResult)
			return  ResSubType1::GetStaticClass();
		return TreeItem::GetStaticClass();
	}

public:
	template <compare_type CT2 = CT>
	ConnectInfoOperator(typename std::enable_if<CT2 == compare_type::none && !HasMinDist && !HasMaxDist>::type* = nullptr)
		:	BinaryOperator(cogInfo(), ResultCls()
			,	Arg1Type::GetStaticClass(), Arg2Type::GetStaticClass()
			)
	{}

	template <compare_type CT2 = CT>
	ConnectInfoOperator(typename std::enable_if<CT2 == compare_type::none && !HasMinDist && HasMaxDist>::type* = nullptr)
		:	TernaryOperator(cogInfo(), ResultCls()
			,	Arg1Type::GetStaticClass(), Arg2Type::GetStaticClass()
			,	DataArray<SqrtDistType>::GetStaticClass()
			)
	{}

	template <compare_type CT2 = CT>
	ConnectInfoOperator(typename std::enable_if<CT2 == compare_type::none && HasMinDist && HasMaxDist>::type* = nullptr)
		:	QuaternaryOperator(cogInfo(), ResultCls()
			,	Arg1Type::GetStaticClass(), Arg2Type::GetStaticClass()
			,	DataArray<SqrtDistType>::GetStaticClass(), DataArray<SqrtDistType>::GetStaticClass()
			)
	{}


	template <compare_type CT2 = CT>
	ConnectInfoOperator(typename std::enable_if<CT2 == compare_type::eq && !HasMinDist && !HasMaxDist>::type* = nullptr)
		:	QuaternaryOperator(OnlyDistResult ? &cogDISTINFO_EQ : &cogCONINFO_EQ, ResultCls()
			,	Arg1Type::GetStaticClass(), DataArray<E>::GetStaticClass()
			,	Arg2Type::GetStaticClass(), DataArray<E>::GetStaticClass()
			)
	{}

	template <compare_type CT2 = CT>
	ConnectInfoOperator(typename std::enable_if<CT2 == compare_type::ne && !HasMinDist && !HasMaxDist>::type* = nullptr)
		:	QuaternaryOperator(OnlyDistResult ? &cogDISTINFO_NE : &cogCONINFO_NE, ResultCls()
			,	Arg1Type::GetStaticClass(), DataArray<E>::GetStaticClass()
			,	Arg2Type::GetStaticClass(), DataArray<E>::GetStaticClass()
			)
	{}

	// Override Operator
	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		UInt32 argCount = 0;

		const AbstrDataItem* arg1A = AsDataItem(args[argCount++]);
		const AbstrDataItem* arg1_ID = (CT != compare_type::none) ? AsDataItem(args[argCount++]) : nullptr;
		const AbstrDataItem* arg2A = AsDataItem(args[argCount++]);
		const AbstrDataItem* arg2_ID = (CT != compare_type::none) ? AsDataItem(args[argCount++]) : nullptr;

		const AbstrDataItem* argMaxDist = (HasMaxDist) ? AsDataItem(args[argCount++]) : nullptr;
		const AbstrDataItem* argMinDist = (HasMinDist) ? AsDataItem(args[argCount++]) : nullptr;
		dms_assert(args.size() == argCount);


		const AbstrUnit* polyUnit    = arg1A->GetAbstrValuesUnit();
		const AbstrUnit* pointUnit   = arg2A->GetAbstrValuesUnit();
		const AbstrUnit* polyEntity  = arg1A->GetAbstrDomainUnit();
		const AbstrUnit* pointEntity = arg2A->GetAbstrDomainUnit();
		polyUnit->UnifyValues (pointUnit, "Values of polygon attribute", "Values of point attribute", UM_Throw);

		if (CT != compare_type::none)
		{
			polyEntity ->UnifyDomain(arg1_ID->GetAbstrDomainUnit(), "e1", "e2", UM_Throw);
			pointEntity->UnifyDomain(arg2_ID->GetAbstrDomainUnit(), "e3", "e4", UM_Throw);
			arg1_ID->GetAbstrValuesUnit()->UnifyValues(arg2_ID->GetAbstrValuesUnit(), "v2", "v4", UM_Throw);
		}
		if (HasMinDist)
			pointEntity->UnifyDomain(argMinDist->GetAbstrDomainUnit(), "Domain of Point attribute", "Domain of Minimum Distances", UnifyMode(UM_Throw | UM_AllowVoidRight));
		if (HasMaxDist)
			pointEntity->UnifyDomain(argMaxDist->GetAbstrDomainUnit(), "Domain of Point attribute", "Domain of Maximum Distances", UnifyMode(UM_Throw| UM_AllowVoidRight));

		bool hasNonVoidMinDist = HasMinDist && !(argMinDist->HasVoidDomainGuarantee());
		bool hasNonVoidMaxDist = HasMaxDist && !(argMaxDist->HasVoidDomainGuarantee());

		const DistUnitType* distUnit = const_unit_cast<SqrtDistType>(DistUnitType::GetStaticClass()->CreateDefault());
		if (!resultHolder)
		{
			if (OnlyDistResult)
				resultHolder = CreateCacheDataItem(pointEntity, distUnit);
			else
				resultHolder = TreeItem::CreateCacheRoot();
		}
		const BoolUnitType* boolUnit = OnlyDistResult ? nullptr : const_unit_cast<Bool  >( BoolUnitType::GetStaticClass()->CreateDefault() );
		const SegmUnitType* segmUnit = OnlyDistResult ? nullptr : const_unit_cast<UInt32>( SegmUnitType::GetStaticClass()->CreateDefault() );


		AbstrDataItem* resSub1 = OnlyDistResult ? AsDataItem(resultHolder.GetNew()) : CreateDataItem(resultHolder, s_Dist, pointEntity, distUnit);
		AbstrDataItem* resSub2 = OnlyDistResult ? nullptr : CreateDataItem(resultHolder, s_ArcID,    pointEntity, polyEntity);
		AbstrDataItem* resSub3 = OnlyDistResult ? nullptr : CreateDataItem(resultHolder, s_CutPoint, pointEntity, pointUnit );
		AbstrDataItem* resSub4 = OnlyDistResult ? nullptr : CreateDataItem(resultHolder, s_InArc,    pointEntity, boolUnit  );
		AbstrDataItem* resSub5 = OnlyDistResult ? nullptr : CreateDataItem(resultHolder, s_InSegm,   pointEntity, boolUnit  );
		AbstrDataItem* resSub6 = OnlyDistResult ? nullptr : CreateDataItem(resultHolder, s_SegmID,   pointEntity, segmUnit  );

		if (resSub2)
			resSub2->SetTSF(DSF_Categorical);

		if (mustCalc)
		{
			Timer processTimer;

			const Arg1Type* arg1 = const_array_cast<PolygonType>(arg1A);
			const Arg2Type* arg2 = const_array_cast<  PointType>(arg2A);

			dms_assert(arg1);
			dms_assert(arg2);

			DataReadLock arg1Lock(arg1A);
			DataReadLock arg2Lock(arg2A);
			DataReadLock arg1_IdLock(arg1_ID);
			DataReadLock arg2_IdLock(arg2_ID);
			DataReadLock argMinDistLock(argMinDist);
			DataReadLock argMaxDistLock(argMaxDist);

			SizeT arg1Count = polyEntity->GetCount();
			SizeT arg2Count = pointEntity->GetCount();
			std::atomic<SizeT> nrArg2 = 0;

			auto arg1Data = arg1->GetLockedDataRead();
			dms_assert(arg1Count == arg1Data.size());
			SpatialIndexType spIndex(arg1Data.begin(), arg1Data.end(), 0);

			const E* polyIDsPtr = nullptr;
			typename DataArray<E>::locked_cseq_t polyIDs;  if (arg1_ID) { polyIDs = const_array_cast<E>(arg1_ID)->GetLockedDataRead(); polyIDsPtr = polyIDs.begin(); }

			DataWriteLock res1Lock(resSub1);
			DataWriteLock res2Lock(resSub2);
			DataWriteLock res3Lock(resSub3);
			DataWriteLock res4Lock(resSub4);
			DataWriteLock res5Lock(resSub5);
			DataWriteLock res6Lock(resSub6);

			parallel_tileloop(pointEntity->GetNrTiles(), [&, this](tile_id t)
				{
					auto arg2Data = arg2->GetLockedDataRead(t);

					const E* pointIDsPtr = nullptr;
					const SqrtDistType* minSqrDistPtr = nullptr;
					const SqrtDistType* maxSqrDistPtr = nullptr;
					typename DataArray<E>::locked_cseq_t pointIDs; if (arg2_ID) { pointIDs = const_array_cast<E>(arg2_ID)->GetLockedDataRead(t); pointIDsPtr = pointIDs.begin(); }
					typename DataArray<SqrtDistType>::locked_cseq_t minSqrDists; if (argMinDist) { minSqrDists = const_array_cast<SqrtDistType>(argMinDist)->GetLockedDataRead(hasNonVoidMinDist ? t : 0); minSqrDistPtr = minSqrDists.begin(); }
					typename DataArray<SqrtDistType>::locked_cseq_t maxSqrDists; if (argMaxDist) { maxSqrDists = const_array_cast<SqrtDistType>(argMaxDist)->GetLockedDataRead(hasNonVoidMaxDist ? t : 0); maxSqrDistPtr = maxSqrDists.begin(); }

					auto arg2DataSize = arg2Data.size();
					if (!arg2DataSize)
						return;

					auto data1 = mutable_array_cast<SqrtDistType>(res1Lock)->GetWritableTile(t); auto r1 = data1.begin();
					//				auto data2 = composite_cast<ResSubType2*>(resSub2)->GetDataWrite(); auto r2 = data2.begin();
					AbstrDataObject* ado2 = OnlyDistResult ? nullptr : res2Lock.get();

					WritableTileLock arcIdDataLock(ado2, t, dms_rw_mode::write_only_all);

					typename ResSubType3::locked_seq_t data3; typename ResSubType3::iterator r3;
					typename ResSubType4::locked_seq_t data4; typename ResSubType4::iterator r4;
					typename ResSubType5::locked_seq_t data5; typename ResSubType5::iterator r5;
					typename ResSubType6::locked_seq_t data6; typename ResSubType6::iterator r6;
					if (!OnlyDistResult)
					{
						data3 = mutable_array_cast<PointType>(res3Lock)->GetWritableTile(t); r3 = data3.begin();
						data4 = mutable_array_cast<Bool>     (res4Lock)->GetWritableTile(t); r4 = data4.begin();
						data5 = mutable_array_cast<Bool>     (res5Lock)->GetWritableTile(t); r5 = data5.begin();
						data6 = mutable_array_cast<SegmID>   (res6Lock)->GetWritableTile(t); r6 = data6.begin();
					}
					if (!arg1Count)
					{
						fast_fill(r1, r1 + arg2DataSize, MAX_VALUE(SqrtDistType)); //dist
						if (!OnlyDistResult)
						{
							ado2->FillWithUInt32Values(tile_loc(t, 0), arg2DataSize, UNDEFINED_VALUE(UInt32));
							fast_undefine(r3, r3 + arg2DataSize); // cut-point
							fast_undefine(r6, r6 + arg2DataSize); // segm-id
						}
					}
					else
					{
						auto streetBegin = arg1Data.begin();

						const PointType* pointBegin = arg2Data.begin();
						const PointType* pointEnd = arg2Data.end();
						const PointType* pointPtr = pointBegin;

						auto filter = [=, &pointPtr](typename Arg1Type::const_iterator streetPtr) ->bool
						{
							if constexpr (CT == compare_type::none)
								return true;
							else
							{
								seq_index_type streetIndex = streetPtr - streetBegin;
								dms_assert(streetIndex < arg1Count);
								E pointID = pointIDsPtr[pointPtr - pointBegin];
								if constexpr (CT == compare_type::eq)
								{
									if (pointID == polyIDsPtr[streetIndex])
										return true;
								}
								else
								{
									static_assert(CT == compare_type::ne);
									if (pointID != polyIDsPtr[streetIndex])
										return true;
								}
								//						return ((CT == compare_type::eq) == (pointID == polyIDsPtr[streetIndex])) || !IsDefined(pointID);
								return !IsDefined(pointID);
							}
						};

						SizeT nrUnreportedPoints = 0;
						SizeT currRow = 0;
						for (; pointPtr != pointEnd; ++r1, ++pointPtr)
						{
							if (IsDefined(*pointPtr))
							{
								IndexedArcProjectionHandle<SqrDistType, CoordType, typename Arg1Type::const_iterator> arcHnd(pointPtr, spIndex, filter, maxSqrDistPtr);
								if (arcHnd.m_FoundAny)
								{
									if (!maxSqrDistPtr || *maxSqrDistPtr > arcHnd.m_MinSqrDist)
										*r1 = Convert<SqrtDistType>(arcHnd.m_Dist);
									else
										MakeUndefined(*r1);
									if (!OnlyDistResult)
									{
										ado2->SetValueAsSizeT(currRow, arcHnd.m_ArcPtr - streetBegin, t);
										*r3 = arcHnd.m_CutPoint;
										*r4 = arcHnd.m_InArc;
										*r5 = arcHnd.m_InSegm;
										*r6 = arcHnd.m_SegmIndex;
									}
									goto pointProcessingCompleted;
								}
							}

							*r1 = MAX_VALUE(SqrtDistType);
							if (!OnlyDistResult)
							{
								ado2->SetValueAsSizeT(currRow, UNDEFINED_VALUE(SizeT), t);
								MakeUndefined(*r3);
								MakeUndefined(*r6);
							}

						pointProcessingCompleted:
							if (hasNonVoidMinDist)
								++minSqrDistPtr;
							if (hasNonVoidMaxDist)
								++maxSqrDistPtr;
							if (!OnlyDistResult)
							{
								++currRow;
								++r3, ++r4, ++r5, ++r6;
							}
							++nrUnreportedPoints;
							if (processTimer.PassedSecs(5))
							{
								nrArg2 += nrUnreportedPoints;
								nrUnreportedPoints = 0;
								reportF(SeverityTypeID::ST_MajorTrace, "%s %s/%s points done", this->GetGroup()->GetName(), nrArg2, arg2Count);
							}
						}
						nrArg2 += nrUnreportedPoints;
					}
				});
			res1Lock.Commit();
			res2Lock.Commit();
			res3Lock.Commit();
			res4Lock.Commit();
			res5Lock.Commit();
			res6Lock.Commit();
			if (!OnlyDistResult)
				resultHolder->SetIsInstantiated();
		}
		return true;
	}
};

// *****************************************************************************
//									FastConnectOperator
// *****************************************************************************

static TokenID s_UnionData = GetTokenID_st("UnionData");
static TokenID s_orgEntity = GetTokenID_st("nr_OrgEntity");
static TokenID s_arc_rel   = GetTokenID_st("arc_rel");

template <class T, class R = seq_index_type, compare_type CT = compare_type::none, typename E= UInt32, typename SqrtDistType = Float64, bool HasMaxDist = false, bool HasMinDist = false>
class FastConnectOperator : std::conditional_t<CT == compare_type::none
	,	std::conditional_t<HasMaxDist, std::conditional_t<HasMinDist, QuaternaryOperator, TernaryOperator>, BinaryOperator>
	,	std::conditional_t<HasMaxDist, std::conditional_t<HasMinDist, SexenaryOperator, QuinaryOperator>, QuaternaryOperator>
	>
{
	typedef T                              PointType;
	typedef Range<T>                       RangeType;
	typedef typename PointType::field_type CoordType;
	typedef SqrtDistType                   SqrDistType;
	typedef std::vector<PointType>         PolygonType;
	typedef Unit<PointType>                PointUnitType;

	typedef Unit<R>                    ResultUnitType;
	typedef DataArray<PolygonType>     ResultSubType;
	typedef DataArray<PolygonType>     Arg1Type;
	typedef DataArray<PointType>       Arg2Type;

	typedef SpatialIndex<CoordType, sequence_array_index<PointType> > SpatialIndexType;
			
public:
	template <compare_type CT2 = CT>
	FastConnectOperator(typename std::enable_if<CT2 == compare_type::none && !HasMaxDist && !HasMinDist>::type* = nullptr)
		:	BinaryOperator(&cogCON, ResultUnitType::GetStaticClass()
			,	Arg1Type::GetStaticClass()
			,	Arg2Type::GetStaticClass()
			)
	{}
	template <compare_type CT2 = CT>
	FastConnectOperator(typename std::enable_if<CT2 == compare_type::none && HasMaxDist && !HasMinDist>::type* = nullptr)
		: TernaryOperator(&cogCON, ResultUnitType::GetStaticClass()
			, Arg1Type::GetStaticClass()
			, Arg2Type::GetStaticClass()
			, DataArray<SqrDistType>::GetStaticClass()
		)
	{}
	template <compare_type CT2 = CT>
	FastConnectOperator(typename std::enable_if<CT2 == compare_type::none && HasMaxDist && HasMinDist>::type* = nullptr)
		: QuaternaryOperator(&cogCON, ResultUnitType::GetStaticClass()
			, Arg1Type::GetStaticClass()
			, Arg2Type::GetStaticClass()
			, DataArray<SqrDistType>::GetStaticClass()
			, DataArray<SqrDistType>::GetStaticClass()
		)
	{}

	template <compare_type CT2 = CT>
	FastConnectOperator(typename std::enable_if<CT2 == compare_type::eq>::type* = nullptr)
		:	QuaternaryOperator(&cogCON_EQ, ResultUnitType::GetStaticClass()
			,	Arg1Type::GetStaticClass(), DataArray<E>::GetStaticClass()
			,	Arg2Type::GetStaticClass(), DataArray<E>::GetStaticClass()
		)
	{}
	template <compare_type CT2 = CT>
	FastConnectOperator(typename std::enable_if<CT2 == compare_type::ne>::type* = nullptr)
		: QuaternaryOperator(&cogCON_NE, ResultUnitType::GetStaticClass()
			, Arg1Type::GetStaticClass(), DataArray<E>::GetStaticClass()
			, Arg2Type::GetStaticClass(), DataArray<E>::GetStaticClass()
		)
	{}

	// Override Operator
	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		arg_index argCount = 0;

		const AbstrDataItem* arg1A = debug_valcast<const AbstrDataItem*>(args[argCount++]);
		const AbstrDataItem* arg1_ID = (CT != compare_type::none) ? debug_valcast<const AbstrDataItem*>(args[argCount++]) : nullptr;
		const AbstrDataItem* arg2A = debug_valcast<const AbstrDataItem*>(args[argCount++]);
		const AbstrDataItem* arg2_ID = (CT != compare_type::none) ? debug_valcast<const AbstrDataItem*>(args[argCount++]) : nullptr;

		const AbstrDataItem* argMaxDist = (HasMaxDist) ? AsDataItem(args[argCount++]) : nullptr;
		const AbstrDataItem* argMinDist = (HasMinDist) ? AsDataItem(args[argCount++]) : nullptr;
		dms_assert(args.size() == argCount);

		const AbstrUnit* polyUnit = arg1A->GetAbstrValuesUnit();
		const AbstrUnit* pointUnit = arg2A->GetAbstrValuesUnit();
		const AbstrUnit* polyEntity = arg1A->GetAbstrDomainUnit();
		const AbstrUnit* pointEntity = arg2A->GetAbstrDomainUnit();
		polyUnit->UnifyValues(pointUnit, "polygon coordinates", "points", UM_Throw);

		if (CT != compare_type::none)
		{
			polyEntity->UnifyDomain(arg1_ID->GetAbstrDomainUnit(), "e1", "e2", UM_Throw);
			pointEntity->UnifyDomain(arg2_ID->GetAbstrDomainUnit(), "e3", "e4", UM_Throw);
			arg1_ID->GetAbstrValuesUnit()->UnifyValues(arg2_ID->GetAbstrValuesUnit(), "v2", "v4", UM_Throw);
		}
		if (HasMinDist)
			pointEntity->UnifyDomain(argMinDist->GetAbstrDomainUnit(), "Domain of points", "Domain of Minimum Distances", UnifyMode(UM_Throw | UM_AllowVoidRight));
		if (HasMaxDist)
			pointEntity->UnifyDomain(argMaxDist->GetAbstrDomainUnit(), "Domain of points", "Domain of Maximum Distances", UnifyMode(UM_Throw | UM_AllowVoidRight));

		bool hasNonVoidMinDist = HasMinDist && !(argMinDist->HasVoidDomainGuarantee());
		bool hasNonVoidMaxDist = HasMaxDist && !(argMaxDist->HasVoidDomainGuarantee());

		ResultUnitType* resDomain = mutable_unit_cast<R>(ResultUnitType::GetStaticClass()->CreateResultUnit(resultHolder));
		dms_assert(resDomain);
		bool createNewResult = !resultHolder;
		resultHolder = resDomain;

		AbstrDataItem* resSub   = CreateDataItem(resDomain, s_UnionData, resDomain, polyUnit,	ValueComposition::Sequence);
		AbstrDataItem* resNrOrg = CreateDataItem(resDomain, s_arc_rel, resDomain, arg1A->GetAbstrDomainUnit());
		resNrOrg->SetTSF(DSF_Categorical);

		if (createNewResult)
		{
			AbstrDataItem* resNrOrg_depreciated = CreateDataItem(resDomain, s_orgEntity, resDomain, arg1A->GetAbstrDomainUnit());
			resNrOrg_depreciated->SetTSF(DSF_Categorical);
			resNrOrg_depreciated->SetTSF(TSF_Depreciated);
			resNrOrg_depreciated->SetReferredItem(resNrOrg);
		}
		MG_PRECONDITION(resSub);

		if (mustCalc)
		{
			Timer processTimer;

			DataReadLock arg1Lock(arg1A);
			DataReadLock arg2Lock(arg2A);
			DataReadLock arg1IDLock(arg1_ID);
			DataReadLock arg2IDLock(arg2_ID);
			DataReadLock argMinDistLock(argMinDist);
			DataReadLock argMaxDistLock(argMaxDist);

			R arg1Count = arg1A->GetAbstrDomainUnit()->GetCount(); 
			R arg2Count = arg2A->GetAbstrDomainUnit()->GetCount();

			R maxResCount = arg1Count + 2*arg2Count;
			
			if(!arg1Count) 
			{
				resDomain->SetCount(0);
				DataWriteLock(resSub  ).Commit();
				DataWriteLock(resNrOrg).Commit();
				return true;
			}

			typename sequence_traits<typename ResultSubType::value_type>::container_type
				resultSubData(maxResCount MG_DEBUG_ALLOCATOR_SRC("Connect: resultSubData.indices"));
			SizeT actualDataSize = 0;
			for (tile_id t=0, tn = arg1A->GetAbstrDomainUnit()->GetNrTiles(); t!=tn; ++t)
				actualDataSize += const_array_cast<PolygonType>(arg1A)->GetLockedDataRead(t).get_sa().actual_data_size();

			resultSubData.data_reserve( actualDataSize + arg2Count MG_DEBUG_ALLOCATOR_SRC("Connect: resultSubData.sequences"));

			OwningPtrSizedArray<R> nrOrgEntityData(arg2Count MG_DEBUG_ALLOCATOR_SRC("Connect: nrOrgEntityData"));
			R* nrOrgEntityDataPtr = nrOrgEntityData.begin();
			R* nrOrgEntityIter = nrOrgEntityDataPtr;

			dms_assert(resultSubData.size() == maxResCount);

			auto resStreetBegin= resultSubData.begin();
			auto resStreetEnd  = resStreetBegin;
			for (tile_id t=0, tn = arg1A->GetAbstrDomainUnit()->GetNrTiles(); t!=tn; ++t)
			{
				auto arg1Data = const_array_cast<PolygonType>(arg1A)->GetLockedDataRead(t);
				resStreetEnd = std::copy(arg1Data.begin(), arg1Data.end(), resStreetEnd);
			}
			auto
				resCutIter    = resStreetEnd + arg2Count, // will be incremented when a street-cut was made
				resCutBegin   = resCutIter,
				resCutEnd     = resultSubData.end();
//				arcPtr; // arcPtr is only assigned in inner loop;

			const E* polyIDsPtr = nullptr;
			typename DataArray<E>::locked_cseq_t polyIDs;
			if (arg1_ID) {
				polyIDs = const_array_cast<E>(arg1_ID)->GetLockedDataRead();
				polyIDsPtr = polyIDs.begin();
			}

			SpatialIndexType spIndex(
				sequence_array_index<PointType>(resStreetBegin),
				sequence_array_index<PointType>(resStreetEnd  ),
				2*arg2Count
			);

			for (tile_id t=0, tn = arg2A->GetAbstrDomainUnit()->GetNrTiles(); t!=tn; ++t)
			{
				auto arg2Data = const_array_cast<PointType  >(arg2A)->GetLockedDataRead(t);

				const E* pointIDsPtr = nullptr;
				typename DataArray<E>::locked_cseq_t pointIDs; 
				if (arg2_ID) {
					pointIDs = const_array_cast<E>(arg2_ID)->GetLockedDataRead(t); pointIDsPtr = pointIDs.begin();
				}

				const PointType* pointBegin = arg2Data.begin();
				const PointType* pointEnd   = arg2Data.end();
				const PointType* pointPtr   = pointBegin;
				const SqrDistType* minSqrDistPtr = nullptr;
				const SqrDistType* maxSqrDistPtr = nullptr;

				typename DataArray<SqrtDistType>::locked_cseq_t minSqrDists; if (argMinDist) { minSqrDists = const_array_cast<SqrtDistType>(argMinDist)->GetLockedDataRead(hasNonVoidMinDist ? t : 0); minSqrDistPtr = minSqrDists.begin(); }
				typename DataArray<SqrtDistType>::locked_cseq_t maxSqrDists; if (argMaxDist) { maxSqrDists = const_array_cast<SqrtDistType>(argMaxDist)->GetLockedDataRead(hasNonVoidMaxDist ? t : 0); maxSqrDistPtr = maxSqrDists.begin(); }

				auto filter = [arg1Count, arg2Count, nrOrgEntityDataPtr, pointIDsPtr, polyIDsPtr, resStreetBegin, pointBegin, &pointPtr](typename ResultSubType::iterator streetPtr) ->bool
				{
					if constexpr (CT == compare_type::none)
						return true;
					seq_index_type streetIndex = streetPtr - resStreetBegin;
					dms_assert(streetIndex < arg1Count + 2 * arg2Count);
					if (streetIndex >= arg1Count)
					{
						dms_assert(streetIndex >= arg1Count + arg2Count);
						streetIndex -= (arg1Count + arg2Count);
						dms_assert(streetIndex < arg2Count);
						streetIndex = nrOrgEntityDataPtr[streetIndex];
					}
					dms_assert(streetIndex < arg1Count);
					E pointID = pointIDsPtr[pointPtr - pointBegin];

					if constexpr (CT == compare_type::eq)
						return pointID == polyIDsPtr[streetIndex] || !IsDefined(pointID);
					else
						return pointID != polyIDsPtr[streetIndex] || !IsDefined(pointID);
				};
				for (;pointPtr != pointEnd; ++pointPtr)
				{
					if (!IsDefined(*pointPtr))
						continue;
					dms_assert(resStreetEnd < resCutBegin);

					IndexedArcProjectionHandle<SqrDistType, CoordType, ResultSubType::iterator> arcHnd(pointPtr, spIndex, filter, maxSqrDistPtr);
					if (arcHnd.m_FoundAny)
					{

						// add Arc with connection
						dms_assert(resStreetEnd->empty());
						resStreetEnd->resize_uninitialized(2);
						auto resPointPtr = resStreetEnd->begin();
						resPointPtr[0] = *pointPtr;
						resPointPtr[1] = arcHnd.m_CutPoint;
						dms_assert(resPointPtr + 2 == resStreetEnd->end());

						// split Arc if neccessary
						if (arcHnd.m_InArc)
						{
							typename ResultSubType::reference arcRef = *(arcHnd.m_ArcPtr);

							dms_assert(resCutIter < resCutEnd);              // still space available in last segment of results
							dms_assert(arcHnd.m_SegmIndex < arcRef.size() - 1);// segmIndex < #segm = #points -1
							dms_assert(resCutIter->empty());
							(*resCutIter).resize_uninitialized(arcRef.size() - arcHnd.m_SegmIndex - (arcHnd.m_InSegm ? 0 : 1));
							dms_assert((*resCutIter).size() > 1); // if match was made on lastNode, then inArc was false.

							resPointPtr = resCutIter->begin();

							if (arcHnd.m_InSegm)
								*resPointPtr++ = arcHnd.m_CutPoint;

							typename sequence_traits<PointType>::pointer
								arcCut = arcRef.begin() + arcHnd.m_SegmIndex + 1,
								arcEnd = arcRef.end();
							resPointPtr = fast_copy(arcCut, arcEnd, resPointPtr);
							dms_assert(resPointPtr == resCutIter->end());

							spIndex.Add(sequence_array_index<PointType>(resCutIter));
							++resCutIter;

							arcRef.erase(arcCut + 1, arcEnd); // if !isSegm then also cutPoint is removed
							*arcCut = arcHnd.m_CutPoint;
							dms_assert(arcRef.size() > 1 && (arcCut + 1) == arcRef.end()); // if match was made on firstNode, then inArc was false.

							R nrArc = arcHnd.m_ArcPtr - resStreetBegin;
							if (nrArc >= arg1Count)
							{
								dms_assert(nrArc >= arg1Count + arg2Count); // nrArc may not point to second segment which are only connections
								nrArc -= (arg1Count + arg2Count);
								dms_assert(nrArc < R(nrOrgEntityIter - nrOrgEntityData.begin()));
								nrArc = nrOrgEntityData[nrArc];
							}
							*nrOrgEntityIter++ = nrArc;
						}
#if defined(MG_DEBUG)
						R nrNewStreets = nrOrgEntityIter - nrOrgEntityDataPtr;
						dms_assert(nrNewStreets + arg1Count + arg2Count == (resCutIter - resultSubData.begin()));
#endif
						++resStreetEnd;

					}
					if (hasNonVoidMinDist)
						++minSqrDistPtr;
					if (hasNonVoidMaxDist)
						++maxSqrDistPtr;
					if (processTimer.PassedSecs(5))
						reportF(SeverityTypeID::ST_MajorTrace, "Connect %s/%s points of tile %s/%s done", pointPtr - pointBegin, pointEnd - pointBegin, t, tn);
				}
			}
			dms_assert(!(resCutBegin < resStreetEnd));
			dms_assert(!(resCutEnd < resCutIter));
			seq_elem_index_type nrOmittedPoints = resCutBegin - resStreetEnd;

			R nrNewStreets = nrOrgEntityIter - nrOrgEntityData.begin();

			dms_assert(resStreetEnd - resultSubData.begin() == arg1Count + (arg2Count - nrOmittedPoints));
			dms_assert(resCutIter - resCutBegin == nrNewStreets);

			resDomain->SetCount(arg1Count + (arg2Count - nrOmittedPoints) + nrNewStreets);

			DataWriteLock resLock(resSub); 

			auto resSubData = mutable_array_cast<PolygonType>(resLock)->GetDataWrite();

			MGD_CHECKDATA(resSubData.get_sa().IsLocked());
			MGD_CHECKDATA(resultSubData.IsLocked());

			resSubData.get_sa().data_reserve(resultSubData.actual_data_size() MG_DEBUG_ALLOCATOR_SRC("Connect: resultSubData.data_reserve"));

			auto ri = resSubData.begin();
			ri = fast_copy(resultSubData.begin(), resStreetEnd, ri);
			ri = fast_copy(resCutBegin, resCutIter, ri);

			dms_assert(ri == resSubData.end());
			dms_assert(resSubData.get_sa().actual_data_size() == resultSubData.actual_data_size());
			dms_assert(!resSubData.get_sa().IsDirty());

			resLock.Commit();

			DataWriteLock resNrOrgLock(resNrOrg);

			tile_id t = no_tile;
			// TODO G8: use TileWriteChannel en visitor to avoid multiple shadowing.
			arg1A->GetAbstrDomainUnit()->InviteUnitProcessor( IdAssigner     (resNrOrgLock.get(), t, 0, 0, arg1Count));
			arg1A->GetAbstrDomainUnit()->InviteUnitProcessor( NullAssigner   (resNrOrgLock.get(), t, arg1Count, arg2Count - nrOmittedPoints) );
			arg1A->GetAbstrDomainUnit()->InviteUnitProcessor( IndexAssigner32(resNrOrg, resNrOrgLock.get(), t, arg1Count + arg2Count  - nrOmittedPoints, nrNewStreets, nrOrgEntityData.begin() ) );

			resNrOrgLock.Commit();
		}
		return true;
	}
	compare_type m_CompareType = compare_type::none;
};

// *****************************************************************************
//									SpatialIndexOper
// *****************************************************************************

// Level      NrQuad  =[4^(Level+1)-1]/3
//     0           1
//     1           5 (Representable by UInt4)
//     2          21
//     3          85 (Representable by (U)Int8)
//     4         341
//     5        1365
//...
//     15 1431655765 = (2^32-1)/3 (Representable by (U)Int32)
//..UInt4 ->  UInt32
//..UInt2 ->  UInt8
//  UInt1 ->  UInt4


template <typename RangeType, typename PointIter>
RangeType GetBounds(PointIter lbFirst, PointIter lbLast, PointIter ubFirst, RangeType bounds)
{
	for (; lbFirst!=lbLast; ++ubFirst, ++lbFirst)
		bounds |= RangeType(*lbFirst, *ubFirst);
	return bounds;
}


template <typename PointType>
UInt32 CalcSpatialIndex(const Range<PointType>& thisRange, Range<PointType> boundingBox, UInt32 level)
{
	dms_assert(IsIncluding(boundingBox, thisRange));
	dms_assert(level <= 15);

	UInt32 result      = 0;
	UInt32 levelWeight = 1;
	for (; level; levelWeight*=4, --level)
	{
		PointType mid = Center(boundingBox);
		UInt32 offset = SpatialIndexImpl::GetQuadrantOffset(1, thisRange, mid); // returns 0 if not in any quadrant
		switch (offset)
		{
			case 0: return result;
			case 1: boundingBox.second.first = mid.first; boundingBox.second.second = mid.second; break;
			case 2: boundingBox.second.first = mid.first; boundingBox.first .second = mid.second; break;
			case 3: boundingBox.first .first = mid.first; boundingBox.second.second = mid.second; break;
			case 4: boundingBox.first .first = mid.first; boundingBox.first .second = mid.second; break;
		}
		result *= 4;
		result += offset;
	}
	return result;
}

#include "OperAttrTer.h"
#include "UnitCreators.h"

template <class T, class LevelType, class QuadIdType>
struct SpatialIndexOper : TernaryAttrOper< QuadIdType, T, T, LevelType>
{
	using PointType = T;
	using RangeType = Range<T>;
	using CoordType = PointType::field_type;

	SpatialIndexOper(AbstrOperGroup* gr)
		: TernaryAttrOper< QuadIdType, T, T, LevelType>(gr, default_unit_creator<QuadIdType>, ValueComposition::Single,	false)
	{}

	void CalcTile(sequence_traits<QuadIdType>::seq_t resData, sequence_traits<T>::cseq_t arg1Data, sequence_traits<T>::cseq_t ubData, sequence_traits<LevelType>::cseq_t levelData, ArgFlags af MG_DEBUG_ALLOCATOR_SRC_ARG) const override
	{
		bool e1Void = af & AF1_ISPARAM;
		bool e2Void = af & AF2_ISPARAM;
		bool e3Void = af & AF3_ISPARAM;

		// TODO: Make Tile aware
		// ISSUE: GetBounds should analyse all tiles to get the boundingBox before indexing can start.
		if (e1Void != e2Void)
			this->GetGroup()->throwOperError("LowerBounds and UpperBounds are required to have the same domain");

		auto
			lbIter = arg1Data.begin()
		,	lbEnd  = arg1Data.end();
		auto ubIter = ubData.begin();

		auto levelIter = levelData.begin();

		UInt32 level; if (e3Void) level = LevelType(*levelIter);

		auto resIter = resData.begin();

		RangeType boundingBox = GetBounds<RangeType>(lbIter, lbEnd, ubIter, RangeType()); // TODO: bring Outside tile stuff by implementing PreCalculate for ternary operators

		for (;lbIter != lbEnd; ++resIter, ++ubIter, ++lbIter)
		{
			if (!e3Void) { level = LevelType(*levelIter); ++levelIter; }
			*resIter = CalcSpatialIndex<PointType>(RangeType(*lbIter, *ubIter), boundingBox, level);
		}
	}
};

// *****************************************************************************
//											INSTANTIATION
// *****************************************************************************


namespace 
{
	template <typename PointType>
	struct ConnectOperators
	{
		ConnectOperators()
			:	spatialIndex4(&cogIndex)
			,	spatialIndex2(&cogIndex)
			,	spatialIndex1(&cogIndex)
			,	connectNPT(false)
			,	connectNPP(true)
			,	cp(false, compare_type::none)
			,	ccp(true, compare_type::none)
			,	cp_eq(false, compare_type::eq)
			,	ccp_eq(true, compare_type::eq)
			,	cp_ne(false, compare_type::ne)
			,	ccp_ne(true, compare_type::ne)
		{}

		FastConnectOperator <PointType, UInt32, compare_type::none, UInt32> fc;
		FastConnectOperator <PointType, UInt32, compare_type::none, UInt32, Float64, true> fcmd64;
		FastConnectOperator <PointType, UInt32, compare_type::none, UInt32, Float64, true, true> fcmdmd64;
		FastConnectOperator <PointType, UInt32, compare_type::none, UInt32, Float32, true> fcmd32;
		FastConnectOperator <PointType, UInt32, compare_type::none, UInt32, Float32, true, true> fcmdmd32;
		FastConnectOperator <PointType, UInt32, compare_type::eq, UInt32> fc_eq;
		FastConnectOperator <PointType, UInt32, compare_type::ne, UInt32> fc_ne;

		ConnectPointOperator<PointType> cp, ccp, cp_eq, ccp_eq, cp_ne, ccp_ne;
		ConnectNeighbourPointOperator<PointType> connectNPT;
		ConnectNeighbourPointOperator<PointType> connectNPP;
		ConnectInfoOperator <PointType, UInt32, compare_type::none, UInt32> ci;
		ConnectInfoOperator <PointType, UInt32, compare_type::none, UInt32, Float64, true> cimd64;
		ConnectInfoOperator <PointType, UInt32, compare_type::none, UInt32, Float64, true, true> cimdmd64;
		ConnectInfoOperator <PointType, UInt32, compare_type::none, UInt32, Float32, true> cimd32;
		ConnectInfoOperator <PointType, UInt32, compare_type::none, UInt32, Float32, true, true> cimdmd32;
		ConnectInfoOperator <PointType, UInt32, compare_type::eq, UInt32> ci_eq;
		ConnectInfoOperator <PointType, UInt32, compare_type::ne, UInt32> ci_ne;
		ConnectInfoOperator <PointType, UInt32, compare_type::none, UInt32, Float64, false, false, true> dc;
		ConnectInfoOperator <PointType, UInt32, compare_type::none, UInt32, Float64, true, false, true> dcmd64;
		ConnectInfoOperator <PointType, UInt32, compare_type::none, UInt32, Float64, true, true, true> dcmdmd64;
		ConnectInfoOperator <PointType, UInt32, compare_type::none, UInt32, Float32, true, false, true> dcmd32;
		ConnectInfoOperator <PointType, UInt32, compare_type::none, UInt32, Float32, true, true, true> dcmdmd32;
		ConnectInfoOperator <PointType, UInt32, compare_type::eq, UInt32, Float64, false, false, true> dc_eq;
		ConnectInfoOperator <PointType, UInt32, compare_type::ne, UInt32, Float64, false, false, true> dc_ne;

		SpatialIndexOper<PointType, UInt4, UInt32>    spatialIndex4;
		SpatialIndexOper<PointType, UInt2, UInt8>     spatialIndex2;
		SpatialIndexOper<PointType, Bool,  UInt4>     spatialIndex1;
	};

	tl_oper::inst_tuple<typelists::seq_points, ConnectOperators<_> > connectOperatorInstances;
}

/******************************************************************************/

