// Copyright (C) 1998-2024 Object Vision b.v.
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "GeoPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif

#include <algorithm>
#include <utility>
#include <vector>

#include <CGAL/Exact_predicates_inexact_constructions_kernel.h>
#include <CGAL/Delaunay_triangulation_2.h>
#include <CGAL/Triangulation_data_structure_2.h>
#include <CGAL/Triangulation_face_base_2.h>
#include <CGAL/Triangulation_vertex_base_with_info_2.h>

#include "mci/CompositeCast.h"

#include "DataArray.h"
#include "DataItemClass.h"
#include "TreeItemClass.h"
#include "AbstrUnit.h"
#include "Unit.h"
#include "UnitClass.h"

#include "IndexAssigner.h"

// *****************************************************************************
//	triangualize: the Delaunay triangulation of a point set, as an edge network
// *****************************************************************************
//
// The result is a network: a unit<uint32> with one row per Delaunay edge and two subitems F1 and F2 that
// relate each edge to the two points of the argument's domain that it connects, in the same shape as
// polygon_connectivity and box_connectivity produce (see geo/dll/src/BoostPolygon.cpp).
//
// Edge count. With m the number of distinct, defined points and h the number of them on the convex hull,
// a planar triangulation has exactly 3m - 3 - h edges (and 2m - 2 - h triangles). Since 3 <= h <= m that
// is at most 3m - 6 (only three hull points) and at least 2m - 3 (all points in convex position); fully
// collinear input degenerates to a 1-dimensional triangulation with m - 1 edges. Linear either way, and
// 3m is a safe reservation.
//
// Predicates decide the triangulation, constructions do not enter the result at all -- only vertex
// indices do -- so the inexact-construction kernel is used here rather than the exact-construction
// CGAL_Traits::Kernel that the polygon operators need.

namespace {

	using DelaunayKernel = CGAL::Exact_predicates_inexact_constructions_kernel;
	using DelaunayPoint = DelaunayKernel::Point_2;
	using DelaunayVb = CGAL::Triangulation_vertex_base_with_info_2<UInt32, DelaunayKernel>;
	using DelaunayFb = CGAL::Triangulation_face_base_2<DelaunayKernel>;
	using DelaunayTds = CGAL::Triangulation_data_structure_2<DelaunayVb, DelaunayFb>;
	using DelaunayTriangulation = CGAL::Delaunay_triangulation_2<DelaunayKernel, DelaunayTds>;

} // anonymous namespace

static TokenID tF1 = GetTokenID_st("F1"), tF2 = GetTokenID_st("F2");

class AbstrTriangualizeOperator : public UnaryOperator
{
protected:
	using ResultingDomainType = UInt32;

	AbstrTriangualizeOperator(AbstrOperGroup* aog, const DataItemClass* pointAttrClass)
		:	UnaryOperator(aog, Unit<ResultingDomainType>::GetStaticClass(), pointAttrClass)
	{}

	// Without this override the base estimator calls this a FREE operation: the result is a unit, so
	// Operator::EstimatePerformance returns early with regime 'meta', zero bytes and high confidence in
	// that zero. Nothing the triangulation holds passes through the DMS allocator -- CGAL allocates its
	// vertices and faces with std::allocator -- so neither the free-store bookkeeping nor the ledger's
	// retained side observes it either. Same reasoning as
	// AbstrPolygonConnectivityOperator::EstimatePerformance (BoostPolygon.cpp).
	auto EstimatePerformance(TreeItemDualRef& resultHolder, const ArgRefs& args) const -> PerformanceEstimationData override
	{
		auto result = UnaryOperator::EstimatePerformance(resultHolder, args);

		if (args.empty())
			return result;
		auto argItem = GetItem(args[0]);
		if (!argItem || !IsDataItem(argItem))
			return result;
		auto argAdi = AsDataItem(argItem);

		AbstrUnit::CountEstimate argCount;
		try { argCount = argAdi->GetAbstrDomainUnit()->EstimateCount(); }
		catch (...) { return result; } // an unresolvable domain just keeps the base's figures

		// Order-of-magnitude modelling constant, not a measurement of a particular run: CGAL's
		// Triangulation_data_structure_2 holds one vertex (point, incident face handle, UInt32 info) and
		// about two faces (3 vertex handles + 3 neighbour handles each) per point, plus the transient
		// site vector that is released before the edge walk.
		static constexpr SizeT DT_BYTES_PER_POINT = 200;

		auto nrPoints = argCount.expected;
		result.inputSize = EstimateDataBytes(argAdi, nrPoints);
		result.inputSizePerChore = result.inputSize; // untiled: the whole domain is read at once
		result.nrChores = 1;
		result.extraTasks = 1;
		result.workingMemorySize = nrPoints * DT_BYTES_PER_POINT;
		result.workingMemorySizePerChore = result.workingMemorySize;

		// The result unit itself carries no data, but its F1/F2 subitems do, and they are written eagerly
		// under a DataWriteLock before Calculate returns. Take the 3m - 6 upper bound from the header.
		auto nrEdges = (nrPoints > 2) ? nrPoints * 3 - 6 : nrPoints;
		result.resultingNrElements = nrEdges;
		result.resultingMemory = nrEdges * 2 * sizeof(ResultingDomainType);
		result.residentMemory = result.resultingMemory;
		result.choreMemory = result.resultingMemory;
		result.expectedCalcTime = calc_time_t(nrPoints) * GetGroup()->GetCalcFactor();
		result.confidence = argCount.confidence;
		return result;
	}

	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		assert(args.size() == 1);

		const AbstrDataItem* arg1A = AsDataItem(args[0]);
		assert(arg1A);

		const AbstrUnit* pointDomain = arg1A->GetAbstrDomainUnit();

		auto res = Unit<ResultingDomainType>::GetStaticClass()->CreateResultUnit(resultHolder.GetNew());
		res->SetTSF(TSF_Categorical);

		AbstrDataItem* resF1 = CreateDataItem(res.get(), tF1, res.get(), pointDomain).get(); // owned by res
		AbstrDataItem* resF2 = CreateDataItem(res.get(), tF2, res.get(), pointDomain).get(); // owned by res

		if (mustCalc)
		{
			DataReadLock arg1Lock(arg1A);

			Calculate(res.get(), resF1, resF2, arg1A);
		}
		resultHolder = res; // DualRef adopts the owning shared_tree_ptr
		return true;
	}
	virtual void Calculate(AbstrUnit* res, AbstrDataItem* resF1, AbstrDataItem* resF2, const AbstrDataItem* arg1A) const = 0;
};

template <typename P>
class TriangualizeOperator : public AbstrTriangualizeOperator
{
	using PointType = P;
	using Arg1Type = DataArray<PointType>;

public:
	TriangualizeOperator(AbstrOperGroup* gr)
		:	AbstrTriangualizeOperator(gr, Arg1Type::GetStaticClass())
	{}

	void Calculate(AbstrUnit* res, AbstrDataItem* resF1, AbstrDataItem* resF2, const AbstrDataItem* arg1A) const override
	{
		auto pointData = const_array_cast<PointType>(arg1A)->GetDataRead();
		SizeT nrPoints = pointData.size();

		std::vector<std::pair<DelaunayPoint, UInt32>> sites;
		sites.reserve(nrPoints);
		for (SizeT i = 0; i != nrPoints; ++i)
		{
			const auto& p = pointData[i];
			if (!IsDefined(p)) // null points take no part in the triangulation and in no edge
				continue;
			sites.emplace_back(DelaunayPoint(p.X(), p.Y()), ThrowingConvert<UInt32>(i));
		}

		std::vector<std::pair<UInt32, UInt32>> edges;
		if (sites.size() > 1)
		{
			DelaunayTriangulation dt;

			// Range insertion spatial-sorts the sites first, which is O(m log m); inserting one by one
			// would degrade badly on unsorted input. Coinciding points collapse onto a single vertex that
			// keeps one of their indices, so a duplicate simply does not appear in F1/F2.
			dt.insert(sites.begin(), sites.end());
			sites = {};

			edges.reserve(dt.number_of_vertices() * 3); // see the 3m - 6 bound in the header

			for (auto ei = dt.finite_edges_begin(), ee = dt.finite_edges_end(); ei != ee; ++ei)
			{
				auto face = ei->first;
				auto i = ei->second;
				auto v1 = face->vertex(dt.cw(i))->info();
				auto v2 = face->vertex(dt.ccw(i))->info();
				edges.emplace_back(std::min(v1, v2), std::max(v1, v2));
			}

			// The traversal order follows CGAL's internal face order; sorting makes the network stable and
			// diff-friendly, which regression references depend on.
			std::sort(edges.begin(), edges.end());
		}

		SizeT nrEdges = edges.size();
		res->SetCount(ThrowingConvert<ResultingDomainType>(nrEdges));

		DataWriteLock resF1Lock(resF1);
		DataWriteLock resF2Lock(resF2);

		IndexAssigner32 indexAssigner1(resF1, resF1Lock.get(), no_tile, 0, nrEdges);
		IndexAssigner32 indexAssigner2(resF2, resF2Lock.get(), no_tile, 0, nrEdges);

		for (SizeT e = 0; e != nrEdges; ++e)
		{
			indexAssigner1.m_Indices[e] = edges[e].first;
			indexAssigner2.m_Indices[e] = edges[e].second;
		}
		indexAssigner1.Store();
		indexAssigner2.Store();

		resF2Lock.Commit();
		resF1Lock.Commit();
	}
};

// *****************************************************************************
//											INSTANTIATION
// *****************************************************************************

#include "utl/TypeListOper.h"
#include "RtcTypeLists.h"

namespace
{
	CommonOperGroup cogTR("triangualize");

	tl_oper::inst_tuple_templ<typelists::seq_points, TriangualizeOperator> trOPers(&cogTR);
}

/******************************************************************************/

