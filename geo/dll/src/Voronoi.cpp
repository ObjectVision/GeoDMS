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
#include "geom/PointOrder.h"

#include "IndexAssigner.h"

// *****************************************************************************
//	Delaunay triangulation, shared by triangualize and voronoi
// *****************************************************************************
//
// triangualize returns the triangulation itself as an edge network: a unit<uint32> with one row per
// Delaunay edge and two subitems F1 and F2 that relate each edge to the two points of the argument's
// domain that it connects, in the same shape as polygon_connectivity and box_connectivity produce
// (see geo/dll/src/BoostPolygon.cpp).
//
// voronoi returns the dual: one Thiessen cell polygon per input point, clipped to the range of the
// unit given as second argument, since the cells of convex-hull points are unbounded.
//
// Edge count. With m the number of distinct, defined points and h the number of them on the convex
// hull, a planar triangulation has exactly 3m - 3 - h edges (and 2m - 2 - h triangles). Since
// 3 <= h <= m that is at most 3m - 6 (only three hull points) and at least 2m - 3 (all points in
// convex position); fully collinear input degenerates to a 1-dimensional triangulation with m - 1
// edges. Linear either way, and 3m is a safe reservation.
//
// Predicates decide the triangulation, constructions do not enter the triangulation result at all --
// only vertex indices do -- so the inexact-construction kernel is used here rather than the
// exact-construction CGAL_Traits::Kernel that the polygon operators need. voronoi does construct
// coordinates, but its cells are built by half-plane clipping rather than from circumcentres, which
// needs no more than the neighbour relation (and, unlike circumcentres, also covers the degenerate
// 1-dimensional case, where there are no faces to take a circumcentre of).

namespace {

	using DelaunayKernel = CGAL::Exact_predicates_inexact_constructions_kernel;
	using DelaunayPoint = DelaunayKernel::Point_2;
	using DelaunayVb = CGAL::Triangulation_vertex_base_with_info_2<UInt32, DelaunayKernel>;
	using DelaunayFb = CGAL::Triangulation_face_base_2<DelaunayKernel>;
	using DelaunayTds = CGAL::Triangulation_data_structure_2<DelaunayVb, DelaunayFb>;
	using DelaunayTriangulation = CGAL::Delaunay_triangulation_2<DelaunayKernel, DelaunayTds>;

	// Order-of-magnitude modelling constant, not a measurement of a particular run: CGAL's
	// Triangulation_data_structure_2 holds one vertex (point, incident face handle, UInt32 info) and
	// about two faces (3 vertex handles + 3 neighbour handles each) per point, plus the transient site
	// vector that is released before the triangulation is walked.
	static constexpr SizeT DT_BYTES_PER_POINT = 200;

	// Feeds every defined point, tagged with its index in the argument's domain, into dt. The range
	// insert spatial-sorts the sites first, which is O(m log m); inserting one by one would degrade
	// badly on unsorted input. Coinciding points collapse onto a single vertex that keeps one of their
	// indices, so a duplicate is simply absent from the triangulation.
	template <typename PointSeq>
	void dms_delaunay_insert(DelaunayTriangulation& dt, const PointSeq& pointData)
	{
		std::vector<std::pair<DelaunayPoint, UInt32>> sites;
		sites.reserve(pointData.size());
		for (SizeT i = 0, n = pointData.size(); i != n; ++i)
		{
			const auto& p = pointData[i];
			if (!IsDefined(p)) // null points take no part in the triangulation
				continue;
			sites.emplace_back(DelaunayPoint(p.X(), p.Y()), ThrowingConvert<UInt32>(i));
		}
		if (!sites.empty())
			dt.insert(sites.begin(), sites.end());
	}

	// Collects the finite edges as index pairs into the argument's domain, lowest index first.
	void dms_delaunay_edges(const DelaunayTriangulation& dt, std::vector<std::pair<UInt32, UInt32>>& edges)
	{
		edges.clear();
		edges.reserve(dt.number_of_vertices() * 3); // see the 3m - 6 bound above

		for (auto ei = dt.finite_edges_begin(), ee = dt.finite_edges_end(); ei != ee; ++ei)
		{
			auto face = ei->first;
			auto i = ei->second;
			auto v1 = face->vertex(dt.cw(i))->info();
			auto v2 = face->vertex(dt.ccw(i))->info();
			edges.emplace_back(std::min(v1, v2), std::max(v1, v2));
		}
	}

} // anonymous namespace

// *****************************************************************************
//	triangualize
// *****************************************************************************

static StaticLateTokenID tF1("F1"), tF2("F2");

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

		DelaunayTriangulation dt;
		dms_delaunay_insert(dt, pointData);
		pointData = {};

		std::vector<std::pair<UInt32, UInt32>> edges;
		dms_delaunay_edges(dt, edges);

		// The traversal order follows CGAL's internal face order; sorting makes the network stable and
		// diff-friendly, which regression references depend on.
		std::sort(edges.begin(), edges.end());

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
//	voronoi
// *****************************************************************************
//
// The Voronoi cell of a site p is the intersection of the half-planes bounded by the perpendicular
// bisectors of p with each of its Delaunay neighbours -- and only Delaunay neighbours contribute a
// bisector that actually touches the cell, which is what makes the dual worth building. The cell is
// therefore computed by clipping the extent rectangle successively by each neighbour's bisector,
// rather than by joining circumcentres. That is uniform: hull sites need no unbounded-ray special
// case (the extent bounds them), and a collinear point set, whose triangulation has no faces at all
// and hence no circumcentres, still yields the correct slabs.
//
// Cost is O(sum of deg^2) over the sites, and a Delaunay vertex has average degree below 6, so this
// is effectively linear; a single site of degree m-1 is the (rare) worst case.

class AbstrVoronoiOperator : public BinaryOperator
{
protected:
	// The result is a POLYGON attribute, so its class is the sequence container's, not the point's --
	// the same split BufferPointOperator makes between its ResultType and Arg1Type.
	AbstrVoronoiOperator(AbstrOperGroup* aog, const DataItemClass* polyAttrClass, const DataItemClass* pointAttrClass, const UnitClass* extentUnitClass)
		:	BinaryOperator(aog, polyAttrClass, pointAttrClass, extentUnitClass)
	{}

	// The result IS a data item here, so unlike triangualize the base estimator books it; only the
	// triangulation and the neighbour lists that Calculate holds outside the DMS allocator are added.
	auto EstimatePerformance(TreeItemDualRef& resultHolder, const ArgRefs& args) const -> PerformanceEstimationData override
	{
		auto result = BinaryOperator::EstimatePerformance(resultHolder, args);

		if (args.empty())
			return result;
		auto argItem = GetItem(args[0]);
		if (!argItem || !IsDataItem(argItem))
			return result;
		auto argAdi = AsDataItem(argItem);

		AbstrUnit::CountEstimate argCount;
		try { argCount = argAdi->GetAbstrDomainUnit()->EstimateCount(); }
		catch (...) { return result; }

		// the CSR neighbour lists: 2 * (3m - 6) UInt32 entries plus two m-sized offset arrays
		static constexpr SizeT ADJACENCY_BYTES_PER_POINT = 8 * sizeof(UInt32);

		auto nrPoints = argCount.expected;
		result.inputSizePerChore = result.inputSize;
		result.nrChores = 1; // untiled: the triangulation is global, so the whole domain is done at once
		result.workingMemorySize = nrPoints * (DT_BYTES_PER_POINT + ADJACENCY_BYTES_PER_POINT);
		result.workingMemorySizePerChore = result.workingMemorySize;
		result.confidence = argCount.confidence;
		return result;
	}

	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		assert(args.size() == 2);

		const AbstrDataItem* arg1A = AsDataItem(args[0]);
		const AbstrUnit* extentUnit = AsUnit(args[1]);
		assert(arg1A);
		assert(extentUnit);

		const AbstrUnit* pointDomain = arg1A->GetAbstrDomainUnit();
		const AbstrUnit* valuesUnit = arg1A->GetAbstrValuesUnit();

		valuesUnit->UnifyValues(extentUnit, "v1", "e2", UM_Throw);

		if (!resultHolder)
			resultHolder = CreateCacheDataItem(pointDomain, valuesUnit, ValueComposition::Polygon);

		if (mustCalc)
		{
			DataReadLock arg1Lock(arg1A);

			auto resItem = AsDataItem(resultHolder.GetNew());
			DataWriteLock resLock(resItem, dms_rw_mode::write_only_mustzero);

			Calculate(resLock.get(), arg1A, extentUnit);

			resLock.Commit();
		}
		return true;
	}
	virtual void Calculate(AbstrDataObject* resObj, const AbstrDataItem* pointItem, const AbstrUnit* extentUnit) const = 0;
};

template <typename P>
class VoronoiOperator : public AbstrVoronoiOperator
{
	using PointType = P;
	using CoordType = scalar_of_t<PointType>;
	using PolygonType = sequence_traits<PointType>::container_type;
	using Arg1Type = DataArray<PointType>;
	using ResultType = DataArray<PolygonType>;

	// Sutherland-Hodgman clip of a convex ring by the half-plane of the points at least as close to p
	// as to q, i.e. the side of the perpendicular bisector of pq that holds p. Evaluated in terms of
	// x - p, so the magnitudes stay at cell scale instead of at coordinate-origin scale.
	static void ClipByBisector(std::vector<DPoint>& ring, std::vector<DPoint>& helper, DPoint p, DPoint q)
	{
		auto dx = q.X() - p.X(), dy = q.Y() - p.Y();
		auto halfSquaredDist = 0.5 * (dx * dx + dy * dy);
		auto side = [=](DPoint x) { return (x.X() - p.X()) * dx + (x.Y() - p.Y()) * dy - halfSquaredDist; };

		helper.clear();
		auto n = ring.size();
		for (SizeT j = 0; j != n; ++j)
		{
			auto a = ring[j];
			auto b = ring[(j + 1 == n) ? 0 : j + 1];
			auto fa = side(a), fb = side(b);

			if (fa <= 0.0)
				helper.push_back(a);
			if ((fa < 0.0 && fb > 0.0) || (fa > 0.0 && fb < 0.0))
			{
				auto t = fa / (fa - fb);
				helper.push_back(shp2dms_order<Float64>(a.X() + t * (b.X() - a.X()), a.Y() + t * (b.Y() - a.Y())));
			}
		}
		ring.swap(helper);
	}

	// The DMS ring convention is the reverse of the counterclockwise ring, closed by repeating the
	// first point -- exactly what cgal_assign_ring (geo/dll/src/CGAL_Traits.h) and bp_assign_ring
	// (rtc/dll/src/geo/BoostPolygon.h) write. Kept local so this TU need not pull in either header.
	template <typename SeqRef>
	static void StoreCounterClockwiseRing(SeqRef&& ref, const std::vector<DPoint>& ring)
	{
		ref.clear();

		auto n = ring.size();
		if (n < 3) // a site outside the extent, or one whose cell the clipping emptied
			return;

		ref.reserve(n + 1 MG_DEBUG_ALLOCATOR_SRC("voronoi"));

		auto storePoint = [&ref](DPoint p)
		{
			ref.push_back(shp2dms_order<CoordType>(p.X(), p.Y()) MG_DEBUG_ALLOCATOR_SRC("voronoi"));
		};

		storePoint(ring[0]); // becomes the closing point of the reversed ring
		for (auto j = n; j != 0; )
			storePoint(ring[--j]);
	}

public:
	VoronoiOperator(AbstrOperGroup* gr)
		:	AbstrVoronoiOperator(gr, ResultType::GetStaticClass(), Arg1Type::GetStaticClass(), Unit<PointType>::GetStaticClass())
	{}

	void Calculate(AbstrDataObject* resObj, const AbstrDataItem* pointItem, const AbstrUnit* extentUnitA) const override
	{
		auto extentRange = debug_cast<const Unit<PointType>*>(extentUnitA)->GetRange();
		if (!IsDefined(extentRange.first) || !IsDefined(extentRange.second)
			|| !(extentRange.first.X() < extentRange.second.X())
			|| !(extentRange.first.Y() < extentRange.second.Y()))
			throwDmsErrF("voronoi", "the second argument must be a unit with a proper range; the cells of the"
				" points on the convex hull are unbounded and that range is what bounds them. Got {}"
				, extentUnitA->GetRangeAsStr(FormattingFlags::None));

		auto x0 = Float64(extentRange.first.X()), y0 = Float64(extentRange.first.Y());
		auto x1 = Float64(extentRange.second.X()), y1 = Float64(extentRange.second.Y());

		auto pointData = const_array_cast<PointType>(pointItem)->GetDataRead();
		auto resData = mutable_array_cast<PolygonType>(resObj)->GetDataWrite(no_tile, dms_rw_mode::write_only_mustzero);
		SizeT nrPoints = pointData.size();
		assert(resData.size() == nrPoints);
		if (!nrPoints)
			return;

		DelaunayTriangulation dt;
		dms_delaunay_insert(dt, pointData);

		// Which indices actually became a site: null points and all but one of a group of coinciding
		// points did not, and they get an empty cell rather than a wrong one.
		std::vector<bool> isSite(nrPoints, false);
		for (auto vi = dt.finite_vertices_begin(), ve = dt.finite_vertices_end(); vi != ve; ++vi)
			isSite[vi->info()] = true;

		std::vector<std::pair<UInt32, UInt32>> edges;
		dms_delaunay_edges(dt, edges);
		dt.clear(); // the neighbour relation below is all that is still needed

		// CSR neighbour lists: one counting pass, a prefix sum, then one scatter pass.
		std::vector<UInt32> nbrStart(nrPoints + 1, 0);
		for (const auto& e : edges)
		{
			++nbrStart[e.first + 1];
			++nbrStart[e.second + 1];
		}
		for (SizeT i = 0; i != nrPoints; ++i)
			nbrStart[i + 1] += nbrStart[i];

		std::vector<UInt32> nbrList(edges.size() * 2);
		auto nbrFill = nbrStart;
		for (const auto& e : edges)
		{
			nbrList[nbrFill[e.first]++] = e.second;
			nbrList[nbrFill[e.second]++] = e.first;
		}
		edges = {};
		nbrFill = {};

		std::vector<DPoint> ring, helper;
		for (SizeT i = 0; i != nrPoints; ++i)
		{
			if (!isSite[i])
			{
				resData[i].clear();
				continue;
			}

			// start from the extent rectangle, counterclockwise
			ring.clear();
			ring.push_back(shp2dms_order<Float64>(x0, y0));
			ring.push_back(shp2dms_order<Float64>(x1, y0));
			ring.push_back(shp2dms_order<Float64>(x1, y1));
			ring.push_back(shp2dms_order<Float64>(x0, y1));

			const auto& pi = pointData[i];
			auto p = shp2dms_order<Float64>(Float64(pi.X()), Float64(pi.Y()));

			for (auto n = nbrStart[i], ne = nbrStart[i + 1]; n != ne && ring.size() >= 3; ++n)
			{
				const auto& qi = pointData[nbrList[n]];
				ClipByBisector(ring, helper, p, shp2dms_order<Float64>(Float64(qi.X()), Float64(qi.Y())));
			}

			StoreCounterClockwiseRing(resData[i], ring);
		}
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
	CommonOperGroup cogVoronoi("voronoi", oper_policy::better_not_in_meta_scripting);

	tl_oper::inst_tuple_templ<typelists::seq_points, TriangualizeOperator> trOPers(&cogTR);
	tl_oper::inst_tuple_templ<typelists::seq_points, VoronoiOperator> voronoiOpers(&cogVoronoi);
}

/******************************************************************************/
