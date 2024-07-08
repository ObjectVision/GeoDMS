// Copyright (C) 1998-2024 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "GeoPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif

#include "mci/CompositeCast.h"
#include "geo/PointOrder.h"
#include "geo/SpatialIndex.h"
#include "ptr/LifetimeProtector.h"

#include "DataController.h"
#include "TreeItemClass.h"
#include "Unit.h"
#include "UnitClass.h"

#include "UnitCreators.h"


namespace {
	CommonOperGroup cogCAN("canyon", oper_policy::better_not_in_meta_scripting);
}

template <typename PointType>
PointType complexmul(const PointType& a, const PointType& b)
{
	return shp2dms_order(PointType(a.Col() * b.Col() - a.Row()* b.Row(), a.Col()* b.Row()+ a.Row()*b.Col()));
}

static TokenID s_RLoc = GetTokenID_st("RLoc");
static TokenID s_RRC = GetTokenID_st("RRC");
static TokenID s_LLoc = GetTokenID_st("LLoc");
static TokenID s_LRC = GetTokenID_st("LRC");

template <typename T>
class CanyonOperator : public NonaryOperator
{
	using PointType = T;
	using scalar_type = scalar_of_t<T>;

	typedef Range<PointType>                   RangeType;
	typedef std::vector<PointType>             PolyType;
	typedef Int16                              HeightType;
	typedef typename PointType::field_type     CoordType;
	typedef typename acc_type<CoordType>::type SqrDistType;

	typedef DataArray<PointType>  Arg1Type; // CalcPoint -> Loc
	typedef DataArray<HeightType> Arg2Type; // CalcPoint -> Height
	typedef DataArray<UInt32>     Arg3Type; // CalcPoint -> Segment
	typedef DataArray<PointType>  Arg4Type; // Segment   -> Loc
	typedef DataArray<PointType>  Arg5Type; // Segment   -> Loc
	typedef DataArray<PolyType>   Arg6Type; // Building  -> *VertexLoc
	typedef DataArray<HeightType> Arg7Type; // Building  -> Height
	typedef DataArray<HeightType> Arg8Type; // MinHeight
	typedef DataArray<CoordType>  Arg9Type; // MaxDistance

	typedef TreeItem ResType;
		typedef DataArray<PointType> Res1Type; // LeftLoc:  Segment -> Loc
		typedef DataArray<CoordType> Res2Type; // RightLoc: Segment -> Loc
		typedef DataArray<PointType> Res3Type; // LeftRC:   Segment -> (Height / Dist)
		typedef DataArray<CoordType> Res4Type; // RightRC:  Segment -> (Height / Dist)

	typedef SpatialIndex<CoordType, typename Arg6Type::const_iterator> SpatialIndexType;

public:
	CanyonOperator()
		: NonaryOperator(&cogCAN, ResType::GetStaticClass(),
				Arg1Type::GetStaticClass()
			,	Arg2Type::GetStaticClass()
			,	Arg3Type::GetStaticClass()
			,	Arg4Type::GetStaticClass()
			,	Arg5Type::GetStaticClass()
			,	Arg6Type::GetStaticClass()
			,	Arg7Type::GetStaticClass()
			,	Arg8Type::GetStaticClass()
			,	Arg9Type::GetStaticClass()
			)
	{}

	// Override Operator
	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
//XXX arg1 en arg2 omdraaien
		dms_assert(args.size() == 9);

		const AbstrDataItem* arg1A = debug_valcast<const AbstrDataItem*>(args[0]);
		const AbstrDataItem* arg2A = debug_valcast<const AbstrDataItem*>(args[1]);
		const AbstrDataItem* arg3A = debug_valcast<const AbstrDataItem*>(args[2]);
		const AbstrDataItem* arg4A = debug_valcast<const AbstrDataItem*>(args[3]);
		const AbstrDataItem* arg5A = debug_valcast<const AbstrDataItem*>(args[4]);
		const AbstrDataItem* arg6A = debug_valcast<const AbstrDataItem*>(args[5]);
		const AbstrDataItem* arg7A = debug_valcast<const AbstrDataItem*>(args[6]);
		const AbstrDataItem* arg8A = debug_valcast<const AbstrDataItem*>(args[7]);
		const AbstrDataItem* arg9A = debug_valcast<const AbstrDataItem*>(args[8]);

		const AbstrUnit* calcPointEntity = arg1A->GetAbstrDomainUnit();
		const AbstrUnit* pointUnit       = arg1A->GetAbstrValuesUnit();
		const AbstrUnit* heightUnit      = arg2A->GetAbstrValuesUnit();
		const AbstrUnit* segmentEntity   = arg3A->GetAbstrValuesUnit();
		const AbstrUnit* buildingEntity  = arg6A->GetAbstrDomainUnit();
		const AbstrUnit* distUnit        = arg9A->GetAbstrValuesUnit();

		calcPointEntity->UnifyDomain(arg2A->GetAbstrDomainUnit(), "e1", "e2", UM_Throw);
		calcPointEntity->UnifyDomain(arg3A->GetAbstrDomainUnit(), "e1", "e3", UM_Throw);
		segmentEntity  ->UnifyDomain(arg4A->GetAbstrDomainUnit(), "v3", "e4", UM_Throw);
		segmentEntity  ->UnifyDomain(arg5A->GetAbstrDomainUnit(), "v3", "e5", UM_Throw);
		buildingEntity ->UnifyDomain(arg7A->GetAbstrDomainUnit(), "e6", "e7", UM_Throw);

		Unit<Void>::GetStaticClass()->CreateDefault()->UnifyDomain(arg8A->GetAbstrDomainUnit(), "Unit<Void>", "e8", UM_Throw);
		Unit<Void>::GetStaticClass()->CreateDefault()->UnifyDomain(arg9A->GetAbstrDomainUnit(), "Unit<Void>", "e9", UM_Throw);

		pointUnit ->UnifyValues(arg4A->GetAbstrValuesUnit(), "v1", "v4", UM_Throw);
		pointUnit ->UnifyValues(arg5A->GetAbstrValuesUnit(), "v1", "v5", UM_Throw);
		pointUnit ->UnifyValues(arg6A->GetAbstrValuesUnit(), "v1", "v6", UM_Throw);
		heightUnit->UnifyValues(arg7A->GetAbstrValuesUnit(), "v2", "v7", UM_Throw);
		heightUnit->UnifyValues(arg8A->GetAbstrValuesUnit(), "v2", "v8", UM_Throw);

		if (!resultHolder)
		{
			resultHolder = TreeItem::CreateCacheRoot();

			ArgRefs tmpArgs; tmpArgs.reserve(2);
			tmpArgs.emplace_back(std::in_place_type<SharedTreeItem>, heightUnit);
			tmpArgs.emplace_back(std::in_place_type<SharedTreeItem>, distUnit);

			LifetimeProtector<TreeItemDualRef> resultRef;
			resultRef->MarkTS(resultHolder.GetLastChangeTS());

			cog_div.FindOperByArgs(tmpArgs)->CreateResultCaller(*resultRef, tmpArgs, nullptr);
			ConstUnitRef rcUnit = debug_cast<const AbstrUnit*>(resultRef->GetOld()); dms_assert(rcUnit);

			CreateDataItem(resultHolder, s_RLoc, calcPointEntity, pointUnit);
			CreateDataItem(resultHolder, s_RRC,  calcPointEntity, rcUnit);
			CreateDataItem(resultHolder, s_LLoc, calcPointEntity, pointUnit);
			CreateDataItem(resultHolder, s_LRC,  calcPointEntity, rcUnit);
		}
		if (mustCalc)
		{
			DataReadLock arg1Lock(debug_valcast<const AbstrDataItem*>(args[0]));
			DataReadLock arg2Lock(debug_valcast<const AbstrDataItem*>(args[1]));
			DataReadLock arg3Lock(debug_valcast<const AbstrDataItem*>(args[2]));
			DataReadLock arg4Lock(debug_valcast<const AbstrDataItem*>(args[3]));
			DataReadLock arg5Lock(debug_valcast<const AbstrDataItem*>(args[4]));
			DataReadLock arg6Lock(debug_valcast<const AbstrDataItem*>(args[5]));
			DataReadLock arg7Lock(debug_valcast<const AbstrDataItem*>(args[6]));
			DataReadLock arg8Lock(debug_valcast<const AbstrDataItem*>(args[7]));
			DataReadLock arg9Lock(debug_valcast<const AbstrDataItem*>(args[8]));

			const Arg1Type* arg1 = const_array_cast<PointType >(arg1A);
			const Arg2Type* arg2 = const_array_cast<HeightType>(arg2A);
			const Arg3Type* arg3 = const_array_cast<UInt32    >(arg3A);
			const Arg4Type* arg4 = const_array_cast<PointType >(arg4A);
			const Arg5Type* arg5 = const_array_cast<PointType >(arg5A);
			const Arg6Type* arg6 = const_array_cast<PolyType  >(arg6A);
			const Arg7Type* arg7 = const_array_cast<HeightType>(arg7A);
			const Arg8Type* arg8 = const_array_cast<HeightType>(arg8A);
			const Arg9Type* arg9 = const_array_cast<CoordType >(arg9A);

			dms_assert(arg1 && arg2 && arg3 && arg4 && arg5 && arg6 && arg7 && arg8 && arg9);

			UInt32 nrCalcPoints = calcPointEntity->GetCount();
			UInt32 nrSegments   = segmentEntity  ->GetCount();
			UInt32 nrBuildings  = buildingEntity  ->GetCount();

			auto arg1Data = arg1->GetDataRead();
			auto arg2Data = arg2->GetDataRead();

			dms_assert(nrCalcPoints == arg1Data.size());
			dms_assert(nrCalcPoints == arg2->GetDataRead().size());
			dms_assert(nrBuildings  == arg6->GetDataRead().size());
			dms_assert(nrBuildings  == arg7->GetDataRead().size());

			HeightType minHeight = arg8->GetDataRead()[0];
			CoordType  maxDist   = arg9->GetDataRead()[0];

			AbstrDataItem* res1 = AsDataItem(resultHolder.GetNew()->GetSubTreeItemByID(s_RLoc));
			AbstrDataItem* res2 = AsDataItem(resultHolder.GetNew()->GetSubTreeItemByID(s_RRC ));
			AbstrDataItem* res3 = AsDataItem(resultHolder.GetNew()->GetSubTreeItemByID(s_LLoc));
			AbstrDataItem* res4 = AsDataItem(resultHolder.GetNew()->GetSubTreeItemByID(s_LRC ));

			DataWriteLock res1Lock(res1);
			DataWriteLock res2Lock(res2);
			DataWriteLock res3Lock(res3);
			DataWriteLock res4Lock(res4);

			if (nrCalcPoints && nrBuildings)
			{
				auto r1 = mutable_array_cast<PointType>(res1Lock)->GetDataWrite(); auto ri1 = r1.begin();
				auto r2 = mutable_array_cast<CoordType>(res2Lock)->GetDataWrite(); auto ri2 = r2.begin();
				auto r3 = mutable_array_cast<PointType>(res3Lock)->GetDataWrite(); auto ri3 = r3.begin();
				auto r4 = mutable_array_cast<CoordType>(res4Lock)->GetDataWrite(); auto ri4 = r4.begin();

				auto buildingPoints = arg6->GetDataRead(); auto buildingPointsBegin = buildingPoints.begin();
				auto buildingHoogte = arg7->GetDataRead(); auto buildingHoogteBegin = buildingHoogte.begin();

				SpatialIndexType spIndex(buildingPointsBegin, arg6->GetDataRead().end(), 0);

				auto
					pointPtr = arg1Data.begin(),
					pointEnd = arg1Data.end();
				auto
					hoogtePtr = arg2->GetDataRead().begin();
				auto
					segmentIdPtr = arg3->GetDataRead().begin();
				auto
					segmentBegin = arg4->GetDataRead().begin();
				auto
					segmentEnd   = arg5->GetDataRead().begin();

				UInt32 dbgCalcPointCounter = 0;
				for (;pointPtr != pointEnd; ++dbgCalcPointCounter, ++ri1, ++ri2, ++ri3, ++ri4, ++segmentIdPtr, ++hoogtePtr, ++pointPtr)
				{
					MakeUndefined(*ri1);
					MakeUndefined(*ri3);
					
					*ri2 = *ri4 = 0;

					if (!IsDefined(*pointPtr))
						continue;

					HeightType hoogte = *hoogtePtr;
					UInt32     segmentId = *segmentIdPtr;
					PointType segmVec  = segmentEnd  [segmentId] - segmentBegin[segmentId];
					PointType diagVec  = shp2dms_order<scalar_type>(-segmVec.Row(),  segmVec.Col()); // sight-axis is perpendicular to street direction
					PointType contrVec = shp2dms_order<scalar_type>( diagVec.Col(), -diagVec.Row()); // sort of complex conjugate to reverse rotatie building and sight-axis to to x-axis
					SqrDistType norm = Norm<SqrDistType>(segmVec);
					dms_assert(norm >= 0);
					if (!norm)
						continue;
					Float64     sqrtNorm = sqrt(Float64(norm));
					dms_assert(sqrtNorm >= 0);

					for (auto iter = spIndex.begin(Inflate(*pointPtr, PointType(maxDist, maxDist))); iter; ++iter)
					{
						typename Arg6Type::const_iterator buildingPtr = (*iter)->get_ptr();
						UInt32 buildingNr = buildingPtr - buildingPointsBegin;
						Float64 hoogteDiff = Int64(buildingHoogteBegin[buildingNr]) - *hoogtePtr;
						if (hoogteDiff < minHeight)
							continue;
						if (buildingPtr->size() < 2)
							continue;

						auto polyBegin = buildingPtr->begin();
						auto polyEnd   = buildingPtr->end  ();
						bool c = false;   // count #segments right of point

						PointType prevPoint = complexmul(polyEnd[-1] - *pointPtr, contrVec); // use *pointPtr as base for numerical stability

						for (; polyBegin != polyEnd; ++polyBegin)
						{
							PointType thisPoint = complexmul(*polyBegin - *pointPtr, contrVec); // use base again

							if	( (0 <  thisPoint.Row() ) != (0 < prevPoint.Row() ) ) // is this line crossing x-axis ??
							{
								/*		Afleiding van conditie dat snijpunt (P,T) met x-as strict rechts van oorsprong ligt
										0 < TX - TY / RC[T->P]
										0 < TX - TY * (PX-TX) / (PY-TY)
										0 < [TX * (PY-TY)- TY * (PX-TX)] / (PY-TY) // bring TX into quotient
										0 < [TX*PY- TY*PX] / (PY-TY)               // cancel TX*TY
										0 < OutProduct(T,P) / (PY-TY)              // rewrite using OutProduct
										0 < OutProduct(T,P) * (PY-TY)              // rewrite assuming PY-TY != 0, thus (PY-TY)^2 > 0
								*/
								dms_assert(thisPoint.Row() != prevPoint.Row() ); // follows from if condition

								SqrDistType outProd = OutProduct<SqrDistType>(thisPoint, prevPoint);
								Float64 dist = outProd / (thisPoint.Row() - prevPoint.Row());
								Float64 scaledDist = dist / sqrtNorm;
								if (dist > 0)
									c = !c;
								else
									scaledDist = - scaledDist;
								dms_assert(scaledDist >= 0);
								if (scaledDist <= hoogteDiff / (MaxValue<CoordType>() / 2))
								{
									c = true;
									break;
								}
								if (scaledDist > maxDist)
									goto nextBuildingPoint;
								CoordType rc = hoogteDiff / scaledDist;
								dms_assert(rc >= 0);
								if (dist >  0) 
								{
									if (rc >  *ri2)
									{
										*ri2 = rc;
										*ri1 = *pointPtr - complexmul(diagVec, shp2dms_order<scalar_type>(dist / norm, 0));
									}
								}
								else
								{
									if (rc >  *ri4)
									{
										*ri4 = rc;
										*ri3= *pointPtr - complexmul(diagVec, shp2dms_order<scalar_type>(dist / norm, 0.0));
									}
								}
							}
						nextBuildingPoint:
							prevPoint = thisPoint;
						}
						if (c)
						{
							*ri1 = *pointPtr;
							*ri2 = MaxValue<CoordType>();
							*ri3 = *pointPtr;
							*ri4 = MaxValue<CoordType>();
							break; // goto nextCalcPoint;
						}
					}
				}
			}
			res1Lock.Commit();
			res2Lock.Commit();
			res3Lock.Commit();
			res4Lock.Commit();
			resultHolder.GetNew()->SetIsInstantiated();
		}
		return true;
	}
};


// *****************************************************************************
//											INSTANTIATION
// *****************************************************************************

#include "RtcTypeLists.h"
#include "utl/TypeListOper.h"

namespace 
{
	tl_oper::inst_tuple_templ<typelists::seq_signed_points, CanyonOperator > canyonOperators;
}

/******************************************************************************/

