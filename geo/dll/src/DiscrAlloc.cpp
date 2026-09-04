// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////

#include "GeoPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif

// discrete_alloc operator: discrete allocation of land units to claims
// through iterated bidding rounds with price adjustment.

#include <algorithm> // std::sort, for the greedy/needy land unit ranking
#include "ser/PairStream.h" // operator<< for Pair, the base of shadow_price
#include "set/VectorFunc.h" // vector_zero_n
#include "xml/XMLOut.h" // MAX_TEXTOUT_SIZE

#include "dbg/debug.h"
#include "dbg/SeverityType.h"
#include "vt/Pair.h"
#include "mci/CompositeCast.h"
#include "mth/Mathlib.h"
#include "ptr/OwningPtrSizedArray.h"
#include "utl/StrFormat.h"
#include "vt/CheckedCalc.h" // CheckedAdd / CheckedSub, see the checked shadow price arithmetic below

#include "CheckedDomain.h"
#include "DataArray.h"
#include "DataItemClass.h"
#include "DisplayValue.h"
#include "MoreDataControllers.h"
#include "OperSignature.h"
#include "TreeItemClass.h"
#include "UnitClass.h"

#include "bi_graph.h" // used to apply Dijkstra on a bi_graph, needed for finding an shortest route through a graph of facets when adjusting the splitter.
#include "PCount.h"
#include "DataArrayValue.h"
#include "mem/MyContainers.h"

#include "RtcInterface.h"        // DMS_GetMajorVersionNumber, for the obsolete claim_* stubs at the end
#include "RtcVersionNumbers.h"   // DMS_VERSION_MAJOR, for their compile-time v21 removal tripwire

/*
	discrete allocation, O(n*k), see:
		Tokuyama, T., & Nakano, J.(1995). Efficient algorithms for the Hitchcock transportation problem. SIAM Journal on Computing, 24(3), 563-578.
		Koomen, E., Hilferink, M., & Borsboom-van Beurden, J. (2011). Introducing land use scanner (pp. 3-21). Springer Netherlands. paragraph 1.3.3

	See Abstract. The described splitter finding and scaling has been inspiration for this implementation
*/

/*
HTP<S> takes the following arguments:

	suitabilities: for each ggType (a.k.a land use type)
		{
			allocUnit -> S
		}

	claims: for each ggType:
		{	
			RegioGrid:	allocUnit->RegioSet[ggType]
			minClaim:	RegioSet[ggType]->UInt32
			maxClaim:	RegioSet[ggType]->UInt32 
		}

results in:
	landuse:	  allocUnit->ggType
	allocResults: for each ggType:
		{
			totalLand:	RegioSet[ggType]->UInt32
			shadowPrice:RegioSet[ggType]->S
		}

*/
/* 
*	summary of the algorithm:
*   - a solution of this HTP is a resulting assignment of land use and related allocResults that does not conflict with claims (i.e. is feasible) and that has a maximum total suitability, i.e. that no other feasible assignment exists with a higher total suitability.
*   - a solution is defined by a shadow price for each claim, such that alocation of all cells according to the suitabilities augmented by the shadow prices of the related claims, will meet the claim constraints.
*   - to quickly find almost correct shadow prices, the algorithm starts with a downscaled version of the problem, where the the number of land units (preferably by pseudo random selection) and claims have been reduced by a power of 4
*   - during each scale up, the shadow prices are adjusted when maximum claims are exceeded abnd after each scale up, minimum claims are satisfied by sufficient reallocation.
*   - for each claim pair that relate to overlapping land units, a.k.a. facet, keep a priority queue of cells ordered by non-decreasing cost of reallocation from the first ggType (aka land use type) to the second ggType.
*   - when a max claim is exceeded, the cell with the lowest reallocation cost is reallocated. 
*	- As each claim can have multiple facets and destination claims may also be saturated, a cheapest path is searched throught the graph of facets to the first accessible non-saturated destination claim. 
*	- The cost of each facet in this path is the difference between the augmented suitability of the source ggType and the augmented suitability of the destination ggType of the first valid land unit that is on top of the facet's priority queue.
*   - when reallocation is administered, each top land unit on the path of facets is reallocated and registered in the facets of the destination claims.
*   - reallocated land units are added to the priority queues of the destination claim but only removed from the priority queues of the source claim once visited (lazily)
*   - note that shadow price adaptations don't affect the relative order of land units in each priority queue, so the order of the priority queues is preserved.
*   - conversely, when a min claim is not met, realllocation is sought of land units with ggTypes who's minimum claim have slack.
* 
*  Complexity:
*  - as the facets are organized such that the cost of reallocation of land units in their priority queues can be calulated in (armortized) constant time
*  - and the number of facets is O(k*k*p) where k is the number of claims and p the number of partitions
*  - making one reallocation is bounded by O(k*p) search steps and adjustments
*  - the scaling provides an upper bound on the expected number of reallocations
*  - The algorithm is O(n*k) where n is the number of land units and k the number of claims.
*
	SMALL PERTURBATIONS
	In order to make exact allocation possible, equal suitabilities are virtually perturbated.
	It should never be the case that for any two cells i,h and types j,k,
	the distance from points i and h to the facet (j,k) is equal
	or formally:

	(R1):

		(S_ij + C_j ) - (S_ik + C_k) <> (S_hj + C_j) - (S_hk + C_k)
	or	(S_ij - S_ik) + (C_j  - C_k) <> (S_hj - S_hk) + ( C_j - C_k)
	or	(S_ij - S_ik)                <> (S_hj - S_hk)

	unless i==h OR j==k.


	This is achieved by applying symbolic perturbations to the cost values and
	require some administration which points are compared. 

	S_ij(epsilon) := S_ij + epsilon*i*j

	Thus S_ij(epsilon) <> S_kh(epsilon) for i<>k XOR j<>h
	since S_ij == Skh implies S_ij(epsilon) - S_kh(epsilon) == epsilon*(ij - kh)

	and (S_ij(epsilon) - S_ik(epsilon)) - (S_hj(epsilon) - S_hk(epsilon))
	== epsilon * [(ij - ik) - (hj - hk)]
	== epsilon * [(i-h)(j-k)],
	which fullfills requirement (R1).

	The sufficiency of (R1) and thus the fact that degeneracies such as S_ij == S_hk for i<>h AND j<>k doesn't matter
	follows from close analysis of the used operators:
		
	- We take and count maxima per cell i of (S_ij + C_j) over j.
	- We keep a queue of cells i for each communicating (j,k) facet, strictly ordered by (S_ij(epsilon) - S_ik(epsilon)), small values have priority
	- We update C_j(epsilon) := C_k(epsilon) - (S_ij(epsilon) - S_ik(epsilon)) using facet (j,k)

	  =>        C[j].first    := C[k].first    - heap(j,k).top.first
	  =>        C[j].second   := C[k].second   - epsilon * i * (j-k)


	See:
	Edelsbrunner, H. And Mücke, E. Simulation of simplicity: a technique to cope with degenerate cases in geometric algorithms, 4th Annual ACM Symposium on Computational Geometry (1988) 118-133.
*/

const bool DMS_DEBUG_DISCRALLOC = MG_DEBUGCODE( true ||) false;

// Which argument layout an operator instantiation takes. NOTE that the last two names describe
// the ARGUMENTS, not the behaviour: FeasibilityTest always runs, for every version. What
// ..._with_feasibility_test adds is an 11th argument, the feasibility certificate, registered
// calc_never and never read -- it exists so a config can make that certificate a dependency of
// the allocation.
enum class discr_alloc_version
{
	no_partition,
	one_partition,
	multiple_partitions_without_feasibility_test,
	multiple_partitions_with_feasibility_test
};

// The allocation regimes are deliberately kept apart: each one is a complete algorithm on its own
// and none of them is used to prime another (no price pre-heating), so that what a result means can
// be documented per operator without reference to the others.
//
//   hitchcock: the exact transportation solve; shadow prices, facet queues and reallocation.
//   greedy   : rank the land units ONCE by their best suitability, then serve them in that order.
//   needy    : idem, but ranked by regret (best minus next best), so the units with the most to
//              lose from being denied their favourite type are served first.
//
// greedy and needy share all their machinery (SolveGreedy) and differ only in the ranking key.
// Neither of them computes or reports shadow prices.
enum class alloc_regime { hitchcock, greedy, needy };

inline CharPtr AllocRegimeName(alloc_regime regime)
{
	switch (regime) {
		case alloc_regime::greedy: return "greedy_alloc";
		case alloc_regime::needy:  return "needy_alloc";
		default:                   return "discrete_alloc";
	}
}

// *****************************************************************************
//									Helping structures
// *****************************************************************************

using atomic_region_count_t = UInt32;  
using claim_type = UInt32;  // will be varied  
using claim_range = Couple<claim_type>; // allow min > max in this data-type  
using claim_id = UInt32;    // set of all claims, a (subset of a) partitioning of #AR * #AT  
using facet_id = UInt32;    // set of claim substitution possibilities  
using facet_code = UInt32;  // set of #AR * k * k, which is renumbered by m_FacetIDs to facet_id  

// The Simulation-of-Simplicity perturbation term, see Edelsbrunner, 1990, and the file header.
// It is the template parameter P throughout this file, NOT a fixed type: the discrete_alloc
// family is instantiated twice, once at Int32 and once -- under the _pi64 name suffix -- at
// Int64, while greedy_alloc and needy_alloc take Int32 only, since they never use it. The
// perturbation reaches i * (K-1) for i the land unit id and K the number of land use types, so
// Int32 aliases two land units within one facet past 2^31 / (K-1) land units, which breaks
// requirement (R1) above silently (the wrap itself is defined). See the instantiations at the
// bottom of this file, and the checked arithmetic that reports the aliasing when it happens.

using land_unit_id = claim_type; // representation of number of units allocated to one class; must correspond with claim_type  
using partitioning_id = UInt8;

const land_unit_id NR_BELOW_THRESHOLD_NOTIFICATIONS = 5;

// -----------------------------------------------------------------------------
// shadow_price<S, P>
// -----------------------------------------------------------------------------
// A price -- or a facet cost -- carrying its Simulation-of-Simplicity perturbation alongside it:
//   .first   the real value, in suitability units
//   .second  the infinitesimal term, the epsilon coefficient derived in the file header
// Pair's lexicographic comparison then reads "compare prices, and on a tie compare
// perturbations", which is exactly the strict order the algorithm needs and the reason
// degenerate (equal suitability) input does not stall it. Arithmetic is componentwise, so the
// perturbations compose just as the epsilon algebra in the file header requires.
//
// Kept small and trivially copyable: it is passed by value throughout the facet queues and the
// splitter search. That is also what the perturbation width costs: 8 bytes at P == Int32 against
// 16 at P == Int64, on a type that claim<S, P> holds twice and that every splitter step copies.
// -----------------------------------------------------------------------------

template <typename S, typename P>
struct shadow_price : Pair<S, P>
{
	using base_type = Pair<S, P>;

	shadow_price(S s, P p) : base_type(s, p) {}
	shadow_price(const base_type& src) : base_type(src) {}
	shadow_price() = default;
};

template <typename S, typename P> struct minmax_traits<shadow_price<S, P> > : minmax_traits< Pair<S, P> > {};


// -----------------------------------------------------------------------------
// Overflow-checked shadow price arithmetic
// -----------------------------------------------------------------------------
// Every addition and subtraction that produces a shadow price goes through the functions below,
// and every one of them can throw. That is deliberate: an overflowed shadow price does not give a
// slightly wrong allocation, it gives one whose optimality argument has silently stopped holding,
// and CheckAllClaims will not necessarily notice. Since C++20 the wrap itself is defined
// behaviour, so there is nothing for a sanitizer to catch either. See issue #1196.
//
// The two components overflow for different reasons and have different remedies, so the thrown
// exception records which one it was:
//   .first  the price, in suitability units. It drifts across scaling rounds and is bounded only
//           by the range of the suitability values the config hands in.
//   .second the Simulation-of-Simplicity perturbation. It reaches i * (K-1) for i a land unit id
//           and K the number of land use types, and it is what the _pi64 operator names widen.
// CalcResult catches this and turns it into a config-level message carrying that advice.
//
// What is NOT checked, and why: compare_oper::GetC, which only ORDERS land units within one facet
// queue and is called O(n * k * log n) times. The same difference IS checked in
// priority_heap::GetC, which is the one that turns it into a cost, so a suitability range wide
// enough to wrap is reported the moment such a land unit reaches the top of a queue.
// -----------------------------------------------------------------------------

enum class price_component { price, perturbation };

// Thrown by everything below. A type of its own, rather than a plain DmsException, so that
// CalcResult can tell an arithmetic overflow -- for which widening the perturbation is the
// remedy -- from any other error raised while solving.
struct shadow_price_overflow : std::exception
{
	shadow_price_overflow(SharedStr why, price_component component)
		: m_Why(why), m_Component(component)
	{}

	CharPtr what() const noexcept override { return m_Why.c_str(); }

	SharedStr       m_Why;
	price_component m_Component;
};

template <price_component C, typename T>
T CheckedPriceComponentAdd(T a, T b)
{
	try {
		return CheckedAdd<T>(a, b, false);
	}
	catch (const DmsException& x) {
		throw shadow_price_overflow(x.AsErrMsg()->Why(), C);
	}
}

template <price_component C, typename T>
T CheckedPriceComponentSub(T a, T b)
{
	try {
		return CheckedSub<T>(a, b, false);
	}
	catch (const DmsException& x) {
		throw shadow_price_overflow(x.AsErrMsg()->Why(), C);
	}
}

template <typename S> S CheckedPriceAdd(S a, S b) { return CheckedPriceComponentAdd<price_component::price>(a, b); }
template <typename S> S CheckedPriceSub(S a, S b) { return CheckedPriceComponentSub<price_component::price>(a, b); }

template <typename P> P CheckedPerturbationAdd(P a, P b) { return CheckedPriceComponentAdd<price_component::perturbation>(a, b); }
template <typename P> P CheckedPerturbationSub(P a, P b) { return CheckedPriceComponentSub<price_component::perturbation>(a, b); }

// Every Simulation-of-Simplicity perturbation formed anywhere below is a land unit id times a
// ggType index, or times a difference of two of them: i*j for type j's bid on land unit i, and
// i*(j-k) for a transfer through facet (j, k) -- see SMALL PERTURBATIONS in the file header. With
// i <= N-1 and both j and |j-k| <= K-1, (N-1)*(K-1) bounds every one of them in magnitude.
//
// The bidding loops carry their running i*j in the increment clause of the for statement, so the
// accumulator steps once past its last use and ends the loop holding i*K. That value is never
// read, but it must still not overflow, which is why the bound checked here is (N-1)*K.
//
// Checking that once, before the solve, is what lets every site below do its perturbation
// arithmetic directly in P: no product can overflow, so none of them needs a per-value check.
// The alternative -- forming each perturbation in Int64 and narrowing it back with a test at every
// use -- asked the same question millions of times over to reach an answer that is fixed by N and
// K alone. Compiled away entirely for P == Int64, where the bound cannot be exceeded: a land unit
// count is a UInt32 and K <= 2^16, so (N-1)*K < 2^48.
template <typename P>
void CheckPerturbationRange(land_unit_id n, UInt32 k)
{
	if constexpr (sizeof(P) < sizeof(Int64))
	{
		if (!n || !k)
			return;

		Int64 highest = Int64(n - 1) * Int64(k);
		if (highest != Int64(P(highest)))
			throw shadow_price_overflow(
				mySSPrintF("The Simulation-of-Simplicity perturbations of {} land units over {} land use types"
					" run up to {}, which does not fit in {}, so two land units within one facet would share a"
					" perturbation and the strict ordering that the facet queues, the termination argument and"
					" the splitter invariants rest on no longer holds"
					, n, k, highest
					, AsString(ValueWrap<P>::GetStaticClass()->GetID())
				)
			,	price_component::perturbation
			);
	}
}

// i * (j - k): the perturbation delta of a transfer through facet (j, k). Safe in P because
// CheckPerturbationRange has already established that (N-1)*K fits, and i <= N-1, |j-k| <= K-1.
template <typename P>
P PerturbationOf(land_unit_id i, P perturbationFactor)
{
	return P(i) * perturbationFactor;
}

template <typename S, typename P>
shadow_price<S, P> CheckedAdd(shadow_price<S, P> a, shadow_price<S, P> b)
{
	return shadow_price<S, P>(CheckedPriceAdd(a.first, b.first), CheckedPerturbationAdd(a.second, b.second));
}

template <typename S, typename P>
shadow_price<S, P> CheckedSub(shadow_price<S, P> a, shadow_price<S, P> b)
{
	return shadow_price<S, P>(CheckedPriceSub(a.first, b.first), CheckedPerturbationSub(a.second, b.second));
}

// The customization point directed_dijkstra (bi_graph.h) accumulates path cost with. The running
// total of facet costs is a shadow price like any other and must not wrap either:
// MaxValue<shadow_price<S, P> >() doubles as the "no free claim" sentinel in both splitters, so a
// wrapped total can compare as CHEAPER than the sentinel and be taken for a real path.
template <typename S, typename P>
shadow_price<S, P> CheckedCostAdd(shadow_price<S, P> a, shadow_price<S, P> b)
{
	return CheckedAdd(a, b);
}

// Accumulate a shadow price into a wider one; used for the reporting totals in DistFromOpt.
template <typename S, typename P, typename S2, typename P2>
void CheckedAccumulate(shadow_price<S, P>& self, shadow_price<S2, P2> other)
{
	static_assert(sizeof(S) >= sizeof(S2) && sizeof(P) >= sizeof(P2)); // else the widening below truncates
	self.first  = CheckedPriceAdd<S>(self.first, other.first);
	self.second = CheckedPerturbationAdd<P>(self.second, other.second);
}


// -----------------------------------------------------------------------------
// claim<S, P>
// -----------------------------------------------------------------------------
// The allocation state of one (ggType, region) pair: how many land units it currently holds, and
// the shadow price that steers land units towards or away from it. A solution of the whole
// problem IS a set of shadow prices -- one per claim -- for which allocating every land unit to
// its highest augmented suitability (S_ij + price_j) happens to respect all claim bounds.
//
//   m_ClaimRange      [min, max]. min > max is rejected by FeasibilityTest, not here, so the
//                     predicates below do not assume min <= max.
//   m_Count           land units currently allocated to this claim.
//   m_ShadowPrice     the current price; positive means oversubscribed, negative undersubscribed.
//   m_StartPrice      snapshot at the start of the scaling round, for reporting the adjustment.
//   m_FirstOutHeapID  head of this claim's outgoing facets, m_FirstInpHeapID of its incoming
//                     ones. Both are singly linked through priority_heap::m_NextOut/InpHeapID,
//                     which is what lets htp_info_t double as the directed_graph over which
//                     FindMstDown / FindMstUp search for a claim with room.
// -----------------------------------------------------------------------------

template <typename S, typename P>
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
	shadow_price<S, P>  m_ShadowPrice, m_StartPrice;
	facet_id         m_FirstOutHeapID; // singly linked via facet array
	facet_id         m_FirstInpHeapID; // singly linked via facet array

	// A claim is at its bound either because the count says so, or because the shadow price has
	// already moved past zero within the slack between min and max -- price and count are two
	// views of the same saturation. Overflow/Underflow are the strict forms of AtMax/AtMin.
	bool Overflow ()  const { return m_Count >  m_ClaimRange.second || (m_Count >  m_ClaimRange.first  && m_ShadowPrice > shadow_price<S, P>()); }
	bool AtMax    ()  const { return m_Count >= m_ClaimRange.second || (m_Count >= m_ClaimRange.first  && m_ShadowPrice > shadow_price<S, P>()); }
	bool Underflow()  const { return m_Count <  m_ClaimRange.first  || (m_Count <  m_ClaimRange.second && m_ShadowPrice < shadow_price<S, P>()); }
	bool AtMin    ()  const { return m_Count <= m_ClaimRange.first  || (m_Count <=  m_ClaimRange.second && m_ShadowPrice < shadow_price<S, P>()); }

	// IsOK() is exactly !Overflow() && !Underflow() written out: inside [min, max] a positive
	// price is only admissible at min and a negative one only at max. The parentheses are load
	// bearing -- de Morgan on the two || forms above is what turns them into these two.
	bool IsOK     ()  const 
	{ 
		return 
				m_Count >= m_ClaimRange.first 
			&&	m_Count <= m_ClaimRange.second 
			&& (m_ShadowPrice <= shadow_price<S, P>() || m_Count == m_ClaimRange.first)
			&& (m_ShadowPrice >= shadow_price<S, P>() || m_Count == m_ClaimRange.second);
	}
};

// ========================== priority_heap ===================================
// One facet: the directed pair (src claim, dst claim) of two claims that share land units and
// differ in ggType. It holds the land units currently allocated to src that could move to dst,
// ordered by the marginal cost of that move,
//     cost(i) = S_src(i) - S_dst(i)
// so that the front is always the cheapest land unit to give up. That is what makes deciding a
// reallocation O(1) once the facet is known, and it is why the order must not depend on the
// shadow prices: those shift both claims by the same amount and so leave the order intact.
//
// ORDERING
// std::push_heap/pop_heap build a MAX-heap under the comparator, and compare_oper calls the
// HIGHER cost "less" -- so front() is the minimum-cost land unit.
//
// TIES
// Equal costs are broken by land unit id, in a direction-dependent way that matches the
// Simulation-of-Simplicity perturbation applied outside this struct (see the file header):
//   src.ggTypeID > dst.ggTypeID (m_LhsDominates): the LOWER id wins;
//   src.ggTypeID < dst.ggTypeID                 : the HIGHER id wins.
// Together with that perturbation the order is strict -- no two land units ever compare equal --
// which is what lets the algorithm terminate deterministically on degenerate input.
//
// LAZY DELETION
// A land unit that has moved elsewhere is NOT removed when it moves; it stays as "dirt" until it
// surfaces. Readers must therefore skip entries whose current allocation is no longer the source
// ggType. RemoveLoserInResultAndCleanupQueues does that, and maintains the invariant that the
// only remaining dirt is covered dirt -- never at the top.
//
// MEMORY
// Inherits privately from my_vec_t<land_unit_id>; the heap range is the whole vector. Across all
// facets these queues jointly hold every land unit that ever lost a confrontation (~n x (k-1)
// entries), which is the operator's real working set -- so they go through the allocation stocks,
// where the census sees them.
//
// Not thread-safe.
// ============================================================================
template <typename S, typename P>
struct priority_heap : private my_vec_t<land_unit_id>
{
	// Construct a facet heap connecting source->target claim.
	// Registers this heap in singly linked adjacency lists of both claims.
	priority_heap(facet_id thisHeapID,
               claim<S, P>* src,
               claim<S, P>* dst,
               const S* srcSuitMapBegin,
               const S* dstSuitMapBegin)
		:	m_NextOutHeapID(src->m_FirstOutHeapID)
		,	m_NextInpHeapID(dst->m_FirstInpHeapID)
		,	m_SourceClaim(src)
		,	m_TargetClaim(dst)
		// signed on purpose: m_ggTypeID is UInt32, so a bare src - dst is an UNSIGNED difference.
		// It only landed on the right negative value because the result used to be narrowed to
		// Int32; at P == Int64 the wrap survives and the tie-break direction below inverts.
		,	m_PerturbationFactor(P(src->m_ggTypeID) - P(dst->m_ggTypeID))
		,	m_Compare(src->m_ggTypeID > dst->m_ggTypeID, srcSuitMapBegin, dstSuitMapBegin)
	{
		dms_assert(m_PerturbationFactor != 0); // same ggType cannot form a facet
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

	// Raw marginal cost S_src(i) - S_dst(i) (ignores shadow prices), overflow-checked because
	// this is the one that becomes a cost; compare_oper::GetC below is the unchecked twin that
	// only orders the queue. See the note on the checked arithmetic above.
	S GetC(land_unit_id i) const
	{
		return CheckedPriceSub(m_Compare.m_SrcSuitabilityMapBegin[i], m_Compare.m_DstSuitabilityMapBegin[i]);
	}

	// The same difference without the check, for htp_info_t::GetLinkCostUnchecked.
	S GetCUnchecked(land_unit_id i) const
	{
		return m_Compare.GetC(i);
	}

	// Pop the current top (highest priority) element.
	// Caller is responsible for skipping stale elements beforehand.
	void pop()
	{ 
		std::pop_heap(this->begin(), this->end(), m_Compare);
		this->pop_back();
	}

	bool empty() const { return this->size() == 0; }

	// Comparator implementing min-heap (via inverted predicate logic).
	struct compare_oper
	{ 
		compare_oper(bool lhsDominates,
               const S* srcSuitMapBegin,
               const S* dstSuitMapBegin)
			:	m_LhsDominates(lhsDominates)
			,	m_SrcSuitabilityMapBegin(srcSuitMapBegin)
			,	m_DstSuitabilityMapBegin(dstSuitMapBegin)
		{}

		// Marginal cost of moving land unit i from src to dst (no shadow prices).
		// Deliberately unchecked: this runs on the heap's hot path and only decides an ORDER.
		// priority_heap::GetC above is the checked twin. See the note on the checked arithmetic.
		S GetC(land_unit_id i) const
		{
			return m_SrcSuitabilityMapBegin[i] - m_DstSuitabilityMapBegin[i];
		}

		// Return true if lhs has strictly LOWER priority than rhs.
		// (std::push_heap expects a "less priority" predicate.)
		bool operator ()(land_unit_id lhs, land_unit_id rhs) const
		{
			S lhsFirst = GetC(lhs);
			S rhsFirst = GetC(rhs);
			return lhsFirst > rhsFirst               // bigger cost => lower priority
				|| (lhsFirst == rhsFirst &&
					( m_LhsDominates
						? (lhs > rhs)               // tie-break strategy A
						: (lhs < rhs)               // tie-break strategy B
					)
       );
		}

		bool LhsDominated() const { return m_LhsDominates; }

	private:
		bool m_LhsDominates; // Direction-dependent tie policy indicator.

	public:
		const S* m_SrcSuitabilityMapBegin = nullptr; // Array start: S_src(i)
		const S* m_DstSuitabilityMapBegin = nullptr; // Array start: S_dst(i)
	} m_Compare;

	// Linked list indices to other outgoing/incoming facets (per claim).
	facet_id          m_NextOutHeapID;
	facet_id          m_NextInpHeapID;

	// Sign encodes direction (src ggTypeID - dst ggTypeID); used for perturbation.
	P                 m_PerturbationFactor;

	// Owning claim pointers (not owning memory).
	claim<S, P>*         m_SourceClaim;
	claim<S, P>*         m_TargetClaim;
};
// ============================================================================
// End priority_heap
// ============================================================================

// -----------------------------------------------------------------------------
// ggType_meta_t / ggType_info_t<S>
// -----------------------------------------------------------------------------
// One land use type (ggType). As with the partitionings the state is split in two:
//
//   ggType_meta_t   what is resolved once, at CreateResultingItems time, and cached in the
//                   result's persistent m_ReadAssets: the name, which partitioning this type's
//                   claims are stated in, and weak handles to its config arguments and to the
//                   result sub-items it owns.
//   ggType_info_t   what only exists while a computation runs: this type's slice of the claims
//                   array, and the read lock on -- plus a direct pointer into -- its suitability
//                   map, which is a memory-mapped file read once per land unit per type.
//
// S is the suitability / price value type (Int32 in every instantiation registered below).
// -----------------------------------------------------------------------------


struct ggType_meta_t
{
	SharedStr                     m_strName;
	TokenID                       m_NameID;
	UInt32                        m_PartitioningID = 0;

	// Raw-ptr hygiene (migration §weak-sweep): these point to config args / result sub-items but live in the
	// persistent htp_meta (m_ReadAssets), which outlives a single computation and can survive into config teardown.
	// Held weak so a deref after the target is destroyed is caught (lock_or_cancel throws task_canceled) instead of
	// dangling. Kept of-interest during compute via funcDC.AddDependency on their DCs (see CreateResultingItems).
	std::weak_ptr<const AbstrDataItem> m_diMinClaims;
	std::weak_ptr<const AbstrDataItem> m_diMaxClaims;

	std::weak_ptr<const AbstrDataItem> m_diSuitabilityMap;
	std::weak_ptr<AbstrDataItem> m_diResShadowPrices;
	std::weak_ptr<AbstrDataItem> m_diResTotalAllocated;

	// The greedy and needy regimes (mustAdjust == false) have no shadow_prices/<name> result member,
	// so m_diResShadowPrices stays an EMPTY weak. That is indistinguishable from an EXPIRED weak, which
	// lock_or_cancel must keep treating as cancellation; hence this explicit "was it ever created" flag.
	// See CreateResultingItems and StoreLanduseTypeInfo (issue #1171).
	bool m_HasResShadowPrices = false;
};

template <typename S>
struct ggType_info_t : ggType_meta_t
{
	claim_id                      m_FirstClaimID = UNDEFINED_VALUE(claim_id);  // ref into m_Claims array
	UInt32                        m_NrClaims = 0;      // limits range of m_Claims array for this ggType (aka land use type)

	DataReadLock                  m_SuitabilityDataLock;
	typename DataArray<S>::locked_cseq_t m_Suitabilities; // 1 per grid-cell, points directly into memory mapped file
};


// -----------------------------------------------------------------------------
// partitioning_meta_t / partitioning_info_t<AR>
// -----------------------------------------------------------------------------
// A "partitioning" groups atomic regions (AR) into regions; several partitionings may coexist
// (municipalities, provinces, planning zones, ...) and each ggType names the one its claims are
// stated in. The AR -> region mapping is either an explicit attribute, or -- when the
// partitioning IS the atomic region set -- the identity.
//
// The split mirrors the meta/info split of this file: partitioning_meta_t is the part cached in
// the result's persistent m_ReadAssets and therefore holds only weak handles (see the raw-ptr
// hygiene notes on the members below for why); partitioning_info_t<AR> adds the state that only
// exists while a computation runs.
//
// Life cycle
//   1. constructed from either an AR -> region attribute or the atomic region unit itself;
//   2. PreparePartitionings assigns m_NrRegions and m_UniqueRegionOffset, which concatenate the
//      region id spaces of all partitionings into one "unique region" space (GetUniqueRegionID);
//   3. GetData() materialises the dense AR -> region array, when there is a mapping attribute;
//   4. GetRegionID / GetUniqueRegionID are then read on the hot allocation path.
//
// GetRegionID discriminates on the POPULATED ARRAY, not on the weak mapping handle: an identity
// mapping leaves the array empty, and the hot loop must not pay a weak lock per atomic region.
//
// Not thread-safe: single-threaded preparation, then read-only queries.
// -----------------------------------------------------------------------------

struct partitioning_meta_t
{
	// Raw-ptr hygiene (migration §weak-sweep): both point to config items but live in the persistent htp_meta
	// (m_ReadAssets), so held weak to catch a post-teardown deref (lock_or_cancel) rather than dangle. The hot
	// per-atomic-region GetRegionID discriminates on the populated m_AtomicRegionPartitioningData array, not on
	// this weak handle, so the hot loop stays free of any weak-lock atomic load.
	std::weak_ptr<const AbstrDataItem> m_AtomicRegionPartitioningDI; // Optional AR->region mapping source
	std::weak_ptr<const AbstrUnit> m_PartitioningUnit;              // Region id unit
	// Held interest on the region-Label attr, resolved on the meta-thread so GetRegionStr's DisplayValue (called
	// from worker threads for error messages) needs neither a FindItem nor a worker-side StartInterest (which
	// would try to create the label DC off the meta-thread -> Check IsMetaThread()||!mayCreate fails). This meta
	// lives in the result's m_ReadAssets; to keep this held interest from being "uncaused" at config-root
	// teardown, the whole htp_meta is marked TSF_ReadAssetsInterestScoped and released by StopInterest -- safe
	// because CalcResult (the only reader of htp_meta besides its creation) never runs without MakeResult first
	// re-creating it (FuncDC::MakeResult re-runs MakeResultImpl when !m_Data).
	// Weak handle to the region-Label attr (resolved on the meta-thread). NOT a held interest: this lives in the
	// persistent htp_meta, so a held interest would be "uncaused" and deadlock the teardown drain, and #968/#1020
	// forbid clearing htp_meta at StopInterest. Kept of-interest during compute via funcDC.AddDependency on its DC
	// (see CreateResultingItems). Only used for region-name error messages in GetRegionStr, which lock()s it and
	// degrades to the region id if it is not currently available/of-interest (never forces interest on a worker).
	mutable std::weak_ptr<const AbstrDataItem> m_ValuesLabelLock;

	bool m_HasPartitioningDI = false; // structural: was this constructed from a DI mapping (vs identity)? Distinct from
	                                  // "m_AtomicRegionPartitioningDI has expired", so discriminators never conflate the two.

	explicit partitioning_meta_t(const AbstrDataItem* atomicRegionPartitioning)
		: m_AtomicRegionPartitioningDI(make_weak_tree(atomicRegionPartitioning))
		, m_PartitioningUnit(make_weak_tree(atomicRegionPartitioning->GetAbstrValuesUnit()))
		, m_HasPartitioningDI(true)
	{
		m_ValuesLabelLock = make_weak_tree(GetPartitioningUnit()->GetLabelAttr().get_ptr()); // store weak; transient interest released here (creates the DC on the meta-thread; AddDependency keeps it of-interest during compute)
	}

	explicit partitioning_meta_t(const AbstrUnit* atomicRegions)
		: m_PartitioningUnit(make_weak_tree(atomicRegions))
	{
		m_ValuesLabelLock = make_weak_tree(GetPartitioningUnit()->GetLabelAttr().get_ptr()); // store weak; transient interest released here (creates the DC on the meta-thread; AddDependency keeps it of-interest during compute)
	}


	// --- Accessors ----------------------------------------------------------

	SharedStr GetName() const
	{
		return m_HasPartitioningDI
			? lock_or_cancel(m_AtomicRegionPartitioningDI)->GetName()
			: lock_or_cancel(m_PartitioningUnit)->GetName();
	}

	// Owning lock; throws task_canceled if the config unit has been torn down. Returned shared so the caller
	// holds the unit for the duration of its use (no raw escaping the momentary lock).
	SharedUnit GetPartitioningUnit() const { return lock_or_cancel(m_PartitioningUnit); }
};

template <typename AR>
struct partitioning_info_t : partitioning_meta_t
{
	using atomic_region_id = AR;

	partitioning_info_t(partitioning_info_t&& rhs) noexcept = default;

	partitioning_info_t& operator=(partitioning_info_t&&) noexcept = default;
	partitioning_info_t(const partitioning_info_t&) = delete;
	partitioning_info_t& operator=(const partitioning_info_t&) = delete;

	explicit partitioning_info_t(const partitioning_meta_t& rhs) noexcept
		: partitioning_meta_t(rhs)
	{}

	// Populate region-id mapping when a data item mapping exists.
	// For identity mappings (no data item) only debug counts are recorded.
	void GetData()
	{
		if (m_HasPartitioningDI)
		{
			auto diLock = lock_or_cancel(m_AtomicRegionPartitioningDI); // owning for this scope; throws if torn down
			const AbstrDataItem* di = diLock.get();
			DataReadLock lock(di);
			auto nrAtomicRegions = di->GetCurrRefObj()->GetNrFeaturesNow();
			MG_DEBUGCODE(md_NrAtomicRegions = nrAtomicRegions);
			m_AtomicRegionPartitioningData = OwningPtrSizedArray<UInt32>(
				nrAtomicRegions,
				dont_initialize MG_DEBUG_ALLOCATOR_SRC("DiscrAlloc: m_AtomicRegionPartitioningData")
			);
			di->GetCurrRefObj()->GetValuesAsUInt32Array(
				tile_loc(0, 0),
				nrAtomicRegions,
				m_AtomicRegionPartitioningData.begin()
			);
		}
		else
		{
			auto nrAtomicRegions = lock_or_cancel(m_PartitioningUnit)->GetCount(); // throws if torn down
			MG_DEBUGCODE(md_NrAtomicRegions = nrAtomicRegions);
		}
	}

	// --- Accessors ----------------------------------------------------------

	// Return the (local) region id for atomic region 'ar'.
	// Pre: GetData() has been called if a data item mapping is used.
	UInt32 GetRegionID(atomic_region_id ar) const
	{
		dbg_assert(ar < md_NrAtomicRegions);
		// Hot path: discriminate on the populated mapping array (filled by GetData when a DI mapping exists),
		// NOT on the weak m_AtomicRegionPartitioningDI -- avoids a per-atomic-region weak-lock atomic load.
		return m_AtomicRegionPartitioningData.begin()
			? m_AtomicRegionPartitioningData[ar]
			: ar;
	}

	// Return a globally unique region id (partition-disambiguated) for atomic region 'ar'.
	// Pre: m_UniqueRegionOffset != 0xFFFFFFFF and GetData() completed if needed.
	UInt32 GetUniqueRegionID(atomic_region_id ar) const
	{
		assert(m_UniqueRegionOffset != static_cast<UInt32>(-1));
		return m_UniqueRegionOffset + GetRegionID(ar);
	}

	// Human-readable label for a region id (local to this partitioning).
	SharedStr GetRegionStr(UInt32 regionID) const
	{
		GuiReadLock lock;
		auto pu = AsUnit(GetPartitioningUnit()->GetCurrRangeItem());
		// Region-name display for error messages, called on worker threads. Use the weak label ONLY if it is
		// already of-interest (its data is available): seeding a fresh interest on a not-of-interest label from a
		// worker would StartInterest 0->1 -> create the label DC off the meta-thread -> IsMetaThread()||!mayCreate.
		// So lock() it, and only when it is already of-interest pass it (a mere bump, no create); otherwise degrade
		// to the raw region id. During Solve the label is normally of-interest via funcDC.AddDependency on its DC.
		if (auto labelItem = m_ValuesLabelLock.lock())
			if (labelItem->GetInterestCount())
			{
				SharedDataItemInterestPtr labelHolder = labelItem.get(); // bump only (already of-interest) -> no DC create
				return mySSPrintF("{} {}", GetName().c_str(),
					DisplayValue(pu.get(), regionID, false, labelHolder, MAX_TEXTOUT_SIZE, lock).c_str());
			}
		return mySSPrintF("{} {}", GetName().c_str(), regionID);
	}

	// Human-readable label for an atomic region (resolved to its region id).
	SharedStr GetAtomicRegionStr(atomic_region_id ar) const
	{
		auto regionID = GetRegionID(ar);
		return GetRegionStr(regionID);
	}

	// --- Data Members -------------------------------------------------------

	OwningPtrSizedArray<UInt32>       m_AtomicRegionPartitioningData;         // Dense AR->region mapping (if DI present)
	UInt32                            m_NrRegions = static_cast<UInt32>(-1);  // Cached region count
	atomic_region_id                  m_UniqueRegionOffset = static_cast<atomic_region_id>(-1); // Global offset for unique ids

#if defined(MG_DEBUG)
	UInt32                            md_NrAtomicRegions = 0;                 // Debug: #atomic regions observed
#endif
};


typedef UPoint cursor_type;
const UInt32 stepFactor = 4;


// regions_info_base is used to store the partitioning information of the atomic regions insofar it does not depend on the index type for atomic regions
// abbreviations:
// AR = atomic region
// UR = unique region
// AT = alllocation type (index of ggType, aka land use type).

struct regions_meta_base
{
	std::shared_ptr<const AbstrDataItem> m_AtomicRegionMap;
};

struct regions_info_base : regions_meta_base
{
	void PreparePermutation(land_unit_id n)
	{
		MG_CHECK(m_N == 0 || m_N == n);
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

	land_unit_id m_N = 0;
	mutable SizeT m_StepSize = 1, m_CurrBase = 0, m_PrevStep = 0, m_CurrPI = 0;

	DataReadLock                      m_AtomicRegionLock;
	std::vector<UInt32>               m_AtomicRegionSizes; // 1 per atomic_region containing the number of cells (sums to n)
	mutable std::unique_ptr<bi_graph> m_Ar2Ur;             // bi_graph that represents AR -> UR relation
};

struct regions_meta_t : regions_meta_base
{
	std::vector<partitioning_meta_t >  m_PartitioningMetas;       // 1 per Unique partitioning (==  p )

	UInt32 GetNrPartitionings() const { return m_PartitioningMetas.size(); }
};

// regions_info_t is used to store the partitioning information of the atomic regions
template <typename AR>
struct regions_info_t : regions_info_base
{
	using atomic_region_id = AR;
	using atomic_region_proxy = AR;

	using atomic_region_data_handle = typename DataArray<atomic_region_id>::locked_cseq_t;

	regions_info_t(const regions_meta_t& meta)
		: regions_info_base{ meta, 0, 1, 0, 0, 0, {}, {}, {} }
	{
		m_Partitionings.reserve(meta.m_PartitioningMetas.size());
		for (const auto& pm : meta.m_PartitioningMetas)
			m_Partitionings.emplace_back(pm);
	}

	WeakPtr<const TileFunctor<atomic_region_id> > m_AtomicRegionMapObj;
	atomic_region_data_handle                     m_AtomicRegionMapData; // 1 per grid-cell           (==  n )
	std::vector<partitioning_info_t<AR> >         m_Partitionings;       // 1 per Unique partitioning (==  p )
	atomic_region_id                              m_NrUniqueRegions = 0; // #ur

	UInt32 GetNrAtomicRegions() const { return m_AtomicRegionSizes.size(); }
	UInt32 GetNrPartitionings() const { return m_Partitionings.size(); }
	UInt32 GetNrUniqueRegions() const { return m_NrUniqueRegions; }
	UInt32 GetAtomicRegionID(land_unit_id i) const { assert(i < m_N); return m_AtomicRegionMapData[i]; }

	UInt32 GetRegionID(atomic_region_id ar, partitioning_id p)       const { return m_Partitionings[p].GetRegionID(ar);      }
	UInt32 GetUniqueRegionID(atomic_region_id ar, partitioning_id p) const { return m_Partitionings[p].GetUniqueRegionID(ar);}
	UInt32 GetUniqueRegionOffset(partitioning_id p)                  const { return m_Partitionings[p].m_UniqueRegionOffset; }

	// ========== ErrorMsg helper funcs

	SharedStr UniqueRegionStr(atomic_region_id ur) const
	{
		SharedStr result = mySSPrintF("Region {} ", ur);
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
		SharedStr result = mySSPrintF("Atomic Region {}", ar);

		for (UInt32 p = 0; p != GetNrPartitionings(); ++p)
		{
			result
				+=	SharedStr(p ? ", " : ": ")
					+	m_Partitionings[p].GetAtomicRegionStr(ar);
		}
		return result;			
	}
};

// specialization for Void, which is used when no partitioning needs to be administered
template <>
struct regions_info_t<Void> : regions_info_base
{
	using atomic_region_id = Void;
	using atomic_region_proxy = UInt32;

	regions_info_t(const regions_meta_t& meta)
		: regions_info_base{ meta, 0, 1, 0, 0, 0, {}, {}, {} }
	{}


	UInt32 GetNrAtomicRegions() const { return 1; }
	UInt32 GetNrPartitionings() const { return 1; }
	UInt32 GetNrUniqueRegions() const { return 1; }
	atomic_region_proxy GetAtomicRegionID(land_unit_id i) const { assert(i < m_N); return 0; }

	UInt32 GetRegionID      (atomic_region_proxy ar, partitioning_id p) const { assert(ar == 0); assert(p == 0);  return 0; }
	UInt32 GetUniqueRegionID(atomic_region_proxy ar, partitioning_id p) const { assert(ar == 0); assert(p == 0);  return 0; }
	UInt32 GetUniqueRegionOffset(partitioning_id p)                  const { assert(p == 0);  return 0; }

	// ========== ErrorMsg helper funcs

	SharedStr UniqueRegionStr(atomic_region_proxy ur) const
	{
		assert(ur == 0);
		static auto result = SharedStr("Set of all land units");
		return result;
	}

	SharedStr AtomicRegionStr(atomic_region_proxy ar) const
	{
		assert(ar == 0);
		return UniqueRegionStr(0);
	}
};

// *****************************************************************************
//									htp_info_t
// *****************************************************************************
/// htp_info_t is used to store information and intermediate results of the DiscreteAlloc algorithm
/// It contains information of their atomic regions, their claims, the facets and the results
// *****************************************************************************
// there is one facet for each claim to claim confrontation, i.e. one facet for each pair of claims related to overlapping land units and for different ggTypes
// for each facet, there is a priority queue of land units that are candidates for reallocation in order to increase (up) or decrease (down) the supply for the claim
// 
// S is the type of the suitability values, typically Int32; it must be addititive, compareable and assignable, and preferrably a signed integer
// AR is the type of the atomic region index, typically UInt16 or Void
// AT is the type of ggTypes index, typically UInt8


template <typename S>
struct htp_meta_extra 
{
	std::weak_ptr<const AbstrUnit>   m_MapDomain;
	std::weak_ptr<const Unit<S> >    m_PriceUnit; // weak: cached in htp_meta (persistent); the unit is of-interest (a suitability-map supplier) whenever used, so lock() at use and fail Calc if it ever fails
};

template <typename S>
struct htp_meta_t : regions_meta_t, htp_meta_extra<S>
{
	std::vector<ggType_meta_t> m_ggTypes; // 1 per ggType              (==  k )
};


template <typename S, typename P, typename AR, typename AT>
struct htp_info_t : regions_info_t<AR>, htp_meta_extra<S>
{
	using typename regions_info_t<AR>::atomic_region_id;
	using typename regions_info_t<AR>::atomic_region_proxy;

	htp_info_t(const htp_meta_t<S>& meta)
		: regions_info_t<AR>{ static_cast<const regions_meta_t&>(meta) }
		, htp_meta_extra<S>(meta)
		, m_TreeBuilder(*this), m_Threshold() 
	{
		m_ggTypes.reserve(meta.m_ggTypes.size());
		for (const auto& ggm : meta.m_ggTypes)
			m_ggTypes.emplace_back(ggm);
	}

	S                                   m_Threshold;

	DataWriteLock                       m_ResultDataLock;
	typename 
	DataArray<AT>::locked_seq_t         m_ResultArray;           // 1 per grid-cell (n); points directly info destination memmapped file
	DataWriteLock                       m_ResultPriceDataLock;

	std::vector<ggType_info_t<S> >      m_ggTypes;               // 1 per ggType              (==  k )
	std::vector<claim<S, P> >              m_Claims;                // 1 per claim region

	std::vector<UInt32> m_PossibleAllocationPerAr2UrLink;        // 1 per #ar * P; related to links    in m_Ar2Ur
	std::vector<UInt32> m_PossibleAllocatedPerUniqueRegion;      // 1 per #ur;     related to dstNodes in m_Ar2Ur

	std::vector<priority_heap<S, P> >       m_Facets;                // 1 per claim to claim confrontation
	std::vector<facet_id>                m_FacetIds;              // 1 per ggType^2 in each atomic region (== #ar * k *k)

	claim<S, P>& GetClaim(UInt32 ar, AT j)
	{ 
		assert(ar < this->GetNrAtomicRegions());
		ggType_info_t<S>& gg = m_ggTypes[j];
		return m_Claims[SizeT(gg.m_FirstClaimID) + this->GetRegionID(ar, gg.m_PartitioningID)];
	}
	priority_heap<S, P>& GetHeap(atomic_region_proxy ar, AT j, AT jj)
	{
		assert(ar < this->GetNrAtomicRegions() );
		AT k = GetK();
		assert(j  < k);
		assert(jj < k);

		return m_Facets[ m_FacetIds[ (SizeT(ar)*k +j ) *k + jj] ];
	}
	UInt32 ClaimID2UniqueRegionID(UInt32 claimID) const
	{
		const claim<S, P>& claim = m_Claims[claimID];
		auto p = m_ggTypes[claim.m_ggTypeID].m_PartitioningID;
		return this->GetUniqueRegionOffset(p) + claim.m_RegionID;
	}

	SharedStr GetClaimRangeStr(const claim<S, P>& cl) const
	{
		UInt32 ggTypeID = cl.m_ggTypeID;

		SharedStr regionStr;
		if constexpr (!std::is_same_v<AR, Void>)
			regionStr = this->m_Partitionings[m_ggTypes[ggTypeID].m_PartitioningID].GetRegionStr(cl.m_RegionID);

		return
			mySSPrintF("ClaimRange(type {} ({}), {}) = [min {}, max {}]",
				ggTypeID, m_ggTypes[ggTypeID].m_strName.c_str(),
				regionStr.c_str(),
				cl.m_ClaimRange.first, cl.m_ClaimRange.second
			);
	}

	// implement directed_graph concept for G(m_Claims, m_Facets) and let it be used by the directed_dijkstra member

	typedef shadow_price<S, P> cost_type;
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

	// The cost of taking this facet: its raw marginal cost plus the price difference of the two
	// claims it connects, as a shadow price.
	//
	// Two spellings of the same value. GetLinkCost is overflow-checked and is what the algorithm
	// runs on; GetLinkCostUnchecked omits the checks and is what the dms_assert expressions use,
	// including through CheckLink -- every call of which is an assertion. An assertion must not be
	// able to throw: that would let a Debug build take a path a Release build never takes, and
	// formatting the error allocates, which the DebugOnlyLock that dms_assert wraps its expression
	// in rightly refuses. Nothing is lost by it: every value an assertion inspects here is also
	// computed, and checked, by the algorithm itself.
	template <bool checked>
	shadow_price<S, P> GetLinkCostImpl(UInt32 heapID) const;

	shadow_price<S, P> GetLinkCost         (UInt32 heapID)  const { return GetLinkCostImpl<true >(heapID); }
	shadow_price<S, P> GetLinkCostUnchecked(UInt32 heapID)  const { return GetLinkCostImpl<false>(heapID); }
	bool               CheckLink           (UInt32 facetID) const;

	// ========== more data members
	directed_dijkstra<htp_info_t> m_TreeBuilder;
	std::vector<UInt32>           m_ClaimIdList;
	land_unit_id                  m_NrBelowThreshold = 0;

#if defined(MG_DEBUG)
	bool CanReportFindMstDown() { return (++md_ReportFindMstDownCounter < 10) || PowerOf2(md_ReportFindMstDownCounter); }
	UInt32                                md_ReportFindMstDownCounter;
#endif
};

template <typename S, typename P, typename AR, typename AT>
struct htp_calc_t //: regions_info_t<AR>
{
};


// *****************************************************************************
//									regions_info_t mf
// *****************************************************************************

template <typename AR>
const bi_graph& GetAr2UrBiGraph(const regions_info_t<AR>* self)
{
	if (self->m_Ar2Ur)
		return *(self->m_Ar2Ur);

	UInt32 nrAtomicRegions = self->GetNrAtomicRegions();
	UInt32 nrUniqueRegions = self->GetNrUniqueRegions();
	UInt32 P = self->GetNrPartitionings();

	self->m_Ar2Ur.reset(new bi_graph(nrAtomicRegions, nrUniqueRegions, P * nrAtomicRegions));
	bi_graph& gr = *(self->m_Ar2Ur);

	// fill graph with links(atomicRegionID, unique region id)
	for (UInt32 p = 0; p != P; ++p)
		for (UInt32 ar = 0; ar != nrAtomicRegions; ++ar)
			gr.AddLink(ar, self->GetUniqueRegionID(ar, p));
	return gr;
}

// *****************************************************************************
//									Facet related funcs
// *****************************************************************************

template <typename S, typename P, typename AR, typename AT>
template <bool checked>
shadow_price<S, P> htp_info_t<S, P, AR, AT>::GetLinkCostImpl(UInt32 facetID) const
{
	dms_assert(facetID < GetNrLinks()); 

	priority_heap<S, P>& ph = const_cast<htp_info_t*>(this)->m_Facets[facetID];  // contains q(a,b), with a=src && b=target of move option

	if (ph.empty())
		return MaxValue<shadow_price<S, P> >();

	UInt32 topI = ph.top();
	dms_assert(m_ResultArray[topI] == ph.m_SourceClaim->m_ggTypeID);

	// calculate (Ga - Gb) - (Qa(Epsilon) - Qb(Epsilon)) 
	//	== (Ga - Gb) + c + Epsilon * p(i) * (Ja - Jb), since c = (Qa - Qb)
	// and Epsilon(a) - Epsilon(b), the delta-Epsilon of a transfer though this facet 
	// corresponds with  p(i)*(Ja - Jb) == p(i) * perturbationfactor(a,b)

	shadow_price<S, P> cost = checked
		? shadow_price<S, P>(ph.GetC         (topI), PerturbationOf<P>(topI, ph.m_PerturbationFactor))
		: shadow_price<S, P>(ph.GetCUnchecked(topI), P(topI) * ph.m_PerturbationFactor);

	// only in the checked instantiation: the unchecked one deliberately lets the arithmetic wrap,
	// and these invariants say nothing about wrapped values.
	if constexpr (checked)
		dms_assert(cost + ph.m_SourceClaim->m_ShadowPrice >= ph.m_TargetClaim->m_ShadowPrice);

	// -(Qa - Qb).
	if constexpr (checked)
		cost = CheckedAdd(cost,
			CheckedSub(
				ph.m_SourceClaim->m_ShadowPrice	// Ga
			,	ph.m_TargetClaim->m_ShadowPrice	// Gb
			)
		);
	else
		cost += (ph.m_SourceClaim->m_ShadowPrice - ph.m_TargetClaim->m_ShadowPrice);

	if constexpr (checked)
		dms_assert(cost >= cost_type());
	return cost;
}

template <typename S, typename P, typename AR, typename AT>
bool htp_info_t<S, P, AR, AT>::CheckLink(UInt32 facetID) const
{
	// unchecked: every call of CheckLink is inside an assertion, see GetLinkCostUnchecked
	return GetLinkCostUnchecked(facetID) < MaxValue<shadow_price<S, P> >(); // if not; link may not be taken
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
			throwErrorF("CheckSolution", "atomic region {} was assigned totalFlow {}, but has at least {} cells",
				srcNode, 
				totalFlow, 
				srcMinCapacity[srcNode]
			);
		if (srcMaxCapacity[srcNode] < totalFlow)
			throwErrorF("CheckSolution", "atomic region {} was assigned totalFlow {}, but has at most {} cells",
				srcNode, 
				totalFlow, 
				srcMaxCapacity[srcNode]
			);
	}
	for (dstNode = 0; dstNode != nrDst; ++dstNode)
	{
		UInt32 totalFlow = SumFlow<dir_backward_tag>(gr, dstNode, lnkAllocated);

		if (dstMinClaims[dstNode] > totalFlow)
			throwErrorF("CheckSolution", "unique region {} was assigned totalFlow {}, but its total minimum claim is {} cells",
				dstNode, 
				totalFlow, 
				dstMinClaims[dstNode]
			);
		if (dstMaxClaims[dstNode] < totalFlow)
			throwErrorF("CheckSolution", "unique region {} was assigned totalFlow {}, but its total maximum claim is {} cells",
				dstNode, 
				totalFlow, 
				dstMaxClaims[dstNode]
			);
	}
}

/**
 * @brief Checks if the allocation problem is feasible given region capacities and claim constraints.
 *
 * This function verifies that the minimum and maximum claim/capacity constraints for all atomic regions (sources)
 * and unique regions (destinations) can be satisfied. It performs several checks:
 *   - Ensures that for each unique region and land use type: minClaim <= maxClaim.
 *   - Checks that the total minimum claims do not exceed the total available capacity, and vice versa.
 *   - For each destination, verifies that the sum of source capacities can satisfy its minimum claim.
 *   - For each source, verifies that the sum of destination max claims can absorb its minimum capacity.
 * If all checks pass, it attempts to allocate the minimum required claims/capacities using a flow-like augmentation.
 * Any violations are reported in strStatus. Returns true if feasible, false otherwise.
 *
 * @tparam AR Atomic region type.
 * @param gr           The bipartite graph representing region-to-region links.
 * @param srcMinCapacity Array of minimum capacities for each source region.
 * @param srcMaxCapacity Array of maximum capacities for each source region.
 * @param dstMinClaims   Array of minimum claims for each destination region.
 * @param dstMaxClaims   Array of maximum claims for each destination region.
 * @param srcAllocated   Array to track allocated cells per source region (in/out).
 * @param dstAllocated   Array to track allocated cells per destination region (in/out).
 * @param lnkAllocated   Array to track allocated cells per link (in/out).
 * @param regInfo        Region info helper for reporting.
 * @param strStatus      Output string for error/status messages.
 * @return true if the allocation is feasible, false otherwise.
 */

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
					"{} has minimum {} and maximum {}; ",
					regInfo.UniqueRegionStr(dstNode).c_str(), 
					dstMinClaims[dstNode],
					dstMaxClaims[dstNode]
				);
				ok = false;
			}
	}

	// test if total claims fit total capacity
	//
	// SizeT, not UInt32: each term is bounded by the land unit count but these sums run over ALL
	// regions, so they are bounded by nrRegions * N. Wrapping made the total SMALL, which inverts
	// the test below -- a feasible problem rejected, or an infeasible one passed on to surface
	// later as an unmet claim. Issue #1196. The same holds for the two link sums further down.
	{
		SizeT totalMinClaim = 0;
		SizeT totalMaxCapacity = 0;

		for (dstNode = 0; dstNode != nrDst; ++dstNode)
			totalMinClaim += dstMinClaims[dstNode];
		for (srcNode = 0; srcNode != nrSrc; ++srcNode)
			totalMaxCapacity += srcMaxCapacity[srcNode];

		if (totalMaxCapacity < totalMinClaim)
		{
			strStatus += mySSPrintF(
				"total of the minimum claims is {} while there are only {} allocatable cells, total minimum claims should be decreased with at least {} cells; ",
				totalMinClaim,
				totalMaxCapacity,
				totalMinClaim - totalMaxCapacity
			);
			ok = false;
		}
	}
	{
		SizeT totalMaxClaim = 0;
		SizeT totalMinCapacity = 0;

		for (dstNode = 0; dstNode != nrDst; ++dstNode)
			totalMaxClaim += dstMaxClaims[dstNode];
		for (srcNode = 0; srcNode != nrSrc; ++srcNode)
			totalMinCapacity += srcMinCapacity[srcNode];

		if (totalMinCapacity > totalMaxClaim)
		{
			strStatus += mySSPrintF(
				"there are {} cells that should be allocated while the total of the maximum claims is only {}, total maximum claims should be increased with at least {} cells; ",
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
		SizeT maxCapacity = 0;
		lnk = gr.GetFirstLink(dstNode, dir_backward_tag());
		while (IsDefined(lnk))
		{
			maxCapacity += srcMaxCapacity[ gr.GetDstNode(lnk, dir_backward_tag()) ];
			lnk = gr.GetNextLink(lnk, dir_backward_tag());
		}
		if (maxCapacity < dstMinClaims[dstNode])
		{
			strStatus += mySSPrintF(
				"total minimum claim for {} is {} while it has only {} allocatable cells, total minimum claims for that region should be decreased with at least {} cells; ",
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

		SizeT maxClaim = 0;
		lnk = gr.GetFirstLink(srcNode, dir_forward_tag());
		while (IsDefined(lnk))
		{
			maxClaim += dstMaxClaims[ gr.GetDstNode(lnk, dir_forward_tag()) ];
			lnk = gr.GetNextLink(lnk, dir_forward_tag());
		}
		if (maxClaim < srcMinCapacity[srcNode])
		{
			strStatus += mySSPrintF(
				"total maximum claim for {} is {} while it has {} cells that should be allocated, total maximum claims for that region should be increased with at least {} cells; ",
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
				"from the total minimum claim of {} cells for {}, {} cannot be allocated (insufficient cells); ",
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
					"from {} allocatable cells in {}; {} cannot be matched to claims (maximum claims too low); ",
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

template <typename S, typename P, typename AR, typename AT>
bool FeasibilityTest(const htp_info_t<S, P, AR, AT>& htpInfo, SharedStr& strStatus)
{
	UInt32 nrAtomicRegions = htpInfo.GetNrAtomicRegions();
	UInt32 nrUniqueRegions = htpInfo.GetNrUniqueRegions();

	const bi_graph& gr = GetAr2UrBiGraph(&htpInfo);

	dms_assert(nrAtomicRegions     == gr.GetNrSrcNodes(dir_forward_tag()));
	dms_assert(nrUniqueRegions     == gr.GetNrDstNodes(dir_forward_tag()));
	dms_assert(htpInfo.GetNrPartitionings() * nrAtomicRegions == gr.GetNrLinks());

	// Aggregate claims to unique regions.
	//
	// Summed in SizeT because the sum runs over the ggTypes and each term can be as large as the
	// land unit count on its own: the common "no limit" idiom -- a maximum claim of N for every
	// type -- reaches K * N, which leaves UInt32 at K >= 11 on a 4e8 cell grid. Issue #1196.
	//
	// What IsFeasible and the flow machinery in bi_graph.h are handed stays UInt32, and that
	// costs nothing on the maximum side: no region can absorb more land units than exist, so a
	// maximum aggregate above N means exactly what N means and is clamped to it below. A MINIMUM
	// aggregate above N is a different matter -- clamping would hide the infeasibility instead of
	// reporting it -- so it is reported here, from the true sum.
	const SizeT N = htpInfo.GetN();

	std::vector<SizeT>  aggrMinSums  (nrUniqueRegions, 0);
	std::vector<SizeT>  aggrMaxSums  (nrUniqueRegions, 0);
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

		UInt32 uniqueRegionOffset = htpInfo.GetUniqueRegionOffset(ggTypeIter->m_PartitioningID);
		assert(uniqueRegionOffset + ggTypeIter->m_NrClaims <= nrUniqueRegions);

		auto
			aggrMinClaimIter = aggrMinSums.begin() + uniqueRegionOffset,
			aggrMaxClaimIter = aggrMaxSums.begin() + uniqueRegionOffset;

		for (; claimIter != claimEnd; ++claimIter)
		{
			if (claimIter->m_ClaimRange.first > claimIter->m_ClaimRange.second)
			{
				strStatus += mySSPrintF("{}: minimum > maximum; ", htpInfo.GetClaimRangeStr(*claimIter).c_str());
				ok = false;
			}
			*aggrMinClaimIter = CheckedAdd<SizeT>(*aggrMinClaimIter, claimIter->m_ClaimRange.first ); ++aggrMinClaimIter;
			*aggrMaxClaimIter = CheckedAdd<SizeT>(*aggrMaxClaimIter, claimIter->m_ClaimRange.second); ++aggrMaxClaimIter;
		}

		++ggTypeIter;
	}

	// narrow to what IsFeasible counts in; see the note at the declarations above
	for (UInt32 ur = 0; ur != nrUniqueRegions; ++ur)
	{
		if (aggrMinSums[ur] > N)
		{
			strStatus += mySSPrintF(
				"total minimum claim for {} is {} while the whole allocation has only {} land units; ",
				htpInfo.UniqueRegionStr(ur).c_str(), aggrMinSums[ur], N
			);
			ok = false;
			continue;
		}
		aggrMinClaims[ur] = UInt32(aggrMinSums[ur]);                 // <= N by the test above
		aggrMaxClaims[ur] = UInt32(Min<SizeT>(aggrMaxSums[ur], N));  // clamped, so <= N as well
	}
	if (!ok)
		return false;

	assert(htpInfo.GetNrAtomicRegions() == gr.GetNrSrcNodes(dir_forward_tag()));
	assert(aggrMinClaims.size()         == gr.GetNrDstNodes(dir_forward_tag()));
	assert(aggrMaxClaims.size()         == gr.GetNrDstNodes(dir_forward_tag()));

	std::vector<UInt32>  srcAllocated(nrAtomicRegions, 0);
	std::vector<UInt32>& allocatedPerLink = const_cast<htp_info_t<S, P, AR, AT>&>(htpInfo).m_PossibleAllocationPerAr2UrLink;
	std::vector<UInt32>& dstAllocated     = const_cast<htp_info_t<S, P, AR, AT>&>(htpInfo).m_PossibleAllocatedPerUniqueRegion;

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

//#include <gsl/gsl>

auto GetClaimAttr(const TreeItem* claimSet, TokenID nameID) -> const AbstrDataItem*
{
	assert(claimSet);
	auto result = AsDynamicDataItem(claimSet->GetConstSubTreeItemByID(nameID));

	if (!result)
		claimSet->throwItemErrorF("Claimset should contain an attribute for {}", nameID);

	result->UpdateMetaInfo();

	if (result->GetDynamicObjClass() != DataItemClass::Find(ValueWrap<UInt32>::GetStaticClass()))
		result->throwItemError("Claim attribute should contain UInt32 values");
	return result.get();
}

template <typename S>
void CreateResultingItems(
	const AbstrDataItem* ggTypeNamesA,
	const AbstrUnit* allocUnit,
	const TreeItem* suitabilitySet,
	const TreeItem* minClaimSet,
	const TreeItem* maxClaimSet,
	const AbstrDataItem* ggTypes2partitioningsA,
	const AbstrDataItem* partitioningNamesA,
	const AbstrUnit* atomicRegionUnit,
	const AbstrDataItem* atomicRegionMapA,
	TreeItem* resShadowPriceContainer,
	TreeItem* resTotalAllocatedContainer,
	htp_meta_t<S>& htpMeta,
	bool mustAdjust, FuncDC& funcDC,
	bool hasPartitionings
)
{
	// init elementary data members
	htpMeta.m_MapDomain = make_weak_tree(allocUnit);
	//	dms_assert(atomicRegionUnit);
	assert(minClaimSet);
	assert(maxClaimSet);
	assert(suitabilitySet);
	SharedStr resultMsg;

	// get array of partitionNames
	UInt32 P = 1;
	if (hasPartitionings)
	{
		DataReadLock partitioningNamesLock;
		const DataArray<SharedStr>* partitioningNames = nullptr;

		if (partitioningNamesA)
		{
			partitioningNamesLock = DataReadLock(partitioningNamesA);
			partitioningNames = const_array_cast<SharedStr>(partitioningNamesA);
			P = partitioningNames->GetDataRead().size();
		}
		htpMeta.m_PartitioningMetas.reserve(P);
		for (UInt32 p = 0; p != P; ++p)
		{
			SharedStr partitioningName = partitioningNames ? partitioningNames->GetIndexedValue(p) : SharedStr(atomicRegionUnit->GetID());
			CDebugContextHandle context("discrete_alloc", partitioningName.c_str(), false);

			const AbstrDataItem* regioRefDI = nullptr;
			if (partitioningNamesA)
			{
				regioRefDI = AsCheckedDataItem(atomicRegionUnit->GetConstSubTreeItemByID(GetTokenID_mt(partitioningName.c_str())).get());
				if (!regioRefDI)
					atomicRegionUnit->throwItemErrorF("SubItem expected with the name {}", partitioningName.c_str());
				regioRefDI->UpdateMetaInfo();
				funcDC.AddDependency(regioRefDI->GetCheckedDC().get());
			}

			if (regioRefDI && !atomicRegionUnit->UnifyDomain(regioRefDI->GetAbstrDomainUnit(), "atomicRegionUnit", "Domain of regional partitioning thereof", UnifyMode(), &resultMsg))
				throwErrorF("discrete_alloc", "unification of domain of partitoning {}({}):\n{}\n with atomic region\n{}\n resulted in\n{}"
					, p, partitioningName, regioRefDI->GetSourceName()
					, atomicRegionUnit->GetSourceName()
					, resultMsg
				);

			if (regioRefDI)
				htpMeta.m_PartitioningMetas.emplace_back(regioRefDI);
			else
				htpMeta.m_PartitioningMetas.emplace_back(atomicRegionUnit);

			if (auto labelItem = htpMeta.m_PartitioningMetas.back().m_ValuesLabelLock.lock()) // meta-thread: label alive here
				if (auto labelDC = labelItem->GetCheckedDC())
					funcDC.AddDependency(labelDC.get()); // keeps the label of-interest during compute (m_OtherSuppliers)
		}
		assert(htpMeta.m_PartitioningMetas.size() == P);
	}
	

	// get array of ggTypeNames
	DataReadLock ggTypesNameLock(ggTypeNamesA);
	const DataArray<SharedStr>* ggTypeNames = const_array_cast<SharedStr>(ggTypeNamesA);

	DataReadLock ggTypes2partitioningsLock;
	const DataArray<partitioning_id>* ggTypes2partitionings = nullptr;

	MG_CHECK((ggTypes2partitioningsA != nullptr) == (partitioningNamesA != nullptr));
	if (ggTypes2partitioningsA && partitioningNamesA)
	{
		// get array of ggTypes2partitionings
		ggTypes2partitioningsLock = DataReadLock(ggTypes2partitioningsA);
		ggTypes2partitionings = const_array_cast<partitioning_id>(ggTypes2partitioningsA);

			if (!ggTypes2partitioningsA->GetAbstrDomainUnit()->UnifyDomain(ggTypeNamesA->GetAbstrDomainUnit(), "Domain of AllocationType partitioning (4th) attribute", "Domain of AllocationType name (1st) attribute", UnifyMode(), &resultMsg))
				throwErrorF("discrete_alloc", "domains of Type->Name mapping (arg1):\n{}\nand Type->Partitioning mapping (arg4):\n{}\nincompatible: {}"
					, ggTypeNamesA->GetSourceName()
					, ggTypes2partitioningsA->GetSourceName()
					, resultMsg
				);

			if (!ggTypes2partitioningsA->GetAbstrValuesUnit()->UnifyDomain(partitioningNamesA->GetAbstrDomainUnit(), "Values of AllocationType partitioning (4th) attribute", "Domain of Partition names (5th) attribute", UnifyMode(), &resultMsg))
				throwErrorF("discrete_alloc", "values of Type->Partitioning mapping (arg4):\n{}\nand Partition->Names mapping (arg5):\n{}\nincompatible: {}"
					, ggTypes2partitioningsA->GetSourceName()
					, partitioningNamesA->GetSourceName()
					, resultMsg
				);
		}
//	}
	UInt32 K = ggTypeNames->GetDataRead().size();
	htpMeta.m_ggTypes.resize(K);

	for (UInt32 j = 0; j < K; ++j)
	{
		ggType_meta_t* gg = begin_ptr(htpMeta.m_ggTypes) + j;
		gg->m_strName = ggTypeNames->GetIndexedValue(j);
		auto contextHandle = MakeLCH([gg]() { return "discrete_alloc_init for Type " + gg->m_strName;  });

		gg->m_NameID = GetTokenID_mt(gg->m_strName.begin(), gg->m_strName.send());
		auto minClaims = GetClaimAttr(minClaimSet, gg->m_NameID);
		auto maxClaims = GetClaimAttr(maxClaimSet, gg->m_NameID);
		gg->m_diMinClaims = make_weak_tree(minClaims);
		gg->m_diMaxClaims = make_weak_tree(maxClaims);

		if (minClaims->WasFailed(FailType::Data)) minClaims->ThrowFail();
		if (maxClaims->WasFailed(FailType::Data)) maxClaims->ThrowFail();

		funcDC.AddDependency(minClaims->GetCheckedDC().get());
		funcDC.AddDependency(maxClaims->GetCheckedDC().get());

		const AbstrUnit* partitioningUnit = nullptr;
		SharedUnit partitioningUnitHolder; // owns a locked partitioning unit for this iteration
		if (hasPartitionings)
		{
			partitioning_id partitioningID = 0;
			if (ggTypes2partitionings)
				partitioningID = ggTypes2partitionings->GetIndexedValue(j);
			if (partitioningID >= htpMeta.GetNrPartitionings())
			{
				MG_CHECK(partitioningNamesA);
				if (partitioningNamesA)
					throwErrorF("discrete_alloc", "Partitioning reference {} for Type {} is not a valid index in {}", partitioningID, gg->m_NameID, partitioningNamesA->GetSourceName());
			}

			gg->m_PartitioningID = partitioningID;

			partitioningUnitHolder = htpMeta.m_PartitioningMetas[gg->m_PartitioningID].GetPartitioningUnit();
			partitioningUnit = partitioningUnitHolder.get();
			assert(partitioningUnit);

			if (ggTypes2partitioningsA && partitioningNamesA)
			{
				if (minClaims && !partitioningUnit->UnifyDomain(minClaims->GetAbstrDomainUnit(), "Partitioning", "Domain of Minimum Claim attribute", UnifyMode(), &resultMsg))
					throwErrorF("discrete_alloc", "values of partitioning {} in AtomicRegions (6th argument):\n{}\nand domain of minimum claim for {} (8th argument):\n{}\nincompatible: {}"
						, htpMeta.m_PartitioningMetas[partitioningID].GetName()
						, lock_or_cancel(htpMeta.m_PartitioningMetas[partitioningID].m_AtomicRegionPartitioningDI)->GetSourceName()
						, gg->m_NameID, minClaims->GetSourceName()
						, resultMsg
					);
				if (maxClaims && !partitioningUnit->UnifyDomain(maxClaims->GetAbstrDomainUnit(), "Partitioning", "Domain of Maximum Claim attribute", UnifyMode(), &resultMsg))
					throwErrorF("discrete_alloc", "values of partitioning {} in AtomicRegions (6th argument):\n{}\nand domain of maximum claim for {} (9th argument):\n{}\nincompatible: {}"
						, htpMeta.m_PartitioningMetas[partitioningID].GetName()
						, lock_or_cancel(htpMeta.m_PartitioningMetas[partitioningID].m_AtomicRegionPartitioningDI)->GetSourceName()
						, gg->m_NameID, maxClaims->GetSourceName()
						, resultMsg
					);
			}
			else
			{
				if (minClaims && !partitioningUnit->UnifyDomain(minClaims->GetAbstrDomainUnit(), "Partitioning", "Domain of Minimum Claim attribute", UnifyMode(), &resultMsg))
					throwErrorF("discrete_alloc", "Regions (4th argument)\nand domain of minimum claim for {} (6th argument):\n{}\nincompatible: {}"
						, gg->m_NameID, minClaims->GetSourceName()
						, resultMsg
					);
				if (maxClaims && !partitioningUnit->UnifyDomain(maxClaims->GetAbstrDomainUnit(), "Partitioning", "Domain of Maximum Claim attribute", UnifyMode(), &resultMsg))
					throwErrorF("discrete_alloc", "Regions (4th argument):\nand domain of maximum claim for {} (7th argument):\n{}\nincompatible: {}"
						, gg->m_NameID, maxClaims->GetSourceName()
						, resultMsg
					);
			}

		}
		else
		{
			partitioningUnit = Unit<Void>::GetStaticClass()->CreateDefault();

			gg->m_PartitioningID = 0;

			if (minClaims && !minClaims->HasVoidDomainGuarantee())
				throwErrorF("discrete_alloc", "domain of minimum claim for {} not allowed for unpartitioned allocation, define claim as parameter and not as attribute.\n{}"
					, gg->m_NameID
					, minClaims->GetSourceName()
				);
			if (maxClaims && !maxClaims->HasVoidDomainGuarantee())
				throwErrorF("discrete_alloc", "domain of maximum claim for {} not allowed for unpartitioned allocation, define claim as parameter and not as attribute.\n{}"
					, gg->m_NameID
					, maxClaims->GetSourceName()
				);
		}

		{
			FixedContextHandle lch("Obtaining a suitability map from the suitability map container");

			auto subItem = suitabilitySet->GetConstSubTreeItemByID(gg->m_NameID);
			if (!subItem)
				throwErrorF("discrete_alloc", "cannot find {} in container {}", gg->m_NameID, suitabilitySet->GetFullName());
			if (!IsDataItem(subItem))
				subItem->throwItemError("is expected to be a DataItem,  a.k.a. attribute");

			const AbstrDataItem* suitMap = AsCertainDataItem(subItem.get());
			gg->m_diSuitabilityMap = make_weak_tree(suitMap);
			suitMap->UpdateMetaInfo();
		}
		const AbstrDataItem* suitMap = gg->m_diSuitabilityMap.lock().get(); // momentary; still owned by suitabilitySet during this setup
		auto suitMapDc = suitMap->GetCheckedDC();
		if (!suitMapDc)
			if (suitMap->WasFailed(FailType::MetaInfo))
				suitMap->ThrowFail();
		MG_CHECK(suitMapDc);
		funcDC.AddDependency(suitMapDc.get());

		if (!allocUnit->UnifyDomain(suitMap->GetAbstrDomainUnit(), "AllocUnit (second argument)", "Domain of suitability map", UnifyMode(), &resultMsg))
			throwErrorF("discrete_alloc", "Domain of suitability map for {}:\n{}\n {} and allocUnit (arg2) incompatible: {}"
				,	gg->m_NameID, suitMap->GetSourceName()
				,	allocUnit->GetSourceName()
				,	resultMsg
			);
		{
			FixedContextHandle priceUnitContext("processing the values unit of a suitability map as a unit of utility");
			const Unit<S>* priceUnit = const_unit_checkedcast<S>(suitMap->GetAbstrValuesUnit());
			if (htpMeta.m_PriceUnit.expired())
				htpMeta.m_PriceUnit = make_weak_tree(priceUnit); // weak borrow; kept of-interest as a supplier during compute
			else if (auto priceUnitLock = htpMeta.m_PriceUnit.lock())
			{
				if (!priceUnitLock->UnifyValues(priceUnit, "First non-default suitability values unit", "A subsequence suitability values unit", UnifyMode(), &resultMsg))
					throwErrorF("discrete_alloc", "values of suitability map for {} incompatible with earlier suitability map values:\n{}", gg->m_NameID, resultMsg);
			}
			else
				throwErrorF("discrete_alloc", "price unit expired while processing suitability map for {}", gg->m_NameID);

			if (mustAdjust)
			{
				gg->m_diResShadowPrices = make_weak_tree(CreateDataItem(
					resShadowPriceContainer
					, gg->m_NameID
					, partitioningUnit
					, priceUnit
				).get()); // owned by resShadowPriceContainer
				gg->m_HasResShadowPrices = true;
			}
		}
		gg->m_diResTotalAllocated =
			make_weak_tree(CreateDataItem(
				resTotalAllocatedContainer
			,	gg->m_NameID
			,	partitioningUnit
			,	Unit<land_unit_id>::GetStaticClass()->CreateDefault()
			).get()); // owned by resTotalAllocatedContainer
	}
	if (atomicRegionMapA)
	{
		if (!allocUnit->UnifyDomain(atomicRegionMapA->GetAbstrDomainUnit(), "second argument", "Domain of the AtomicRegions attribute", UnifyMode(), &resultMsg))
			throwErrorF("discrete_alloc", "Domain of atomic region map:\n{}\nand allocUnit (arg2):\n{} incompatible:\n{}"
				, atomicRegionMapA->GetSourceName()
				, allocUnit->GetSourceName()
				, resultMsg
			);
	}
}

// *****************************************************************************
//									Prepare
// *****************************************************************************

template <typename S, typename P, typename AR, typename AT>
void PrepareClaims(htp_info_t<S, P, AR, AT>& htpInfo)
{
	UInt32 K = htpInfo.GetK();

	// count total nrClaims and calc offsets in m_Claims array based on cardinality of each ggType's partitioning
	UInt32 nrClaims = 0;
	for (UInt32 j=0; j!=K; ++j)
	{
		ggType_info_t<S>& gg = htpInfo.m_ggTypes[j];


		gg.m_FirstClaimID = nrClaims;
		if constexpr (std::is_same_v<AR, Void>)
			gg.m_NrClaims = 1;
		else
		{
			auto partitioningUnit = htpInfo.m_Partitionings[gg.m_PartitioningID].GetPartitioningUnit();
			assert(partitioningUnit);
			gg.m_NrClaims = partitioningUnit->GetCount();
		}
		nrClaims += gg.m_NrClaims;
	}

	// load claim min, max into m_Claims
	htpInfo.m_Claims.reserve(nrClaims);
	for (UInt32 j=0; j!=K; ++j)
	{
		ggType_info_t<S>& gg = htpInfo.m_ggTypes[j];

		auto minClaimsLock = lock_or_cancel(gg.m_diMinClaims); // owning for this scope; throws if torn down
		auto maxClaimsLock = lock_or_cancel(gg.m_diMaxClaims);
		const AbstrDataItem* minClaimsDI = minClaimsLock.get();
		const AbstrDataItem* maxClaimsDI = maxClaimsLock.get();

		dms_assert(IsDataReady(minClaimsDI->GetCurrUltimateItem().get()));
		dms_assert(IsDataReady(maxClaimsDI->GetCurrUltimateItem().get()));

		DataReadLock lockClaimMin(minClaimsDI);
		DataReadLock lockClaimMax(maxClaimsDI);

		for (UInt32 r = 0; r != gg.m_NrClaims; ++r)
		{
			htpInfo.m_Claims.push_back(
				claim<S, P>(
					j, r,
					claim_range(
						const_array_cast<claim_type>(minClaimsDI)->GetIndexedValue(r),
						const_array_cast<claim_type>(maxClaimsDI)->GetIndexedValue(r)
					)
				)
			);
		}
	}
	dms_assert(htpInfo.m_Claims.size() == nrClaims);
}

template <typename S, typename P, typename AR, typename AT>
void PreparePartitionings(htp_info_t<S, P, AR, AT>& htpInfo, const AbstrUnit* allocUnit, const AbstrDataItem* atomicRegionMapA, const Unit<AR>* atomicRegionUnit)
{
	assert(allocUnit);
	assert((atomicRegionMapA ==nullptr)==(atomicRegionUnit == nullptr));

	auto p = htpInfo.GetNrPartitionings();

	UInt32 nrAtomicRegions = 1;
	if constexpr (!std::is_same_v<AR, Void>)
	{
		assert(atomicRegionMapA);
		assert(atomicRegionUnit);
		assert(htpInfo.m_NrUniqueRegions == 0);

		for (UInt32 j = 0; j != p; ++j)
		{
			partitioning_info_t<AR>& pInfo = htpInfo.m_Partitionings[j];

			UInt32 nrRegions = pInfo.GetPartitioningUnit()->GetCount();

			pInfo.m_UniqueRegionOffset = htpInfo.m_NrUniqueRegions;
			pInfo.m_NrRegions = nrRegions;

			pInfo.GetData();
			htpInfo.m_NrUniqueRegions += nrRegions;
		}
		// collect atomicRegionCounts
		htpInfo.m_AtomicRegionMap = make_shared_tree(atomicRegionMapA, existing_obj{});
		htpInfo.m_AtomicRegionLock = DataReadLock(atomicRegionMapA);
		assert(htpInfo.m_AtomicRegionLock.IsLocked());
		htpInfo.m_AtomicRegionMapObj = const_array_cast<AR>(atomicRegionMapA);
		nrAtomicRegions = atomicRegionUnit->GetCount();
	}
	else
	{
		assert(!atomicRegionMapA);
		assert(!atomicRegionUnit);
	}

	vector_zero_n(htpInfo.m_AtomicRegionSizes, nrAtomicRegions);

	land_unit_id n = ThrowingConvert<land_unit_id>(allocUnit->GetCount());
	if constexpr (!std::is_same_v<AR, Void>)
	{
		MG_CHECK(allocUnit->UnifyDomain(atomicRegionMapA->GetAbstrDomainUnit()));
		SizeT nrLandUnits = 0;
		tile_id nrLandUnitTiles = atomicRegionMapA->GetAbstrDomainUnit()->GetNrTiles();
		for (tile_id t = 0; t != nrLandUnitTiles; ++t)
		{
			auto atomicRegionMapData = htpInfo.m_AtomicRegionMapObj->GetLockedDataRead(t);
			nrLandUnits += atomicRegionMapData.size();

			pcount_container<AR, UInt32>(
				IterRange<UInt32*>(&htpInfo.m_AtomicRegionSizes)
				, atomicRegionMapData
				, atomicRegionUnit->GetRange()
				, atomicRegionMapA->GetCheckMode()
				, false
			);
		}
		if (nrLandUnits != n)
			lock_or_cancel(htpInfo.m_MapDomain)->throwItemErrorF(
				"Land Unit set had {} elements, but total nr of elements in tiles is {}. Use a land unit set with a completely covering tiling",
				n,
				nrLandUnits
			);
	}
	else
	{
		assert(htpInfo.m_AtomicRegionSizes.size() == 1);
		htpInfo.m_AtomicRegionSizes[0] = n;
	}
	htpInfo.m_N = n;
}

template <typename S, typename P, typename AR, typename AT>
void DataReadLockSuitabilities(htp_info_t<S, P, AR, AT>& htpInfo)
{
	UInt32 K = htpInfo.GetK();

	// suitabilityMaps
	for (UInt32 j=0; j!=K; ++j)
	{
		ggType_info_t<S>& gg = htpInfo.m_ggTypes[j];
		auto suitMapLock = lock_or_cancel(gg.m_diSuitabilityMap); // owning for this scope; throws if torn down
		gg.m_SuitabilityDataLock = DataReadLock(suitMapLock.get());
		dms_assert(gg.m_SuitabilityDataLock.IsLocked());

		gg.m_Suitabilities = const_array_cast<S>(gg.m_SuitabilityDataLock.get_ptr())->GetDataRead();
	}
}

template <typename S, typename P, typename AR, typename AT>
void PrepareResultTileLock(htp_info_t<S, P, AR, AT>& htpInfo, bool initUndefined)
{
	htpInfo.m_ResultArray = mutable_array_cast<AT>(htpInfo.m_ResultDataLock)->GetDataWrite(no_tile, dms_rw_mode::write_only_all);

	if (initUndefined)
		fast_fill(htpInfo.m_ResultArray.begin(), htpInfo.m_ResultArray.end(), UNDEFINED_VALUE(AT));
}

template <typename S, typename P, typename AR, typename AT>
void PrepareTileLock(htp_info_t<S, P, AR, AT>& htpInfo)
{
	if constexpr (!std::is_same_v<AR, Void>)
	{
		const DataArray<AR>* atomicRegionObj = htpInfo.m_AtomicRegionMapObj;
		assert(atomicRegionObj);
		htpInfo.m_AtomicRegionMapData = atomicRegionObj->GetDataRead();
	}

	UInt32 K = htpInfo.GetK();
	MG_CHECK(K >= 1);

	// suitabilityMaps
	for (UInt32 j=0; j!=K; ++j)
	{
		ggType_info_t<S>& gg = htpInfo.m_ggTypes[j];
		gg.m_Suitabilities = const_array_checked_cast<S>(lock_or_cancel(gg.m_diSuitabilityMap).get())->GetDataRead();
		htpInfo.PreparePermutation(gg.m_Suitabilities.size());
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

	PrepareResultTileLock<S, P, AR>(htpInfo, false);
}

template <typename S, typename P, typename AR, typename AT>
void PrepareFacets(htp_info_t<S, P, AR, AT>& htpInfo)
{
	UInt32 K = htpInfo.GetK();
	UInt32 nrAtomicRegions = htpInfo.GetNrAtomicRegions();

	htpInfo.m_FacetIds.reserve(SizeT(nrAtomicRegions) * K * K);

	typedef std::pair<claim<S, P>*, claim<S, P>*> claim_pair;

	std::map<claim_pair, UInt32> allocatedQueIds;

	for (UInt32 ar = 0; ar != nrAtomicRegions; ++ar)
	{
		for (UInt32 j=0; j !=K; ++j)
		{
			claim<S, P>* claimJ = &htpInfo.GetClaim(ar, j);
			for (UInt32 jj=0; jj!=K; ++jj) 
			{
				if (jj == j)
					htpInfo.m_FacetIds.push_back(-1); // ease of admin
				else
				{
					claim<S, P>* claimJJ = &htpInfo.GetClaim(ar, jj);
					claim_pair claimPair(claimJ, claimJJ);
					if (allocatedQueIds.find(claimPair) == allocatedQueIds.end())
					{
						UInt32 heapID = htpInfo.m_Facets.size();
						allocatedQueIds[claimPair] = heapID;
						htpInfo.m_Facets.push_back(
							priority_heap<S, P>(
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
	dms_assert(htpInfo.m_FacetIds.size() == SizeT(nrAtomicRegions) * K * K); // postcondition of this function;
	// it lived in PrepareReport until the greedy/needy regimes started skipping PrepareFacets altogether.
}

template <typename S, typename P, typename AR, typename AT>
void PrepareReport(htp_info_t<S, P, AR, AT>& htpInfo)
{
	reportF(SeverityTypeID::ST_MajorTrace, "DiscrAlloc: Prepare created alloc structs for "
		"{} cells, {} landuse types, {} (min-max) claims, {} unique partitionings, "
		"{} atomic regions, {} unique regions, and {} priority queues",
		htpInfo.GetN(), 
		htpInfo.GetK(), 
		htpInfo.GetNrNodes(), 
		htpInfo.GetNrPartitionings(), 
		htpInfo.GetNrAtomicRegions(), 
		htpInfo.GetNrUniqueRegions(), 
		htpInfo.GetNrLinks()
	);
}

// *****************************************************************************
//									Update
// *****************************************************************************

template <typename S, typename P, typename AR, typename AT>
void RemoveLoserInResultAndCleanupQueues(htp_info_t<S, P, AR, AT>& htpInfo
	,	typename htp_info_t<S, P, AR, AT>::atomic_region_proxy ar
	,	land_unit_id i
	,	AT losing_ggTypeID)
{
	UInt32 K = htpInfo.m_ggTypes.size();
	for (UInt32 j=0; j!=K; ++j) if (j != losing_ggTypeID)
	{
		priority_heap<S, P>& ph = htpInfo.GetHeap(ar, losing_ggTypeID, j);

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
template <typename S, typename P, typename AR, typename AT>
void InsertWinnerInResultAndReallocQueues(
	htp_info_t<S, P, AR, AT>& htpInfo
,	typename htp_info_t<S, P, AR, AT>::atomic_region_proxy ar
,	land_unit_id i
,	AT winning_ggTypeID)
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

		priority_heap<S, P>& ph = htpInfo.GetHeap(ar, winning_ggTypeID, j);

		UInt32 popNode = ph.m_TargetClaim - begin_ptr( htpInfo.m_Claims );
		dms_assert( currNode == ph.m_SourceClaim - begin_ptr( htpInfo.m_Claims ) );

#if defined(MG_DEBUG) // DEBUG
		if( ph.m_SourceClaim->m_ShadowPrice // Ga
			+	shadow_price<S, P>(
					htpInfo.m_ggTypes[winning_ggTypeID].m_Suitabilities[i] - htpInfo.m_ggTypes[j].m_Suitabilities[i],
					PerturbationOf<P>(i, ph.m_PerturbationFactor)
				)                                  // -(Qa - Qb)
			<	ph.m_TargetClaim->m_ShadowPrice)   // Gb
		{
			DBG_START("DiscrAlloc", "InsertWinnerInResultAndReallocQueues", true);
			reportF(SeverityTypeID::ST_Warning,
				"Problem defending cell {} from {}${} against {}${} ",
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
				DBG_TRACE(("{}${}, reached by Link[{}]({},{}) was incremented by ${}",
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
				DBG_TRACE(("Suitability at cell {} AT {}[{}] = {}",
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

template <typename S, typename P, typename AR, typename AT>
UInt32 FindMstDown(
	htp_info_t<S, P, AR, AT>&                     htpInfo, 
	UInt32                                     rootClaimID,
	typename htp_info_t<S, P, AR, AT>::cost_type& minLinkCost //cost until dst of (free)link; thus including GetLinkCost(currLink)
)
{
	DBG_START("DiscrAlloc", "FindMstDown", DMS_DEBUG_DISCRALLOC);

	UInt32 minLink = UNDEFINED_VALUE(UInt32); // corresponds with given minLinkCost if not INF.

	htpInfo.m_TreeBuilder.init_tree(rootClaimID, dir_forward_tag() ); // calls fix_node and brings all outgoing links in queue

#if defined(MG_DEBUG)
	if (htpInfo.m_TreeBuilder.empty() && htpInfo.CanReportFindMstDown() )
	{
		reportF(SeverityTypeID::ST_MajorTrace, "FindMstDown: no adjustments possible for {}",
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

		const directed_heap_elem<typename htp_info_t<S, P, AR, AT>::cost_type>& currElem = htpInfo.m_TreeBuilder.top(); 

		UInt32 currLink = currElem.Link();
		auto   linkCost = currElem.Cost(); //cost until dst of link; thus including GetLinkCost(currLink)
		DBG_TRACE(( "currLink {} with linkCost {}", currLink, AsString(linkCost).c_str() ));

		const claim<S, P>* targetClaim = htpInfo.m_Facets[currLink].m_TargetClaim;

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
			DBG_TRACE(("FindMstDown returns free lowerbound at link {} at cost {}", minLink, AsString(minLinkCost).c_str()));
			return minLink;
		}

		UInt32 dstNode = targetClaim - begin_ptr( htpInfo.m_Claims );
		if (!atMax)
		{
			DBG_TRACE(("FindMstDown signals target claim {}({}, {}) of Facet {} as vacant", 
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
			DBG_TRACE(("FindMstDown: TargetClaim ({}, {}) has overflow", 
				targetClaim->m_ggTypeID, targetClaim->m_RegionID
			));
		}
#endif
		DBG_TRACE(("FindMstDown fixes link to target claim {}({}, {}) of Facet {} at cost {}", 
			dstNode, targetClaim->m_ggTypeID, targetClaim->m_RegionID,
			currLink,
			AsString(linkCost).c_str()
		));
		

		htpInfo.m_TreeBuilder.fix_link(	currLink, linkCost, dir_forward_tag() );         // bring all links from the destination of currLink into queue for further processing
		htpInfo.m_ClaimIdList.push_back( dstNode ); // maintain ordered built MST for splitter adjustments

		dms_assert( htpInfo.CheckLink(currLink) );
	}
}

template <typename S, typename P, typename AR, typename AT>
UInt32 FindMstUp(
	htp_info_t<S, P, AR, AT>&                     htpInfo, 
	UInt32                                     rootClaimID,
	typename htp_info_t<S, P, AR, AT>::cost_type& minLinkCost //cost until dst of (free)link; thus including GetLinkCost(currLink)
)
{
	using price_type = shadow_price<S, P>; // comma-free spelling: dms_assert is a macro, so a bare shadow_price<S, P>() would split its arguments
	DBG_START("DiscrAlloc", "FindMstUp", DMS_DEBUG_DISCRALLOC);

	UInt32 minLink = UNDEFINED_VALUE(UInt32); // corresponds with given minLinkCost if not INF.

	htpInfo.m_TreeBuilder.init_tree(rootClaimID, dir_backward_tag() ); // calls fix_node and brings all outgoing links in queue

#if defined(MG_DEBUG)
	if (htpInfo.m_TreeBuilder.empty())
	{
		reportF(SeverityTypeID::ST_MajorTrace, "FindMstUp: no adjustments possible for {}",
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

		const directed_heap_elem<typename htp_info_t<S, P, AR, AT>::cost_type>& currElem = htpInfo.m_TreeBuilder.top(); 

		UInt32 currLink = currElem.Link();
		auto   linkCost = currElem.Cost(); //cost until dst of link; thus including GetLinkCost(currLink)
		DBG_TRACE(( "currLink {} with linkCost {}", currLink, AsString(linkCost).c_str() ));

		const claim<S, P>* sourceClaim = htpInfo.m_Facets[currLink].m_SourceClaim;

		bool atMin = sourceClaim->AtMin();
		if (atMin && sourceClaim->m_Count > sourceClaim->m_ClaimRange.first)
		{
			dms_assert(sourceClaim->m_ShadowPrice < price_type()); // else it wouldnt be AtMin
			if (linkCost - sourceClaim->m_ShadowPrice < minLinkCost)
			{
				minLinkCost = linkCost - sourceClaim->m_ShadowPrice;
				minLink     = currLink;
			}
		}
		if (linkCost > minLinkCost)
		{
			DBG_TRACE(("FindMstUp returns free lowerbound at link {} at cost {}", minLink, AsString(minLinkCost).c_str()));
			return minLink;
		}

		UInt32 srcNode = sourceClaim - begin_ptr( htpInfo.m_Claims );
		if (!atMin)
		{
			DBG_TRACE(("FindMstUp signals source claim {}({}, {}) of Facet {} as vacant", 
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
			DBG_TRACE(("FindMstUp: SourceClaim {}({}, {}) has overflow", 
				srcNode, sourceClaim->m_ggTypeID, sourceClaim->m_RegionID
			));
		}
#endif

		DBG_TRACE(("FindMstUp fixes link to source claim ({}, {}) of Facet {} at cost {}", 
			sourceClaim->m_ggTypeID, sourceClaim->m_RegionID,
			currLink,
			AsString(linkCost).c_str()
		));
		

		htpInfo.m_TreeBuilder.fix_link(	currLink, linkCost, dir_backward_tag() );         // bring all links from the destination of currLink into queue for further processing

		htpInfo.m_ClaimIdList.push_back( srcNode ); // maintain ordered built MST for splitter adjustments

		dms_assert( htpInfo.CheckLink(currLink) );
	}
}

template <typename S, typename P, typename AR, typename AT>
bool UpdateSplitterDown(htp_info_t<S, P, AR, AT>& htpInfo, claim<S, P>& root)
{
	using price_type = shadow_price<S, P>; // comma-free spelling: dms_assert is a macro, so a bare shadow_price<S, P>() would split its arguments
	DBG_START("DiscrAllocCells", "UpdateSplitterDown", DMS_DEBUG_DISCRALLOC);

	// make shortest path tree (dijstra) with claims as vertices and priority heaps as edges
	UInt32 rootClaimID = &root - begin_ptr( htpInfo.m_Claims );
	dms_assert(rootClaimID < htpInfo.GetNrNodes());

	DBG_TRACE(("SrcClaim {}({}, {}) with claimrange [{},{}] and price {} has {} assignees", 
		rootClaimID, root.m_ggTypeID, root.m_RegionID, 
		root.m_ClaimRange.first, root.m_ClaimRange.second, 
		AsString(root.m_ShadowPrice).c_str(),
		root.m_Count
	));


	price_type freeClaimCost = MaxValue<price_type>(); //cost until dst of (free)link;
	if (root.m_Count <= root.m_ClaimRange.second) // Overflow created by going over min with positive price
	{
		dms_assert(root.m_Count -1 == root.m_ClaimRange.first);
		dms_assert(root.m_ShadowPrice > price_type());
		freeClaimCost = root.m_ShadowPrice;
		DBG_TRACE( ("SrcClaim is over lowerbound with positive price reduction possible") );
	}

	UInt32 freeLink = FindMstDown(htpInfo, rootClaimID, freeClaimCost);

	if (freeClaimCost == MaxValue<price_type>())
		return false;

	DBG_TRACE(("FindMstDown returned FreeLink {} at cost {}", freeLink, AsString(freeClaimCost).c_str() ));
	// adjust G such that transport from root to nearest free claim becomes a free lunch

	root.m_ShadowPrice = CheckedSub(root.m_ShadowPrice, freeClaimCost);

	// adjust G on whole MST (including dead ends) in ClaimIdList order (from root) 
	// to prevent unadministered facet crossings
	// laat schadowprijs van target stijgen op basis van (Ga - Gb) = (Qa - Qb)  
	//	=>  Gb := Ga - (Qa - Qb) = Ga + c, want c = -(Qa - Qb)

	for (auto claimIdPtr = htpInfo.m_ClaimIdList.begin(), claimIdEnd = htpInfo.m_ClaimIdList.end(); claimIdPtr != claimIdEnd; ++claimIdPtr)
	{
		const directed_heap_elem<shadow_price<S, P> >& traceBack = 
			htpInfo.m_TreeBuilder.get_traceback(*claimIdPtr);

		priority_heap<S, P>& ph = htpInfo.m_Facets[traceBack.Link()];

		UInt32 i = ph.top();
		S      c = ph.GetC(i);
		shadow_price<S, P> transferCost(c, PerturbationOf<P>(i, ph.m_PerturbationFactor));

		// this heap was visited and cleaned since last reallocation
		dms_assert(htpInfo.m_ResultArray[i] == ph.m_SourceClaim->m_ggTypeID);

		dms_assert( ph.m_TargetClaim->m_ShadowPrice
			>=	ph.m_SourceClaim->m_ShadowPrice + transferCost );

		ph.m_TargetClaim->m_ShadowPrice = CheckedAdd(ph.m_SourceClaim->m_ShadowPrice, transferCost);

		dms_assert(htpInfo.GetLinkCostUnchecked( traceBack.Link() ) == price_type()); 
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
		priority_heap<S, P>& ph = htpInfo.m_Facets[freeLink];
		land_unit_id i = ph.top(); 
		DBG_TRACE(
			(	"Relax Facet {}: ({}, {})->({}, {}) with cell {}", 
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
		dms_assert(htpInfo.GetLinkCostUnchecked(freeLink) == price_type()); 


		dms_assert(htpInfo.m_ResultArray[i] == ph.m_SourceClaim->m_ggTypeID);

		UInt32 ggTypeIdSrc = ph.m_SourceClaim->m_ggTypeID;
		UInt32 ggTypeIdDst = ph.m_TargetClaim->m_ggTypeID;

		assert(htpInfo.m_ResultArray[i] == ggTypeIdSrc); ph.m_SourceClaim->m_Count--;
		auto ar = htpInfo.GetAtomicRegionID(i);
		RemoveLoserInResultAndCleanupQueues <S, P, AR, AT>(htpInfo, ar, i, ggTypeIdSrc);
		InsertWinnerInResultAndReallocQueues<S, P, AR, AT>(htpInfo, ar, i, ggTypeIdDst);
		assert(htpInfo.m_ResultArray[i] == ggTypeIdDst); ph.m_TargetClaim->m_Count++;
		
#if defined(MG_DEBUG)
		if (ph.m_TargetClaim->Overflow())
		{
			reportF(SeverityTypeID::ST_MajorTrace, "UpdateSplitterDown: Realloc.Target {} has overflow",
				htpInfo.GetClaimRangeStr( *ph.m_TargetClaim ).c_str()
			);
		}

		if (ph.m_SourceClaim->Overflow())
		{
			reportF(SeverityTypeID::ST_MajorTrace, "UpdateSplitterDown: Realloc.Source {} has overflow",
				htpInfo.GetClaimRangeStr( *ph.m_SourceClaim ).c_str()
			);
		}
#endif

		dms_assert( ph.empty() || htpInfo.CheckLink(freeLink) ); 

		freeLink = htpInfo.m_TreeBuilder.get_traceback(htpInfo.GetSrcNode(freeLink, dir_forward_tag() )).Link();
	}

	return true;
}

template <typename S, typename P, typename AR, typename AT>
bool UpdateSplitterUp(htp_info_t<S, P, AR, AT>& htpInfo, claim<S, P>& root)
{
	using price_type = shadow_price<S, P>; // comma-free spelling: dms_assert is a macro, so a bare shadow_price<S, P>() would split its arguments
	DBG_START("DiscrAllocMinClaims", "UpdateSplitterUp", DMS_DEBUG_DISCRALLOC);

	// make shortest path tree (dijstra) with claims as vertices and priority heaps as edges
	UInt32 rootClaimID = &root - begin_ptr( htpInfo.m_Claims );
	dms_assert(rootClaimID < htpInfo.GetNrNodes());

	DBG_TRACE(("SrcClaim {}({}, {}) with claimrange [{},{}] and price {} has {} assignees", 
		rootClaimID, root.m_ggTypeID, root.m_RegionID, 
		root.m_ClaimRange.first, root.m_ClaimRange.second, AsString(root.m_ShadowPrice).c_str(),
		root.m_Count
	));


	price_type freeClaimCost = MaxValue<price_type>(); //cost until dst of (free)link;
	if (root.m_Count >= root.m_ClaimRange.first) // Underflow created by going over min with negative price
	{
		dms_assert(root.m_ShadowPrice < price_type());
		freeClaimCost = CheckedSub(price_type(), root.m_ShadowPrice);
		DBG_TRACE( ("SrcClaim is over lowerbound with positive price reduction possible") );
	}

	UInt32 freeLink = FindMstUp(htpInfo, rootClaimID, freeClaimCost);

	if (freeClaimCost == MaxValue<price_type>())
		return false;

	DBG_TRACE(("FindMstUp returned FreeLink {} at cost {}", freeLink, AsString(freeClaimCost).c_str() ));
	// adjust G such that transport from root to nearest free claim becomes a free lunch

	root.m_ShadowPrice = CheckedAdd(root.m_ShadowPrice, freeClaimCost);

	// adjust G on whole MST (including dead ends) in ClaimIdList order (from root) 
	// to prevent unadministered facet crossings
	// laat schadowprijs van target stijgen op basis van (Ga - Gb) = (Qa - Qb)  
	//	=>  Gb := Ga - (Qa - Qb) = Ga + c, want c = -(Qa - Qb)

	std::vector<UInt32>::const_iterator
		claimIdPtr = htpInfo.m_ClaimIdList.begin(),
		claimIdEnd = htpInfo.m_ClaimIdList.end();
	while (claimIdPtr != claimIdEnd)
	{
		const directed_heap_elem<shadow_price<S, P> >& traceBack = 
			htpInfo.m_TreeBuilder.get_traceback(*claimIdPtr++);

		priority_heap<S, P>& ph = htpInfo.m_Facets[traceBack.Link()];

		UInt32            i  = ph.top();
		S                 c  = ph.GetC(i);
		shadow_price<S, P> transferCost(c, PerturbationOf<P>(i, ph.m_PerturbationFactor));

		// this heap was visited and cleaned since last reallocation
		dms_assert(htpInfo.m_ResultArray[i] == ph.m_SourceClaim->m_ggTypeID);

		dms_assert( ph.m_TargetClaim->m_ShadowPrice
			>=	ph.m_SourceClaim->m_ShadowPrice + transferCost );

		ph.m_SourceClaim->m_ShadowPrice = CheckedSub(ph.m_TargetClaim->m_ShadowPrice, transferCost);

		dms_assert(htpInfo.GetLinkCostUnchecked( traceBack.Link() ) == price_type()); 
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
		priority_heap<S, P>& ph = htpInfo.m_Facets[freeLink];
		SizeT i = ph.top(); 

		DBG_TRACE(
			(	"Pull Facet {}: ({}, {})->({}, {}) with cell {}", 
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
		assert(htpInfo.GetLinkCostUnchecked(freeLink) == price_type()); 


		assert(htpInfo.m_ResultArray[i] == ph.m_SourceClaim->m_ggTypeID);

		UInt32 ggTypeIdSrc = ph.m_SourceClaim->m_ggTypeID;
		UInt32 ggTypeIdDst = ph.m_TargetClaim->m_ggTypeID;

		assert(htpInfo.m_ResultArray[i] == ggTypeIdSrc); ph.m_SourceClaim->m_Count--;
		auto ar = htpInfo.GetAtomicRegionID(i);
		RemoveLoserInResultAndCleanupQueues <S, P, AR, AT>(htpInfo, ar, i, ggTypeIdSrc);
		InsertWinnerInResultAndReallocQueues<S, P, AR, AT>(htpInfo, ar, i, ggTypeIdDst);
		assert(htpInfo.m_ResultArray[i] == ggTypeIdDst); ph.m_TargetClaim->m_Count++;
		
//		dms_assert(!ph.m_TargetClaim->Overflow()); target will be relaxed in next pull
#if defined(MG_DEBUG)
		if (ph.m_SourceClaim->Overflow())
		{
			reportF(SeverityTypeID::ST_MajorTrace, "UpdateSplitterUp: Realloc.Source {} has overflow",
				htpInfo.GetClaimRangeStr( *ph.m_SourceClaim ).c_str()
			);
		}
#endif

		assert( ph.empty() || htpInfo.CheckLink(freeLink) ); 

		freeLink = htpInfo.m_TreeBuilder.get_traceback(htpInfo.GetSrcNode(freeLink, dir_backward_tag() )).Link();
	}

	return true;
}

// *****************************************************************************
//									DiscrAlloc
// *****************************************************************************

template <typename AR>
struct ArOrVoid
{
	using type = AR;
	static void Inc(const AR*& ptr)
	{
		assert(ptr);
		++ptr;
	}
	static UInt32 Deref(const AR* ptr)
	{
		assert(ptr);
		return *ptr;
	}
};

template <>
struct ArOrVoid<Void>
{
	using type = UInt32;
	static void Inc(const UInt32* ptr)
	{
		assert(!ptr);
	}
	static UInt32 Deref(const UInt32* ptr)
	{
		assert(!ptr);
		return 0;
	}
};

struct DistFromOpt
{
	UInt32 nrLandUnits, nrSubOptimal, nrDueToBelowThreshold;
	// Int64, not UInt64: a net negative total is a legitimate outcome of negative suitabilities
	// and must report as negative rather than wrap. Both components are widened from S / P so
	// the sums have room.
	shadow_price<Int64, Int64> totalSuit, totalDistFromOpt;
	tile_id tn;

	template <typename S, typename P, typename AR, typename AT>
	DistFromOpt(htp_info_t<S, P, AR, AT>& htpInfo)
		:	nrLandUnits()
		,	nrSubOptimal()
		,	nrDueToBelowThreshold()
		,	totalSuit()
		,	totalDistFromOpt()
		,	tn( lock_or_cancel(htpInfo.m_MapDomain)->GetNrTiles() )
	{
		UInt32 K = htpInfo.GetK();

		land_unit_id N = htpInfo.m_N;
		nrLandUnits += N;
//			nrOptions   += (K-1)*N;

		const typename ArOrVoid<AR>::type* armIter = nullptr;
		if constexpr (!std::is_same<AR, Void>::value)
			armIter = htpInfo.m_AtomicRegionMapData.begin();

		for(land_unit_id i=0; i < N; ArOrVoid<AR>::Inc(armIter), ++i) // cell index 0..N
		{
			AT currBuyer = htpInfo.m_ResultArray[i];
			if (!IsDefined(currBuyer))
				continue;

			UInt32 ar = ArOrVoid<AR>::Deref(armIter);

			shadow_price<S, P> currPrice(htpInfo.m_ggTypes[currBuyer].m_Suitabilities[i], PerturbationOf<P>(i, P(currBuyer)));
			CheckedAccumulate(totalSuit, currPrice);
			currPrice = CheckedAdd(currPrice, htpInfo.GetClaim(ar, currBuyer).m_ShadowPrice);

			P iXj = 0; // i*j, as in the bidding loop of DiscrAllocCells; bounded by CheckPerturbationRange
			// belowThreshold is only read when foundHigherBidder is set, and the branch that sets
			// the latter always assigns the former -- initialised anyway so the coupling cannot rot.
			bool belowThreshold = false, foundHigherBidder = false;

			for(UInt32 j=0; j!=K; ++j, iXj += P(i))
			{
				S Sij = htpInfo.m_ggTypes[j].m_Suitabilities[i];

				// An undefined suitability means this type is no option for this land unit at all,
				// so there is no bid to compare the assignment against. The bidding loop of
				// DiscrAllocCells reaches the same conclusion through `s < m_Threshold`: null is
				// MIN_VALUE(S) and MinValue<S>() is MIN_VALUE(S)+1 (has_min_as_null), so a null
				// sorts below even the default threshold and is skipped there before any
				// arithmetic. This loop is a replica of that one, and skipping has to happen here
				// too -- it consults the threshold only AFTER the add, to classify the deviation.
				//
				// Without this, CheckedAdd() below gets null + the claim's shadow price and reports
				// a numeric overflow, which fails the whole allocation from what is only a status
				// message ("DiscrAlloc completed with ... at most N from optimum"). Before the
				// checked arithmetic of #1196 that add wrapped instead: null + a negative price
				// came out large and POSITIVE, so a type that was never an option compared as a
				// better bid and silently inflated nrSubOptimal and totalDistFromOpt. The
				// dms_assert(belowThreshold) below did not catch it -- a null IS below the
				// threshold -- and in Release it is CC_ASSUME. So those figures have been wrong
				// for every model with null suitabilities; this is not merely an overflow fix.
				//
				// Only UNdefined values are skipped, not everything below the threshold: measuring
				// deviations that the threshold caused is exactly what this loop is for.
				if (!IsDefined(Sij))
					continue;

				shadow_price<S, P> bid(Sij, iXj);  // small pertubation (SoS) for making a difference between similar cells
				bid = CheckedAdd(bid, htpInfo.GetClaim(ar, j).m_ShadowPrice);
				if (currPrice < bid)
				{
					foundHigherBidder = true;
					// kept as two steps rather than one bid - currPrice: the difference of two
					// S values need not fit in S, whereas the running total is already Int64.
					totalDistFromOpt.first  = CheckedPriceSub<Int64>(CheckedPriceAdd<Int64>(totalDistFromOpt.first, bid.first), currPrice.first);
					totalDistFromOpt.second = CheckedPerturbationSub<Int64>(CheckedPerturbationAdd<Int64>(totalDistFromOpt.second, bid.second), currPrice.second);

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

template <typename S, typename P, typename AR, typename AT>
void CheckAllLinks(htp_info_t<S, P, AR, AT>& htpInfo)
{
	using price_type = shadow_price<S, P>; // comma-free spelling: dms_assert is a macro, so a bare shadow_price<S, P>() would split its arguments
	UInt32 L = htpInfo.GetNrLinks();
	for (UInt32 l = 0; l != L; ++l)
		dms_assert(htpInfo.GetLinkCostUnchecked(l) >= price_type());
}

#endif

template <typename S, typename P, typename AR, typename AT>
bool CheckAllClaims(const htp_info_t<S, P, AR, AT>& htpInfo, SharedStr* resultPtr)
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
				mySSPrintF("{}; {} allocated for price {}; ",
					htpInfo.GetClaimRangeStr(*claimIter).c_str(),
					claimIter->m_Count, 
					AsString(claimIter->m_ShadowPrice).c_str()
				);
			
			reportF(SeverityTypeID::ST_MajorTrace, "CheckAllClaims failed: {}", claimResult.c_str());
			if (resultPtr)
				(*resultPtr) += claimResult;
		}
	}
	return isAllOK;
}

template <typename S, typename P, typename AR, typename AT>
void DiscrAllocCellsBegin(htp_info_t<S, P, AR, AT>& htpInfo, UInt32 nextI)
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
	reportF(SeverityTypeID::ST_MajorTrace, "DiscrAlloc {}: claims for {} cells: min={}; max={}",
		htpInfo.GetN(), nextI,
		sumMinClaim,
		sumMaxClaim
	);
}

template <typename S, typename P, typename AR, typename AT>
void DiscrAllocEnd(htp_info_t<S, P, AR, AT>& htpInfo, UInt32 currI)
{
	// set StartPrice to CurrPrice and report maximum difference
	auto
		claimIter = htpInfo.m_Claims.begin(),
		claimEnd  = htpInfo.m_Claims.end();
	shadow_price<S, P> minPriceDiff, maxPriceDiff;
	while (claimIter != claimEnd)
	{
		shadow_price<S, P> priceDiff = CheckedSub(claimIter->m_ShadowPrice, claimIter->m_StartPrice);
		MakeMin(minPriceDiff, priceDiff);
		MakeMax(maxPriceDiff, priceDiff);

		claimIter->m_StartPrice = claimIter->m_ShadowPrice;

		++claimIter;
	}
	reportF(SeverityTypeID::ST_MajorTrace, "DiscrAllocCells {}:  {} cells completed: price adjustments range from {} to {}",
		htpInfo.GetN(), currI,
		AsString(minPriceDiff).c_str(),
		AsString(maxPriceDiff).c_str()
	);
}

template <typename S, typename P, typename AR, typename AT>
void DiscrAllocCells(htp_info_t<S, P, AR, AT>& htpInfo, UInt32 currI, UInt32 nextI)
{
	DiscrAllocCellsBegin(htpInfo, nextI);

	land_unit_id N = htpInfo.m_N;
	UInt32 K = htpInfo.GetK();
	UInt32 rapFreq = 1000000, tmpK = K; while (tmpK > 3 && rapFreq > 100)  { rapFreq /= 10; tmpK /= 10; }

	if constexpr (!std::is_same_v<AR, Void>)
	{
		assert(htpInfo.m_AtomicRegionMapData.size() == N);
		assert(htpInfo.m_ResultArray.size() == N);
	}

	UInt32 d_nrSplits = 0;

	for( ; currI < nextI; htpInfo.GetNextPermutationValue(), ++currI)
	{
		assert(htpInfo.m_CurrPI < htpInfo.m_N);
		auto ar = htpInfo.GetAtomicRegionID( htpInfo.m_CurrPI );
		assert(ar < htpInfo.GetNrAtomicRegions()); // guaranteed by IncrementAtomicRegionCount
//		assert(!IsDefined(htpInfo.m_ResultArray[htpInfo.m_CurrPI]));

		UInt32 highestBidder = UNDEFINED_VALUE(UInt32);
		shadow_price<S, P> highestBid = MinValue<shadow_price<S, P> >();

		P c = 0; // i*j, the SoS perturbation of type j's bid for land unit i; bounded by CheckPerturbationRange

		for(UInt32 j=0; j!=K; ++j, c += P(htpInfo.m_CurrPI))
		{
			S s = htpInfo.m_ggTypes[j].m_Suitabilities[htpInfo.m_CurrPI];
			if (s < htpInfo.m_Threshold) continue;
			shadow_price<S, P> bid = htpInfo.GetClaim(ar, j).m_ShadowPrice;
			                bid.first  = CheckedPriceAdd(bid.first, s);
			                bid.second = CheckedPerturbationAdd(bid.second, c); // small pertubation (SoS) for making a difference between similar cells

			if (highestBid < bid)
			{
				highestBid    = bid;
				highestBidder = j;
			}
		}

		// insert highestBidder into solution and queues
		if (highestBidder == UNDEFINED_VALUE(UInt32) )
		{
			++htpInfo.m_NrBelowThreshold;
			if (htpInfo.m_NrBelowThreshold <= NR_BELOW_THRESHOLD_NOTIFICATIONS)
				reportF(SeverityTypeID::ST_MajorTrace, "DiscrAllocCells: all suitabilities of cell {} are below the threshold {}",
					htpInfo.m_CurrPI, htpInfo.m_Threshold
				); 
			htpInfo.m_ResultArray[htpInfo.m_CurrPI] = UNDEFINED_VALUE(AT);
			continue;
		}

		InsertWinnerInResultAndReallocQueues<S, P, AR, AT>(htpInfo, ar, htpInfo.m_CurrPI, highestBidder);

		// Update total and move if facing claim-restriction
		claim<S, P>& claim = htpInfo.GetClaim(ar, highestBidder);
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
						"DiscrAlloc Warning: UpdateSplitterDown({}) failed; now {} allocated",
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
				"DiscrAllocCells {}: Progress {}/{}; {} calls to UpdateSplitterDown",
				N, currI, nextI, d_nrSplits
			); 
	}
	dms_assert(htpInfo.m_CurrPI >= htpInfo.m_N);
	reportF(SeverityTypeID::ST_MajorTrace,
		"DiscrAllocCells {}: {} cells completed with {} calls to UpdateSplitterDown",
		N, currI, d_nrSplits
	); 
}

template <typename S, typename P, typename AR, typename AT>
void DiscrAllocMinClaims(htp_info_t<S, P, AR, AT>& htpInfo)
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
				"DiscrAlloc Warning: UpdateSplitterUp({}) failed; only {} allocated",
				htpInfo.GetClaimRangeStr( *claimIter ).c_str(),
				claimIter->m_Count
			); 
		++claimIter;
	}

	reportF(SeverityTypeID::ST_MajorTrace,
		"DiscrAllocMinClaims completed with {} calls to UpdateSplitterUp",
		count
	); 
}

template <typename S, typename P, typename AR, typename AT>
void SolveRange(htp_info_t<S, P, AR, AT>& htpInfo, UInt32 firstI, UInt32 lastI)
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

// a * b / d, evaluated in 64 bits and narrowed back to the UInt32 claim type. ScaleClaims keeps
// b <= d, which bounds the quotient by a, but that is the CALLER's invariant and not this
// function's, so the narrowing is checked rather than assumed. Issue #1196.
UInt32 muldiv_u32(UInt32 a, UInt32 b, UInt32 d)
{
	UInt64 p64 = UInt64(a) * UInt64(b); // exact: both operands fit in 32 bits
	if (!d)
	{
		dms_assert(!p64);
		return 0;
	}
	UInt64 q64 = p64 / d;
	if (q64 > MAX_VALUE(UInt32))
		throwDmsErrF("discrete_alloc: scaling the claim {} by {}/{} gives {}, which does not fit in the UInt32 claim type",
			a, b, d, q64
		);
	return UInt32(q64);
}

struct ClaimScaler: std::vector<claim_range>
{
//	std::vector<claim<S, P> >& m_Claims;

	template <typename S, typename P>
	ClaimScaler(std::vector<claim<S, P> >& claims)
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

	template <typename S, typename P>
	void RestoreClaims(std::vector<claim<S, P> >& claims)
	{
		auto
			claimIter = claims.begin(),
			claimEnd  = claims.end();
		for (const_iterator orgClaimRangePtr = begin(); claimIter != claimEnd; ++claimIter)
			claimIter->m_ClaimRange = *orgClaimRangePtr++;
	}

	template <typename S, typename P, typename AR, typename AT>
	void ScaleClaims(htp_info_t<S, P, AR, AT>& htpInfo, UInt32* atomicRegionCount, UInt32 N, UInt32 nextI)
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
			RestoreClaims<S, P>(htpInfo.m_Claims);
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
void IncrementAtomicRegionCount(std::vector<claim_type>& atomicRegionCount, const regions_info_t<AR>& regionInfo, land_unit_id i, land_unit_id e)
{
	// count per ar with stepSize
	for (; i < e; regionInfo.GetNextPermutationValue(), ++i)
	{
		assert(regionInfo.m_CurrPI < regionInfo.m_N);
		AR ar = regionInfo.GetAtomicRegionID(regionInfo.m_CurrPI);
		if (ar >= atomicRegionCount.size())
			regionInfo.m_AtomicRegionMap->GetAbstrValuesUnit()->throwItemErrorF(
					"Value {}{} out of range of valid Atomic Regions"
				,	ar
				,	IsDefined(ar) ? "" : " (a.k.a. null-value)"
			);
		++atomicRegionCount[ar];
	}
	assert(regionInfo.m_CurrPI >= regionInfo.m_N);
}

template <>
void IncrementAtomicRegionCount<Void>(std::vector<claim_type>& atomicRegionCount, const regions_info_t<Void>& regionInfo, land_unit_id i, land_unit_id e)
{
	assert(atomicRegionCount.size() == 1);
	atomicRegionCount[0] += (e - i);
}

template <typename S, typename P, typename AR, typename AT>
void Solve(htp_info_t<S, P, AR, AT>& htpInfo, S threshold, AbstrDataObject* resPrices)
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
		reportF(SeverityTypeID::ST_MajorTrace, "DiscrAlloc: SolveScaled per {} cells", htpInfo.m_StepSize);

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
//					SolveGreedy: the greedy_alloc and needy_alloc regimes
// *****************************************************************************
//
// One static ranking of the land units, then at most two sweeps over that ranking. No shadow
// prices, no facet queues, no reallocation: once a land unit has been given a type it keeps it.
//
// RANKING (once, before anything is allocated; see rank_unit below)
//   Per land unit take the suitabilities that reach the threshold. 'best' is the highest of them
//   and 'next' the second highest, or the threshold itself when the unit has only one admissible
//   type -- the threshold is the value at which allocating stops being worthwhile, so it is the
//   natural stand-in for "this unit would go unallocated instead".
//     greedy_alloc ranks by  best         -- the highest bidders are served first.
//     needy_alloc  ranks by  best - next  -- the units with the most to lose are served first.
//   Ties are broken by land unit index, so a run is reproducible and independent of tiling.
//   The ranking is NOT recomputed while allocating: a bid is what a land unit is worth, not a
//   moving target, which is what makes the outcome easy to explain.
//
// SWEEP 1 (skipped when every minimum claim is 0)
//   Reserve for the minimum claims: each land unit, in ranking order, goes to its best type whose
//   claim is still below its MINIMUM. Units that cannot help any deficient claim are left for
//   sweep 2. Reserving first matters: after sweep 2 the attractive units are spent and the
//   minimums can only be met by taking back land units that are wanted elsewhere.
//
// SWEEP 2
//   Every land unit still free, in ranking order, goes to its best type whose claim is below its
//   MAXIMUM. A unit for which every admissible claim is full stays unallocated and is counted.
//
// Complexity O(N*K + N*log N) in time and O(N) extra memory (the ranking vector, sizeof(ranked_unit)
// per land unit); in particular it never builds the O(#atomicRegions * K^2) facet administration
// that the hitchcock regime needs, which is where most of that regime's constant factor sits.
//
// This is a heuristic. Land units and claims each form a partition matroid, so maximising total
// suitability over their intersection is exactly what discrete_alloc solves; a one-pass greedy on
// such an intersection can be off, in the worst case by a factor two. Use discrete_alloc when the
// allocation has to be optimal.

struct ranked_unit
{
	Float64      m_Score;
	land_unit_id m_Unit;
};

inline bool CompareRankedUnits(const ranked_unit& a, const ranked_unit& b)
{
	if (a.m_Score != b.m_Score)
		return a.m_Score > b.m_Score; // highest score first
	return a.m_Unit < b.m_Unit;       // deterministic tie-break, independent of tiling
}

struct greedy_totals
{
	land_unit_id m_NrAllocated = 0;
	land_unit_id m_NrReserved = 0;      // allocated by sweep 1 (minimum claims)
	land_unit_id m_NrNoCapacity = 0;    // admissible, but every reachable claim was full
	Int64        m_TotalSuitability = 0;
};

// Give land unit i its best type whose claim still has room, and administer the allocation.
// minPhase: room means "below the MINIMUM claim" instead of "below the maximum claim". That cannot
// overshoot the maximum, because FeasibilityTest has already rejected every claim with min > max.
template <typename S, typename P, typename AR, typename AT>
bool GreedyAllocateUnit(htp_info_t<S, P, AR, AT>& htpInfo, land_unit_id i, bool minPhase, Int64& totalSuitability)
{
	auto ar = htpInfo.GetAtomicRegionID(i);
	UInt32 K = htpInfo.GetK();

	UInt32 winner = UNDEFINED_VALUE(UInt32);
	S      winningBid = S();
	for (UInt32 j = 0; j != K; ++j)
	{
		S s = htpInfo.m_ggTypes[j].m_Suitabilities[i];
		if (s < htpInfo.m_Threshold)
			continue;

		// shadow prices stay zero in these regimes, so a plain count test says it all; using
		// AtMax() here would drag the (unused) price terms of claim<S, P> into the criterion.
		const claim<S, P>& c = htpInfo.GetClaim(ar, AT(j));
		if (c.m_Count >= (minPhase ? c.m_ClaimRange.first : c.m_ClaimRange.second))
			continue;

		if (winner == UNDEFINED_VALUE(UInt32) || winningBid < s)
		{
			winningBid = s;
			winner = j;
		}
	}
	if (winner == UNDEFINED_VALUE(UInt32))
		return false;

	htpInfo.m_ResultArray[i] = AT(winner);
	++htpInfo.GetClaim(ar, AT(winner)).m_Count;
	totalSuitability += Int64(winningBid);
	return true;
}

template <typename S, typename P, typename AR, typename AT>
greedy_totals SolveGreedy(htp_info_t<S, P, AR, AT>& htpInfo, S threshold, alloc_regime regime, AbstrDataObject* resPrices)
{
	CDebugContextHandle debugContext("DiscrAlloc", "SolveGreedy", true);
	CharPtr regimeName = AllocRegimeName(regime);

	htpInfo.m_Threshold = IsDefined(threshold) ? threshold : MinValue<S>();
	const S thr = htpInfo.m_Threshold;

	const land_unit_id N = htpInfo.GetN();
	const UInt32       K = htpInfo.GetK();
	const bool         isNeedy = (regime == alloc_regime::needy);

	greedy_totals totals;

	// ---- rank the land units once
	std::vector<ranked_unit> ranking;
	ranking.reserve(N);
	for (land_unit_id i = 0; i != N; ++i)
	{
		htpInfo.m_ResultArray[i] = UNDEFINED_VALUE(AT); // also the "still free" marker for the sweeps

		bool any = false;
		S best = thr, next = thr;
		for (UInt32 j = 0; j != K; ++j)
		{
			S s = htpInfo.m_ggTypes[j].m_Suitabilities[i];
			if (s < thr)
				continue;
			if (!any || best < s)
			{
				if (any)
					next = best;
				best = s;
				any = true;
			}
			else if (next < s)
				next = s;
		}
		if (!any)
		{
			++htpInfo.m_NrBelowThreshold;
			if (htpInfo.m_NrBelowThreshold <= NR_BELOW_THRESHOLD_NOTIFICATIONS)
				reportF(SeverityTypeID::ST_MajorTrace, "{}: all suitabilities of land unit {} are below the threshold {}",
					regimeName, i, AsString(thr).c_str()
				);
			continue;
		}
		ranking.push_back(ranked_unit{ isNeedy ? Float64(best) - Float64(next) : Float64(best), i });
	}
	std::sort(ranking.begin(), ranking.end(), CompareRankedUnits);

	reportF(SeverityTypeID::ST_MajorTrace, "{}: ranked {} of {} land units by {}; {} below the threshold {}",
		regimeName, ranking.size(), N,
		isNeedy ? "regret (best minus next best suitability)" : "best suitability",
		htpInfo.m_NrBelowThreshold, AsString(thr).c_str()
	);

	// ---- sweep 1: reserve for the minimum claims
	SizeT totalDeficit = 0;
	for (const auto& c : htpInfo.m_Claims)
		totalDeficit += c.m_ClaimRange.first;

	if (totalDeficit)
	{
		reportF(SeverityTypeID::ST_MajorTrace, "{}: reserving for minimum claims totalling {} land units", regimeName, totalDeficit);
		for (const auto& ru : ranking)
		{
			if (!totalDeficit)
				break; // every minimum claim is met; the rest is sweep 2's business
			if (GreedyAllocateUnit(htpInfo, ru.m_Unit, true, totals.m_TotalSuitability))
			{
				++totals.m_NrReserved;
				--totalDeficit;
			}
		}
		reportF(SeverityTypeID::ST_MajorTrace, "{}: minimum claims sweep allocated {} land units, {} short",
			regimeName, totals.m_NrReserved, totalDeficit
		);
	}

	// ---- sweep 2: the remaining land units, against the maximum claims
	for (const auto& ru : ranking)
	{
		if (IsDefined(htpInfo.m_ResultArray[ru.m_Unit]))
			continue; // reserved by sweep 1
		if (!GreedyAllocateUnit(htpInfo, ru.m_Unit, false, totals.m_TotalSuitability))
			++totals.m_NrNoCapacity;
	}
	totals.m_NrAllocated = land_unit_id(ranking.size()) - totals.m_NrNoCapacity;

	reportF(SeverityTypeID::ST_MajorTrace, "{}: allocated {} land units ({} of them reserved for minimum claims); "
		"{} below the threshold and {} without any claim capacity left",
		regimeName, totals.m_NrAllocated, totals.m_NrReserved,
		htpInfo.m_NrBelowThreshold, totals.m_NrNoCapacity
	);

	StoreBidPricesCurrTile(htpInfo, resPrices, true);
	return totals;
}


// *****************************************************************************
//									StoreLanduseTypeInfo
// *****************************************************************************

template <typename S, typename P, typename AR, typename AT>
void StoreLanduseTypeInfo(htp_info_t<S, P, AR, AT>& htpInfo, bool isFeasible)
{
	for (UInt32 j = 0; j!= htpInfo.m_ggTypes.size(); ++j)
	{
		ggType_info_t<S>& ggTypeInfo = htpInfo.m_ggTypes[j];

		UInt32 K = ggTypeInfo.m_NrClaims;
		auto firstClaim = htpInfo.m_Claims.begin() + ggTypeInfo.m_FirstClaimID;

		// total_allocated/<name> is always part of the result
		{
			auto resTALock = lock_or_cancel(ggTypeInfo.m_diResTotalAllocated); // owning for this scope; throws if torn down
			DataWriteLock taLock(resTALock.get(), dms_rw_mode::write_only_all);

			DataArray<land_unit_id>* doResTA = mutable_array_checkedcast<land_unit_id>(taLock);
			auto daResTA = doResTA->GetDataWrite(no_tile, dms_rw_mode::write_only_all); auto resTAiter = daResTA.begin();
			dms_assert(daResTA.size() == K);

			if (isFeasible)
				for (auto claimIter = firstClaim, claimEnd = firstClaim + K; claimIter != claimEnd; ++claimIter)
					*resTAiter++ = claimIter->m_Count;
			else
				fast_fill(resTAiter, resTAiter+K, UNDEFINED_VALUE(UInt32));

			taLock.Commit();
		}

		// shadow_prices/<name> only exists for the adjusting (discrete_alloc) variants; the greedy_alloc and
		// needy_alloc families run with mustAdjust == false, so CreateResultingItems left m_diResShadowPrices
		// EMPTY. Issue #1171:
		// locking an empty weak throws task_canceled, which OperationContext::Run_with_catch swallows, leaving the
		// result neither computed nor failed -- observed as a spinning retry ("hang") up to 20.8.0 and as an
		// internal "m_Ptr->WasFailed()" check failure afterwards.
		if (!ggTypeInfo.m_HasResShadowPrices)
			continue;

		auto resSPLock = lock_or_cancel(ggTypeInfo.m_diResShadowPrices); // owning for this scope; throws if torn down
		DataWriteLock spLock(resSPLock.get(), dms_rw_mode::write_only_all);

		DataArray<S>* doResSP = mutable_array_checkedcast<S>(spLock);
		auto daResSP = doResSP->GetDataWrite(no_tile, dms_rw_mode::write_only_all); auto resSPiter = daResSP.begin();
		dms_assert(daResSP.size() == K);

		if (isFeasible)
			for (auto claimIter = firstClaim, claimEnd = firstClaim + K; claimIter != claimEnd; ++claimIter)
				*resSPiter++ = claimIter->m_ShadowPrice.first;
		else
			fast_fill(resSPiter, resSPiter+K, UNDEFINED_VALUE(S));

		spLock.Commit();
	}
}


template <typename S, typename P, typename AR, typename AT>
void StoreBidPricesCurrTile(htp_info_t<S, P, AR, AT>& htpInfo, AbstrDataObject* bidPrices, bool isFeasible)
{
	auto priceData = mutable_array_checkedcast<S>(bidPrices)->GetDataWrite(no_tile, dms_rw_mode::write_only_all);

	land_unit_id n = htpInfo.m_N;

	if (isFeasible)
		for (land_unit_id i = 0; i!=n; ++i)
		{
			AT j = htpInfo.m_ResultArray[i];
			if (IsDefined(j))
				priceData[i] = CheckedPriceAdd<S>(htpInfo.m_ggTypes[j].m_Suitabilities[i], htpInfo.GetClaim( htpInfo.GetAtomicRegionID(i), j).m_ShadowPrice.first);
			else
				priceData[i] = UNDEFINED_VALUE(S);
		}
	else
		fast_fill(priceData.begin(), priceData.end(), UNDEFINED_VALUE(S));
}

// *****************************************************************************
//									HitchcockTransportation operator
// *****************************************************************************

template <typename S, typename P, discr_alloc_version DAV, typename AR, typename AT>
class HitchcockTransportationOperator : public VariadicOperator
{
	typedef DataArray<SharedStr>  Arg1Type;          // ggTypes->name
	typedef AbstrUnit             Arg2Type;          // domain of cells
//	typedef TreeItem              Arg3Type;          // container with columns with suitability maps
	typedef DataArray<S>          PriceType;         // suitability map type
	typedef DataArray<partitioning_id> Arg4Type;     // ggType -> partitioning
	typedef DataArray<SharedStr>  Arg5Type;          // partitions->name
//	typedef Unit<AR>              Arg6Type;          // atomic regions to unique regions mapping container
//	typedef DataArray<AR>         Arg7Type;          // atomic regions map
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
	using htp_meta_type = htp_meta_t<S>;
	using htp_info_type = htp_info_t<S, P, AR, AT>;

	// Which algorithm this operator group runs; see alloc_regime. Only the hitchcock regime
	// computes shadow prices, so only it gets shadow_prices/<name> result members.
	const alloc_regime m_Regime;
	bool MustAdjust() const { return m_Regime == alloc_regime::hitchcock; }

	arg_index GetNrArguments() const
	{
		if constexpr (DAV == discr_alloc_version::no_partition)
			return 6;
		else if constexpr (DAV == discr_alloc_version::one_partition)
			return 8;
		else if constexpr (DAV == discr_alloc_version::multiple_partitions_without_feasibility_test)
			return 10;
		else
		{
			static_assert(DAV == discr_alloc_version::multiple_partitions_with_feasibility_test);
			return 11;
		}
	}

public:
	HitchcockTransportationOperator(AbstrOperGroup* gr, alloc_regime regime)
		:	VariadicOperator(gr, ResultType::GetStaticClass(), GetNrArguments())
		,	m_Regime(regime)
	{
		ClassCPtr* argClsIter = m_ArgClasses.get();
		*argClsIter++ = Arg1Type::GetStaticClass(); // ggTypes->name array
		*argClsIter++ = Arg2Type::GetStaticClass(); // allocUnit
		*argClsIter++ = TreeItem::GetStaticClass(); // suitability maps
		if constexpr (DAV >= discr_alloc_version::multiple_partitions_without_feasibility_test)
		{
			*argClsIter++ = Arg4Type::GetStaticClass(); // ggTypes->partitions
			*argClsIter++ = Arg5Type::GetStaticClass(); // partitions->name
		}
		if constexpr (DAV >= discr_alloc_version::one_partition)
		{
			*argClsIter++ = Unit<AR>::GetStaticClass(); // atomicRegions to unique regions mapping container
			*argClsIter++ = DataArray<AR>::GetStaticClass(); // atomicRegions map
		}
		*argClsIter++ = TreeItem::GetStaticClass(); // minClaim container
		*argClsIter++ = TreeItem::GetStaticClass(); // maxClaimContainer
		*argClsIter++ = Arg10Threshold::GetStaticClass();  // cutoff threshold
		if constexpr (DAV == discr_alloc_version::multiple_partitions_with_feasibility_test)
		{
			*argClsIter++ = TreeItem::GetStaticClass(); // feasibility certificate
		}
		assert(argClsIter == m_ArgClassesEnd);
	}

	// batch E + §12.8 (the array-spec join): discrete_alloc's result-member
	// obligations split in two. The INPUT obligations (which members the
	// suitabilities/claims containers must hold, and the name-directed
	// shadow_prices/<name> + total_allocated/<name> outputs) are computed from
	// the meta-read type-name array and the partitionings/suitabilities -- those
	// stay deferred prose. But the FIXED result members are STRUCTURAL, and the
	// flagship -- landuse = attribute<AT>(allocUnit), AT the typeNames' DOMAIN --
	// is derivable symbolically even when typeNames/allocUnit are formals: this
	// describes typeNames as an ATTRIBUTE so AT gets a var (used in BOTH the
	// typeNames domain role and landuse's values role -- the K2 bridge, so
	// landuse's values IDENTITY is AT). The result member set is deliberately
	// INCOMPLETE (no ResultMembersComplete): the name-directed members and the
	// conditional bid_price mean an unknown member DEFERS, never errors -- the
	// ruled broad placeholder (a/landuse types; a/anything_else defers).
	bool DescribeSignature(AbstrSignatureBuilder& sb) const override
	{
		arg_index n = GetNrArguments();
		auto a0 = dynamic_cast<const DataItemClass*>(GetArgClass(0));
		// ATv, not AT: the enclosing class template already has a type parameter AT, and a local
		// that shadows a template parameter is ill-formed ([temp.local]/6) -- GCC rejects it,
		// MSVC accepts it silently.
		sig_var A = sb.UnitVar("allocUnit"), ATv = no_sig_var;
		if (a0)
		{
			sig_var TN = sb.UnitVar("typeName"); sb.MemberValueClass(TN, a0->GetValuesType()); // string names
			ATv = sb.UnitVar("AllocTypes");
			sb.ArgName(0, "typeNames"); sb.ArgAttr(0, TN, ATv, ValueComposition::Single);        // attribute<string>(AT)
		}
		else
			sb.ArgDeferred(0, "typeNames");
		sb.ArgUnit(1, A);
		// K11b: the CONSUMED suitability members are looked up by TYPE NAME (arg 0's
		// values), and each is an attribute over the allocUnit -- so the shared member
		// domain is A and the naming array is argument 0. The container may carry
		// further members (helpers, per-type weights): they are never read, and a
		// claim over them would falsely reject working configs. The shared price
		// VALUES unit stays undeclared: the operator casts the suitability values, so
		// a cross-member values claim is not established.
		sb.ArgContainer(2, "suitabilities: per-type attribute<S>(allocUnit), keyed by the type names", A, no_sig_var, 0);
		arg_index i = 3;
		if (n >= 10) // multiple partitions
		{
			sb.ArgDeferred(i++, "ggTypes2partitionings: attribute<PartId>(typeNames.domain)");
			if (auto a4 = dynamic_cast<const DataItemClass*>(GetArgClass(i)))
				sb.ArgMetaValue(i, a4->GetValuesType(), "partitioningNames: names the partitionings");
			else
				sb.ArgDeferred(i, "partitioningNames");
			++i;
		}
		if (n >= 8) // one partition and up: atomic-region unit + map
		{
			sb.ArgUnit(i++, sb.UnitVar("atomicRegionUnit"));    // single-use var
			sb.ArgDeferred(i++, "atomicRegionMap: attribute<AR>(allocUnit)");
		}
		sb.ArgContainer(i++, "minClaims: per-type claim, keyed by the type names");
		sb.ArgContainer(i++, "maxClaims: per-type claim, keyed by the type names");
		sb.ArgDeferred(i++, "threshold: Int32 cutoff");
		if (n == 11)
			sb.ArgDeferred(i++, "feasibilityCertificate");
		sb.DeferredRelation("suitabilities[t].domain == allocUnit; the claim and suitability members are keyed by the typeNames values (K11/K12); shadow_prices/<name> + total_allocated/<name> are name-directed -- their types come from the partitionings/suitabilities, not the type-names, so they stay deferred");
		// §12.8: the FIXED structural result members over allocUnit (A), keyed by A;
		// landuse's values ride AT (the typeNames' domain) -- the ruling's flagship.
		// The set is INCOMPLETE by design -- see the class comment. (status/statusFlag
		// are void parameters, deliberately left deferred; typing them would need two
		// distinct default-class vars anyway -- a DefaultUnit shares the role token
		// "default", so two would collide in the unifier's (owner,inst,role) keying.)
		if (ATv != no_sig_var)
			sb.ResultContainerMember("landuse", ATv, A, ValueComposition::Single);        // attribute<AT>(allocUnit)
		sb.ResultContainerMember("bid_price",  no_sig_var, A, ValueComposition::Single); // attribute<S>(allocUnit), conditional (present iff a suitability map is found)

		// K11b result side: the NAME-DIRECTED member families. Per ggType (named by
		// the typeNames values, argument 0) CheckAndPrepare creates
		//   total_allocated/<name> = CreateDataItem(.., partitioningUnit, default<claim_type>)
		//   shadow_prices/<name>   = CreateDataItem(.., partitioningUnit, priceUnit)   [iff MustAdjust()]
		// Only the UNPARTITIONED variants are described: there partitioningUnit is
		// provably Unit<Void> (the else-branch of hasPartitionings), so both families
		// are parameters. With partitionings the domain is a per-type partitioning
		// unit -- not one variable -- so those variants keep deferring.
		// The value classes are exact per member instantiation: claim_type for the
		// totals, S (the suitability price class) for the shadow prices.
		if constexpr (DAV == discr_alloc_version::no_partition)
		{
			sig_var voidDom = sb.VoidDomain();
			sig_var TA = sb.UnitVar("totalAllocated");
			sb.MemberValueClass(TA, ResultTotalType::GetStaticClass()->GetValuesType());
			sb.ResultContainerMemberSet("total_allocated", 0, TA, voidDom, ValueComposition::Single);
			if (MustAdjust())
			{
				sig_var SP = sb.UnitVar("shadowPrice");
				sb.MemberValueClass(SP, ResultShadowPriceType::GetStaticClass()->GetValuesType());
				sb.ResultContainerMemberSet("shadow_prices", 0, SP, voidDom, ValueComposition::Single);
			}
		}
		return true;
	}

	void CreateResultCaller(TreeItemDualRef& resultHolder, const ArgRefs& args, LispPtr) const override
	{
		assert(args.size() == GetNrArguments());

		const AbstrDataItem* ggTypeNamesA = AsDataItem(args[0]);
		dms_assert(ggTypeNamesA);

		const Unit<AT>*  ggTypeSet = checked_domain<AT>(GetItem(args[0]), "a1");

		const AbstrUnit* allocUnit = AsUnit(GetItem(args[1]));

		UInt32 nrTypes = ggTypeSet->GetCount();
		MG_CHECK(nrTypes <= MAX_VALUE(AT));

		const AbstrDataItem* ggTypes2partitioningsA = nullptr;
		const AbstrDataItem* regionNamesA = nullptr;
		auto argIter = args.begin() + 3;
		if constexpr (DAV >= discr_alloc_version::multiple_partitions_without_feasibility_test)
		{
			ggTypes2partitioningsA = AsDataItem(*argIter++);
			assert(ggTypes2partitioningsA);

			regionNamesA = AsDataItem(*argIter++);
			assert(regionNamesA);
		}

		const Unit<AR>* atomicRegionUnit = nullptr;
		const AbstrDataItem* atomicRegionMapA = nullptr;
		if constexpr (DAV >= discr_alloc_version::one_partition)
		{
			atomicRegionUnit = debug_cast<const Unit<AR>*>(GetItem(*argIter++));
			assert(atomicRegionUnit);
			//		debug_cast<FuncDC&>(resultHolder).AddDependency(atomicRegionUnit->GetAsLispExpr()); is al implicit dependency as being calculating argument

			atomicRegionMapA = AsDataItem(*argIter++);
			MG_CHECK(atomicRegionMapA);
		}
		auto minClaimSet = GetItem(*argIter++); // arg 3, 5, or 7
		auto maxClaimSet = GetItem(*argIter++); // arg 4, 6, or 8

		if (!resultHolder)
			resultHolder = TreeItem::CreateCacheRoot();
		dbg_assert(resultHolder);
		TreeItem* res = resultHolder.GetNew();
		assert(res);

		AbstrDataItem* resLanduse = CreateDataItem(res, GetTokenID_mt("landuse"), allocUnit, ggTypeSet).get(); // owned by res
		resLanduse->SetTSF(TSF_Categorical);

		AbstrDataItem* resStatus =
			CreateDataItem(
				res
				, GetTokenID_mt("status")
				, Unit<Void>  ::GetStaticClass()->CreateDefault()
				, Unit<SharedStr>::GetStaticClass()->CreateDefault()
			).get(); // owned by res
		resStatus->SetKeepDataState(true);

		AbstrDataItem* resStatusFlag =
			CreateDataItem(
				res
				, GetTokenID_mt("statusFlag")
				, Unit<Void>::GetStaticClass()->CreateDefault()
				, Unit<Bool>::GetStaticClass()->CreateDefault()
			).get(); // owned by res
		resStatusFlag->SetKeepDataState(true);

		TreeItem* resShadowPriceContainer = res->CreateItem(GetTokenID_mt("shadow_prices")).get();
		TreeItem* resTotalAllocatedContainer = res->CreateItem(GetTokenID_mt("total_allocated")).get();

		// htp_meta is made ONCE here (persistent, only rebuilt by MakeResult when the result is invalidated), so it
		// must NOT be cleared at StopInterest (that is what #968/#1020 disabled). Instead its cached-meta members
		// are weak (below) so it holds no interest/ownership -> no "uncaused" interest to deadlock the teardown drain.
		resultHolder->m_ReadAssets.emplace<htp_meta_type>();
		htp_meta_type& htpMeta = *rtc::any::any_cast<htp_meta_type>(&resultHolder->m_ReadAssets);

		// make AtomicRegionsSet and UniqeRegions -> (AtomicRegionsSet -> Region)
		CreateResultingItems(
			ggTypeNamesA,
			allocUnit,
			GetItem(args[2]),       // suitability maps container
			minClaimSet, maxClaimSet,
			ggTypes2partitioningsA, regionNamesA, atomicRegionUnit, atomicRegionMapA, // RegionGrids
			resShadowPriceContainer,
			resTotalAllocatedContainer,
			htpMeta,
			MustAdjust(), debug_refcast<FuncDC&>(resultHolder)
		,	!std::is_same_v<AR, Void>
		);

		AbstrDataItem* resPrices = nullptr;
		if (auto priceUnitLock = htpMeta.m_PriceUnit.lock())
			resPrices = CreateDataItem(res, GetTokenID_mt("bid_price"), allocUnit, priceUnitLock.get()).get(); // owned by res
	}

	// Runs one step of the allocation and turns a shadow price overflow into a config-level error
	// that names the remedy. The checked arithmetic near the top of this file explains why these
	// steps throw at all; here is where the caller learns what to do about it.
	template <typename Func>
	auto WithPerturbationAdvice(Func&& func) const -> decltype(func())
	{
		try {
			return func();
		}
		catch (const shadow_price_overflow& x)
		{
			CharPtr groupName = GetGroup()->GetNameStr();

			SharedStr advice;
			if constexpr (sizeof(P) < sizeof(Int64))
				advice = mySSPrintF("\nConsider using {}_pi64: the same operator with the"
					" Simulation-of-Simplicity perturbation term carried in Int64 instead of {}."
					, groupName
					, AsString(ValueWrap<P>::GetStaticClass()->GetID())
				);
			else
				advice = SharedStr("\nThis is already the _pi64 variant, which carries the widest"
					" supported perturbation term.");

			// A price overflow is a different problem with a different remedy, so say so even
			// though the _pi64 advice above stands for any overflow in an Int32-perturbation run.
			if (x.m_Component == price_component::price)
				advice += "\nNote that it was the PRICE component that overflowed, which a wider"
					" perturbation does not address: narrow the range of the suitability values, so"
					" that the shadow price adjustments keep room in their value type.";

			throwDmsErrF("{}: numeric overflow in the shadow price arithmetic.\n{}.{}"
				, groupName
				, x.m_Why
				, advice
			);
		}
	}

	bool CalcResult(TreeItemDualRef& resultHolder, const ArgRefs& args, std::vector<ItemReadLock> readLocks, Explain::Context* context) const override
	{
		assert(args.size() == GetNrArguments());

		htp_meta_type& htpMeta = *rtc::any::any_cast<htp_meta_type>(&resultHolder->m_ReadAssets);
		auto htpInfo = htp_info_type(htpMeta);

//	Recreate result MetaInfo
		TreeItem* res = resultHolder.GetNew();
		assert(res);

		AbstrDataItem* resLanduse = AsDataItem(res->GetSubTreeItemByID(GetTokenID_mt("landuse")));

		AbstrDataItem* resPrices = nullptr;
		if (!htpInfo.m_PriceUnit.expired())
			resPrices = AsDataItem(res->GetSubTreeItemByID(GetTokenID_mt("bid_price")));

		AbstrDataItem* resStatus     = AsDataItem(res->GetSubTreeItemByID(GetTokenID_mt("status")));
		AbstrDataItem* resStatusFlag = AsDataItem(res->GetSubTreeItemByID(GetTokenID_mt("statusFlag")));

		SharedStr strStatus;
		bool isFeasible;

		if (htpInfo.m_PriceUnit.expired())
		{
			res->Fail("No suitability maps", FailType::Data);
			isFeasible = false;
		}
		else
		{
			reportD(SeverityTypeID::ST_MajorTrace, "DiscrAlloc.Prepare started");

			auto allocUnit = AsUnit(GetItem(args[1]));
			PrepareClaims(htpInfo);
			auto argIter = args.begin() + 3;
			if constexpr (DAV >= discr_alloc_version::one_partition)
			{
				if constexpr (DAV > discr_alloc_version::one_partition)
					argIter += 2;
				auto atomicRegionUnit = debug_cast<const Unit<AR>*>(GetItem(*argIter++));
				const AbstrDataItem* atomicRegionMapA = AsDataItem(*argIter++);
				PreparePartitionings<S, P, AR, AT>(htpInfo, allocUnit, atomicRegionMapA, atomicRegionUnit);
			}
			else
				PreparePartitionings<S, P, AR, AT>(htpInfo, allocUnit, nullptr, nullptr);

			// N is known from here on, and K was known before it: the single point at which the
			// perturbation range can be settled for the whole run, so that no inner loop has to
			// re-ask. Wrapped like the solve steps, so an unrepresentable range still reports
			// itself as the config-level error that names the _pi64 remedy -- now before any work
			// is done rather than partway through the allocation.
			WithPerturbationAdvice([&] { CheckPerturbationRange<P>(htpInfo.GetN(), htpInfo.GetK()); });

			isFeasible = FeasibilityTest(htpInfo, strStatus);
			if (!isFeasible)
				strStatus = "DiscrAlloc.FeasibilityTest failed: "+ strStatus;
			else
			{
				DataReadLockSuitabilities(htpInfo);
				if (MustAdjust())
					PrepareFacets(htpInfo); // reallocation queues; greedy/needy never reallocate
				PrepareReport(htpInfo);
			}

			// all data read ?
			resultHolder.StopSupplInterest();

			htpInfo.m_ResultDataLock      = DataWriteLock(resLanduse, dms_rw_mode::write_only_all);
			htpInfo.m_ResultPriceDataLock = DataWriteLock(resPrices,  dms_rw_mode::write_only_all);

			if (isFeasible)
			{
				reportD(SeverityTypeID::ST_MajorTrace, "DiscrAlloc.Solve started with suitability container ", GetItem(args[2])->GetFullName().c_str());
				S threshold = GetTheCurrValue<S>(GetItem(argIter[2]));

				PrepareTileLock(htpInfo);

				greedy_totals greedyTotals;
				if (MustAdjust())
					WithPerturbationAdvice([&] { Solve(htpInfo, threshold, htpInfo.m_ResultPriceDataLock.get()); });
				else
					greedyTotals = WithPerturbationAdvice([&] { return SolveGreedy(htpInfo, threshold, m_Regime, htpInfo.m_ResultPriceDataLock.get()); });

				if (htpInfo.m_NrBelowThreshold > 0)
				{
					reportF(SeverityTypeID::ST_MajorTrace, "{} units{} with suitability for all categories below the threshold of {} and therefore unallocated"
						, htpInfo.m_NrBelowThreshold
						, htpInfo.m_NrBelowThreshold > NR_BELOW_THRESHOLD_NOTIFICATIONS ? ", of which only the first 5 were reported," : ""
						, AsString(threshold)
					);
				}
				isFeasible = CheckAllClaims(htpInfo, &strStatus);
				if (isFeasible && !MustAdjust())
				{
					// No shadow prices, so no distance-from-optimum analysis: greedy/needy give up
					// optimality by design and DistFromOpt asserts that every deviation from the best
					// bid is caused by the threshold, which is exactly what these regimes break.
					dms_assert(strStatus.empty());
					strStatus = mySSPrintF("{} completed", AllocRegimeName(m_Regime));
					strStatus += mySSPrintF(" with a total magnified suitability of {} over {} allocated land units",
						greedyTotals.m_TotalSuitability, greedyTotals.m_NrAllocated
					);
					if (greedyTotals.m_NrAllocated)
						strStatus += mySSPrintF(" = {:f} per land unit",
							Float64(greedyTotals.m_TotalSuitability) / Float64(greedyTotals.m_NrAllocated)
						);
					if (greedyTotals.m_NrReserved)
						strStatus += mySSPrintF(", of which {} were reserved for minimum claims", greedyTotals.m_NrReserved);
					if (htpInfo.m_NrBelowThreshold)
						strStatus += mySSPrintF("; {} land units left unallocated below the threshold", htpInfo.m_NrBelowThreshold);
					if (greedyTotals.m_NrNoCapacity)
						strStatus += mySSPrintF("; {} land units left unallocated for lack of claim capacity", greedyTotals.m_NrNoCapacity);
					strStatus += "; not necessarily optimal, use discrete_alloc for that";
				}
				else if (isFeasible)
				{
					dms_assert(strStatus.empty());
					DistFromOpt distData = WithPerturbationAdvice([&] { return DistFromOpt(htpInfo); });

					strStatus = "DiscrAlloc completed";

					if (distData.tn > 1)
						strStatus += mySSPrintF(" for {} tiles", distData.tn);

					strStatus += mySSPrintF(" with a total magnified suitability of {} over {} land units", 
						AsString(distData.totalSuit.first).c_str(), 
						distData.nrLandUnits
					);
					if (distData.nrLandUnits)
						strStatus += mySSPrintF(" = {:f} per land unit", 
							Float64(distData.totalSuit.first) / Float64(distData.nrLandUnits)
						);

					if (distData.nrSubOptimal)
					{
						strStatus += mySSPrintF(" with at most {} from optimum due to {}(={:f}%) better options", 
							AsString(distData.totalDistFromOpt.first).c_str(), 
							distData.nrSubOptimal, 100.0 * Float64(distData.nrSubOptimal) / Float64(distData.nrLandUnits)
						);
						if (distData.nrLandUnits)
							strStatus += mySSPrintF(" = {:f} per land unit", 
								Float64(distData.totalDistFromOpt.first) / Float64(distData.nrLandUnits)
							);

						if (distData.nrDueToBelowThreshold)
						{
							if (distData.nrSubOptimal > distData.nrDueToBelowThreshold)
								strStatus += mySSPrintF(" of which {}", distData.nrDueToBelowThreshold);
							strStatus += " due to the threshold blocking effective shadow price adjustments";
						}

						if (distData.nrSubOptimal > distData.nrDueToBelowThreshold)
						{
							if ( distData.nrDueToBelowThreshold)
								strStatus +=  mySSPrintF(" and the remaining {} options", distData.nrSubOptimal - distData.nrDueToBelowThreshold);
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

		dms_assert(!strStatus.empty() || res->WasFailed(FailType::Data));
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
//	INSTANTIATION of the various previously defined HitchcockTransportationOperators
// *****************************************************************************

namespace 
{
	oper_arg_policy np_oap[6] = {
			oper_arg_policy::calc_always,         // ggTypeNameArray
			oper_arg_policy::calc_as_result,      // domainUnit
			oper_arg_policy::subst_with_subitems, // suitabilities
			oper_arg_policy::subst_with_subitems, // minClaims
			oper_arg_policy::subst_with_subitems, // maxClaims
			oper_arg_policy::calc_as_result,      // cutoff value
	};

	oper_arg_policy sp_oap[8] = {
		    oper_arg_policy::calc_always,         // ggTypeNameArray
		    oper_arg_policy::calc_as_result,      // domainUnit
			oper_arg_policy::subst_with_subitems, // suitabilities
			oper_arg_policy::subst_with_subitems,	// atomicRegions met regioRefs
	        oper_arg_policy::calc_as_result,        // atomicRegions map
			oper_arg_policy::subst_with_subitems, // minClaims
			oper_arg_policy::subst_with_subitems, // maxClaims
		    oper_arg_policy::calc_as_result,      // cutoff value
	};

	oper_arg_policy da_oap[11] = {
			oper_arg_policy::calc_always,         // ggTypeNameArray
			oper_arg_policy::calc_as_result,      // domainUnit
			oper_arg_policy::subst_with_subitems, // suitabilities
			oper_arg_policy::calc_always,				// ggType -> partitioning relation
			oper_arg_policy::calc_always,				// partitioningNameArray
			oper_arg_policy::subst_with_subitems,	// atomicRegions met regioRefs
			oper_arg_policy::calc_as_result,        // atomicRegions map
			oper_arg_policy::subst_with_subitems, // minClaims
			oper_arg_policy::subst_with_subitems, // maxClaims
			oper_arg_policy::calc_as_result,      // cutoff value
			oper_arg_policy::calc_never,          // feasibilityCertificate
	};

	SpecialOperGroup hitchcockGroup   ("discrete_alloc",          11, da_oap, oper_policy::better_not_in_meta_scripting);
	SpecialOperGroup hitchcockGroup_16("discrete_alloc_16",       11, da_oap, oper_policy::better_not_in_meta_scripting);
	SpecialOperGroup greedyGroup   ("greedy_alloc", 11, da_oap, oper_policy::better_not_in_meta_scripting);
	SpecialOperGroup greedyGroup_16("greedy_alloc_16", 11, da_oap, oper_policy::better_not_in_meta_scripting);
	SpecialOperGroup needyGroup    ("needy_alloc", 11, da_oap, oper_policy::better_not_in_meta_scripting);
	SpecialOperGroup needyGroup_16 ("needy_alloc_16", 11, da_oap, oper_policy::better_not_in_meta_scripting);

	SpecialOperGroup hitchcockGroup_sp   ("discrete_alloc_sp", 8, sp_oap, oper_policy::better_not_in_meta_scripting);
	SpecialOperGroup hitchcockGroup_sp_16("discrete_alloc_sp_16", 8, sp_oap, oper_policy::better_not_in_meta_scripting);
	SpecialOperGroup greedyGroup_sp      ("greedy_alloc_sp", 8, sp_oap, oper_policy::better_not_in_meta_scripting);
	SpecialOperGroup greedyGroup_sp_16   ("greedy_alloc_sp_16", 8, sp_oap, oper_policy::better_not_in_meta_scripting);
	SpecialOperGroup needyGroup_sp       ("needy_alloc_sp", 8, sp_oap, oper_policy::better_not_in_meta_scripting);
	SpecialOperGroup needyGroup_sp_16    ("needy_alloc_sp_16", 8, sp_oap, oper_policy::better_not_in_meta_scripting);

	SpecialOperGroup hitchcockGroup_np("discrete_alloc_np", 6, np_oap, oper_policy::better_not_in_meta_scripting);
	SpecialOperGroup hitchcockGroup_np_16("discrete_alloc_np_16", 6, np_oap, oper_policy::better_not_in_meta_scripting);
	SpecialOperGroup greedyGroup_np("greedy_alloc_np", 6, np_oap, oper_policy::better_not_in_meta_scripting);
	SpecialOperGroup greedyGroup_np_16("greedy_alloc_np_16", 6, np_oap, oper_policy::better_not_in_meta_scripting);
	SpecialOperGroup needyGroup_np ("needy_alloc_np", 6, np_oap, oper_policy::better_not_in_meta_scripting);
	SpecialOperGroup needyGroup_np_16("needy_alloc_np_16", 6, np_oap, oper_policy::better_not_in_meta_scripting);

	// The _pi64 twins of the six discrete_alloc names above. Same arguments, same results, same
	// algorithm: the ONLY difference is that the Simulation-of-Simplicity perturbation term is
	// carried in Int64 instead of Int32 (the P template parameter, see the note near the top of
	// this file), which moves the land unit count at which two units within one facet start
	// sharing a perturbation -- and requirement (R1) stops holding -- out of reach of any grid.
	//
	// greedy_alloc and needy_alloc have NO _pi64 twin: those regimes use neither shadow prices nor
	// the perturbation, so the suffix would name an operator that differs from the plain one in
	// nothing but the eight extra bytes each claim's unused shadow price would carry.
	//
	// Spelled out rather than generated, so that every operator name a config can write is
	// greppable in this file.
	SpecialOperGroup hitchcockGroup_pi64   ("discrete_alloc_pi64",    11, da_oap, oper_policy::better_not_in_meta_scripting);
	SpecialOperGroup hitchcockGroup_16_pi64("discrete_alloc_16_pi64", 11, da_oap, oper_policy::better_not_in_meta_scripting);

	SpecialOperGroup hitchcockGroup_sp_pi64   ("discrete_alloc_sp_pi64",    8, sp_oap, oper_policy::better_not_in_meta_scripting);
	SpecialOperGroup hitchcockGroup_sp_16_pi64("discrete_alloc_sp_16_pi64", 8, sp_oap, oper_policy::better_not_in_meta_scripting);

	SpecialOperGroup hitchcockGroup_np_pi64   ("discrete_alloc_np_pi64",    6, np_oap, oper_policy::better_not_in_meta_scripting);
	SpecialOperGroup hitchcockGroup_np_16_pi64("discrete_alloc_np_16_pi64", 6, np_oap, oper_policy::better_not_in_meta_scripting);

	constexpr auto ar_hitchcock = alloc_regime::hitchcock;
	constexpr auto ar_greedy    = alloc_regime::greedy;
	constexpr auto ar_needy     = alloc_regime::needy;

	// The operator groups that one argument layout registers, split by regime because the two
	// families are instantiated over different perturbation types: the hitchcock regime over both
	// Int32 and Int64, the heuristic ones over Int32 only. Within each, the _16 names take UInt16
	// land use types and the others UInt8. Passing the set in lets the operator set templates
	// below be written once and instantiated per perturbation type.
	struct hitchcock_group_set
	{
		AbstrOperGroup* m_Default;
		AbstrOperGroup* m_Default16;
	};

	struct heuristic_group_set
	{
		AbstrOperGroup* m_Greedy;
		AbstrOperGroup* m_Needy;
		AbstrOperGroup* m_Greedy16;
		AbstrOperGroup* m_Needy16;
	};

	const hitchcock_group_set npHitchcock32 = { &hitchcockGroup_np, &hitchcockGroup_np_16 };
	const hitchcock_group_set spHitchcock32 = { &hitchcockGroup_sp, &hitchcockGroup_sp_16 };
	const hitchcock_group_set daHitchcock32 = { &hitchcockGroup,    &hitchcockGroup_16    };

	const hitchcock_group_set npHitchcock64 = { &hitchcockGroup_np_pi64, &hitchcockGroup_np_16_pi64 };
	const hitchcock_group_set spHitchcock64 = { &hitchcockGroup_sp_pi64, &hitchcockGroup_sp_16_pi64 };
	const hitchcock_group_set daHitchcock64 = { &hitchcockGroup_pi64,    &hitchcockGroup_16_pi64    };

	const heuristic_group_set npHeuristic = { &greedyGroup_np, &needyGroup_np, &greedyGroup_np_16, &needyGroup_np_16 };
	const heuristic_group_set spHeuristic = { &greedyGroup_sp, &needyGroup_sp, &greedyGroup_sp_16, &needyGroup_sp_16 };
	const heuristic_group_set daHeuristic = { &greedyGroup,    &needyGroup,    &greedyGroup_16,    &needyGroup_16    };

	// The hitchcock regime of one argument layout (DAV + AR), for both land use type widths.
	template <typename S, typename P, discr_alloc_version DAV, typename AR>
	struct hitchcock_operators
	{
		hitchcock_operators(const hitchcock_group_set& g)
			:	htpDefault  (g.m_Default,   ar_hitchcock)
			,	htpDefault16(g.m_Default16, ar_hitchcock)
		{}

		HitchcockTransportationOperator<S, P, DAV, AR, UInt8 > htpDefault;
		HitchcockTransportationOperator<S, P, DAV, AR, UInt16> htpDefault16;
	};

	// The two heuristic regimes of the same layout. P is Int32 in every instantiation of this one:
	// see the note at the _pi64 group declarations above for why they have no wide-perturbation
	// twin. It is still a parameter so that both families read the same.
	template <typename S, typename P, discr_alloc_version DAV, typename AR>
	struct heuristic_operators
	{
		heuristic_operators(const heuristic_group_set& g)
			:	htpGreedy  (g.m_Greedy,   ar_greedy)
			,	htpNeedy   (g.m_Needy,    ar_needy)
			,	htpGreedy16(g.m_Greedy16, ar_greedy)
			,	htpNeedy16 (g.m_Needy16,  ar_needy)
		{}

		HitchcockTransportationOperator<S, P, DAV, AR, UInt8 > htpGreedy, htpNeedy;
		HitchcockTransportationOperator<S, P, DAV, AR, UInt16> htpGreedy16, htpNeedy16;
	};

	// The three partitioned layouts at once, for one perturbation type and atomic region type.
	template <typename S, typename P, typename AR>
	struct hitchcock_operators_per_layout
	{
		hitchcock_operators_per_layout(const hitchcock_group_set& sp, const hitchcock_group_set& da)
			:	htpOnePartition(sp)
			,	htpManyPartitionsNoFeasibilityTest(da)
			,	htpManyPartitionsWithFeasibilityTest(da)
		{}

		hitchcock_operators<S, P, discr_alloc_version::one_partition, AR> htpOnePartition;
		hitchcock_operators<S, P, discr_alloc_version::multiple_partitions_without_feasibility_test, AR> htpManyPartitionsNoFeasibilityTest;
		hitchcock_operators<S, P, discr_alloc_version::multiple_partitions_with_feasibility_test   , AR> htpManyPartitionsWithFeasibilityTest;
	};

	template <typename S, typename P, typename AR>
	struct heuristic_operators_per_layout
	{
		heuristic_operators_per_layout(const heuristic_group_set& sp, const heuristic_group_set& da)
			:	htpOnePartition(sp)
			,	htpManyPartitionsNoFeasibilityTest(da)
			,	htpManyPartitionsWithFeasibilityTest(da)
		{}

		heuristic_operators<S, P, discr_alloc_version::one_partition, AR> htpOnePartition;
		heuristic_operators<S, P, discr_alloc_version::multiple_partitions_without_feasibility_test, AR> htpManyPartitionsNoFeasibilityTest;
		heuristic_operators<S, P, discr_alloc_version::multiple_partitions_with_feasibility_test   , AR> htpManyPartitionsWithFeasibilityTest;
	};

	// S (the suitability and price type) is Int32 throughout. P (the perturbation type) is Int32
	// for the plain names and Int64 for the six discrete_alloc _pi64 twins.
	constexpr auto dav_np = discr_alloc_version::no_partition;

	hitchcock_operators<Int32, Int32, dav_np, Void> htpNoPartition(npHitchcock32);
	heuristic_operators<Int32, Int32, dav_np, Void> heuNoPartition(npHeuristic);

	hitchcock_operators_per_layout<Int32, Int32, UInt32> htp3232(spHitchcock32, daHitchcock32);
	hitchcock_operators_per_layout<Int32, Int32, UInt16> htp3216(spHitchcock32, daHitchcock32);
	hitchcock_operators_per_layout<Int32, Int32, UInt8 > htp3208(spHitchcock32, daHitchcock32);

	heuristic_operators_per_layout<Int32, Int32, UInt32> heu3232(spHeuristic, daHeuristic);
	heuristic_operators_per_layout<Int32, Int32, UInt16> heu3216(spHeuristic, daHeuristic);
	heuristic_operators_per_layout<Int32, Int32, UInt8 > heu3208(spHeuristic, daHeuristic);

	hitchcock_operators<Int32, Int64, dav_np, Void> htpNoPartition_pi64(npHitchcock64);

	hitchcock_operators_per_layout<Int32, Int64, UInt32> htp3232_pi64(spHitchcock64, daHitchcock64);
	hitchcock_operators_per_layout<Int32, Int64, UInt16> htp3216_pi64(spHitchcock64, daHitchcock64);
	hitchcock_operators_per_layout<Int32, Int64, UInt8 > htp3208_pi64(spHitchcock64, daHitchcock64);

	// *************************************************************************
	// OBSOLETE claim_* stubs -- REMOVE IN v21, see issue #1177
	//
	// These names have no implementation: they only reserve the name so that a
	// config still using them gets the message below instead of "operator name
	// not found". Their meaning used to come from claim-correction rewrite rules
	// in RewriteExpr.lsp, which were removed there. They moved here from
	// IpfAlloc.cpp when that file was deleted with the gutted ipf_alloc operator
	// (#1177); this is their home because claim correction is allocation input.
	//
	// The v21 removal is guaranteed by TWO tripwires:
	//  (a) the static_assert below -- bumping DMS_VERSION_MAJOR to 21 fails the
	//      BUILD, with a message that says what to do;
	//  (b) the throwDmsErrD in ClaimOperatorsFlag() as a runtime backstop.
	// (a) exists because (b) alone is close to undiagnosable: ClaimOperatorsFlag
	// runs from a STATIC INITIALIZER, so its throw escapes through DllMain as
	// STATUS_DLL_INIT_FAILED (0xC0000142) -- every exe then fails to start with
	// no message at all. The same pair now guards the other "remove at v21" sites:
	// Subset.cpp (subset), Dijkstra.cpp (dijkstra_*), ConnectedParts.cpp (PartNr).
	// *************************************************************************

	static_assert(DMS_VERSION_MAJOR <= 20,
		"v21: REMOVE the obsolete claim_* operator stubs below (GeoDMS issue #1177) "
		"rather than bumping the major version with them still registered.");

	CharPtr rewriteObsoleteWarning = "claim correction related rewrite rules have been removed from RewriteExpr.lsp";

	oper_policy ClaimOperatorsFlag()
	{
		if (DMS_GetMajorVersionNumber() < 20)
			return oper_policy::depreciated;
		if (DMS_GetMajorVersionNumber() <= 20)
			return oper_policy::obsolete;

		throwDmsErrD("This code should be removed in v21"); // see the static_assert above
	}

	const oper_policy PHASE_OUT_FLAG = ClaimOperatorsFlag();

	Obsolete< CommonOperGroup > claimStubs[] =
	{
		Obsolete< CommonOperGroup >(rewriteObsoleteWarning, "claim_div", oper_policy::better_not_in_meta_scripting | PHASE_OUT_FLAG),
		Obsolete< CommonOperGroup >(rewriteObsoleteWarning, "claim_divF64", oper_policy::better_not_in_meta_scripting | PHASE_OUT_FLAG),
		Obsolete< CommonOperGroup >(rewriteObsoleteWarning, "claim_divF32D", oper_policy::better_not_in_meta_scripting | PHASE_OUT_FLAG),
		Obsolete< CommonOperGroup >(rewriteObsoleteWarning, "claim_corr", oper_policy::better_not_in_meta_scripting | PHASE_OUT_FLAG),
		Obsolete< CommonOperGroup >(rewriteObsoleteWarning, "claim_corrF32D", oper_policy::better_not_in_meta_scripting | PHASE_OUT_FLAG),
		Obsolete< CommonOperGroup >(rewriteObsoleteWarning, "claim_corrF32DL", oper_policy::better_not_in_meta_scripting | PHASE_OUT_FLAG),

		Obsolete< CommonOperGroup >(rewriteObsoleteWarning, "claim_minmax_corrF32", oper_policy::better_not_in_meta_scripting | PHASE_OUT_FLAG),
		Obsolete< CommonOperGroup >(rewriteObsoleteWarning, "claim_minmax_corrF64", oper_policy::better_not_in_meta_scripting | PHASE_OUT_FLAG),

		Obsolete< CommonOperGroup >(rewriteObsoleteWarning, "claim_minmax_corrF32D", oper_policy::better_not_in_meta_scripting | PHASE_OUT_FLAG),
		Obsolete< CommonOperGroup >(rewriteObsoleteWarning, "claim_minmax_corrF32L", oper_policy::better_not_in_meta_scripting | PHASE_OUT_FLAG)
	};
}

/******************************************************************************/
