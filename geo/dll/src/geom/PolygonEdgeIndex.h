// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

/*
 *  PolygonEdgeIndex<PointType>: for the polygons with many vertices that a set of points can lie in, the edges of each
 *  polygon listed per horizontal slab, so that a point-in-polygon test reads the edges that can meet the point's row
 *  instead of every edge (#1320). Built once beside the spatial index of point_in_polygon, point_in_ranked_polygon and
 *  point_in_all_polygons, then only read by the threads that query it.
 */

#ifndef __GEO_POLYGONEDGEINDEX_H
#define __GEO_POLYGONEDGEINDEX_H

#include "geom/IsInside.h"
#include "geom/Point.h"
#include "geom/Range.h"
#include "mem/MyContainers.h"
#include "ParallelTiles.h"

#include <algorithm>
#include <cmath>
#include <limits>

// When a polygon gets an edge index, and how it is cut into slabs.
namespace PolygonEdgeIndexParams
{
	// A polygon gets an edge index when it has at least this many vertices. Measured with 10 million points over 400 squares
	// on OVSRV10 (#1320): with 9 vertices the index made no difference, with 17 it saved 0.004 to 0.011 s of about 0.11 s, with
	// 33 0.014 to 0.020 s and with 65 0.03 to 0.05 s of about 0.14 s. Below 32 vertices the gain is small, and every polygon
	// that gets an index costs its build and its memory.
	constexpr SizeT MinVertices = 32;

	// The polygons get edge indices only when there are at least this many points per polygon that would get one. Building the
	// index of a polygon costs a few walks over its vertices (#1320 measured 6 to 7, on one thread, for one polygon of 16
	// million vertices), and a point saves at most one walk for each candidate polygon that the spatial index yields for it.
	// This is less than those 6 to 7 because a point often has more than one candidate and the polygons are indexed in
	// parallel; with fewer points per polygon the index costs more than it saves.
	constexpr SizeT MinPointsPerPolygon = 4;

	// A polygon of k vertices is cut into k / EdgesPerSlab slabs of equal height, ...
	constexpr SizeT EdgesPerSlab = 8;

	// ... or fewer, when the slab lists would otherwise hold more than this many entries per edge: an edge is listed in every
	// slab its y-range meets, and a boundary that runs up and down many times over the polygon's height has edges that meet
	// many slabs each. That bounds the index at this many edge numbers per vertex.
	constexpr SizeT MaxEntriesPerEdge = 4;
}

template <typename PointType>
struct PolygonEdgeIndex
{
	using ScalarType = scalar_of_t<PointType>;
	using RangeType  = Range<PointType>;

	// The edges of one polygon of k vertices, listed per slab. Edge j runs from vertex j - 1 to vertex j, and edge 0 from vertex
	// k - 1 to vertex 0, as IsInside walks them, so the closing edge and the edges that connect the rings of a polygon with
	// holes or islands are edges like any other.
	//
	// Why the answer is that of IsInside: IsCrossingRightOfOrigin counts an edge only when one of its ends lies strictly above
	// the point's row and the other does not, so only an edge whose closed y-range [lo, hi] holds the point's y can count.
	// SlabOf is a non-decreasing function of y, so for lo <= y <= hi it gives SlabOf(lo) <= SlabOf(y) <= SlabOf(hi), and an
	// edge listed in the slabs SlabOf(lo) to SlabOf(hi) is in the slab of every point it can count for, whatever the rounding
	// of the slab boundaries. A horizontal edge, lo == hi, has its ends on the same side of every row and never counts, so it
	// is not listed: the sides of rectangles and of polygons from grid cells are half of their edges. No edge is listed twice
	// in one slab, and each edge listed is tested with the same function and the same differences as IsInside tests it, so
	// the parity of the count is the same. Two things the argument needs:
	//  - the sign of v.y - p.y is that of the comparison of v.y with p.y. That holds for floating point and, as a difference
	//    of two values in [YMin, YMax], for an integer type whose range holds YMax - YMin; for an unsigned type the
	//    difference wraps, and then an edge counts only when one end lies on the point's row, which its closed y-range holds
	//    too. A signed polygon whose height does not fit is not indexed, and neither is a signed polygon with a vertex whose
	//    y is null, as its difference with p.y can overflow;
	//  - an edge with an end whose y is null (NaN for a floating point type) has no y-range: such edges are tested for every
	//    point, in the list after the last slab.
	// A point whose y lies outside [YMin, YMax] gets the walk over all vertices; the spatial index yields a polygon only for a
	// point within its bounding box, so for these operators that does not occur.
	struct PolygonSlabs
	{
		ScalarType m_YMin = ScalarType(), m_YMax = ScalarType(); // the lowest and highest defined y of the vertices
		Float64    m_SlabsPerUnit = 0;  // m_NrSlabs / (m_YMax - m_YMin), 0 for one slab
		UInt32     m_NrSlabs = 0;       // 0: the polygon has no index
		UInt32     m_NrVertices = 0;
		my_vec_t<UInt32> m_SlabStart;   // m_NrSlabs + 2 offsets: slab s lists m_Edges[m_SlabStart[s], m_SlabStart[s + 1]), and the
		my_vec_t<UInt32> m_Edges;       // range from m_SlabStart[m_NrSlabs] holds the edges with an end whose y is null

		bool IsBuilt() const { return m_NrSlabs; }

		// Non-decreasing in y for y >= m_YMin: a rounded difference with a constant and a rounded product with a positive
		// constant both are, and so is the truncation of a non-negative value.
		UInt32 SlabOf(ScalarType y) const
		{
			Float64 s = (Float64(y) - Float64(m_YMin)) * m_SlabsPerUnit;
			return s < m_NrSlabs ? UInt32(s) : m_NrSlabs - 1;
		}

		template <typename Poly>
		void Build(const Poly& poly)
		{
			using namespace PolygonEdgeIndexParams;
			SizeT k = poly.size();
			if (k < MinVertices || k > std::numeric_limits<UInt32>::max())
				return;
			auto v = poly.begin();

			// the y-range of the vertices with a defined y, and the sum of the heights of the edges between them
			bool hasDefinedY = false;
			bool hasNullY = false;
			Float64 sumOfEdgeHeights = 0;
			ScalarType yPrev = v[k - 1].Row();
			for (SizeT j = 0; j != k; ++j)
			{
				ScalarType y = v[j].Row();
				if (!IsDefined(y))
					hasNullY = true;
				else
				{
					if (!hasDefinedY)
					{
						m_YMin = m_YMax = y;
						hasDefinedY = true;
					}
					else
					{
						MakeMin(m_YMin, y);
						MakeMax(m_YMax, y);
					}
					if (IsDefined(yPrev))
						sumOfEdgeHeights += std::abs(Float64(y) - Float64(yPrev));
				}
				yPrev = y;
			}
			if (!hasDefinedY)
				return;
			if constexpr (std::is_integral_v<ScalarType> && std::is_signed_v<ScalarType>)
			{
				static_assert(sizeof(ScalarType) < sizeof(Int64));
				if (hasNullY || Int64(m_YMax) - Int64(m_YMin) > Int64(std::numeric_limits<ScalarType>::max()))
					return;
			}

			// The number of slabs. An edge of height dy meets at most dy / slabHeight + 2 slabs, so the lists hold at most
			// nrSlabs * sumOfEdgeHeights / height + 2 * k entries, which MaxEntriesPerEdge bounds.
			Float64 height = Float64(m_YMax) - Float64(m_YMin);
			UInt32 nrSlabs = 1;
			if (height > 0 && std::isfinite(height))
			{
				Float64 s = Float64(k) / EdgesPerSlab;
				if (sumOfEdgeHeights > 0)
					MakeMin(s, Float64(MaxEntriesPerEdge - 2) * Float64(k) * height / sumOfEdgeHeights);
				if (s >= 2)
					nrSlabs = UInt32(s);
			}
			m_NrSlabs = nrSlabs;
			m_SlabsPerUnit = (nrSlabs > 1) ? nrSlabs / height : 0;
			m_NrVertices = UInt32(k);

			// Per edge j, the first and last slab it is listed in, or none for a horizontal edge, or the list after the last slab for
			// an edge with an end whose y is null. SlabOf(min(y, yPrev)) is min(SlabOf(y), SlabOf(yPrev)), as SlabOf is
			// non-decreasing, so the slab of each vertex is computed once per pass.
			auto forEachEdge = [this, v, k, nrSlabs](auto&& addToSlab)
			{
				ScalarType yPrev = v[k - 1].Row();
				UInt32 sPrev = IsDefined(yPrev) ? SlabOf(yPrev) : 0;
				for (SizeT j = 0; j != k; ++j)
				{
					ScalarType y = v[j].Row();
					UInt32 s = IsDefined(y) ? SlabOf(y) : 0;
					if (!IsDefined(y) || !IsDefined(yPrev))
						addToSlab(nrSlabs, j);
					else if (y != yPrev)
						for (UInt32 i = std::min(s, sPrev), e = std::max(s, sPrev); i <= e; ++i)
							addToSlab(i, j);
					yPrev = y;
					sPrev = s;
				}
			};

			// count the entries per slab, then make the counts offsets
			my_vec_t<SizeT> count(nrSlabs + 1, 0); // the last one counts the edges with an end whose y is null
			forEachEdge([&count](UInt32 slab, SizeT) { ++count[slab]; });
			SizeT total = 0;
			for (SizeT n : count)
				total += n;
			if (total > std::numeric_limits<UInt32>::max())
			{
				m_NrSlabs = 0;
				return;
			}
			m_SlabStart.resize(nrSlabs + 2);
			m_SlabStart[0] = 0;
			for (UInt32 s = 0; s <= nrSlabs; ++s)
				m_SlabStart[s + 1] = UInt32(m_SlabStart[s] + count[s]);

			// fill the lists, each in the order of the edges
			m_Edges.resize(total);
			my_vec_t<UInt32> cursor(m_SlabStart.begin(), m_SlabStart.end() - 1);
			forEachEdge([this, &cursor](UInt32 slab, SizeT j) { m_Edges[cursor[slab]++] = UInt32(j); });
		}

		template <typename Poly>
		bool IsInside(const Poly& poly, PointType point) const
		{
			assert(IsBuilt());
			assert(poly.size() == m_NrVertices);
			ScalarType py = point.Row();
			if (!(m_YMin <= py && py <= m_YMax)) // also for a null y
				return ::IsInside(poly.begin(), poly.end(), point);

			auto v = poly.begin();
			const UInt32* edges = m_Edges.data();
			bool c = false;
			auto testEdges = [&c, v, edges, last = m_NrVertices - 1, point](UInt32 i, UInt32 e)
			{
				for (; i != e; ++i)
				{
					UInt32 j = edges[i];
					if (IsCrossingRightOfOrigin(v[j ? j - 1 : last] - point, v[j] - point))
						c = !c;
				}
			};
			UInt32 s = SlabOf(py);
			testEdges(m_SlabStart[s], m_SlabStart[s + 1]);
			testEdges(m_SlabStart[m_NrSlabs], m_SlabStart[m_NrSlabs + 1]);
			return c;
		}
	};

	PolygonEdgeIndex() = default;

	// The edge indices of the polygons [first, last) that have at least MinVertices vertices and whose bounding box touches
	// pointBox, the bounding box of the nrPoints points to be tested, as spIndex, the spatial index over the same polygons,
	// yields them; none when there are fewer than MinPointsPerPolygon points per such polygon. A polygon that a point cannot
	// lie in is never tested, so indexing it would be wasted: think of the addresses of one village tested against all the
	// neighbourhoods of a country.
	template <typename SpatialIndexType, typename PolyIter>
	PolygonEdgeIndex(const SpatialIndexType& spIndex, PolyIter first, PolyIter last, const RangeType& pointBox, SizeT nrPoints)
	{
		using namespace PolygonEdgeIndexParams;
		if (first == last || !nrPoints || pointBox.inverted())
			return;

		std::vector<SizeT> candidates;
		for (auto iter = spIndex.begin_touching(pointBox); iter; ++iter)
		{
			auto polyPtr = (*iter)->get_ptr();
			if ((*polyPtr).size() >= MinVertices)
				candidates.emplace_back(polyPtr - first);
		}
		if (candidates.empty() || nrPoints < MinPointsPerPolygon * candidates.size())
			return;
		std::sort(candidates.begin(), candidates.end()); // the index yields them in the order of its nodes

		my_vec_t<PolygonSlabs> polygons(candidates.size());
		parallel_for<SizeT>(candidates.size(), [&polygons, &candidates, first](SizeT c)
			{
				polygons[c].Build(*(first + candidates[c]));
			}
		);

		m_SlabsOf.assign(last - first, 0);
		for (SizeT c = 0; c != candidates.size(); ++c)
			if (polygons[c].IsBuilt())
			{
				m_Polygons.emplace_back(std::move(polygons[c]));
				m_SlabsOf[candidates[c]] = UInt32(m_Polygons.size());
			}
		if (m_Polygons.empty())
			m_SlabsOf.clear();
	}

	SizeT NrIndexedPolygons() const { return m_Polygons.size(); }

	// IsInside(poly, point) for polygon number polyIndex of those the index was built from, with its edge index if it has one
	template <typename Poly>
	bool IsInside(SizeT polyIndex, const Poly& poly, PointType point) const
	{
		if (!m_SlabsOf.empty())
			if (UInt32 i = m_SlabsOf[polyIndex])
				return m_Polygons[i - 1].IsInside(poly, point);
		return ::IsInside(poly.begin(), poly.end(), point);
	}

private:
	my_vec_t<UInt32>       m_SlabsOf;  // per polygon its position in m_Polygons plus one, 0 for no edge index; empty when none has one
	my_vec_t<PolygonSlabs> m_Polygons;
};

#endif // __GEO_POLYGONEDGEINDEX_H
