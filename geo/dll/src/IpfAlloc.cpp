// Copyright (C) 1998-2024 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "GeoPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif

#include "DataArray.h"
#include "CheckedDomain.h"
#include "UnitClass.h"
#include "TreeItemClass.h"
#include "utl/mySPrintF.h"

#include "bi_graph.h"
#include "dbg/SeverityType.h"

/*
IPF<S> takes the following arguments:

	suitabilities: for each ggType:
		{
			gridDomain -> Eur/m2: S
		}


	claims: for each ggType:
		{	
			RegioGrid:	gridDomain->RegioSet
			minClaim:	RegioSet->UInt32
			maxClaim:	RegioSet->UInt32 
		}

	free_land: gridDomain -> bool 


results in:
	landuse:	     gridDomain->ggType
	allocResults: for each ggType:
		{
			totalLand:	RegioSet->UInt32
			shadowPrice:RegioSet->S
		}

*/

// *****************************************************************************
//									Factet related funcs
// *****************************************************************************

/*
struct region_bounds
{
	std::vector<Range<UInt32> > m_AtomicRegionBounds;
	std::vector<Range<UInt32> > m_UniqueRegionBounds;

	region_bounds(const regions_info_t& ri)
		:	m_AtomicRegionBounds(ri.m_AtomicRegions.size())
		,	m_UniqueRegionBounds(ri.m_AtomicRegionIdOffsets.size())
	{
		const atomic_region_id* armPtr = ri.m_AtomicRegionMap.begin();
		const atomic_region_id* armEnd = ri.m_AtomicRegionMap.end();
		UInt32 i =0;
		while (armPtr != armEnd)
			m_AtomicRegionBounds[*armPtr++] |= i++;

		UInt32 AR = ri.GetNrAtomicRegions(), r=0;
		for (; r!= AR; ++r)
		{
			UInt32 pi = 0, pe = ri.m_Partitionings.size();
			while (pi != pe)
				m_UniqueRegionBounds[ri.GetUniqueRegionID(r, pi++)] |= m_AtomicRegionBounds[r];
		}
	}
};
*/

// *****************************************************************************
//									BigStep
// *****************************************************************************

/*
typedef htp_info_t<Int32>      htp;
typedef shadow_price<Int32>    s_price;

s_price GetMyPrice(htp& htpInfo, UInt32 i, UInt32 j)
{
	atomic_region_id ar = htpInfo.m_AtomicRegionMap[i];

	return  htpInfo.GetClaim(ar, j).m_ShadowPrice
		+	s_price(
				htpInfo.m_ggTypes[j].m_Suitabilities[i],
				i*j);

}

s_price GetHighestBid(htp& htpInfo, UInt32 i)
{
	atomic_region_id ar = htpInfo.m_AtomicRegionMap[i];

	s_price highestBid = htpInfo.GetClaim(ar, 0).m_ShadowPrice;
		    highestBid.first  += htpInfo.m_ggTypes[0].m_Suitabilities[i];

	UInt32 k = htpInfo.m_ggTypes.size();
	UInt32 c = 0;

	for(UInt32 j=0; j!=k; ++j)
	{
		s_price bid = htpInfo.GetClaim(ar, j).m_ShadowPrice;
			    bid.first  += htpInfo.m_ggTypes[j].m_Suitabilities[i];
			    bid.second += c; c += i; // small pertubation (SoS) for making a difference between similar cells

		if (highestBid < bid)
			highestBid = bid;
	}

	return highestBid;
}

s_price GetNextBid(htp& htpInfo, UInt32 i, UInt32 currJ)
{
	atomic_region_id ar = htpInfo.m_AtomicRegionMap[i];

	s_price highestBid = MIN_VALUE(s_price);

	UInt32 k = htpInfo.m_ggTypes.size();
	UInt32 c = 0;

	for(UInt32 j=0; j!=k; ++j, c+=i) if (j != currJ)
	{
		s_price bid = htpInfo.GetClaim(ar, j).m_ShadowPrice;
			    bid.first  += htpInfo.m_ggTypes[j].m_Suitabilities[i];
			    bid.second += c; // small pertubation (SoS) for making a difference between similar cells
		
		if (highestBid < bid)
			highestBid = bid;
	}

	return highestBid;
}

void InitClaims(htp& htpInfo)
{
	UInt32 c = htpInfo.m_Claims.size();
	for (UInt32 i = 0; i!=c; ++i)
	{
		htpInfo.m_Claims[i].m_Count = 0;
		htpInfo.m_Claims[i].m_ShadowPrice = htpInfo.m_Claims[i].m_NewPrice;
	}
}


void InitalAlloc(htp& htpInfo)
{
	UInt32 n = htpInfo.m_AtomicRegionMap.size();
	UInt32 k = htpInfo.m_ggTypes.size();

	InitClaims(htpInfo);

	dms_assert(htpInfo.m_ResultArray.size() == n);

	UInt32 i=0; // cell index 0..n
	while(i != n)
	{		
		atomic_region_id ar = htpInfo.m_AtomicRegionMap[i];

		UInt32 highestBidder = 0;
		s_price highestBid = htpInfo.GetClaim(ar, 0).m_ShadowPrice;
		        highestBid.first  += htpInfo.m_ggTypes[0].m_Suitabilities[i];

		UInt32 c = i;

		for(UInt32 j=1; j!=k; ++j)
		{
			s_price bid = htpInfo.GetClaim(ar, j).m_ShadowPrice;
			        bid.first  += htpInfo.m_ggTypes[j].m_Suitabilities[i];
			        bid.second += c; c += i; // small pertubation (SoS) for making a difference between similar cells

			if (highestBid < bid)
			{
				highestBid    = bid;
				highestBidder = j;
			}
		}

		// Update total and move if facing claim-restriction
		htpInfo.m_ResultArray[i] = highestBidder;
		htpInfo.GetClaim(ar, highestBidder).m_Count++;

		++i;
	}
}
*/
// *****************************************************************************
//									Solve
// *****************************************************************************

/*

template <typename S>
UInt32 Solve(htp_info_t<S>& htpInfo)
{
	DBG_START("IpfAlloc", "Solve", true);

	region_bounds bounds(htpInfo);

	std::vector<s_price> shadowPriceBuffer;

	const int MAX_ITER = 50;

	for (UInt32 iter=0; iter!=MAX_ITER; ++iter)
	{
		reportF(ST_MajorTrace, "DiscreteAlloc iter %d/%d", iter, MAX_ITER);

		InitalAlloc(htpInfo); // recount every time just to be sure

		UInt32 C = htpInfo.m_Claims.size();

		UInt32 totalClaimDeviation = 0;
		for (UInt32 c = 0; c!=C; ++c)
		{
			claim<Int32>& claimInfo = htpInfo.m_Claims[c];

			UInt32  currQ = claimInfo.m_Count;

			if (currQ < claimInfo.m_ClaimRange.first)
				totalClaimDeviation += (claimInfo.m_ClaimRange.first - currQ);
			if (currQ > claimInfo.m_ClaimRange.second)
				totalClaimDeviation += (currQ - claimInfo.m_ClaimRange.second);
		}

		reportF(ST_MajorTrace, "DiscreteAlloc deviation %d", totalClaimDeviation);

		if (iter == MAX_ITER)
			return totalClaimDeviation;


		for (c = 0; c!=C; ++c)
		{
			claim<Int32>& claimInfo = htpInfo.m_Claims[c];
			claimInfo.m_NewPrice = claimInfo.m_ShadowPrice;

			bool mustGrow   = claimInfo.m_Count < claimInfo.m_ClaimRange.first
				|| claimInfo.m_Count < claimInfo.m_ClaimRange.second 
				&& claimInfo.m_ShadowPrice < s_price();

			bool mustShrink = claimInfo.m_Count > claimInfo.m_ClaimRange.second
				|| claimInfo.m_Count > claimInfo.m_ClaimRange.first 
				&& claimInfo.m_ShadowPrice > s_price();

			if (!(mustGrow || mustShrink))
				continue;
			dms_assert(!(mustGrow && mustShrink));

			UInt32                j         = claimInfo.m_ggTypeID;
			ggType_info_t<Int32>& ggTypeInfo= htpInfo.m_ggTypes[j];
			UInt32                partID    = ggTypeInfo.m_PartitioningID;
			UInt32                regID     = claimInfo.m_RegionID;
			partitioning_info_t&    pi        = htpInfo.m_Partitionings[partID];

			Range<UInt32> rRange = bounds.m_UniqueRegionBounds[pi.m_UniqueRegionOffset + regID];

			shadowPriceBuffer.clear();

			if (mustGrow)
			{
				dms_assert(rRange.first <= rRange.second);
				for (UInt32 i=rRange.first; i != rRange.second; ++i)
				{
					atomic_region_id ar = htpInfo.m_AtomicRegionMap[i];
					if ((htpInfo.m_RegionIdPtrs[ar])[partID] == regID)
					{
						if (htpInfo.m_ResultArray[i] != j)
						{
							shadowPriceBuffer.push_back(GetHighestBid(htpInfo, i) - GetMyPrice(htpInfo, i, j) );
							dms_assert(shadowPriceBuffer.back() >= s_price());
						}
					}
				}
				std::sort(shadowPriceBuffer.begin(), shadowPriceBuffer.end());
				s_price* ptr = shadowPriceBuffer.begin();
				s_price* end = shadowPriceBuffer.end();
				if (ptr == end) continue;

				UInt32  currQ = claimInfo.m_Count;
				s_price currP = claimInfo.m_ShadowPrice;

				if (currQ < claimInfo.m_ClaimRange.first)
				{
					UInt32 buyAnyway = Min(claimInfo.m_ClaimRange.first - currQ, UInt32(end - ptr) );
					ptr   += buyAnyway;
					currQ += buyAnyway;
				}
				while (currQ < claimInfo.m_ClaimRange.second
					&& ptr != end 
					&& *ptr + currP < s_price()
				)
				{
					ptr++;
					currQ++;
				}
				if(ptr == end) --ptr;
				claimInfo.m_NewPrice += *ptr;
			}
			else
			{
				dms_assert(mustShrink);

				for (UInt32 i=rRange.first; i != rRange.second; ++i)
				{
					atomic_region_id ar = htpInfo.m_AtomicRegionMap[i];
					if ((htpInfo.m_RegionIdPtrs[ar])[partID] == regID)
					{
						if (htpInfo.m_ResultArray[i] == j)
						{
							shadowPriceBuffer.push_back(GetMyPrice(htpInfo, i, j) - GetNextBid(htpInfo, i,j));
							dms_assert(shadowPriceBuffer.back() >= s_price());
						}
					}
				}
				std::sort(shadowPriceBuffer.begin(), shadowPriceBuffer.end());
				s_price* ptr = shadowPriceBuffer.begin();
				s_price* end = shadowPriceBuffer.end();
				if (ptr == end) continue;

				UInt32  currQ = claimInfo.m_Count;
				s_price currP = claimInfo.m_ShadowPrice;

				if (currQ > claimInfo.m_ClaimRange.second)
				{
					UInt32 buyAnyway = Min(currQ - claimInfo.m_ClaimRange.second, UInt32(end - ptr) );
					ptr   += buyAnyway;
					currQ -= buyAnyway;
				}
				while (currQ > claimInfo.m_ClaimRange.first
					&& ptr != end 
					&& currP > *ptr
				)
				{
					ptr++;
					currQ--;
				}
				if(ptr == end) --ptr;
				claimInfo.m_NewPrice -= *ptr;
			}
		}
	}
	return true;
}
*/

// *****************************************************************************
//									HitchcockTransportation operator
// *****************************************************************************

template <class S>
class IterativeProportionalFittingOperator : public DecimalOperator
{
	typedef DataArray<SharedStr> Arg1Type;          // ggTypes->name
	typedef AbstrUnit          Arg2Type;          // domain of cells
//	typedef TreeItem           Arg3Type;          // container with columns with suitability maps
	typedef DataArray<S>       PriceType;         // suitability map type
	typedef DataArray<UInt8>   Arg4Type;          // ggType -> partitioning
	typedef DataArray<SharedStr>  Arg5Type;          // partitions->name
	typedef Unit<UInt16>       Arg6Type;          // atomic regions container
	typedef DataArray<UInt32>  ClaimType;         // Arg6 & 7 are containers with min resp. max claims; 
	typedef DataArray<S>       Arg9Threshold;     // Cutoff threshold
	                                              // arg9 is feasibility certificate

	typedef TreeItem           ResultType;        // container with landuse and allocResults
	typedef DataArray<UInt8>   ResultLandUseType; // columnIndex per row
	typedef ClaimType          ResultTotalType;   
	typedef PriceType          ResultShadowPriceType; 

public:
	IterativeProportionalFittingOperator(AbstrOperGroup* gr)
		:	DecimalOperator(gr, ResultType::GetStaticClass(), 
				Arg1Type::GetStaticClass(), // ggTypes->name array
				Arg2Type::GetStaticClass(), // gridDomain
				TreeItem::GetStaticClass(), // suitability maps
				Arg4Type::GetStaticClass(), // ggTypes->partitions
				Arg5Type::GetStaticClass(), // partitions->name
				Arg6Type::GetStaticClass(), // atomicRegions container
				TreeItem::GetStaticClass(), // minClaim container
				TreeItem::GetStaticClass(), // maxClaimContainer
				Arg9Threshold::GetStaticClass(),  // cutoff threshold
				TreeItem::GetStaticClass() // feasibility certificate
			) 
	{}

	// Override Operator
	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		dms_assert(args.size() == 10);

		const AbstrDataItem* ggTypeNamesA = debug_cast<const AbstrDataItem*>(args[0]);
		dms_assert(ggTypeNamesA);

		const Unit<UInt8>* ggTypeSet = checked_domain<UInt8>(args[0], "a1");

		const AbstrUnit* gridDomain = debug_cast<const AbstrUnit*>(args[1]);

		UInt32 nrTypes = ggTypeSet->GetCount();
		MG_CHECK(nrTypes < 256);

		const AbstrDataItem* ggTypes2partitioningsA = debug_cast<const AbstrDataItem*>(args[3]);
		dms_assert(ggTypes2partitioningsA);

		const AbstrDataItem* regionNamesA = debug_cast<const AbstrDataItem*>(args[4]);
		dms_assert(regionNamesA);

		const AbstrUnit* atomicRegionUnit = debug_cast<const Arg6Type*>(args[5]);
		dms_assert(atomicRegionUnit);

		const AbstrDataItem* atomicRegionMapA = AsCertainDataItem( atomicRegionUnit->GetConstSubTreeItemByID(GetTokenID_mt("UnionData")) );

		if (!resultHolder)
			resultHolder = TreeItem::CreateCacheRoot();
		dbg_assert(resultHolder);

		AbstrDataItem* resLanduse = 
			CreateDataItem(
				resultHolder.GetNew(), 
				GetTokenID_mt("landuse"), 
				gridDomain, ggTypeSet
			);

		AbstrDataItem* resStatus = 
			CreateDataItem(
				resultHolder.GetNew()
			,	GetTokenID_mt("status")
			,	Unit<Void>  ::GetStaticClass()->CreateDefault() 
			,	Unit<SharedStr>::GetStaticClass()->CreateDefault()
			);


		TreeItem* resShadowPriceContainer = 
			resultHolder.GetNew()->CreateItem(GetTokenID_mt("shadow_prices"));

		TreeItem* resTotalAllocatedContainer = 
			resultHolder.GetNew()->CreateItem(GetTokenID_mt("total_allocated"));

//		htp_info_t<S> htpInfo;
		SharedStr strStatus;

		// make AtomicRegionsSet and UniqeRegions -> (AtomicRegionsSet -> Region)
/*
		CreateResultingItems(
				ggTypeNamesA, 
				gridDomain,
				args[2],       // suitabilitySet
				args[6],       // minClaimSet
				args[7],       // maxClaimSet
				ggTypes2partitioningsA, regionNamesA, atomicRegionUnit, // RegionGrids
				resShadowPriceContainer,
				resTotalAllocatedContainer,
				htpInfo
		);
*/
		if (!mustCalc)
			return true;

		reportD(SeverityTypeID::ST_MajorTrace, "IpfAlloc: Prepare started");
/*
		if (!Prepare(
			gridDomain, atomicRegionUnit, atomicRegionMapA,
			htpInfo,
			strStatus)
		)
		{
			reportD(ST_MajorTrace, "IpfAlloc: Prepare suspended");
			return false;
		}
*/
		reportD(SeverityTypeID::ST_MajorTrace, "IpfAlloc: Prepare completed with status ", strStatus.c_str());

		bool isFeasible = (strStatus == "OK");

		reportD(SeverityTypeID::ST_MajorTrace, "IpfAlloc: Solve started");

		DataWriteLock lockResLanduse(resLanduse, dms_rw_mode::write_only_mustzero);

		if (isFeasible)
		{
			S threshold = GetTheCurrValue<S>(args[6]);

//			htpInfo.m_ResultArray = debug_cast<DataArray<UInt8>*>(resLanduse->GetDataObj())->GetDataWrite();

//			UInt32 remainingDev = Solve(htpInfo);

//			strStatus = mySPrintF("IpfAlloc completed with %d cells outside claims", remainingDev);
		}

//		StoreLanduseTypeInfo(htpInfo); // from header file

		SetTheValue<SharedStr>(resStatus, strStatus);

		lockResLanduse.Commit();

		return true;
	}
};

// *****************************************************************************
//											INSTANTIATION
// *****************************************************************************

namespace 
{
	oper_arg_policy ipf_oap[10] = { 
		oper_arg_policy::calc_always,         // ggTypeNameArray
		oper_arg_policy::calc_as_result,      // domainUnit
		oper_arg_policy::subst_with_subitems, // suitabilities
		oper_arg_policy::calc_always,         // ggType -> partitioning relation
		oper_arg_policy::calc_always,         // partitioningNameArray
		oper_arg_policy::subst_with_subitems, // atomicRegions met regioRefs & UnionData
		oper_arg_policy::subst_with_subitems, // minClaims
		oper_arg_policy::subst_with_subitems, // maxClaims
		oper_arg_policy::calc_as_result,      // cutoff value
		oper_arg_policy::subst_with_subitems, // feasibilityCertificate
	};
	
	SpecialOperGroup ipfGroup("ipf_alloc", 10, ipf_oap);
	IterativeProportionalFittingOperator<Int32> ipf(&ipfGroup);

	CharPtr rewriteObsoleteWarning = "claim correction related rewrite rules have been removed from RewriteExpr.lsp";

	Obsolete< CommonOperGroup > whatever[] =
	{
		Obsolete< CommonOperGroup >(rewriteObsoleteWarning, "claim_div", oper_policy::better_not_in_meta_scripting | oper_policy::depreciated),
//		Obsolete< CommonOperGroup >(rewriteObsoleteWarning, "claim_divF32", oper_policy::better_not_in_meta_scripting | oper_policy::depreciated),
		Obsolete< CommonOperGroup >(rewriteObsoleteWarning, "claim_divF64", oper_policy::better_not_in_meta_scripting | oper_policy::depreciated),
		Obsolete< CommonOperGroup >(rewriteObsoleteWarning, "claim_divF32D", oper_policy::better_not_in_meta_scripting | oper_policy::depreciated),
		Obsolete< CommonOperGroup >(rewriteObsoleteWarning, "claim_corr", oper_policy::better_not_in_meta_scripting | oper_policy::depreciated),
//		Obsolete< CommonOperGroup >(rewriteObsoleteWarning, "claim_corrF32", oper_policy::better_not_in_meta_scripting | oper_policy::depreciated),
		Obsolete< CommonOperGroup >(rewriteObsoleteWarning, "claim_corrF32D", oper_policy::better_not_in_meta_scripting | oper_policy::depreciated),
		Obsolete< CommonOperGroup >(rewriteObsoleteWarning, "claim_corrF32DL", oper_policy::better_not_in_meta_scripting | oper_policy::depreciated),

		Obsolete< CommonOperGroup >(rewriteObsoleteWarning, "claim_minmax_corrF32", oper_policy::better_not_in_meta_scripting | oper_policy::depreciated),
		Obsolete< CommonOperGroup >(rewriteObsoleteWarning, "claim_minmax_corrF64", oper_policy::better_not_in_meta_scripting | oper_policy::depreciated),

		Obsolete< CommonOperGroup >(rewriteObsoleteWarning, "claim_minmax_corrF32D", oper_policy::better_not_in_meta_scripting | oper_policy::depreciated),
		Obsolete< CommonOperGroup >(rewriteObsoleteWarning, "claim_minmax_corrF32L", oper_policy::better_not_in_meta_scripting | oper_policy::depreciated)
	};
}

/******************************************************************************/
