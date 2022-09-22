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

#include "dbg/Debug.h"
#include "dbg/SeverityType.h"
#include "geo/Pair.h"
#include "mci/CompositeCast.h"
#include "mth/MathLib.h"
#include "ptr/OwningPtrSizedArray.h"
#include "ser/PairStream.h"
#include "utl/mySPrintF.h"
#include "xml/XmlTreeOut.h"

#include "CheckedDomain.h"
#include "DataArray.h"
#include "DataLockContainers.h"
#include "DisplayValue.h"
#include "OperationContext.h"
#include "TreeItemClass.h"
#include "UnitClass.h"

#include "bi_graph.h"
#include "PCount.h"

/*
HT<S> takes the following arguments:

	suitabilities: for each ggType:
		{
			allocUnit -> Eur/m2: S
		}


	claims: for each ggType:
		{	
			RegioGrid:	allocUnit->RegioSet
			minClaim:	RegioSet->UInt32
			maxClaim:	RegioSet->UInt32 
		}

	free_land: allocUnit -> bool 


results in:
	landuse:	     allocUnit->ggType
	allocResults: for each ggType:
		{
			totalLand:	RegioSet->UInt32
			shadowPrice:RegioSet->S
		}

*/
/* 
	SMALL PERTUBATIONS
	In order to make exact allocation possible, equal suitabilities are virtually pertubated.
	It should never be the case that for any two cells i,l and types j,k,
	the distance from points Si and Sl to the facet (j,k) is equal
	or formally:

	(R1):

		(Sij + Cj) - (Sik + Ck) <> (Slj + Cj) - (Slk + Ck)
	or	(Sij - Sik) + (Cj - Ck) <> (Slj - Slk) + ( Cj - Ck)
	or	(Sij - Sik)             <> (Slj - Slk)

	unless i==l OR j==k.


	This is achieved by applying symbolic pertubations to the cost values and
	require some administration which points are compared. 

	Sij(epsilon) := Sij + epsilon*i*j

	Thus Sij(epsilon) <> Skl(epsilon) for i<>k XOR j<>l
	since Sij == Skl implies Sij(epsilon) - Skl(epsilon) == epsilon*(ij - kl)

	and (Sij(epsilon) - Sik(epsilon)) - (Slj(epsilon) - Slk(epsilon))
	== epsilon * [(ij - ik) - (lj - lk)]
	== epsilon * [(i-l)(j-k)],
	which fullfills requirement (R1).

	The sufficiency of (R1) and thus the fact that degeneracies such as Sij == Slk for i<>l AND j<>k doesn't matter
	follows from close analysis of the used operators:
		
	- We take and count maxima per cell i of (Sij + Cj) over j.
	- We keep a queue of cells i for each communicating (j,k) facet, strictly ordered by (Sij(epsilon) - Sik(epsilon)), small values have priority
	- We update Cj(epsilon) := Ck(epsilon) - (Sij(epsilon) - Sik(epsilon)) using facet (j,k)
	  =>        Cj.first    := Ck.first    - heap(j,k).top.first
	  =>        Cj.second   := Ck.second   - epsilon*i*(j-k)


	See:
	Edelsbrunner, H. And Mcke, E. Simulation of simplicity: a technique to cope with degenerate cases in geometric algorithms, 4th Annual ACM Symposium on Computational Geometry (1988) 118-133.
*/

#define EPSILON(x) (x)

const bool DMS_DEBUG_DISCRALLOC = MG_DEBUGCODE( true ||) false;

// *****************************************************************************
//									Helping structures
// *****************************************************************************

typedef UInt32             atomic_region_count_t;
typedef UInt32             claim_type;  // will be varied
typedef Couple<claim_type> claim_range; // allow min > max in this data-type
typedef UInt32             claim_id;    // set of all claims, a (subset of a) partitioning of #AR * #AT
typedef UInt32             facet_id;    // set of claim substitution possibilities
typedef UInt32             facet_code;  // set of #AR * k * k, which is renumbered by m_FacetIDs to facet_id

typedef Int32  perturbation_type; // Simulation of Simplicity, see Edelsbrunner, 1990
typedef claim_type land_unit_id; // representation of number of units allocated to one class; must correspond with claim_type

const land_unit_id NR_BELOW_THRESHOLD_NOTIFICATIONS = 5;

template <typename S, typename P = perturbation_type>
struct shadow_price : Pair<S, P> 
{
	shadow_price(S s, P p) : Pair<S, P>(s,p) {}
	shadow_price(const Pair<S, P>& src) : Pair<S, P>(src) {}
	shadow_price() {}
};

template <typename S> struct minmax_traits<shadow_price<S> > : minmax_traits< Pair<S, perturbation_type> > {};

template <typename S>
struct claim 
{
	claim(UInt32 g, UInt32 r, const claim_range& claimRange)
		:	m_ggTypeID( g )
		,	m_RegionID( r)
		,	m_ClaimRange(claimRange)
		,	m_Count(0)
		,	m_FirstOutHeapID( UNDEFINED_VALUE(UInt32) )
		,	m_FirstInpHeapID( UNDEFINED_VALUE(UInt32) )
	{}

	UInt32           m_ggTypeID, m_RegionID;
	claim_range      m_ClaimRange;
	claim_type       m_Count;
	shadow_price<S>  m_ShadowPrice, m_StartPrice;
	facet_id         m_FirstOutHeapID; // singly linked list through array (ptrs are invalidated during growing construction)
	facet_id         m_FirstInpHeapID; // singly linked list through array (ptrs are invalidated during growing construction)
	bool Overflow ()  const { return m_Count >  m_ClaimRange.second || m_Count >  m_ClaimRange.first  && m_ShadowPrice > shadow_price<S>(); }
	bool AtMax    ()  const { return m_Count >= m_ClaimRange.second || m_Count >= m_ClaimRange.first  && m_ShadowPrice > shadow_price<S>(); }
	bool Underflow()  const { return m_Count <  m_ClaimRange.first  || m_Count <  m_ClaimRange.second && m_ShadowPrice < shadow_price<S>(); }
	bool AtMin    ()  const { return m_Count <= m_ClaimRange.first  || m_Count <= m_ClaimRange.second && m_ShadowPrice < shadow_price<S>(); }
	bool IsOK     ()  const 
	{ 
		return 
				m_Count >= m_ClaimRange.first 
			&&	m_Count <= m_ClaimRange.second 
			&& m_ShadowPrice <= shadow_price<S>() || m_Count == m_ClaimRange.first
			&& m_ShadowPrice >= shadow_price<S>() || m_Count == m_ClaimRange.second;
	}
};


template <typename S>
struct priority_heap : private std::vector<land_unit_id>
{
	priority_heap(facet_id thisHeapID, claim<S>* src, claim<S>* dst, const S* srcSuitMapBegin, const S* dstSuitMapBegin)
		:	m_NextOutHeapID(src->m_FirstOutHeapID)
		,	m_NextInpHeapID(dst->m_FirstInpHeapID)
		,	m_SourceClaim(src)
		,	m_TargetClaim(dst)
		,	m_PerturbationFactor(src->m_ggTypeID - dst->m_ggTypeID)
		,	m_Compare(src->m_ggTypeID > dst->m_ggTypeID, srcSuitMapBegin, dstSuitMapBegin)
	{
		dms_assert(m_PerturbationFactor != 0); // claims of the same type are of the same partitioning and cannot overlap
		src->m_FirstOutHeapID = thisHeapID;
		dst->m_FirstInpHeapID = thisHeapID;

		// if src > dst then equal cells with small indices are less distant from dst and have higher throwout priority
		//	thus, smaller is higher, hence lhs_dominates to return true for smaller priority
		// if src < dst, equal cells with large indices are less distant and have higher priority

		dms_assert((m_PerturbationFactor > 0) == m_Compare.LhsDominated()); // same story, different application context
	}
	void add(land_unit_id i) // add cell-id of positive cost value; least positive gets priority; when equal, last cell gets priority if src<dst else first
	{
		push_back(i);
		std::push_heap(begin(), end(), m_Compare);
	}
	land_unit_id top() const  // returns cell-id (i) of cell nearest to facet(src, dst); heap can be dirty due to non-removed contents that was transported to different claims
	{ 
		dms_assert(!empty()); 
		return front();
	}
	void SetSuitData(const S* srcSuitMapBegin, const S* dstSuitMapBegin)
	{
		clear();
		m_Compare.m_SrcSuitabilityMapBegin = srcSuitMapBegin;
		m_Compare.m_DstSuitabilityMapBegin = dstSuitMapBegin;
	}

	S GetC(land_unit_id i) const // returns cost of transporting i from src to dst if shadowprices had still been zero.
	{
		return m_Compare.GetC(i);
	}

	void pop()       
	{ 
		std::pop_heap(begin(), end(), m_Compare); pop_back(); 
	}
	bool empty() const { return size() == 0; }

	struct compare_oper { 
		compare_oper(bool lhsDominates, const S* srcSuitMapBegin, const S* dstSuitMapBegin)
			:	m_LhsDominates(lhsDominates)
			,	m_SrcSuitabilityMapBegin(srcSuitMapBegin)
			,	m_DstSuitabilityMapBegin(dstSuitMapBegin)
		{}

		S GetC(land_unit_id i) const // returns cost of transporting i from src to dst if shadowprices had still been zero.
		{
			return m_SrcSuitabilityMapBegin[i] - m_DstSuitabilityMapBegin[i];
		}

		bool operator ()(land_unit_id lhs, land_unit_id rhs) const // return true to indicate (strict) lower priority
		{
			S lhsFirst = GetC(lhs);
			S rhsFirst = GetC(rhs);
			return lhsFirst > rhsFirst || // transport more costly, thus return true for less priority
				((lhsFirst == rhsFirst) 
				&& (	m_LhsDominates
						?	(lhs> rhs)
						:	(lhs< rhs)
					)
				);
		}
		bool LhsDominated() const { return m_LhsDominates; }

	private:
		bool     m_LhsDominates; // srcGgTypeId > dstGgTypeId, thus higher has less priority and return true
	public:
		const S* m_SrcSuitabilityMapBegin;
		const S* m_DstSuitabilityMapBegin;
	} m_Compare;

	facet_id          m_NextOutHeapID, m_NextInpHeapID;
	perturbation_type m_PerturbationFactor;

	claim<S>*        m_SourceClaim;
	claim<S>*        m_TargetClaim;
};

template <typename S>
struct ggType_info_t
{
	SharedStr                     m_strName;
	TokenID                       m_NameID;

	const AbstrDataItem*          m_diMinClaims;
	const AbstrDataItem*          m_diMaxClaims;

	const AbstrDataItem*          m_diSuitabilityMap;
	      AbstrDataItem*          m_diResShadowPrices = nullptr;
	      AbstrDataItem*          m_diResTotalAllocated = nullptr;

	UInt32                        m_PartitioningID;
	claim_id                      m_FirstClaimID;  // ref into m_Claims array
	UInt32                        m_NrClaims;      // limits range of m_Claims array for this ggType

	DataReadLock                  m_SuitabilityDataLock;
	typename DataArray<S>::locked_cseq_t m_Suitabilities; // 1 per grid-cell, points directly into memory mapped file
};

template <typename AR>
struct partitioning_info_t
{
	typedef AR atomic_region_id;

	explicit partitioning_info_t(const AbstrDataItem* atomicRegionPartitioning)
		:	m_AtomicRegionPartitioningDI (atomicRegionPartitioning)
#if defined(MG_DEBUG)
		,	md_NrAtomicRegions(0)
#endif
	{
		m_ValuesLabelLock = GetPartitioningUnit()->GetLabelAttr();
	}

	partitioning_info_t(partitioning_info_t&& rhs)
		:	m_AtomicRegionPartitioningDI  (std::move(rhs.m_AtomicRegionPartitioningDI  ))
		,	m_AtomicRegionPartitioningData(std::move(rhs.m_AtomicRegionPartitioningData))
		,	m_NrRegions(rhs.m_NrRegions)
		,	m_UniqueRegionOffset(rhs.m_UniqueRegionOffset)
		,	m_ValuesLabelLock(std::move(rhs.m_ValuesLabelLock))
#if defined(MG_DEBUG)
		,	md_NrAtomicRegions(rhs.md_NrAtomicRegions)
#endif
	{
	}
	const AbstrDataItem*              m_AtomicRegionPartitioningDI;
	OwningPtrSizedArray<UInt32>       m_AtomicRegionPartitioningData;
	UInt32                            m_NrRegions;
	atomic_region_id                  m_UniqueRegionOffset;
	mutable SharedDataItemInterestPtr m_ValuesLabelLock;
#if defined(MG_DEBUG)
	UInt32                            md_NrAtomicRegions;
#endif

	void GetData()
	{
		DataReadLock lock(m_AtomicRegionPartitioningDI);
		auto nrAtomicRegions = m_AtomicRegionPartitioningDI->GetCurrRefObj()->GetNrFeaturesNow();
		MG_DEBUGCODE( md_NrAtomicRegions = nrAtomicRegions);
		m_AtomicRegionPartitioningData = OwningPtrSizedArray<UInt32>(nrAtomicRegions MG_DEBUG_ALLOCATOR_SRC_STR("DiscrAlloc: m_AtomicRegionPartitioningData"));
		m_AtomicRegionPartitioningDI->GetCurrRefObj()->GetValuesAsUInt32Array(tile_loc(0, 0), nrAtomicRegions, m_AtomicRegionPartitioningData.begin());
	}

	TokenStr GetName()                            const { return m_AtomicRegionPartitioningDI->GetName(); }
	const AbstrUnit* GetPartitioningUnit()        const { return m_AtomicRegionPartitioningDI->GetAbstrValuesUnit(); }
	UInt32 GetRegionID(atomic_region_id ar)       const { dbg_assert(m_AtomicRegionPartitioningData && ar < md_NrAtomicRegions);
	                                                      return m_AtomicRegionPartitioningData[ar]; }
	UInt32 GetUniqueRegionID(atomic_region_id ar) const { return m_UniqueRegionOffset + GetRegionID(ar);             }

	SharedStr GetRegionStr(UInt32 regionID) const
	{
		GuiReadLock lock;
		return mySSPrintF("%s %s", 
			GetName().c_str(),
			DisplayValue(GetPartitioningUnit(), regionID, false, m_ValuesLabelLock, MAX_TEXTOUT_SIZE, lock).c_str()
		);
	}
};


typedef UPoint cursor_type;
const UInt32 stepFactor = 4;

struct regions_info_base
{
	regions_info_base()
		:	m_N()
	{}

	void PreparePermutation(SizeT n)
	{
		m_N = n;
	}

	void SetStepSize(SizeT s, SizeT ps)
	{
		if (ps)
			m_PrevStep = m_CurrBase;
		else
			m_PrevStep = -1;

		m_StepSize = s;
		if (ps)
			m_CurrBase = m_CurrBase % s;
		else
			m_CurrBase = ((m_N-1) % s) /2;
		m_CurrPI = m_CurrBase;

		AvoidRepetition();
	}

	SizeT GetNrSteps() const
	{
		return (m_N +  (m_StepSize-1) - m_CurrBase ) / m_StepSize;
	}

	void GetNextPermutationValue() const
	{
		dms_assert(m_CurrPI < m_N); // INVARIANT (after initialization)
		m_CurrPI += m_StepSize;

		AvoidRepetition();
	}

	void AvoidRepetition() const
	{
		if (m_CurrPI >= m_PrevStep)
		{
			m_CurrPI   += m_StepSize;
			m_PrevStep += (m_StepSize*stepFactor);
		}
	}

	cursor_type GetCursor() const { return cursor_type(m_CurrPI, m_PrevStep); }
	void SetCursor(cursor_type c) {m_CurrPI = c.first; m_PrevStep = c.second; }

	SizeT m_N;
	mutable SizeT m_StepSize, m_CurrBase, m_PrevStep, m_CurrPI;

	SharedPtr<const AbstrDataItem> m_AtomicRegionMap;
	DataReadLock                   m_AtomicRegionLock;
	std::vector<UInt32>            m_AtomicRegionSizes; // 1 per atomic_region containing nr cells (sums to n)
	mutable OwningPtr<bi_graph>    m_Ar2Ur;             // bi_graph that represents AR -> UR relation
};

template <typename AR>
struct regions_info_t : regions_info_base
{
	using atomic_region_id = AR;
	using atomic_region_data_handle = typename DataArray<atomic_region_id>::locked_cseq_t;
	regions_info_t()
		:	m_NrUniqueRegions()
	{}

	WeakPtr<const TileFunctor<atomic_region_id> > m_AtomicRegionMapObj;
	atomic_region_data_handle                     m_AtomicRegionMapData; // 1 per grid-cell           (==  n )
	std::vector<partitioning_info_t<AR> >         m_Partitionings;       // 1 per Unique partitioning (==  p )
	atomic_region_id                              m_NrUniqueRegions;     // #ur

	UInt32 GetNrAtomicRegions() const { return m_AtomicRegionSizes.size(); }
	UInt32 GetNrPartitionings() const { return m_Partitionings.size(); }
	UInt32 GetNrUniqueRegions() const { return m_NrUniqueRegions; }

	UInt32 GetRegionID(atomic_region_id ar, UInt32 p)       const { return m_Partitionings[p].GetRegionID(ar);      }
	UInt32 GetUniqueRegionID(atomic_region_id ar, UInt32 p) const { return m_Partitionings[p].GetUniqueRegionID(ar);}
	const AbstrUnit* GetPartitioningUnit(UInt8 p)           const { return m_Partitionings[p].GetPartitioningUnit();}
	const bi_graph&  GetAr2UrBiGraph()                      const;

	// ========== ErrorMsg helper funcs

	SharedStr UniqueRegionStr(atomic_region_id ur) const
	{
		SharedStr result = mySSPrintF("Region %u ", ur);
		UInt32 p;

		for (p = 0; p != GetNrPartitionings(); ++p)
		{
			if (ur < m_Partitionings[p].m_NrRegions)
				return result + ": " + m_Partitionings[p].GetRegionStr(ur); 
			ur -= m_Partitionings[p].m_NrRegions;
		}
		return result;
	}

	SharedStr AtomicRegionStr(atomic_region_id ar) const
	{
		SharedStr result = mySSPrintF("Atomic Region %u", ar);

		for (UInt32 p = 0; p != GetNrPartitionings(); ++p)
		{
			result
				+=	SharedStr(p ? ", " : ": ")
					+	m_Partitionings[p].GetRegionStr(m_Partitionings[p].GetRegionID(ar));
		}
		return result;			
	}
};

template <typename S, typename AR, typename AT>
struct htp_info_t : regions_info_t<AR>
{
	using typename regions_info_t<AR>::atomic_region_id;

	htp_info_t() : m_TreeBuilder(*this), m_Threshold() {}

	S                                   m_Threshold;

	WeakPtr<const AbstrUnit>            m_MapDomain;

	DataWriteLock                       m_ResultDataLock;
	typename 
	DataArray<AT>::locked_seq_t         m_ResultArray;           // 1 per grid-cell (n); points directly info destination memmapped file
	DataWriteLock                       m_ResultPriceDataLock;

	std::vector<ggType_info_t<S> >      m_ggTypes;               // 1 per ggType              (==  k )
	std::vector<claim<S> >              m_Claims;                // 1 per claim region

	std::vector<UInt32> m_PossibleAllocationPerAr2UrLink;        // 1 per #ar * P; related to links    in m_Ar2Ur
	std::vector<UInt32> m_PossibleAllocatedPerUniqueRegion;      // 1 per #ur;     related to dstNodes in m_Ar2Ur

	std::vector<priority_heap<S> >       m_Facets;                // 1 per claim to claim confrontation
	std::vector<facet_id>                m_FacetIds;              // 1 per ggType^2 in each atomic region (== #ar * k *k)

//	DataReadLockContainer               m_Locks;                 // contains locks on suitability maps
	claim<S>& GetClaim(atomic_region_id ar, AT j)
	{ 
		ggType_info_t<S>& gg = m_ggTypes[j];
		return m_Claims[gg.m_FirstClaimID + this->GetRegionID(ar, gg.m_PartitioningID)];
	}
	priority_heap<S>& GetHeap(atomic_region_id ar, AT j, AT jj)
	{
		dms_assert(ar < this->GetNrAtomicRegions() );
		AT k = GetK();
		dms_assert(j  < k);
		dms_assert(jj < k);

		return m_Facets[ m_FacetIds[ (facet_code(ar)*k +j ) *k + jj] ];
	}
	UInt32 ClaimID2UniqueRegionID(UInt32 claimID) const
	{
		const claim<S>& claim = m_Claims[claimID];
		return 
				this->m_Partitionings[
					m_ggTypes[ claim.m_ggTypeID ].m_PartitioningID
				].m_UniqueRegionOffset
			+	claim.m_RegionID;
	}

	SharedStr GetClaimRangeStr(const claim<S>& cl) const
	{
		UInt32 ggTypeID = cl.m_ggTypeID;
		return
			mySSPrintF("ClaimRange(type %u (%s), %s) = [min %u, max %u]",
				ggTypeID,
				m_ggTypes[ggTypeID].m_strName.c_str(),
				this->m_Partitionings[ m_ggTypes[ cl.m_ggTypeID ].m_PartitioningID ].GetRegionStr(cl.m_RegionID).c_str(),
				cl.m_ClaimRange.first, cl.m_ClaimRange.second
			);
	}

	// implement directed_graph concept for G(m_Claims, m_Facets) and let it be used by the directed_dijkstra member

	typedef shadow_price<S> cost_type;
	UInt32 GetNrNodes()                 const { return m_Claims.size(); }
	UInt32 GetNrLinks()                 const { return m_Facets.size(); }
	UInt32 GetK()                       const { return m_ggTypes.size(); }
	land_unit_id GetN()                 const { dms_assert(this->m_N); return this->m_N; }         // nr of land units in all tiles
	UInt32 GetFirstLink(UInt32 claimID, dir_forward_tag ) const { dms_assert(claimID < GetNrNodes()); return m_Claims[claimID].m_FirstOutHeapID;  }
	UInt32 GetFirstLink(UInt32 claimID, dir_backward_tag) const { dms_assert(claimID < GetNrNodes()); return m_Claims[claimID].m_FirstInpHeapID;  }
	UInt32 GetNextLink (UInt32 heapID, dir_forward_tag )  const { dms_assert(heapID  < GetNrLinks()); return m_Facets[ heapID].m_NextOutHeapID; }
	UInt32 GetNextLink (UInt32 heapID, dir_backward_tag)  const { dms_assert(heapID  < GetNrLinks()); return m_Facets[ heapID].m_NextInpHeapID; }
	UInt32 GetDstNode  (UInt32 heapID, dir_forward_tag )  const { dms_assert(heapID  < GetNrLinks()); return m_Facets[ heapID].m_TargetClaim - begin_ptr( m_Claims ); }
	UInt32 GetSrcNode  (UInt32 heapID, dir_forward_tag )  const { dms_assert(heapID  < GetNrLinks()); return m_Facets[ heapID].m_SourceClaim - begin_ptr( m_Claims ); }
	UInt32 GetDstNode  (UInt32 heapID, dir_backward_tag)  const { return GetSrcNode(heapID, dir_forward_tag()); }
	UInt32 GetSrcNode  (UInt32 heapID, dir_backward_tag)  const { return GetDstNode(heapID, dir_forward_tag()); }

	shadow_price<S> GetLinkCost (UInt32 heapID)  const;
	bool            CheckLink   (UInt32 facetID) const;

	// ========== more data members
	directed_dijkstra<htp_info_t> m_TreeBuilder;
	std::vector<UInt32>           m_ClaimIdList;
	SharedPtr<const Unit<S> >     m_PriceUnit;
	land_unit_id                  m_NrBelowThreshold = 0;

#if defined(MG_DEBUG)
	bool CanReportFindMstDown() { return (++md_ReportFindMstDownCounter < 10) || PowerOf2(md_ReportFindMstDownCounter); }
	UInt32                                md_ReportFindMstDownCounter;
#endif
};

// *****************************************************************************
//									regions_info_t mf
// *****************************************************************************

template <typename AR>
const bi_graph& regions_info_t<AR>::GetAr2UrBiGraph() const
{
	if (m_Ar2Ur)
		return *m_Ar2Ur;

	UInt32 nrAtomicRegions = GetNrAtomicRegions();
	UInt32 nrUniqueRegions = GetNrUniqueRegions();
	UInt32 P = GetNrPartitionings();

	m_Ar2Ur.assign( new bi_graph(nrAtomicRegions, nrUniqueRegions, P * nrAtomicRegions) );
	bi_graph& gr = *m_Ar2Ur;

	// fill graph with links(atomicRegionID, unique region id)
	for (UInt32 p = 0; p != P; ++p)
		for (UInt32 ar = 0; ar != nrAtomicRegions; ++ar)
			gr.AddLink(ar, GetUniqueRegionID(ar, p));
	return gr;
}

// *****************************************************************************
//									Factet related funcs
// *****************************************************************************

template <typename S, typename AR, typename AT>
shadow_price<S> htp_info_t<S, AR, AT>::GetLinkCost(UInt32 facetID) const
{
	dms_assert(facetID < GetNrLinks()); 

	priority_heap<S>& ph = const_cast<htp_info_t*>(this)->m_Facets[facetID];  // contains q(a,b), with a=src && b=target of move option

	if (ph.empty())
		return MAX_VALUE(shadow_price<S>);

	UInt32 topI = ph.top();
	dms_assert(m_ResultArray[topI] == ph.m_SourceClaim->m_ggTypeID);

	// calculate (Ga - Gb) - (Qa(Epsilon) - Qb(Epsilon)) 
	//	== (Ga - Gb) + c + Epsilon * p(i) * (Ja - Jb), since c = (Qa - Qb)
	// and Epsilon(a) - Epsilon(b), the delta-Epsilon of a transfer though this facet 
	// corresponds with  p(i)*(Ja - Jb) == p(i) * perturbationfactor(a,b)

	shadow_price<S> cost(
		ph.GetC(topI),
		EPSILON(topI * ph.m_PerturbationFactor) 
	);

	dms_assert(cost + ph.m_SourceClaim->m_ShadowPrice >= ph.m_TargetClaim->m_ShadowPrice);

	// -(Qa - Qb).
	cost +=
		(	ph.m_SourceClaim->m_ShadowPrice	// Ga
		-	ph.m_TargetClaim->m_ShadowPrice	// Gb
		);

	dms_assert(cost >= shadow_price<S>());
	return cost;
}

template <typename S, typename AR, typename AT>
bool htp_info_t<S, AR, AT>::CheckLink(UInt32 facetID) const
{
	return GetLinkCost(facetID) < MAX_VALUE(shadow_price<S>); // if not; link may not be taken
}

// *****************************************************************************
//									FeasibilityTest
// *****************************************************************************

template <typename GRAPH_DIR>
UInt32 SumFlow(
	const bi_graph& gr,
	UInt32          srcNode,
	const UInt32*   lnkAllocated,
	GRAPH_DIR       graphDir = GRAPH_DIR()
)
{
	UInt32 totalFlow = 0;
	UInt32 lnk = gr.GetFirstLink(srcNode, graphDir);
	while (IsDefined(lnk))
	{
		totalFlow += lnkAllocated[lnk];
		lnk = gr.GetNextLink(lnk, graphDir);
	}
	return totalFlow;
}

void CheckSolution(
	const bi_graph& gr,
	const UInt32*   srcMinCapacity,
	const UInt32*   srcMaxCapacity,
	const UInt32*   dstMinClaims,
	const UInt32*   dstMaxClaims,
	const UInt32*   lnkAllocated
)
{
	UInt32 srcNode, dstNode;

	UInt32 nrSrc = gr.GetNrSrcNodes(dir_forward_tag());
	UInt32 nrDst = gr.GetNrDstNodes(dir_forward_tag());

	for (srcNode = 0; srcNode != nrSrc; ++srcNode)
	{
		UInt32 totalFlow = SumFlow<dir_forward_tag>(gr, srcNode, lnkAllocated);

		if (srcMinCapacity[srcNode] > totalFlow)
			throwErrorF("CheckSolution", "atomic region %u was assigned totalFlow %u, but has at least %u cells",
				srcNode, 
				totalFlow, 
				srcMinCapacity[srcNode]
			);
		if (srcMaxCapacity[srcNode] < totalFlow)
			throwErrorF("CheckSolution", "atomic region %u was assigned totalFlow %u, but has at most %u cells",
				srcNode, 
				totalFlow, 
				srcMaxCapacity[srcNode]
			);
	}
	for (dstNode = 0; dstNode != nrDst; ++dstNode)
	{
		UInt32 totalFlow = SumFlow<dir_backward_tag>(gr, dstNode, lnkAllocated);

		if (dstMinClaims[dstNode] > totalFlow)
			throwErrorF("CheckSolution", "unique region %u was assigned totalFlow %u, but its total minimum claim is %u cells",
				dstNode, 
				totalFlow, 
				dstMinClaims[dstNode]
			);
		if (dstMaxClaims[dstNode] < totalFlow)
			throwErrorF("CheckSolution", "unique region %u was assigned totalFlow %u, but its total maximum claim is %u cells",
				dstNode, 
				totalFlow, 
				dstMaxClaims[dstNode]
			);
	}
}

template <typename AR>
bool IsFeasible(
	const bi_graph& gr,
	const UInt32*   srcMinCapacity,
	const UInt32*   srcMaxCapacity,
	const UInt32*   dstMinClaims,
	const UInt32*   dstMaxClaims,
	      UInt32*   srcAllocated,
	      UInt32*   dstAllocated,
	      UInt32*   lnkAllocated,
	const regions_info_t<AR>& regInfo,
	      SharedStr&   strStatus
)
{
	UInt32 srcNode, dstNode, lnk;

	UInt32 nrSrc = gr.GetNrSrcNodes(dir_forward_tag());
	UInt32 nrDst = gr.GetNrDstNodes(dir_forward_tag());

	bool ok = true;
	// test that each minClaim <= maxClaim
	{
		for (dstNode = 0; dstNode != nrDst; ++dstNode)
			if (dstMinClaims[dstNode] > dstMaxClaims[dstNode])
			{
				strStatus += mySSPrintF(
					"%s has minimum %u and maximum %u; ",
					regInfo.UniqueRegionStr(dstNode).c_str(), 
					dstMinClaims[dstNode],
					dstMaxClaims[dstNode]
				);
				ok = false;
			}
	}

	// test if total claims fit total capacity
	{
		UInt32 totalMinClaim = 0;
		UInt32 totalMaxCapacity = 0;

		for (dstNode = 0; dstNode != nrDst; ++dstNode)
			totalMinClaim += dstMinClaims[dstNode];
		for (srcNode = 0; srcNode != nrSrc; ++srcNode)
			totalMaxCapacity += srcMaxCapacity[srcNode];

		if (totalMaxCapacity < totalMinClaim)
		{
			strStatus += mySSPrintF(
				"total of the minimum claims is %u while there are only %u allocatable cells, total minimum claims should be decreased with at least %u cells; ",
				totalMinClaim,
				totalMaxCapacity,
				totalMinClaim - totalMaxCapacity
			);
			ok = false;
		}
	}
	{
		UInt32 totalMaxClaim = 0;
		UInt32 totalMinCapacity = 0;

		for (dstNode = 0; dstNode != nrDst; ++dstNode)
			totalMaxClaim += dstMaxClaims[dstNode];
		for (srcNode = 0; srcNode != nrSrc; ++srcNode)
			totalMinCapacity += srcMinCapacity[srcNode];

		if (totalMinCapacity > totalMaxClaim)
		{
			strStatus += mySSPrintF(
				"there are %u cells that should be allocated while the total of the maximum claims is only %u, total maximum claims should be increased with at least %u cells; ",
				totalMinCapacity,
				totalMaxClaim,
				totalMinCapacity - totalMaxClaim
			);
			ok = false;
		}
	}	

	// test each dst restriction: min <= sum area for each atomicregion in dst
	for (dstNode = 0; dstNode != nrDst; ++dstNode)
	{
		UInt32 maxCapacity = 0;
		lnk = gr.GetFirstLink(dstNode, dir_backward_tag());
		while (IsDefined(lnk))
		{
			maxCapacity += srcMaxCapacity[ gr.GetDstNode(lnk, dir_backward_tag()) ];
			lnk = gr.GetNextLink(lnk, dir_backward_tag());
		}
		if (maxCapacity < dstMinClaims[dstNode])
		{
			strStatus += mySSPrintF(
				"total minimum claim for %s is %u while it has only %u allocatable cells, total minimum claims for that region should be decreased with at least %u cells; ",
				regInfo.UniqueRegionStr(dstNode).c_str(),
				dstMinClaims[dstNode],
				maxCapacity,
				dstMinClaims[dstNode] - maxCapacity
			);
			ok = false;
		}
	}

	// test each src capacity: capacity <= sum max claim for each dst that contains
	for (srcNode = 0; srcNode != nrSrc; ++srcNode)
	{
		dms_assert(srcMinCapacity[srcNode] <= srcMaxCapacity[srcNode]);

		UInt32 maxClaim = 0;
		lnk = gr.GetFirstLink(srcNode, dir_forward_tag());
		while (IsDefined(lnk))
		{
			maxClaim += dstMaxClaims[ gr.GetDstNode(lnk, dir_forward_tag()) ];
			lnk = gr.GetNextLink(lnk, dir_forward_tag());
		}
		if (maxClaim < srcMinCapacity[srcNode])
		{
			strStatus += mySSPrintF(
				"total maximum claim for %s is %u while it has %u cells that should be allocated, total maximum claims for that region should be increased with at least %u cells; ",
				regInfo.AtomicRegionStr(srcNode).c_str(),
				maxClaim,
				srcMinCapacity[srcNode],
				srcMinCapacity[srcNode] - maxClaim
			);
			ok = false;
		}
	}

	if (!ok)
		return false;

	bi_graph_dijkstra biGraphAugmenter(gr);

	// match minimum claims per unique region within maximum per atomic region
	for (dstNode = 0; dstNode != nrDst; ++dstNode)
	{
		UInt32 minClaim = dstMinClaims[dstNode];
		dstAllocated[dstNode] += minClaim;

		UInt32 excess = biGraphAugmenter.allocate<dir_backward_tag>(
			dstNode, minClaim, 
			srcMaxCapacity, 
			lnkAllocated, 
			srcAllocated
		);
		if (excess != 0)
		{
			strStatus += mySSPrintF(
				"from the total minimum claim of %u cells for %s, %u cannot be allocated (insufficient cells); ",
				minClaim,
				regInfo.UniqueRegionStr(dstNode).c_str(),
				excess
			);
			ok = false;
		}
	}

	// allocate remaining capacity per atomic region withing max limits of atomic region
	for (srcNode = 0; srcNode != nrSrc; ++srcNode)
	{
		UInt32 minCapacity = srcMinCapacity[srcNode];
		UInt32 currUsed    = srcAllocated[srcNode];
		if (minCapacity > currUsed)
		{
			UInt32 minAllocated = minCapacity - currUsed;
			srcAllocated[srcNode] = minCapacity;

			UInt32 excess = biGraphAugmenter.allocate<dir_forward_tag>(
				srcNode, minAllocated, 
				dstMaxClaims, 
				lnkAllocated, 
				dstAllocated
			);
			if (excess != 0)
			{
				strStatus += mySSPrintF(
					"from %u allocatable cells in %s; %u cannot be matched to claims (maximum claims too low); ",
					minCapacity,
					regInfo.AtomicRegionStr(srcNode).c_str(),
					excess
				);
				ok = false;
			}
		}
	}
	return ok;
}

template <typename S, typename AR, typename AT>
bool FeasibilityTest(const htp_info_t<S, AR, AT>& htpInfo, SharedStr& strStatus)
{
	UInt32 nrAtomicRegions = htpInfo.GetNrAtomicRegions();
	UInt32 nrUniqueRegions = htpInfo.GetNrUniqueRegions();

	const bi_graph& gr = htpInfo.GetAr2UrBiGraph();

	dms_assert(nrAtomicRegions     == gr.GetNrSrcNodes(dir_forward_tag()));
	dms_assert(nrUniqueRegions     == gr.GetNrDstNodes(dir_forward_tag()));
	dms_assert(htpInfo.GetNrPartitionings() * nrAtomicRegions == gr.GetNrLinks());

	// aggregate claims to unique regions
	std::vector<UInt32> aggrMinClaims(nrUniqueRegions, 0);
	std::vector<UInt32> aggrMaxClaims(nrUniqueRegions, 0);

	auto
		ggTypeIter = htpInfo.m_ggTypes.begin(),
		ggTypeEnd  = htpInfo.m_ggTypes.end();

	bool ok = true;
	while (ggTypeIter != ggTypeEnd)
	{
		auto
			claimIter = htpInfo.m_Claims.begin() + ggTypeIter->m_FirstClaimID,
			claimEnd  = claimIter                + ggTypeIter->m_NrClaims;

		UInt32 uniqueRegionOffset = htpInfo.m_Partitionings[ggTypeIter->m_PartitioningID].m_UniqueRegionOffset;
		dms_assert(uniqueRegionOffset + ggTypeIter->m_NrClaims <= nrUniqueRegions);

		auto
			aggrMinClaimIter = aggrMinClaims.begin() + uniqueRegionOffset,
			aggrMaxClaimIter = aggrMaxClaims.begin() + uniqueRegionOffset;

		for (; claimIter != claimEnd; ++claimIter)
		{
			if (claimIter->m_ClaimRange.first > claimIter->m_ClaimRange.second)
			{
				strStatus += mySSPrintF("%s: minimum > maximum; ", htpInfo.GetClaimRangeStr(*claimIter).c_str());
				ok = false;
			}
			*aggrMinClaimIter++ += claimIter->m_ClaimRange.first;
			*aggrMaxClaimIter++ += claimIter->m_ClaimRange.second;
		}

		++ggTypeIter;
	}
	if (!ok)
		return false;

	dms_assert(htpInfo.m_AtomicRegionSizes.size() == gr.GetNrSrcNodes(dir_forward_tag()));
	dms_assert(aggrMinClaims.size()     == gr.GetNrDstNodes(dir_forward_tag()));
	dms_assert(aggrMaxClaims.size()     == gr.GetNrDstNodes(dir_forward_tag()));

	std::vector<UInt32>  srcAllocated(nrAtomicRegions, 0);
	std::vector<UInt32>& allocatedPerLink = const_cast<htp_info_t<S, AR, AT>&>(htpInfo).m_PossibleAllocationPerAr2UrLink;
	std::vector<UInt32>& dstAllocated     = const_cast<htp_info_t<S, AR, AT>&>(htpInfo).m_PossibleAllocatedPerUniqueRegion;

	vector_zero_n(allocatedPerLink, gr.GetNrLinks());
	vector_zero_n(dstAllocated,     nrUniqueRegions);

	if	(!	IsFeasible(gr, 
				begin_ptr( htpInfo.m_AtomicRegionSizes ), // srcMinCapacity
				begin_ptr( htpInfo.m_AtomicRegionSizes ), // srcMaxCapacity
				begin_ptr( aggrMinClaims    ),      
				begin_ptr( aggrMaxClaims    ), 
				begin_ptr( srcAllocated     ),
				begin_ptr( dstAllocated     ),
				begin_ptr( allocatedPerLink ),
				htpInfo,
				strStatus
			)
		)
		return false;

	CheckSolution(gr, 
		begin_ptr( htpInfo.m_AtomicRegionSizes ), 
		begin_ptr( htpInfo.m_AtomicRegionSizes ),
		begin_ptr( aggrMinClaims    ),
		begin_ptr( aggrMaxClaims    ), 
		begin_ptr( allocatedPerLink )
	);
	return true;
}

// *****************************************************************************
//									CreateResultingItems
// *****************************************************************************

const AbstrDataItem* GetClaimAttr(const TreeItem* claimSet, TokenID nameID)
{
	auto result = AsDynamicDataItem(claimSet->GetConstSubTreeItemByID(nameID));

	if (!result)
	{
		//	claimSet->throwItemErrorF("Claimset should contain an attribute for %s", nameID.GetStr().c_str());
		return nullptr;
	}

	result->UpdateMetaInfo();

	if (result->GetDynamicObjClass() != DataItemClass::Find(ValueWrap<UInt32>::GetStaticClass()))
		result->throwItemError("Should contain UInt32 values");
	return result;
}

template <typename S, typename AR, typename AT>
void CreateResultingItems(
	const AbstrDataItem*  ggTypeNamesA,
	const AbstrUnit*      allocUnit,
	const TreeItem*       suitabilitiesSet,
	const TreeItem*       minClaimSet,
	const TreeItem*       maxClaimSet,
	const AbstrDataItem*  ggTypes2partitioningsA,
	const AbstrDataItem*  partitioningNamesA,
	const AbstrUnit*      atomicRegionUnit,
	const AbstrDataItem*  atomicRegionMapA,
	TreeItem*            resShadowPriceContainer,
	TreeItem*            resTotalAllocatedContainer,
	htp_info_t<S, AR, AT>&   htpInfo,
	bool mustAdjust, OperationContext* fc
)
{
	// init elementary data members
	htpInfo.m_MapDomain = allocUnit;
	dms_assert(atomicRegionUnit);
	dms_assert(minClaimSet);
	dms_assert(maxClaimSet);
	dms_assert(suitabilitiesSet);
	SharedStr resultMsg;

	// get array of partitionNames
	DataReadLock partitioningNamesLock(partitioningNamesA);
	const DataArray<SharedStr>* partitioningNames = const_array_cast<SharedStr>(partitioningNamesA);
	UInt32 P = partitioningNames->GetDataRead().size();

	htpInfo.m_Partitionings.reserve(P);
	for (UInt32 p=0; p!=P; ++p)
	{
		SharedStr partitioningName = partitioningNames->GetIndexedValue(p);
		CDebugContextHandle context("discrete_alloc", partitioningName.c_str(), false);

		const AbstrDataItem* regioRefDI = AsCheckedDataItem(atomicRegionUnit->GetConstSubTreeItemByID(GetTokenID_mt(partitioningName.c_str())));
		if (!regioRefDI)
			atomicRegionUnit->throwItemErrorF("SubItem expected with the name %s", partitioningName.c_str());
		regioRefDI->UpdateMetaInfo();
		fc->AddDependency(regioRefDI->GetCheckedDC());

		if (!atomicRegionUnit->UnifyDomain(regioRefDI->GetAbstrDomainUnit(), UnifyMode(), &resultMsg))
			throwErrorF("discrete_alloc", "unification of domain of partitoning %d(%s):\n%s\n with atomic region\n%s\n resulted in\n%s"
				,	p, partitioningName, regioRefDI->GetSourceName()
				,	atomicRegionUnit->GetSourceName()
				,	resultMsg
			);

		htpInfo.m_Partitionings.push_back( partitioning_info_t<AR>(regioRefDI) );
		if (htpInfo.m_Partitionings.back().m_ValuesLabelLock)
		{
			fc->AddDependency(htpInfo.m_Partitionings.back().m_ValuesLabelLock->GetCheckedDC());
		}
	}
	dms_assert(htpInfo.m_Partitionings.size() == P);

	// get array of ggTypeNames
	DataReadLock ggTypesNameLock(ggTypeNamesA);
	const DataArray<SharedStr>* ggTypeNames = const_array_cast<SharedStr>(ggTypeNamesA);

	// get array of ggTypes2partitionings
	DataReadLock ggTypes2partitioningsLock(ggTypes2partitioningsA);
	const DataArray<UInt8>* ggTypes2partitionings = const_array_cast<UInt8>(ggTypes2partitioningsA);

	if (!ggTypes2partitioningsA->GetAbstrDomainUnit()->UnifyDomain(ggTypeNamesA      ->GetAbstrDomainUnit(), UnifyMode(), &resultMsg))
		throwErrorF("discrete_alloc", "domains of Type->Name mapping (arg1):\n%s\nand Type->Partitioning mapping (arg4):\n%s\nincompatible: %s", ggTypeNamesA->GetSourceName(), ggTypes2partitioningsA->GetSourceName(), resultMsg);

	if (!ggTypes2partitioningsA->GetAbstrValuesUnit()->UnifyDomain(partitioningNamesA->GetAbstrDomainUnit(), UnifyMode(), &resultMsg))
		throwErrorF("discrete_alloc", "values of Type->Partitioning mapping (arg4):\n%s\nand Partition->Names mapping (arg5):\n%s\nincompatible: %s", ggTypes2partitioningsA->GetSourceName(), partitioningNamesA->GetSourceName(), resultMsg);

	UInt32 K = ggTypeNames->GetDataRead().size();
	htpInfo.m_ggTypes.resize(K);

	for (UInt32 j=0; j!=K; ++j)
	{
		ggType_info_t<S>* gg = begin_ptr(htpInfo.m_ggTypes) + j;
		gg->m_strName = ggTypeNames->GetIndexedValue(j);
		auto contextHandle = MakeLCH([gg]() { return "discrete_alloc_init for Type " + gg->m_strName;  });

		gg->m_NameID         = GetTokenID_mt(gg->m_strName.begin(), gg->m_strName.send());
		gg->m_diMinClaims    = GetClaimAttr(minClaimSet, gg->m_NameID);
		gg->m_diMaxClaims    = GetClaimAttr(maxClaimSet, gg->m_NameID);
		fc->AddDependency(gg->m_diMinClaims->GetCheckedDC());
		fc->AddDependency(gg->m_diMaxClaims->GetCheckedDC());

		auto partitioningID = ggTypes2partitionings->GetIndexedValue(j);
		if (partitioningID >= htpInfo.GetNrPartitionings())
			throwErrorF("discrete_alloc", "Partitioning reference %d for Type %s is not a valid index in %s", partitioningID, gg->m_NameID, partitioningNamesA->GetSourceName());
		
		gg->m_PartitioningID = partitioningID;
		const AbstrUnit* partitioningUnit = htpInfo.GetPartitioningUnit( gg->m_PartitioningID );

		if (gg->m_diMinClaims && !partitioningUnit->UnifyDomain(gg->m_diMinClaims->GetAbstrDomainUnit(), UnifyMode(), &resultMsg))
			throwErrorF("discrete_alloc", "values of partitioning %s in AtomicRegions (arg6):\n%s\nand domain of minimum claim for %s (arg8):\n%s\nincompatible: %s"
//				, partitioningID
				, htpInfo.m_Partitionings[partitioningID].GetName()
				, htpInfo.m_Partitionings[partitioningID].m_AtomicRegionPartitioningDI->GetSourceName()
				, gg->m_NameID, gg->m_diMinClaims->GetSourceName()
				, resultMsg
			);
		if (gg->m_diMaxClaims && !partitioningUnit->UnifyDomain(gg->m_diMaxClaims->GetAbstrDomainUnit(), UnifyMode(), &resultMsg))
			throwErrorF("discrete_alloc", "values of partitioning %s in AtomicRegions (arg6):\n%s\nand domain of maximum claim for %s (arg8):\n%s\nincompatible: %s"
//				, partitioningID
				, htpInfo.m_Partitionings[partitioningID].GetName()
				, htpInfo.m_Partitionings[partitioningID].m_AtomicRegionPartitioningDI->GetSourceName()
				, gg->m_NameID, gg->m_diMaxClaims->GetSourceName()
				, resultMsg
			);

		gg->m_diSuitabilityMap = AsCertainDataItem(suitabilitiesSet->GetConstSubTreeItemByID(gg->m_NameID));
		gg->m_diSuitabilityMap->UpdateMetaInfo();
		fc->AddDependency(gg->m_diSuitabilityMap->GetCheckedDC());
		if (!allocUnit->UnifyDomain(gg->m_diSuitabilityMap->GetAbstrDomainUnit(), UnifyMode(), &resultMsg))
			throwErrorF("discrete_alloc", "Domain of suitability map for %s:\n%s\n %s and allocUnit (arg2) incompatible: %s"
				,	gg->m_NameID, gg->m_diSuitabilityMap->GetSourceName()
				,	allocUnit->GetSourceName()
				,	resultMsg
			);
		{
			FixedContextHandle priceUnitContext("processing the values unit of a suitability map as a unit of utility");
			const Unit<S>* priceUnit = const_unit_checkedcast<S>(gg->m_diSuitabilityMap->GetAbstrValuesUnit());
			if (!htpInfo.m_PriceUnit)
				htpInfo.m_PriceUnit = priceUnit;
			else
				if (!htpInfo.m_PriceUnit->UnifyValues(priceUnit, UnifyMode(), &resultMsg))
					throwErrorF("discrete_alloc", "values of suitability map for %s incompatible with earlier suitability map values:\n%s", gg->m_NameID, resultMsg);

			if (mustAdjust)
				gg->m_diResShadowPrices = CreateDataItem(
					resShadowPriceContainer
					, gg->m_NameID
					, partitioningUnit
					, priceUnit
				);
		}
		gg->m_diResTotalAllocated = 
			CreateDataItem(
				resTotalAllocatedContainer
			,	gg->m_NameID
			,	partitioningUnit
			,	Unit<land_unit_id>::GetStaticClass()->CreateDefault()
			);
	}
	if (!allocUnit->UnifyDomain(atomicRegionMapA->GetAbstrDomainUnit(), UnifyMode(), &resultMsg))
		throwErrorF("discrete_alloc", "Domain of atomic region map (arg7):\n%s\nand allocUnit (arg2):\n%s incompatible:\n%s"
			, atomicRegionMapA->GetSourceName()
			, allocUnit->GetSourceName()
			, resultMsg
		);
}

// *****************************************************************************
//									Prepare
// *****************************************************************************

template <typename S, typename AR, typename AT>
void PrepareClaims(htp_info_t<S, AR, AT>& htpInfo)
{
	UInt32 K = htpInfo.GetK();

	// count total nrClaims and calc offsets in m_Claims array based on cardinality of each ggType's partitioning
	UInt32 nrClaims = 0;
	for (UInt32 j=0; j!=K; ++j)
	{
		ggType_info_t<S>& gg = htpInfo.m_ggTypes[j];

		const AbstrUnit* partitioningUnit = htpInfo.GetPartitioningUnit( gg.m_PartitioningID );

		gg.m_FirstClaimID = nrClaims;
		gg.m_NrClaims = partitioningUnit->GetCount();
		nrClaims += gg.m_NrClaims;
	}

	// load claim min, max into m_Claims
	htpInfo.m_Claims.reserve(nrClaims);
	for (UInt32 j=0; j!=K; ++j)
	{
		ggType_info_t<S>& gg = htpInfo.m_ggTypes[j];

		dms_assert(IsDataReady(gg.m_diMinClaims->GetCurrUltimateItem()));
		dms_assert(IsDataReady(gg.m_diMaxClaims->GetCurrUltimateItem()));

		DataReadLock lockClaimMin(gg.m_diMinClaims);
		DataReadLock lockClaimMax(gg.m_diMaxClaims);

		for (UInt32 r = 0; r != gg.m_NrClaims; ++r)
		{
			htpInfo.m_Claims.push_back(
				claim<S>(
					j, r, 
					claim_range(
						const_array_cast<claim_type>(gg.m_diMinClaims)->GetIndexedValue(r),
						const_array_cast<claim_type>(gg.m_diMaxClaims)->GetIndexedValue(r)
					)
				)
			);
		}
	}
	dms_assert(htpInfo.m_Claims.size() == nrClaims);
}

template <typename S, typename AR, typename AT>
void PreparePartitionings(htp_info_t<S, AR, AT>& htpInfo, const AbstrDataItem* atomicRegionMapA, const Unit<AR>* atomicRegionUnit)
{
	UInt32 p = htpInfo.GetNrPartitionings();

	dms_assert( htpInfo.m_NrUniqueRegions == 0 );
	for (UInt32 j = 0; j!=p; ++j)
	{
		partitioning_info_t<AR>& pInfo = htpInfo.m_Partitionings[j];

		UInt32 nrRegions = pInfo.GetPartitioningUnit()->GetCount();

		pInfo.m_UniqueRegionOffset = htpInfo.m_NrUniqueRegions;
		pInfo.m_NrRegions          = nrRegions;

		pInfo.GetData();
//		pInfo.m_DataLock = DataReadLock(pInfo.m_AtomicRegionPartitioningDI);
//		htpInfo.m_Locks.push_back(DataReadLock(pInfo.m_AtomicRegionPartitioningDI));
//		dms_assert(pInfo.m_DataLock.IsLocked());

//		pInfo.m_AtomicRegionPartitioningData = pInfo.m_AtomicRegionPartitioningObj->GetDataRead().begin();
		htpInfo.m_NrUniqueRegions += nrRegions;
	}

	// collect atomicRegionCounts
	htpInfo.m_AtomicRegionMap = atomicRegionMapA;
	htpInfo.m_AtomicRegionLock = DataReadLock(atomicRegionMapA);
	dms_assert(htpInfo.m_AtomicRegionLock.IsLocked());
	htpInfo.m_AtomicRegionMapObj = const_array_cast<AR>(atomicRegionMapA);

	UInt32 nrAtomicRegions = atomicRegionUnit->GetCount();
	vector_zero_n(htpInfo.m_AtomicRegionSizes, nrAtomicRegions);

	SizeT nrLandUnits = 0;
	tile_id nrLandUnitTiles = atomicRegionMapA->GetAbstrDomainUnit()->GetNrTiles();
	for (tile_id t=0; t!=nrLandUnitTiles; ++t)
	{
		auto atomicRegionMapData = htpInfo.m_AtomicRegionMapObj->GetLockedDataRead(t);
		nrLandUnits += atomicRegionMapData.size();

		pcount_container<AR, UInt32>(
			IterRange<UInt32*>(&htpInfo.m_AtomicRegionSizes)
		,	atomicRegionMapData
		,	atomicRegionUnit->GetRange()
		,	atomicRegionMapA->GetCheckMode()
		,	false
		);
	}
	land_unit_id n = ThrowingConvert<land_unit_id>( atomicRegionMapA->GetAbstrDomainUnit()->GetCount() );
	htpInfo.m_N = n;
	if (nrLandUnits != n)
		htpInfo.m_MapDomain->throwItemErrorF(
			"Land Unit set had %u elements, but total nr of elements in tiles is %u. Use a land unit set with a completely covering tiling",
			n, 
			nrLandUnits
		);
}

template <typename S, typename AR, typename AT>
void DataReadLockSuitabilities(htp_info_t<S, AR, AT>& htpInfo)
{
	UInt32 K = htpInfo.GetK();

	// suitabilityMaps
	for (UInt32 j=0; j!=K; ++j)
	{
		ggType_info_t<S>& gg = htpInfo.m_ggTypes[j];
		gg.m_SuitabilityDataLock = DataReadLock(gg.m_diSuitabilityMap);
		dms_assert(gg.m_SuitabilityDataLock.IsLocked());

		gg.m_Suitabilities = const_array_cast<S>(gg.m_SuitabilityDataLock.get_ptr())->GetDataRead();
	}
}

template <typename S, typename AR, typename AT>
void PrepareResultTileLock(htp_info_t<S, AR, AT>& htpInfo, bool initUndefined)
{
	htpInfo.m_ResultArray = mutable_array_cast<AT>(htpInfo.m_ResultDataLock)->GetDataWrite(no_tile, dms_rw_mode::write_only_all);

	if (initUndefined)
		fast_fill(htpInfo.m_ResultArray.begin(), htpInfo.m_ResultArray.end(), UNDEFINED_VALUE(AT));
}

template <typename S, typename AR, typename AT>
void PrepareTileLock(htp_info_t<S, AR, AT>& htpInfo)
{
	const DataArray<AR>* atomicRegionObj = htpInfo.m_AtomicRegionMapObj;
	dms_assert(atomicRegionObj);
	htpInfo.m_AtomicRegionMapData = atomicRegionObj->GetDataRead();

	htpInfo.PreparePermutation(htpInfo.m_AtomicRegionMapData.size());

	UInt32 K = htpInfo.GetK();

	// suitabilityMaps
	for (UInt32 j=0; j!=K; ++j)
	{
		ggType_info_t<S>& gg = htpInfo.m_ggTypes[j];
		gg.m_Suitabilities = const_array_checkedcast<S>(gg.m_diSuitabilityMap)->GetDataRead();
	}

	auto 
		pi = htpInfo.m_Facets.begin(),
		pe = htpInfo.m_Facets.end();

	for (; pi !=pe; ++pi)
	{
		pi->SetSuitData(
			htpInfo.m_ggTypes[ pi->m_SourceClaim->m_ggTypeID ].m_Suitabilities.begin()
		,	htpInfo.m_ggTypes[ pi->m_TargetClaim->m_ggTypeID ].m_Suitabilities.begin()
		);
	}

	PrepareResultTileLock<S, AR>(htpInfo, false);
}

template <typename S, typename AR, typename AT>
void PrepareFacets(htp_info_t<S, AR, AT>& htpInfo)
{
	UInt32 K = htpInfo.GetK();
	UInt32 nrAtomicRegions = htpInfo.GetNrAtomicRegions();

	htpInfo.m_FacetIds.reserve(nrAtomicRegions * K * K);

	typedef std::pair<claim<S>*, claim<S>*> claim_pair;

	std::map<claim_pair, UInt32> allocatedQueIds;

	for (UInt32 ar = 0; ar != nrAtomicRegions; ++ar)
	{
		for (UInt32 j=0; j !=K; ++j)
		{
			claim<S>* claimJ = &htpInfo.GetClaim(ar, j);
			for (UInt32 jj=0; jj!=K; ++jj) 
			{
				if (jj == j)
					htpInfo.m_FacetIds.push_back(-1); // ease of admin
				else
				{
					claim<S>* claimJJ = &htpInfo.GetClaim(ar, jj);
					claim_pair claimPair(claimJ, claimJJ);
					if (allocatedQueIds.find(claimPair) == allocatedQueIds.end())
					{
						UInt32 heapID = htpInfo.m_Facets.size();
						allocatedQueIds[claimPair] = heapID;
						htpInfo.m_Facets.push_back(
							priority_heap<S>(
								heapID, 
								claimJ, claimJJ, 
								htpInfo.m_ggTypes[ j].m_Suitabilities.begin(), 
								htpInfo.m_ggTypes[jj].m_Suitabilities.begin()
							)
						);
					}
					htpInfo.m_FacetIds.push_back(allocatedQueIds[claimPair]);
				}
			}
		}
	}
}

template <typename S, typename AR, typename AT>
void PrepareReport(htp_info_t<S, AR, AT>& htpInfo)
{
	reportF(SeverityTypeID::ST_MajorTrace, "DiscrAlloc: Prepare created alloc structs for "
		"%u cells, %u landuse types, %u (min-max) claims, %u unique partitionings, "
		"%u atomic regions, %u unique regions, and %u priority queues",
		htpInfo.GetN(), 
		htpInfo.GetK(), 
		htpInfo.GetNrNodes(), 
		htpInfo.GetNrPartitionings(), 
		htpInfo.GetNrAtomicRegions(), 
		htpInfo.GetNrUniqueRegions(), 
		htpInfo.GetNrLinks()
	);
#if defined(MG_DEBUG)
	UInt32 K  = htpInfo.GetK();
	UInt32 ar = htpInfo.GetNrAtomicRegions();
	dms_assert(htpInfo.m_FacetIds.size()     == ar * K * K ); // htpInfo.GetNrLinks()
#endif
}

// *****************************************************************************
//									Update
// *****************************************************************************

template <typename S, typename AR, typename AT>
void RemoveLoserInResultAndCleanupQueues(htp_info_t<S, AR, AT>& htpInfo, AR ar, SizeT i, UInt32 losing_ggTypeID)
{
	UInt32 K = htpInfo.m_ggTypes.size();
	for (UInt32 j=0; j!=K; ++j) if (j != losing_ggTypeID)
	{
		priority_heap<S>& ph = htpInfo.GetHeap(ar, losing_ggTypeID, j);

		if (!ph.empty() && ph.top() == i)
		{
			do
			{
				ph.pop();
				if (ph.empty())
					goto exit;
			} while (ph.top() == i);

			// also clean uncovered dirt (INVARIANT: only possible dirt is covered dirt).
			while (htpInfo.m_ResultArray[ph.top()] != losing_ggTypeID)
			{
				ph.pop();
				if (ph.empty())
					goto exit;
			} 
			dms_assert(ph.top() != i); // we assume i is part of the queue ordering
		}
	exit:
		dms_assert(ph.empty() || htpInfo.CheckLink(&ph - begin_ptr( htpInfo.m_Facets )) );
	}
}

// insert highestBidder into solution and queues
template <typename S, typename AR, typename AT>
void InsertWinnerInResultAndReallocQueues(htp_info_t<S, AR, AT>& htpInfo, AR ar, UInt32 i, UInt32 winning_ggTypeID)
{
	htpInfo.m_ResultArray[i] = winning_ggTypeID; // actual allocation

	UInt32 currNode = &(htpInfo.GetClaim(ar, winning_ggTypeID)) - begin_ptr( htpInfo.m_Claims );

	dms_assert(!htpInfo.m_ClaimIdList.size()           // indicator for active MST => being called from UpdateSplitterDown or Up
		||	htpInfo.m_TreeBuilder.is_flagged(currNode) // from UpdateSplitter, currNode is in the migration path; thus connection with source exists
	);

	UInt32 K = htpInfo.m_ggTypes.size();
	for (UInt32 j=0; j!=K; ++j) if (j != winning_ggTypeID)
	{
		S s = htpInfo.m_ggTypes[j].m_Suitabilities[i];
		if (s < htpInfo.m_Threshold) continue;

		priority_heap<S>& ph = htpInfo.GetHeap(ar, winning_ggTypeID, j);

		UInt32 popNode = ph.m_TargetClaim - begin_ptr( htpInfo.m_Claims );
		dms_assert( currNode == ph.m_SourceClaim - begin_ptr( htpInfo.m_Claims ) );

#if defined(MG_DEBUG) // DEBUG
		if( ph.m_SourceClaim->m_ShadowPrice // Ga
			+	shadow_price<S>(
					htpInfo.m_ggTypes[winning_ggTypeID].m_Suitabilities[i] - htpInfo.m_ggTypes[j].m_Suitabilities[i], 
					i * ph.m_PerturbationFactor
				)                                  // -(Qa - Qb)
			<	ph.m_TargetClaim->m_ShadowPrice)   // Gb
		{
			DBG_START("DiscrAlloc", "InsertWinnerInResultAndReallocQueues", true);
			reportF(SeverityTypeID::ST_Warning,
				"Problem defending cell %u from %s$%s against %s$%s ",
					i,
					htpInfo.GetClaimRangeStr( htpInfo.m_Claims[currNode] ).c_str(), AsString(ph.m_SourceClaim->m_ShadowPrice).c_str(),
					htpInfo.GetClaimRangeStr( htpInfo.m_Claims[popNode ] ).c_str(), AsString(ph.m_TargetClaim->m_ShadowPrice).c_str()
			);
			
			std::vector<UInt32>::const_iterator
				claimIdPtr = htpInfo.m_ClaimIdList.begin(),
				claimIdEnd = htpInfo.m_ClaimIdList.end();
			while (claimIdPtr != claimIdEnd)
			{
				UInt32 facetID = htpInfo.m_TreeBuilder.get_traceback(*claimIdPtr).Link();
				DBG_TRACE(("%s$%s, reached by Link[%u](%s,%s) was incremented by $%s",
						htpInfo.GetClaimRangeStr(htpInfo.m_Claims[*claimIdPtr]).c_str(), AsString(htpInfo.m_Claims[*claimIdPtr].m_ShadowPrice).c_str(),
						facetID,
						htpInfo.GetClaimRangeStr(htpInfo.m_Claims[htpInfo.GetSrcNode(facetID, dir_forward_tag())]).c_str(),
						htpInfo.GetClaimRangeStr(htpInfo.m_Claims[htpInfo.GetDstNode(facetID, dir_forward_tag())]).c_str(),
						AsString(htpInfo.m_TreeBuilder.get_traceback(*claimIdPtr).Cost()).c_str()
					)
				);
				dms_assert( htpInfo.m_Facets[facetID].empty() || htpInfo.CheckLink(facetID) );
				++claimIdPtr;
			}

			for (UInt32 jj=0; jj!=K; ++jj)
			{
				DBG_TRACE(("Suitability at cell %u AT %u[%u] = %d",
					i, jj,
					&(htpInfo.GetClaim(ar, jj)) - begin_ptr( htpInfo.m_Claims ),
					htpInfo.m_ggTypes[jj].m_Suitabilities[i]
				));

			}

//			throwErrorD("Internal Error", "DiscreteAlloc");
		}
#endif
		dms_assert(ph.empty() || htpInfo.CheckLink(&ph - begin_ptr( htpInfo.m_Facets )) );

		ph.add(i); // more expensive change => more positive => less relevant => deeper in priority queue
		dms_assert(htpInfo.CheckLink(&ph - begin_ptr( htpInfo.m_Facets )) );
	}
}


// *****************************************************************************
//									UpdateSplitterDown
// Updates ShadowPrices (splitter) in order to move away 1 point from "root" claim
// A MST from the root claim over adjacent claims is made in order to find
// a shortest path to a free claim (than can absorb a point from the overflow)
//
// This shortest path is used to restore the invariance that
// - no claims have overflow (> maxClaim or > minClaim and shadowPrice > 0) and
// - all points i are allocated to the claim j with the corrected maximum ( Sij + shadowprice(j) )
// *****************************************************************************

template <typename S, typename AR, typename AT>
UInt32 FindMstDown(
	htp_info_t<S, AR, AT>&                     htpInfo, 
	UInt32                                     rootClaimID,
	typename htp_info_t<S, AR, AT>::cost_type& minLinkCost //cost until dst of (free)link; thus including GetLinkCost(currLink)
)
{
	DBG_START("DiscrAlloc", "FindMstDown", DMS_DEBUG_DISCRALLOC);

	UInt32 minLink = UNDEFINED_VALUE(UInt32); // corresponds with given minLinkCost if not INF.

	htpInfo.m_TreeBuilder.init_tree(rootClaimID, dir_forward_tag() ); // calls fix_node and brings all outgoing links in queue

#if defined(MG_DEBUG)
	if (htpInfo.m_TreeBuilder.empty() && htpInfo.CanReportFindMstDown() )
	{
		reportF(SeverityTypeID::ST_MajorTrace, "FindMstDown: no adjustments possible for %s",
				htpInfo.GetClaimRangeStr( htpInfo.m_Claims[rootClaimID] ) .c_str()
		);
	}
#endif

	dms_assert( htpInfo.m_ClaimIdList.empty());

	while (true)
	{
		if (!htpInfo.m_TreeBuilder.get_next( dir_forward_tag() ))
		{
			DBG_TRACE(("FindMstDown reached EndOfHeap without finding free claim"));
			return UNDEFINED_VALUE(UInt32); // no free claim found
		}

		const directed_heap_elem<typename htp_info_t<S, AR, AT>::cost_type>& currElem = htpInfo.m_TreeBuilder.top(); 

		UInt32 currLink = currElem.Link();
		auto   linkCost = currElem.Cost(); //cost until dst of link; thus including GetLinkCost(currLink)
		DBG_TRACE(( "currLink %u with linkCost %s", currLink, AsString(linkCost).c_str() ));

		const claim<S>* targetClaim = htpInfo.m_Facets[currLink].m_TargetClaim;

		bool atMax = targetClaim->AtMax();
		if (atMax && targetClaim->m_Count < targetClaim->m_ClaimRange.second)
		{
			if (targetClaim->m_ShadowPrice + linkCost < minLinkCost)
			{
				minLinkCost = targetClaim->m_ShadowPrice + linkCost;
				minLink     = currLink;
			}
		}
		if (linkCost > minLinkCost)
		{
			DBG_TRACE(("FindMstDown returns free lowerbound at link %u at cost %s", minLink, AsString(minLinkCost).c_str()));
			return minLink;
		}

		UInt32 dstNode = targetClaim - begin_ptr( htpInfo.m_Claims );
		if (!atMax)
		{
			DBG_TRACE(("FindMstDown signals target claim %u(%u, %u) of Facet %u as vacant", 
				dstNode, targetClaim->m_ggTypeID, targetClaim->m_RegionID,
				currLink)
			);

			htpInfo.m_TreeBuilder.add_node(dstNode, currLink, linkCost ); 
			htpInfo.m_ClaimIdList.push_back(dstNode); // maintain ordered built MST for splitter adjustments
			dms_assert( IsDefined(currLink) ); // we did check that feasible solution exists

			minLinkCost = linkCost;
			return currLink;
		}

		htpInfo.m_TreeBuilder.pop_node();

		dms_assert( htpInfo.CheckLink(currLink) );

#if defined MG_DEBUG
		if ( targetClaim->Overflow() )
		{
			DBG_TRACE(("FindMstDown: TargetClaim (%u, %u) has overflow", 
				targetClaim->m_ggTypeID, targetClaim->m_RegionID
			));
		}
#endif
		DBG_TRACE(("FindMstDown fixes link to target claim %u(%u, %u) of Facet %u at cost %s", 
			dstNode, targetClaim->m_ggTypeID, targetClaim->m_RegionID,
			currLink,
			AsString(linkCost).c_str()
		));
		

		htpInfo.m_TreeBuilder.fix_link(	currLink, linkCost, dir_forward_tag() );         // bring all links from the destination of currLink into queue for further processing
		htpInfo.m_ClaimIdList.push_back( dstNode ); // maintain ordered built MST for splitter adjustments

		dms_assert( htpInfo.CheckLink(currLink) );
	}
}

template <typename S, typename AR, typename AT>
UInt32 FindMstUp(
	htp_info_t<S, AR, AT>&                     htpInfo, 
	UInt32                                     rootClaimID,
	typename htp_info_t<S, AR, AT>::cost_type& minLinkCost //cost until dst of (free)link; thus including GetLinkCost(currLink)
)
{
	DBG_START("DiscrAlloc", "FindMstUp", DMS_DEBUG_DISCRALLOC);

	UInt32 minLink = UNDEFINED_VALUE(UInt32); // corresponds with given minLinkCost if not INF.

	htpInfo.m_TreeBuilder.init_tree(rootClaimID, dir_backward_tag() ); // calls fix_node and brings all outgoing links in queue

#if defined(MG_DEBUG)
	if (htpInfo.m_TreeBuilder.empty())
	{
		reportF(SeverityTypeID::ST_MajorTrace, "FindMstUp: no adjustments possible for %s",
				htpInfo.GetClaimRangeStr( htpInfo.m_Claims[rootClaimID] ).c_str()
		);
	}
#endif

	dms_assert( htpInfo.m_ClaimIdList.empty());

	while ( true )
	{
		if (!htpInfo.m_TreeBuilder.get_next( dir_backward_tag() ))
		{
			DBG_TRACE(("FindMstUp reached EndOfHeap without finding free claim"));
			return UNDEFINED_VALUE(UInt32); // no free claim found
		}

		const directed_heap_elem<typename htp_info_t<S, AR, AT>::cost_type>& currElem = htpInfo.m_TreeBuilder.top(); 

		UInt32 currLink = currElem.Link();
		auto   linkCost = currElem.Cost(); //cost until dst of link; thus including GetLinkCost(currLink)
		DBG_TRACE(( "currLink %u with linkCost %s", currLink, AsString(linkCost).c_str() ));

		const claim<S>* sourceClaim = htpInfo.m_Facets[currLink].m_SourceClaim;

		bool atMin = sourceClaim->AtMin();
		if (atMin && sourceClaim->m_Count > sourceClaim->m_ClaimRange.first)
		{
			dms_assert(sourceClaim->m_ShadowPrice < shadow_price<S>()); // else it wouldnt be AtMin
			if (linkCost - sourceClaim->m_ShadowPrice < minLinkCost)
			{
				minLinkCost = linkCost - sourceClaim->m_ShadowPrice;
				minLink     = currLink;
			}
		}
		if (linkCost > minLinkCost)
		{
			DBG_TRACE(("FindMstUp returns free lowerbound at link %u at cost %s", minLink, AsString(minLinkCost).c_str()));
			return minLink;
		}

		UInt32 srcNode = sourceClaim - begin_ptr( htpInfo.m_Claims );
		if (!atMin)
		{
			DBG_TRACE(("FindMstUp signals source claim %u(%u, %u) of Facet %u as vacant", 
				srcNode, sourceClaim->m_ggTypeID, sourceClaim->m_RegionID,
				currLink)
			);

			htpInfo.m_TreeBuilder.add_node(srcNode, currLink, linkCost ); 
			htpInfo.m_ClaimIdList.push_back(srcNode); // maintain ordered built MST for splitter adjustments
			dms_assert( IsDefined(currLink) ); // we did check that feasible solution exists

			minLinkCost = linkCost;
			return currLink;
		}

		htpInfo.m_TreeBuilder.pop_node();

		dms_assert( htpInfo.CheckLink(currLink) );

#if defined MG_DEBUG
		if ( sourceClaim->Overflow() )
		{
			DBG_TRACE(("FindMstUp: SourceClaim %u(%u, %u) has overflow", 
				srcNode, sourceClaim->m_ggTypeID, sourceClaim->m_RegionID
			));
		}
#endif

		DBG_TRACE(("FindMstUp fixes link to source claim (%u, %u) of Facet %u at cost %s", 
			sourceClaim->m_ggTypeID, sourceClaim->m_RegionID,
			currLink,
			AsString(linkCost).c_str()
		));
		

		htpInfo.m_TreeBuilder.fix_link(	currLink, linkCost, dir_backward_tag() );         // bring all links from the destination of currLink into queue for further processing

		htpInfo.m_ClaimIdList.push_back( srcNode ); // maintain ordered built MST for splitter adjustments

		dms_assert( htpInfo.CheckLink(currLink) );
	}
}

template <typename S, typename AR, typename AT>
bool UpdateSplitterDown(htp_info_t<S, AR, AT>& htpInfo, claim<S>& root)
{
	DBG_START("DiscrAllocCells", "UpdateSplitterDown", DMS_DEBUG_DISCRALLOC);

	// make shortest path tree (dijstra) with claims as vertices and priority heaps as edges
	UInt32 rootClaimID = &root - begin_ptr( htpInfo.m_Claims );
	dms_assert(rootClaimID < htpInfo.GetNrNodes());

	DBG_TRACE(("SrcClaim %u(%u, %u) with claimrange [%d,%d] and price %s has %u assignees", 
		rootClaimID, root.m_ggTypeID, root.m_RegionID, 
		root.m_ClaimRange.first, root.m_ClaimRange.second, 
		AsString(root.m_ShadowPrice).c_str(),
		root.m_Count
	));


	typename htp_info_t<S, AR, AT>::cost_type freeClaimCost = MAX_VALUE(shadow_price<S>); //cost until dst of (free)link;
	if (root.m_Count <= root.m_ClaimRange.second) // Overflow created by going over min with positive price
	{
		dms_assert(root.m_Count -1 == root.m_ClaimRange.first);
		dms_assert(root.m_ShadowPrice > shadow_price<S>());
		freeClaimCost = root.m_ShadowPrice;
		DBG_TRACE( ("SrcClaim is over lowerbound with positive price reduction possible") );
	}

	UInt32 freeLink = FindMstDown(htpInfo, rootClaimID, freeClaimCost);

	if (freeClaimCost == MAX_VALUE(shadow_price<S>))
		return false;

	DBG_TRACE(("FindMstDown returned FreeLink %u at cost %s", freeLink, AsString(freeClaimCost).c_str() ));
	// adjust G such that transport from root to nearest free claim becomes a free lunch

	root.m_ShadowPrice -= freeClaimCost;

	// adjust G on whole MST (including dead ends) in ClaimIdList order (from root) 
	// to prevent unadministered facet crossings
	// laat schadowprijs van target stijgen op basis van (Ga - Gb) = (Qa - Qb)  
	//	=>  Gb := Ga - (Qa - Qb) = Ga + c, want c = -(Qa - Qb)

	for (auto claimIdPtr = htpInfo.m_ClaimIdList.begin(), claimIdEnd = htpInfo.m_ClaimIdList.end(); claimIdPtr != claimIdEnd; ++claimIdPtr)
	{
		const directed_heap_elem<shadow_price<S> >& traceBack = 
			htpInfo.m_TreeBuilder.get_traceback(*claimIdPtr);

		priority_heap<S>& ph = htpInfo.m_Facets[traceBack.Link()];

		UInt32 i = ph.top();
		S      c = ph.GetC(i);

		// this heap was visited and cleaned since last reallocation
		dms_assert(htpInfo.m_ResultArray[i] == ph.m_SourceClaim->m_ggTypeID);

		dms_assert( ph.m_TargetClaim->m_ShadowPrice
			>=	ph.m_SourceClaim->m_ShadowPrice
			+	shadow_price<S>(c, EPSILON(i * ph.m_PerturbationFactor) ) );

		ph.m_TargetClaim->m_ShadowPrice
			=	ph.m_SourceClaim->m_ShadowPrice
			+	shadow_price<S>(c, EPSILON(i * ph.m_PerturbationFactor) );

		dms_assert(htpInfo.GetLinkCost( traceBack.Link() ) == shadow_price<S>()); 
	}
	
#if defined(MG_DEBUG) // DEBUG BEGIN: check that all claimIds are still valid 
	{
		for (auto claimIdPtr = htpInfo.m_ClaimIdList.begin(), claimIdEnd = htpInfo.m_ClaimIdList.end(); claimIdPtr != claimIdEnd; ++claimIdPtr)
			dms_assert( htpInfo.CheckLink(htpInfo.m_TreeBuilder.get_traceback(*claimIdPtr).Link()) );
	}
#endif	// DEBUG END


	// realloc cells on path to first found claim with room (traceback from this node)

	while (IsDefined(freeLink))
	{
		// remove (c,i) from ph and add in the reverse queues
		priority_heap<S>& ph = htpInfo.m_Facets[freeLink];
		UInt32 i = ph.top(); 
		DBG_TRACE(
			(	"Relax Facet %u: (%u, %u)->(%u, %u) with cell %u", 
				freeLink, 
				ph.m_SourceClaim->m_ggTypeID,
				ph.m_SourceClaim->m_RegionID,
				ph.m_TargetClaim->m_ggTypeID,
				ph.m_TargetClaim->m_RegionID,
				i
			)
		);

		dms_assert(
			std::find(
				htpInfo.m_ClaimIdList.begin(), 
				htpInfo.m_ClaimIdList.end(), 
				htpInfo.GetDstNode(freeLink, dir_forward_tag() )
			) != htpInfo.m_ClaimIdList.end()
		);

		dms_assert(
			htpInfo.m_TreeBuilder.get_traceback(
				htpInfo.GetDstNode(freeLink, dir_forward_tag() ) 
			).Link() 
			== freeLink
		);

		// curr  forms a path through the adjusted MST indicated by ClaimList
		// We checked that freeLink is in ClaimList AND that each ClaimList elem guarantees the following
		dms_assert(htpInfo.GetLinkCost(freeLink) == shadow_price<S>()); 


		dms_assert(htpInfo.m_ResultArray[i] == ph.m_SourceClaim->m_ggTypeID);

		UInt32 ggTypeIdSrc = ph.m_SourceClaim->m_ggTypeID;
		UInt32 ggTypeIdDst = ph.m_TargetClaim->m_ggTypeID;

		dms_assert(htpInfo.m_ResultArray[i] == ggTypeIdSrc); ph.m_SourceClaim->m_Count--;
		AR ar = htpInfo.m_AtomicRegionMapData[i];
		RemoveLoserInResultAndCleanupQueues (htpInfo, ar, i, ggTypeIdSrc);
		InsertWinnerInResultAndReallocQueues(htpInfo, ar, i, ggTypeIdDst);
		dms_assert(htpInfo.m_ResultArray[i] == ggTypeIdDst); ph.m_TargetClaim->m_Count++;
		
#if defined(MG_DEBUG)
		if (ph.m_TargetClaim->Overflow())
		{
			reportF(SeverityTypeID::ST_MajorTrace, "UpdateSplitterDown: Realloc.Target %s has overflow",
				htpInfo.GetClaimRangeStr( *ph.m_TargetClaim ).c_str()
			);
		}

		if (ph.m_SourceClaim->Overflow())
		{
			reportF(SeverityTypeID::ST_MajorTrace, "UpdateSplitterDown: Realloc.Source %s has overflow",
				htpInfo.GetClaimRangeStr( *ph.m_SourceClaim ).c_str()
			);
		}
#endif

		dms_assert( ph.empty() || htpInfo.CheckLink(freeLink) ); 

		freeLink = htpInfo.m_TreeBuilder.get_traceback(htpInfo.GetSrcNode(freeLink, dir_forward_tag() )).Link();
	}

	return true;
}

template <typename S, typename AR, typename AT>
bool UpdateSplitterUp(htp_info_t<S, AR, AT>& htpInfo, claim<S>& root)
{
	DBG_START("DiscrAllocMinClaims", "UpdateSplitterUp", DMS_DEBUG_DISCRALLOC);

	// make shortest path tree (dijstra) with claims as vertices and priority heaps as edges
	UInt32 rootClaimID = &root - begin_ptr( htpInfo.m_Claims );
	dms_assert(rootClaimID < htpInfo.GetNrNodes());

	DBG_TRACE(("SrcClaim %u(%u, %u) with claimrange [%d,%d] and price %s has %u assignees", 
		rootClaimID, root.m_ggTypeID, root.m_RegionID, 
		root.m_ClaimRange.first, root.m_ClaimRange.second, AsString(root.m_ShadowPrice).c_str(),
		root.m_Count
	));


	typename htp_info_t<S, AR, AT>::cost_type freeClaimCost = MAX_VALUE(shadow_price<S>); //cost until dst of (free)link;
	if (root.m_Count >= root.m_ClaimRange.first) // Underflow created by going over min with negative price
	{
		dms_assert(root.m_ShadowPrice < shadow_price<S>());
		freeClaimCost = - root.m_ShadowPrice;
		DBG_TRACE( ("SrcClaim is over lowerbound with positive price reduction possible") );
	}

	UInt32 freeLink = FindMstUp(htpInfo, rootClaimID, freeClaimCost);

	if (freeClaimCost == MAX_VALUE(shadow_price<S>))
		return false;

	DBG_TRACE(("FindMstUp returned FreeLink %u at cost %s", freeLink, AsString(freeClaimCost).c_str() ));
	// adjust G such that transport from root to nearest free claim becomes a free lunch

	root.m_ShadowPrice += freeClaimCost;

	// adjust G on whole MST (including dead ends) in ClaimIdList order (from root) 
	// to prevent unadministered facet crossings
	// laat schadowprijs van target stijgen op basis van (Ga - Gb) = (Qa - Qb)  
	//	=>  Gb := Ga - (Qa - Qb) = Ga + c, want c = -(Qa - Qb)

	std::vector<UInt32>::const_iterator
		claimIdPtr = htpInfo.m_ClaimIdList.begin(),
		claimIdEnd = htpInfo.m_ClaimIdList.end();
	while (claimIdPtr != claimIdEnd)
	{
		const directed_heap_elem<shadow_price<S> >& traceBack = 
			htpInfo.m_TreeBuilder.get_traceback(*claimIdPtr++);

		priority_heap<S>& ph = htpInfo.m_Facets[traceBack.Link()];

		UInt32            i  = ph.top();
		S                 c  = ph.GetC(i);

		// this heap was visited and cleaned since last reallocation
		dms_assert(htpInfo.m_ResultArray[i] == ph.m_SourceClaim->m_ggTypeID);

		dms_assert( ph.m_TargetClaim->m_ShadowPrice
			>=	ph.m_SourceClaim->m_ShadowPrice
			+	shadow_price<S>(c, EPSILON(i * ph.m_PerturbationFactor) ) );

		ph.m_SourceClaim->m_ShadowPrice
			=	ph.m_TargetClaim->m_ShadowPrice
			-	shadow_price<S>(c, EPSILON(i * ph.m_PerturbationFactor) );

		dms_assert(htpInfo.GetLinkCost( traceBack.Link() ) == shadow_price<S>()); 
	}
	

#if defined(MG_DEBUG) // DEBUG BEGIN: check that all claimIds are still valid 
	{
		for (auto claimId: htpInfo.m_ClaimIdList)
			dms_assert( htpInfo.CheckLink(htpInfo.m_TreeBuilder.get_traceback(claimId).Link()) );
	}
#endif	// DEBUG END


	// realloc cells on path to first found claim with room (traceback from this node)
	//

	while (IsDefined(freeLink))
	{
		// remove (c,i) from ph and add in the reverse queues
		priority_heap<S>& ph = htpInfo.m_Facets[freeLink];
		SizeT i = ph.top(); 

		DBG_TRACE(
			(	"Pull Facet %u: (%u, %u)->(%u, %u) with cell %u", 
				freeLink, 
				ph.m_SourceClaim->m_ggTypeID,
				ph.m_SourceClaim->m_RegionID,
				ph.m_TargetClaim->m_ggTypeID,
				ph.m_TargetClaim->m_RegionID,
				i
			)
		);

		dms_assert(
			std::find(
				htpInfo.m_ClaimIdList.begin(), 
				htpInfo.m_ClaimIdList.end(), 
				htpInfo.GetDstNode(freeLink, dir_backward_tag() )
			) != htpInfo.m_ClaimIdList.end()
		); 

		dms_assert(
			htpInfo.m_TreeBuilder.get_traceback(
				htpInfo.GetDstNode(freeLink, dir_backward_tag() ) 
			).Link() 
			== freeLink
		);

		// curr  forms a path through the adjusted MST indicated by ClaimList
		// We checked that freeLink is in ClaimList AND that each ClaimList elem guarantees the following
		dms_assert(htpInfo.GetLinkCost(freeLink) == shadow_price<S>()); 


		dms_assert(htpInfo.m_ResultArray[i] == ph.m_SourceClaim->m_ggTypeID);

		UInt32 ggTypeIdSrc = ph.m_SourceClaim->m_ggTypeID;
		UInt32 ggTypeIdDst = ph.m_TargetClaim->m_ggTypeID;

		dms_assert(htpInfo.m_ResultArray[i] == ggTypeIdSrc); ph.m_SourceClaim->m_Count--;
		AR ar = htpInfo.m_AtomicRegionMapData[i];
		RemoveLoserInResultAndCleanupQueues (htpInfo, ar, i, ggTypeIdSrc);
		InsertWinnerInResultAndReallocQueues(htpInfo, ar, i, ggTypeIdDst);
		dms_assert(htpInfo.m_ResultArray[i] == ggTypeIdDst); ph.m_TargetClaim->m_Count++;
		
//		dms_assert(!ph.m_TargetClaim->Overflow()); target will be relaxed in next pull
#if defined(MG_DEBUG)
		if (ph.m_SourceClaim->Overflow())
		{
			reportF(SeverityTypeID::ST_MajorTrace, "UpdateSplitterUp: Realloc.Source %s has overflow",
				htpInfo.GetClaimRangeStr( *ph.m_SourceClaim ).c_str()
			);
		}
#endif

		dms_assert( ph.empty() || htpInfo.CheckLink(freeLink) ); 

		freeLink = htpInfo.m_TreeBuilder.get_traceback(htpInfo.GetSrcNode(freeLink, dir_backward_tag() )).Link();
	}

	return true;
}

// *****************************************************************************
//									DiscrAlloc
// *****************************************************************************

struct DistFromOpt
{
	UInt32 nrLandUnits, nrSubOptimal, nrDueToBelowThreshold;
	shadow_price<UInt64, Int64> totalSuit, totalDistFromOpt;
	tile_id tn;

	template <typename S, typename AR, typename AT>
	DistFromOpt(htp_info_t<S, AR, AT>& htpInfo)
		:	nrLandUnits()
		,	nrSubOptimal()
		,	nrDueToBelowThreshold()
		,	totalSuit()
		,	totalDistFromOpt()
		,	tn( htpInfo.m_MapDomain->GetNrTiles() )
	{
		UInt32 K = htpInfo.GetK();

		land_unit_id N = htpInfo.m_N;
		nrLandUnits += N;
//			nrOptions   += (K-1)*N;

		const AR* armIter = htpInfo.m_AtomicRegionMapData.begin();

		for(land_unit_id i=0; i < N; ++armIter, ++i) // cell index 0..N 
		{
			AT currBuyer = htpInfo.m_ResultArray[i];
			if (!IsDefined(currBuyer))
				continue;

			AR ar = *armIter; 

			shadow_price<S> currPrice(htpInfo.m_ggTypes[currBuyer].m_Suitabilities[i], i*currBuyer);
			totalSuit += currPrice;
			currPrice += htpInfo.GetClaim(ar, currBuyer).m_ShadowPrice;

			UInt32 iXj = 0;
			bool belowThreshold, foundHigherBidder = false;

			for(UInt32 j=0; j!=K; ++j)
			{
				S Sij = htpInfo.m_ggTypes[j].m_Suitabilities[i];
				shadow_price<S> bid(Sij, iXj);  // small pertubation (SoS) for making a difference between similar cells
				bid += htpInfo.GetClaim(ar, j).m_ShadowPrice;
				iXj += i;
				if (currPrice < bid)
				{
					foundHigherBidder = true;
					totalDistFromOpt.first += (bid.first - currPrice.first);
					totalDistFromOpt.second += bid.second;
					totalDistFromOpt.second -= currPrice.second;

					belowThreshold = (Sij < htpInfo.m_Threshold);
						
					dms_assert(belowThreshold); // check that deviation is legally caused by threshold that blocks very aversive shadow prices from being effective
					currPrice = bid; // prevent double counting of alternateve higher bids.
				}
			}
			if (foundHigherBidder)
			{
				++nrSubOptimal;
				if (belowThreshold)
					++nrDueToBelowThreshold;
			}
		}
	}
};

#if defined(MG_DEBUG)

template <typename S, typename AR, typename AT>
void CheckAllLinks(htp_info_t<S, AR, AT>& htpInfo)
{
	UInt32 L = htpInfo.GetNrLinks();
	for (UInt32 l = 0; l != L; ++l)
		dms_assert(htpInfo.GetLinkCost(l) >= shadow_price<S>());
}

#endif

template <typename S, typename AR, typename AT>
bool CheckAllClaims(const htp_info_t<S, AR, AT>& htpInfo, SharedStr* resultPtr)
{
	bool isAllOK = true;
	auto
		claimIter = htpInfo.m_Claims.begin(),
		claimEnd  = htpInfo.m_Claims.end();

	for (; claimIter != claimEnd; ++claimIter)
	{
		if (! claimIter->IsOK())
		{
			isAllOK = false;
			SharedStr claimResult = 
				mySSPrintF("%s; %u allocated for price %s; ",
					htpInfo.GetClaimRangeStr(*claimIter).c_str(),
					claimIter->m_Count, 
					AsString(claimIter->m_ShadowPrice).c_str()
				);
			
			reportF(SeverityTypeID::ST_MajorTrace, "CheckAllClaims failed: %s", claimResult.c_str());
			if (resultPtr)
				(*resultPtr) += claimResult;
		}
	}
	return isAllOK;
}

template <typename S, typename AR, typename AT>
void DiscrAllocCellsBegin(htp_info_t<S, AR, AT>& htpInfo, UInt32 nextI)
{
	// calc sum claims for report
	auto
		claimIter = htpInfo.m_Claims.begin(),
		claimEnd  = htpInfo.m_Claims.end();
	UInt32 sumMinClaim=0, sumMaxClaim=0;
	while (claimIter != claimEnd)
	{
		sumMinClaim += claimIter->m_ClaimRange.first;
		sumMaxClaim += claimIter->m_ClaimRange.second;

		++claimIter;
	}
	reportF(SeverityTypeID::ST_MajorTrace, "DiscrAlloc %u: claims for %u cells: min=%u; max=%u",
		htpInfo.GetN(), nextI,
		sumMinClaim,
		sumMaxClaim
	);
}

template <typename S, typename AR, typename AT>
void DiscrAllocEnd(htp_info_t<S, AR, AT>& htpInfo, UInt32 currI)
{
	// set StartPrice to CurrPrice and report maximum difference
	auto
		claimIter = htpInfo.m_Claims.begin(),
		claimEnd  = htpInfo.m_Claims.end();
	shadow_price<S> minPriceDiff, maxPriceDiff;
	while (claimIter != claimEnd)
	{
		shadow_price<S> priceDiff = claimIter->m_ShadowPrice - claimIter->m_StartPrice;
		MakeMin(minPriceDiff, priceDiff);
		MakeMax(maxPriceDiff, priceDiff);

		claimIter->m_StartPrice = claimIter->m_ShadowPrice;

		++claimIter;
	}
	reportF(SeverityTypeID::ST_MajorTrace, "DiscrAllocCells %u:  %u cells completed: price adjustments range from %s to %s",
		htpInfo.GetN(), currI,
		AsString(minPriceDiff).c_str(),
		AsString(maxPriceDiff).c_str()
	);
}

template <typename S, typename AR, typename AT>
void DiscrAllocCells(htp_info_t<S, AR, AT>& htpInfo, UInt32 currI, UInt32 nextI)
{
	DiscrAllocCellsBegin(htpInfo, nextI);

	land_unit_id N = htpInfo.m_N;
	UInt32 K = htpInfo.GetK();
	UInt32 rapFreq = 1000000, tmpK = K; while (tmpK > 3 && rapFreq > 100)  { rapFreq /= 10; tmpK /= 10; }

	dms_assert(htpInfo.m_AtomicRegionMapData.size() == N);
	dms_assert(htpInfo.m_ResultArray        .size() == N);

	const AR* armPtr = htpInfo.m_AtomicRegionMapData.begin();
	
	UInt32 d_nrSplits = 0;

	for( ; currI < nextI; htpInfo.GetNextPermutationValue(), ++currI)
	{
		dms_assert(htpInfo.m_CurrPI < htpInfo.m_N);
		AR ar = armPtr[htpInfo.m_CurrPI];
		dms_assert(ar < htpInfo.GetNrAtomicRegions()); // guaranteed by IncrementAtomicRegionCount
//		dms_assert(!IsDefined(htpInfo.m_ResultArray[htpInfo.m_CurrPI]));

		UInt32 highestBidder = UNDEFINED_VALUE(UInt32);
		shadow_price<S> highestBid = MIN_VALUE(shadow_price<S>);

		perturbation_type c = 0;

		for(UInt32 j=0; j!=K; ++j, c += htpInfo.m_CurrPI)
		{
			S s = htpInfo.m_ggTypes[j].m_Suitabilities[htpInfo.m_CurrPI];
			if (s < htpInfo.m_Threshold) continue;
			shadow_price<S> bid = htpInfo.GetClaim(ar, j).m_ShadowPrice;
			                bid.first  += s;
			                bid.second += c; // small pertubation (SoS) for making a difference between similar cells

			if (highestBid < bid)
			{
				highestBid    = bid;
				highestBidder = j;
			}
		}

		// insert highestBidder into solution and queues
		if (highestBidder == UNDEFINED_VALUE(UInt32) )
		{
			if (++htpInfo.m_NrBelowThreshold <= NR_BELOW_THRESHOLD_NOTIFICATIONS)
				reportF(SeverityTypeID::ST_MajorTrace,
					"DiscrAllocCells: all suitabilities of cell %u are below the threshold %d",
					htpInfo.m_CurrPI, htpInfo.m_Threshold
				); 
			htpInfo.m_ResultArray[htpInfo.m_CurrPI] = UNDEFINED_VALUE(AT);
			continue;
		}
		InsertWinnerInResultAndReallocQueues(htpInfo, ar, htpInfo.m_CurrPI, highestBidder);

		// Update total and move if facing claim-restriction
		claim<S>& claim = htpInfo.GetClaim(ar, highestBidder);
		++ claim.m_Count;
		if ( claim.Overflow() )
		{
			bool ok = UpdateSplitterDown(htpInfo, claim);
			htpInfo.m_ClaimIdList.clear();
			d_nrSplits++;

			if (!ok)
			{
				dms_assert(claim.m_Count > claim.m_ClaimRange.second);
				SizeT excess = claim.m_Count - claim.m_ClaimRange.second;
				if (PowerOf2(excess)) // only report power of 2 excess to limit quadratic behaviour of event log listbox and errors after 1000000 lines
					reportF(SeverityTypeID::ST_MajorTrace,
						"DiscrAlloc Warning: UpdateSplitterDown(%s) failed; now %u allocated",
						htpInfo.GetClaimRangeStr( claim ).c_str(),
						claim.m_Count
				); 
			}
			#if defined(MG_DEBUG)
				if (d_nrSplits % 20000 == 0)
					CheckAllLinks(htpInfo);
			#endif
		}

		if (currI % rapFreq==0) 
			reportF(SeverityTypeID::ST_MajorTrace,
				"DiscrAllocCells %u: Progress %u/%u; %u calls to UpdateSplitterDown",
				N, currI, nextI, d_nrSplits
			); 
	}
	dms_assert(htpInfo.m_CurrPI >= htpInfo.m_N);
	reportF(SeverityTypeID::ST_MajorTrace,
		"DiscrAllocCells %u: %u cells completed with %u calls to UpdateSplitterDown",
		N, currI, d_nrSplits
	); 
}

template <typename S, typename AR, typename AT>
void DiscrAllocMinClaims(htp_info_t<S, AR, AT>& htpInfo)
{
	UInt32 count = 0;
	auto
		claimIter = htpInfo.m_Claims.begin(),
		claimEnd  = htpInfo.m_Claims.end();
	while (claimIter != claimEnd)
	{
		bool ok = true;
		while (ok && claimIter->Underflow())
		{
			ok = UpdateSplitterUp(htpInfo, *claimIter);
			htpInfo.m_ClaimIdList.clear();
			++count;
		}
		if (!ok)
			reportF(SeverityTypeID::ST_MajorTrace,
				"DiscrAlloc Warning: UpdateSplitterUp(%s) failed; only %u allocated",
				htpInfo.GetClaimRangeStr( *claimIter ).c_str(),
				claimIter->m_Count
			); 
		++claimIter;
	}

	reportF(SeverityTypeID::ST_MajorTrace,
		"DiscrAllocMinClaims completed with %u calls to UpdateSplitterUp",
		count
	); 
}

template <typename S, typename AR, typename AT>
void SolveRange(htp_info_t<S, AR, AT>& htpInfo, UInt32 firstI, UInt32 lastI)
{
	DiscrAllocCells(htpInfo, firstI, lastI);
	#if defined(MG_DEBUG)
		CheckAllLinks(htpInfo);
	#endif
	DiscrAllocMinClaims(htpInfo);

	#if defined(MG_DEBUG)
		CheckAllLinks (htpInfo);
		CheckAllClaims(htpInfo, nullptr);
	#endif

	DiscrAllocEnd(htpInfo, lastI);
}

// *****************************************************************************
//									Solve
// *****************************************************************************

UInt32 muldiv_u32(UInt32 a, UInt32 b, UInt32 d)
{
	UInt64 a64 = a;
	UInt64 p64 = a64 * b;
	dms_assert(!a || p64 / a == b);
	if (!d)
	{
		dms_assert(!p64);
		return 0;
	}
	UInt32 r64 = p64 / d;
	return r64;
}

struct ClaimScaler: std::vector<claim_range>
{
//	std::vector<claim<S> >& m_Claims;

	template <typename S>
	ClaimScaler(std::vector<claim<S> >& claims)
	{
		reserve(claims.size());

		// save claimRange and set to 0 to make increasing sequence possible
		auto
			claimIter = claims.begin(),
			claimEnd  = claims.end();
		while (claimIter != claimEnd)
		{
			push_back(claimIter->m_ClaimRange);
			claimIter->m_ClaimRange = claim_range(0, 0);	
			++claimIter;
		}
	}

	template <typename S>
	void RestoreClaims(std::vector<claim<S> >& claims)
	{
		auto
			claimIter = claims.begin(),
			claimEnd  = claims.end();
		for (const_iterator orgClaimRangePtr = begin(); claimIter != claimEnd; ++claimIter)
			claimIter->m_ClaimRange = *orgClaimRangePtr++;
	}

	template <typename S, typename AR, typename AT>
	void ScaleClaims(htp_info_t<S, AR, AT>& htpInfo, UInt32* atomicRegionCount, UInt32 N, UInt32 nextI)
	{
		//	scale each claim: 
		//		for each AtomicRegion in UniqueRegion of (ggType, regionID)
		//			claim += orgClaim  * atomicRegionDensity * 
		//			with atomicRegionDensity := (ar_count / ar_size) * 
		//			and  ggTypeShare         := (link_flow / ar_size)
		//			with ar_count is the # of land units in ar so far at the current scaling (at the first nextI land units).
		//			with ar_size  is the # of land units in ar from the entire N land units.
		//			with link_flow is the # of cells from UniqueRegion ur that could possibly be allocated to AtomicRegion ar (result from the feasibility test that doesn't observe TransitionPotentials nor allow)

		if (nextI == N)
		{
			RestoreClaims<S>(htpInfo.m_Claims);
			return;
		}

		auto
			claimIter = htpInfo.m_Claims.begin(),
			claimEnd  = htpInfo.m_Claims.end();

		auto orgClaimRangePtr = begin();
		UInt32 claimID = 0;
		while (claimIter != claimEnd)
		{
			UInt32 totSize = 0, totCount = 0;
			UInt32 ur = htpInfo.ClaimID2UniqueRegionID(claimID);
			UInt32 lnk = htpInfo.m_Ar2Ur->GetFirstLink(ur, dir_backward_tag());
			while (IsDefined(lnk))
			{
				UInt32 ar = htpInfo.m_Ar2Ur->GetDstNode(lnk, dir_backward_tag());

				UInt32 ar_size   = htpInfo.m_AtomicRegionSizes[ar];
				UInt32 ar_count  = atomicRegionCount[ar];
				totSize += ar_size;
				totCount+= ar_count;
				
				lnk = htpInfo.m_Ar2Ur->GetNextLink(lnk, dir_backward_tag() );
			}

			// make sure claims are a non-decreasing sequence
			MakeMax(claimIter->m_ClaimRange.first,
				muldiv_u32( orgClaimRangePtr->first,  totCount, totSize ) );
			MakeMax(claimIter->m_ClaimRange.second,
				muldiv_u32( orgClaimRangePtr->second, totCount, totSize ) + 1);
			MakeMin(claimIter->m_ClaimRange.first,  orgClaimRangePtr->first );
			MakeMin(claimIter->m_ClaimRange.second, orgClaimRangePtr->second);
			dms_assert(claimIter->m_ClaimRange.first <= claimIter->m_ClaimRange.second);

			++claimID;
			++orgClaimRangePtr;
			++claimIter;
		}
	}
};

template <typename AR>
void IncrementAtomicRegionCount(std::vector<UInt32>& atomicRegionCount, const regions_info_t<AR>& regionInfo, SizeT i, SizeT e)
{
	// count per ar with stepSize
	for (; i < e; regionInfo.GetNextPermutationValue(), ++i)
	{
		dms_assert(regionInfo.m_CurrPI < regionInfo.m_N);
		UInt32 ar = regionInfo.m_AtomicRegionMapData[regionInfo.m_CurrPI];
		if (ar >= atomicRegionCount.size())
			regionInfo.m_AtomicRegionMap->GetAbstrValuesUnit()->throwItemErrorF("Value %u out of range of valid Atomic Regions", ar);
		++atomicRegionCount[ar];
	}
	dms_assert(regionInfo.m_CurrPI >= regionInfo.m_N);
}

template <typename S, typename AR, typename AT>
void Solve(htp_info_t<S, AR, AT>& htpInfo, S threshold, AbstrDataObject* resPrices)
{
	CDebugContextHandle debugContext("DiscrAlloc", "Solve", true);

	ClaimScaler orgClaimRangeArray(htpInfo.m_Claims);
	dms_assert( orgClaimRangeArray.size() == htpInfo.GetNrNodes() );

	htpInfo.m_Threshold = IsDefined(threshold) ? threshold : MinValue<S>();

	SizeT Na = htpInfo.GetN(), currI = 0;

	SizeT stepSize = 1;
	while (Na / stepSize > 1000) 
		stepSize *= stepFactor;

	htpInfo.SetStepSize(stepSize, 0);

	std::vector<UInt32> atomicRegionCount(htpInfo.GetNrAtomicRegions(), 0);
	UInt32* atomicRegionCountPtr = begin_ptr(atomicRegionCount);
	while (htpInfo.m_StepSize > 1)
	{
		reportF(SeverityTypeID::ST_MajorTrace, "DiscrAlloc: SolveScaled per %u cells", htpInfo.m_StepSize);

		UInt32 nextI = htpInfo.GetNrSteps();
		
		cursor_type curr = htpInfo.GetCursor();

		IncrementAtomicRegionCount(atomicRegionCount, htpInfo, currI, nextI); // count per ar with stepSize
		orgClaimRangeArray.ScaleClaims(htpInfo, atomicRegionCountPtr, Na, nextI);

		htpInfo.SetCursor(curr);
		SolveRange(htpInfo, currI, nextI);

		currI = nextI;

		htpInfo.SetStepSize(htpInfo.m_StepSize / stepFactor, htpInfo.m_StepSize);
	}

	// reset m_ClaimRanges to original values
	cursor_type curr = htpInfo.GetCursor();

	IncrementAtomicRegionCount(atomicRegionCount, htpInfo, currI, Na); // count per ar with stepSize
	orgClaimRangeArray.ScaleClaims(htpInfo, atomicRegionCountPtr, Na, Na);

	htpInfo.SetCursor(curr);

	SolveRange(htpInfo, currI, Na);
	StoreBidPricesCurrTile(htpInfo, resPrices, true);
}


// *****************************************************************************
//									StoreLanduseTypeInfo
// *****************************************************************************

template <typename S, typename AR, typename AT>
void StoreLanduseTypeInfo(htp_info_t<S, AR, AT>& htpInfo, bool isFeasible)
{
	for (UInt32 j = 0; j!= htpInfo.m_ggTypes.size(); ++j)
	{
		ggType_info_t<S>& ggTypeInfo = htpInfo.m_ggTypes[j];

		DataWriteLock spLock(ggTypeInfo.m_diResShadowPrices  , dms_rw_mode::write_only_all);
		DataWriteLock taLock(ggTypeInfo.m_diResTotalAllocated, dms_rw_mode::write_only_all);

		DataArray<S           >* doResSP = mutable_array_checkedcast<S>(spLock);
		DataArray<land_unit_id>* doResTA = mutable_array_checkedcast<land_unit_id>(taLock);
		auto daResSP = doResSP->GetDataWrite(no_tile, dms_rw_mode::write_only_all); auto resSPiter = daResSP.begin();
		auto daResTA = doResTA->GetDataWrite(no_tile, dms_rw_mode::write_only_all); auto resTAiter = daResTA.begin();

		UInt32 K = ggTypeInfo.m_NrClaims;
		dms_assert(daResSP.size() == K);
		dms_assert(daResTA.size() == K);

		if (isFeasible)
		{
			auto
				claimIter = htpInfo.m_Claims.begin() + ggTypeInfo.m_FirstClaimID,
				claimEnd  = claimIter                + K;
			while(claimIter != claimEnd)
			{
				*resSPiter++ = claimIter->m_ShadowPrice.first;
				*resTAiter++ = claimIter->m_Count;
				++claimIter;
			}
		}
		else
		{
			fast_fill(resSPiter, resSPiter+K, UNDEFINED_VALUE(S));
			fast_fill(resTAiter, resTAiter+K, UNDEFINED_VALUE(UInt32));
		}
		spLock.Commit();
		taLock.Commit();
	}
}


template <typename S, typename AR, typename AT>
void StoreBidPricesCurrTile(htp_info_t<S, AR, AT>& htpInfo, AbstrDataObject* bidPrices, bool isFeasible)
{
	auto priceData = mutable_array_checkedcast<S>(bidPrices)->GetDataWrite(no_tile, dms_rw_mode::write_only_all);

	land_unit_id n = htpInfo.m_N;

	if (isFeasible)
		for (land_unit_id i = 0; i!=n; ++i)
		{
			AT j = htpInfo.m_ResultArray[i];
			if (IsDefined(j))
				priceData[i] = htpInfo.m_ggTypes[j].m_Suitabilities[i] + htpInfo.GetClaim( htpInfo.m_AtomicRegionMapData[i], j).m_ShadowPrice.first;
			else
				priceData[i] = UNDEFINED_VALUE(S);
		}
	else
		fast_fill(priceData.begin(), priceData.end(), UNDEFINED_VALUE(S));
}

// *****************************************************************************
//									HitchcockTransportation operator
// *****************************************************************************

template <typename S, typename AR, typename AT>
class HitchcockTransportationOperator : public UndenaryOperator
{
	typedef DataArray<SharedStr>  Arg1Type;          // ggTypes->name
	typedef AbstrUnit             Arg2Type;          // domain of cells
//	typedef TreeItem              Arg3Type;          // container with columns with suitability maps
	typedef DataArray<S>          PriceType;         // suitability map type
	typedef DataArray<UInt8>      Arg4Type;          // ggType -> partitioning
	typedef DataArray<SharedStr>  Arg5Type;          // partitions->name
	typedef Unit<AR>              Arg6Type;          // atomic regions to unique regions mapping container
	typedef DataArray<AR>         Arg7Type;          // atomic regions map
	typedef DataArray<claim_type> ClaimType;         // Arg8 & 9 are containers with min resp. max claims; 
	typedef DataArray<S>          Arg10Threshold;    // Cutoff threshold
	                                              // arg11 is feasibility certificate
	// notes
	// All 'maps' (Arg3 subitems and Arg7) have the same domain, which can be tiles but doesn't have to be 2D. They specify attribute values per land unit.
	// The AR->UR mappings cannot be tiled. AR is relatively small (usally UInt16)

	typedef TreeItem      ResultType;        // container with landuse and allocResults
	typedef DataArray<AT> ResultLandUseType; // columnIndex per row
	typedef ClaimType     ResultTotalType;   
	typedef PriceType     ResultShadowPriceType; 
	using htp_info_type = htp_info_t<S, AR, AT>;
	bool m_MustAdjust;

public:
	HitchcockTransportationOperator(AbstrOperGroup* gr, bool mustAdjust)
		:	UndenaryOperator(gr, ResultType::GetStaticClass(), 
				Arg1Type::GetStaticClass(), // ggTypes->name array
				Arg2Type::GetStaticClass(), // allocUnit
				TreeItem::GetStaticClass(), // suitability maps
				Arg4Type::GetStaticClass(), // ggTypes->partitions
				Arg5Type::GetStaticClass(), // partitions->name
				Arg6Type::GetStaticClass(), // atomicRegions to unique regions mapping container
				Arg7Type::GetStaticClass(), // atomicRegions map
				TreeItem::GetStaticClass(), // minClaim container
				TreeItem::GetStaticClass(), // maxClaimContainer
				Arg10Threshold::GetStaticClass(),  // cutoff threshold
				TreeItem::GetStaticClass() // feasibility certificate
			) 
		,	m_MustAdjust(mustAdjust)
	{}

	void CreateResultCaller(TreeItemDualRef& resultHolder, const ArgRefs& args, OperationContext* fc, LispPtr) const override
	{
		dms_assert(args.size() == 11);

		const AbstrDataItem* ggTypeNamesA = AsDataItem(args[0]);
		dms_assert(ggTypeNamesA);

		const Unit<AT>*  ggTypeSet = checked_domain<AT>(GetItem(args[0]));

		const AbstrUnit* allocUnit = debug_cast<const AbstrUnit*>(GetItem(args[1]));

		UInt32 nrTypes = ggTypeSet->GetCount();
		MG_CHECK(nrTypes <= MAX_VALUE(AT));

		const AbstrDataItem* ggTypes2partitioningsA = AsDataItem(args[3]);
		dms_assert(ggTypes2partitioningsA);

		const AbstrDataItem* regionNamesA = AsDataItem(args[4]);
		dms_assert(regionNamesA);

		const Unit<AR>* atomicRegionUnit = debug_cast<const Arg6Type*>(GetItem(args[5]));
		dms_assert(atomicRegionUnit);
//		fc->AddDependency(atomicRegionUnit->GetAsLispExpr()); is al implicit dependency as being calculating argument

		const AbstrDataItem* atomicRegionMapA = AsDataItem(args[6]);
		MG_CHECK(atomicRegionMapA);

		if (!resultHolder)
			resultHolder = TreeItem::CreateCacheRoot();
		dbg_assert(resultHolder);
		TreeItem* res = resultHolder.GetNew();
		dms_assert(res);

		AbstrDataItem* resLanduse = CreateDataItem(res, GetTokenID_mt("landuse"), allocUnit, ggTypeSet);

		AbstrDataItem* resStatus =
			CreateDataItem(
				res
				, GetTokenID_mt("status")
				, Unit<Void>  ::GetStaticClass()->CreateDefault()
				, Unit<SharedStr>::GetStaticClass()->CreateDefault()
			);
		resStatus->SetKeepDataState(true);

		AbstrDataItem* resStatusFlag =
			CreateDataItem(
				res
				, GetTokenID_mt("statusFlag")
				, Unit<Void>::GetStaticClass()->CreateDefault()
				, Unit<Bool>::GetStaticClass()->CreateDefault()
			);
		resStatusFlag->SetKeepDataState(true);

		TreeItem* resShadowPriceContainer = res->CreateItem(GetTokenID_mt("shadow_prices"));
		TreeItem* resTotalAllocatedContainer = res->CreateItem(GetTokenID_mt("total_allocated"));

		fc->m_MetaInfo = make_noncopyable_any<htp_info_type>();
		htp_info_type& htpInfo = *noncopyable_any_cast<htp_info_type>(&fc->m_MetaInfo);

		// make AtomicRegionsSet and UniqeRegions -> (AtomicRegionsSet -> Region)
		CreateResultingItems(
			ggTypeNamesA,
			allocUnit,
			GetItem(args[2]),       // suitability maps container
			GetItem(args[7]),       // minClaimSet
			GetItem(args[8]),       // maxClaimSet
			ggTypes2partitioningsA, regionNamesA, atomicRegionUnit, atomicRegionMapA, // RegionGrids
			resShadowPriceContainer,
			resTotalAllocatedContainer,
			htpInfo,
			m_MustAdjust, fc
		);

		AbstrDataItem* resPrices = nullptr;
		if (htpInfo.m_PriceUnit)
			resPrices = CreateDataItem(res, GetTokenID_mt("bid_price"), allocUnit, htpInfo.m_PriceUnit);
	}

	bool CalcResult(TreeItemDualRef& resultHolder, const ArgRefs& args, OperationContext* fc, Explain::Context* context) const override
	{
		dms_assert(args.size() == 11);
		htp_info_type& htpInfo = *noncopyable_any_cast<htp_info_type>(&fc->m_MetaInfo);

		const Unit<AR>* atomicRegionUnit = debug_cast<const Arg6Type*>(GetItem(args[5]));
//		dms_assert(atomicRegionUnit->GetCurrRangeItem()->DataAllocated());

		const AbstrDataItem* atomicRegionMapA = AsDataItem(args[6]);

//	Recreate result MetaInfo
		TreeItem* res = resultHolder.GetNew();
		dms_assert(res);

		AbstrDataItem* resLanduse = AsDataItem(res->GetSubTreeItemByID(GetTokenID_mt("landuse")));

		AbstrDataItem* resPrices = nullptr;
		if (htpInfo.m_PriceUnit)
			resPrices = AsDataItem(res->GetSubTreeItemByID(GetTokenID_mt("bid_price")));

		AbstrDataItem* resStatus     = AsDataItem(res->GetSubTreeItemByID(GetTokenID_mt("status")));
		AbstrDataItem* resStatusFlag = AsDataItem(res->GetSubTreeItemByID(GetTokenID_mt("statusFlag")));

		SharedStr strStatus;
		bool isFeasible;

		if (!htpInfo.m_PriceUnit)
		{
			res->Fail("No suitability maps", FR_Data);
			isFeasible = false;
		}
		else
		{
			reportD(SeverityTypeID::ST_MajorTrace, "DiscrAlloc.Prepare started");

			PrepareClaims(htpInfo);
			PreparePartitionings(htpInfo, atomicRegionMapA, atomicRegionUnit);

			isFeasible = FeasibilityTest(htpInfo, strStatus);
			if (!isFeasible)
				strStatus = "DiscrAlloc.FeasibilityTest failed: "+ strStatus;
			else
			{
				DataReadLockSuitabilities(htpInfo);
				PrepareFacets(htpInfo);
				PrepareReport(htpInfo);
			}

			// all data read ?
			resultHolder.StopSupplInterest();

			htpInfo.m_ResultDataLock      = DataWriteLock(resLanduse, dms_rw_mode::write_only_all);
			htpInfo.m_ResultPriceDataLock = DataWriteLock(resPrices,  dms_rw_mode::write_only_all);

			if (isFeasible)
			{
				reportD(SeverityTypeID::ST_MajorTrace, "DiscrAlloc.Solve started with suitability container ", GetItem(args[2])->GetFullName().c_str());
				S threshold = GetTheCurrValue<S>(GetItem(args[9]));

				PrepareTileLock(htpInfo);
				Solve(htpInfo, threshold, htpInfo.m_ResultPriceDataLock.get());

				if (htpInfo.m_NrBelowThreshold > 0)
				{
					reportF(SeverityTypeID::ST_MajorTrace, "%d units%s without land use type suitability above the threshold and therefore unallocated"
						, htpInfo.m_NrBelowThreshold
						, htpInfo.m_NrBelowThreshold > NR_BELOW_THRESHOLD_NOTIFICATIONS ? ", of which only the first 5 were reported," : ""
							
					);
				}
				isFeasible = CheckAllClaims(htpInfo, &strStatus);
				if (isFeasible)
				{
					dms_assert(strStatus.empty());
					DistFromOpt distData(htpInfo);

					strStatus = "DiscrAlloc completed";

					if (distData.tn > 1)
						strStatus += mySSPrintF(" for %u tiles", distData.tn);

					strStatus += mySSPrintF(" with a total magnified suitability of %s over %u land units", 
						AsString(distData.totalSuit.first).c_str(), 
						distData.nrLandUnits
					);
					if (distData.nrLandUnits)
						strStatus += mySSPrintF(" = %lf per land unit", 
							Float64(distData.totalSuit.first) / Float64(distData.nrLandUnits)
						);

					if (distData.nrSubOptimal)
					{
						strStatus += mySSPrintF(" with at most %s from optimum due to %u(=%lf%%) better options", 
							AsString(distData.totalDistFromOpt.first).c_str(), 
							distData.nrSubOptimal, 100.0 * Float64(distData.nrSubOptimal) / Float64(distData.nrLandUnits)
						);
						if (distData.nrLandUnits)
							strStatus += mySSPrintF(" = %lf per land unit", 
								Float64(distData.totalDistFromOpt.first) / Float64(distData.nrLandUnits)
							);

						if (distData.nrDueToBelowThreshold)
						{
							if (distData.nrSubOptimal > distData.nrDueToBelowThreshold)
								strStatus += mySSPrintF(" of which %u", distData.nrDueToBelowThreshold);
							strStatus += " due to the threshold blocking effective shadow price adjustments";
						}

						if (distData.nrSubOptimal > distData.nrDueToBelowThreshold)
						{
							if ( distData.nrDueToBelowThreshold)
								strStatus +=  mySSPrintF(" and the remaining %u options", distData.nrSubOptimal - distData.nrDueToBelowThreshold);
							if (distData.tn > 1)
								strStatus += " possibly due to shadow price variations between tiles";
							else
								strStatus += " UNEXPECTED and SUSPECT, please report";
						}
					}
					else
						strStatus += ", which is optimal";
				}
			}
			else
			{
				PrepareResultTileLock(htpInfo, true);
				StoreBidPricesCurrTile(htpInfo, htpInfo.m_ResultPriceDataLock.get(), false);
			}

			StoreLanduseTypeInfo(htpInfo, isFeasible);

			dms_assert(resPrices);
			htpInfo.m_ResultArray = {}; // commit shadow tile to ResultDataLock
			htpInfo.m_ResultPriceDataLock.Commit();
			htpInfo.m_ResultDataLock.Commit();
		}

		reportD(SeverityTypeID::ST_MajorTrace, strStatus.c_str() );

		dms_assert(!strStatus.empty() || res->WasFailed(FR_Data));
//		DataWriteLock resStatusLock(resStatus, dms_rw_mode::write_only_mustzero);
		SetTheValue<SharedStr>(resStatus, strStatus);
//		resStatusLock.Commit();

//		DataWriteLock resStatusFlagLock(resStatusFlag, dms_rw_mode::write_only_mustzero);
		SetTheValue<Bool>(resStatusFlag, isFeasible);
//		resStatusFlagLock.Commit();

		res->SetIsInstantiated();
		return true;
	}
};

// *****************************************************************************
//											INSTANTIATION
// *****************************************************************************

namespace 
{
	oper_arg_policy da_oap[11] = { 
		    oper_arg_policy::calc_always,                                    // ggTypeNameArray
		    oper_arg_policy::calc_as_result,                                 // domainUnit
			oper_arg_policy::subst_with_subitems, // suitabilities
		    oper_arg_policy::calc_always,                                    // ggType -> partitioning relation
		    oper_arg_policy::calc_always,                                    // partitioningNameArray
			oper_arg_policy::subst_with_subitems, // atomicRegions met regioRefs
	        oper_arg_policy::calc_as_result,                                 // atomicRegions map
			oper_arg_policy::subst_with_subitems, // minClaims
			oper_arg_policy::subst_with_subitems, // maxClaims
		    oper_arg_policy::calc_as_result,                                 // cutoff value
			oper_arg_policy::calc_never,                                    // feasibilityCertificate
	};

	SpecialOperGroup hitchcockGroup   ("discrete_alloc",          11, da_oap);
	SpecialOperGroup hitchcockGroup_16("discrete_alloc_16",       11, da_oap);
	SpecialOperGroup greedyGroup("greedy_alloc", 11, da_oap);
	SpecialOperGroup greedyGroup_16("greedy_alloc_16", 11, da_oap);

	template <typename S, typename AR>
	struct HitchcockTransportationOperators
	{
		HitchcockTransportationOperators()
			: htpDefault(&hitchcockGroup, true)
			, htpDefault16(&hitchcockGroup_16, true) // only do untiled for large choice sets, don't expect this to run on Win32 anyway.
			, htpGreedy(&greedyGroup, false)
			, htpGreedy16(&greedyGroup_16, false) // only do untiled for large choice sets, don't expect this to run on Win32 anyway.
		{}


		HitchcockTransportationOperator<S, AR, UInt8> htpDefault, htpGreedy;
		HitchcockTransportationOperator<S, AR, UInt16> htpDefault16, htpGreedy16;
	};

	HitchcockTransportationOperators<Int32, UInt32> htp3232;
	HitchcockTransportationOperators<Int32, UInt16> htp3216;
	HitchcockTransportationOperators<Int32, UInt8 > htp3208;
}

/******************************************************************************/
