// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

/*
 *  The dms_ polygon backend (issue #1214): a fault-tolerant snap-rounding sweep, and the
 *  adapters that let the shared operator machinery use it as a fifth geometry_library, next
 *  to GEOS_Traits.h, CGAL_Traits.h and the boost ones.
 */

#if !defined(DMS_GEO_DMS_TRAITS_H)
#define DMS_GEO_DMS_TRAITS_H

// ==== What it promises ================================================================
//
// The Boolean operations on polygon values, for all six point types. Unlike the library
// backends this one does not require valid input: rings may self-intersect, self-touch,
// overlap or touch each other, be nearly but not quite closed, or carry near-coincident
// vertices. The semantics that makes this well defined is the EVEN-ODD RULE, stated here
// explicitly as the GeoDMS meaning of a polygon sequence:
//
//     a point is inside the polygon when a ray from it to infinity crosses the boundary an
//     odd number of times.
//
// Ring orientation therefore plays no role on input (the result is still written with
// clockwise shells and counter-clockwise holes, the GeoDMS convention that area() is
// positive for). The zero-width corridors that connect the rings of a multi-polygon in the
// sequence layout are walked twice, in opposite directions, and cancel modulo 2, so the
// sequence as a whole is one closed walk, and that is exactly how it is read: consecutive
// points are segments, the final point is connected back to the first, and ring detection
// is not needed at all.
//
// ==== Tolerance =========================================================================
//
// Intersections are snapped to a grid, and every vertex of the input is snapped to the same
// grid first. That is the tolerance: two vertices closer than a grid cell become one, a
// sliver thinner than a cell collapses (and, being a polygon operator, a collapsed line or
// point is simply not in the result: polygon in, polygon out). The grid is
//
//   - for integer coordinates: the integer grid itself, so a vertex is never moved and the
//     result is exact in the sense that all its vertices are integer points;
//   - for float coordinates: a power of two derived per element pair, the coarser of the
//     unit in the last place of the largest coordinate magnitude and (joint extent) / 2^36.
//     That is far below any real precision of geographic data and it keeps the exact
//     arithmetic below within 128 bits;
//   - whatever an operator's own grid argument says, for the three-argument forms.
//
// An operator that folds MANY operands together (a dissolve, the cells of a Minkowski sum)
// must not let the derived cell change from one reduction to the next, or its answer would
// depend on the fold order. Such a caller derives one cell up front, with DeriveCell over
// all the operands, and hands it to every engine it then uses (SetFixedCell). Lattices for
// a fixed cell share one origin rule, so all the partial results stay on one lattice.
//
// ==== The algorithm =====================================================================
//
// 1. Quantize both sequences to the grid; internal coordinates are Int64 pixel centres in
//    [0, 2^36], relative to the grid line at or below the joint bounding box, so that the
//    lattice is the same for every pair with the same cell. Build the closed walks as
//    segments tagged with their operand (A or B).
// 2. Node the segments by ITERATED SNAP ROUNDING until a fixed point:
//      hot pixels := every segment endpoint plus the pixel of every proper crossing (the
//                    crossing computed exactly with 128-bit rationals, rounded half up);
//      every segment is replaced by the chain through the centres of all hot pixels it
//      meets (pixel = half-open unit square, so pixels partition the plane and the chain
//      has no ties); identical fragments merge and their per-operand multiplicities are
//      kept modulo 2.
//    A pass that splits nothing is the fixed point: no proper crossings, no vertex in the
//    interior of a fragment, no partial overlaps. Snapping can create new incidences (a
//    moved vertex lands on a third segment), which is why the passes are iterated; in
//    practice two suffice.
// 3. A plane sweep over the fragments, lexicographic in (x, y) with the usual symbolic tilt
//    so that vertical fragments order above everything else through their lower endpoint,
//    keeps the active fragments in a std::set. The parity of A and of B in the face just
//    below a fragment is the parity above its predecessor in the set, so both face parities
//    of every fragment are known after one sweep, whatever the number of components,
//    without any point location.
// 4. A fragment is a result boundary when the Boolean function of (insideA, insideB) differs
//    on its two sides; it is directed so that the result lies on its right.
// 5. The directed edges are chained into rings by turning into the interior at every vertex,
//    and a walk that returns to a vertex it has visited pinches the loop off as its own
//    ring, so every ring is simple. Clockwise rings (positive GeoDMS area) are shells, the
//    others holes.
// 6. A second sweep, over the ring edges, finds the shell of every hole: the active edge
//    directly below a hole's lexicographically first vertex bounds the polygon interior
//    around that vertex, so it belongs to the hole's shell or to a sibling hole processed
//    earlier.
// 7. The rings are written in the GeoDMS multi-polygon layout, the one geos_write_mp and
//    bp_assign_mp produce: shell, holes, back-track points, and the polygons themselves
//    connected by their back-track points. Shells and holes are ordered by their first
//    vertex and start at it, and a vertex between two collinear boundary edges (a noding
//    artefact) is left out, so the result does not depend on the operand order or on the
//    vertex order of the input: A + B == B + A holds exactly, and A * A is A with its
//    collinear vertices dropped.
//
// ==== Exact arithmetic ==================================================================
//
// All predicates (orientation, fraction comparison, the segment/pixel test) and the crossing
// pixel are computed exactly. With internal coordinates below 2^36, differences are below
// 2^37, a cross product below 2^75, and the numerator of a crossing coordinate,
// p.x * den + r.x * num, below 2^114: comfortably within
// boost::multiprecision::int128_t. Orientation takes an Int64 fast path when all differences
// are below 2^31, which is nearly always.
//
// ==== Who uses this header ==============================================================
//
//   - PolygonOper.cpp: dms_intersect, dms_union, dms_xor, dms_difference, the binary
//     operators, in both their two-argument and their three-argument (explicit grid) form.
//   - BoostPolygon.cpp: dms_polygon, dms_union_polygon and their split_ variants through
//     DmsPolySet and union_dms_polygons below, and dms_overlay_polygon /
//     dms_polygon_connectivity through the geometry_library::dms branch of
//     PolygonOverlayOperator.
//   - BoostGeometryImpl.h: dms_minkowski_sum and dms_minkowski_difference, which union the
//     convex cells of minkowski.h with this sweep instead of with a library.

#include "dbg/Diagnostics.h"    // throwErrorF and MG_CHECK2
#include "geom/Area.h"         // Area: the sign that tells a shell from a hole when splitting
#include "geom/RingIterator.h" // SA_ConstRingIterator: walking the rings of a polygon value
#include "geom/SpatialIndex.h" // the quadtree over hot pixels
#include "ptr/IterCast.h"      // begin_ptr, which SA_ConstRingIterator takes its base from
#include "vt/GeoSequence.h"    // the polygon container types
#include "vt/MinMax.h"         // MakeMin, MakeMax, Max

#include <boost/multiprecision/cpp_int.hpp>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <format>
#include <limits>
#include <set>
#include <string>
#include <vector>

namespace dms_overlay
{

using Int128 = boost::multiprecision::int128_t;
using GPoint = Point<Int64>;    // internal grid coordinates: pixel centres
using GRect  = Range<GPoint>;
using EdgeId = UInt32;

constexpr SizeT  NO_INDEX = SizeT(-1);
constexpr Int64  COORD_LIMIT = Int64(1) << 36;  // internal coordinates are in [0, COORD_LIMIT]
constexpr Int64  FAST_LIMIT  = Int64(1) << 31;  // differences below this: products fit in Int64
constexpr UInt32 MAX_NODING_ROUNDS = 64;

enum class BoolOp { Intersection, Union, Xor, Difference };

// *****************************************************************************
//	exact predicates
// *****************************************************************************

inline GPoint MakeGPoint(Int64 x, Int64 y) { return shp2dms_order<Int64>(x, y); }

inline bool LexLess(const GPoint& p, const GPoint& q)
{
	return p.X() < q.X() || (p.X() == q.X() && p.Y() < q.Y());
}

inline int  Sign(Int64 v) { return (v > 0) - (v < 0); }
inline bool WithinFastLimit(Int64 v) { return v > -FAST_LIMIT && v < FAST_LIMIT; }

inline Int128 Cross128(Int64 ax, Int64 ay, Int64 bx, Int64 by)
{
	return Int128(ax) * by - Int128(ay) * bx;
}

inline int CrossSign(Int64 ax, Int64 ay, Int64 bx, Int64 by)
{
	if (WithinFastLimit(ax) && WithinFastLimit(ay) && WithinFastLimit(bx) && WithinFastLimit(by))
		return Sign(ax * by - ay * bx);
	return Cross128(ax, ay, bx, by).sign();
}

// +1 when c lies to the left of the directed line a -> b (counter-clockwise turn, x to the right and
// y upward), -1 to the right, 0 when collinear.
inline int Orient(const GPoint& a, const GPoint& b, const GPoint& c)
{
	return CrossSign(b.X() - a.X(), b.Y() - a.Y(), c.X() - a.X(), c.Y() - a.Y());
}

// floor(num / den) for den > 0; the quotient is a grid coordinate, so it fits an Int64.
inline Int64 FloorDiv(const Int128& num, const Int128& den)
{
	assert(den > 0);
	Int128 q = num / den; // truncates toward zero
	if (num < 0 && q * den != num)
		--q;
	return q.convert_to<Int64>();
}

// floor(a / b) for Int64 with b > 0
inline Int64 FloorDivInt(Int64 a, Int64 b)
{
	assert(b > 0);
	Int64 q = a / b;
	if (a < 0 && q * b != a)
		--q;
	return q;
}

struct Fraction { Int128 num, den; }; // den > 0

inline int Compare(const Fraction& a, const Fraction& b)
{
	Int128 l = a.num * b.den, r = b.num * a.den;
	return (l > r) - (l < r);
}

// Angular order of directions, counter-clockwise from the positive x axis: [0, 180) before [180, 360),
// and within a half plane b comes after a when b is counter-clockwise of a. Distinct directions in one
// half plane are never opposite, so the cross product decides.
inline int HalfPlane(Int64 dx, Int64 dy) { return (dy > 0 || (dy == 0 && dx > 0)) ? 0 : 1; }

inline bool AngleLess(Int64 ax, Int64 ay, Int64 bx, Int64 by)
{
	int ha = HalfPlane(ax, ay), hb = HalfPlane(bx, by);
	if (ha != hb)
		return ha < hb;
	return CrossSign(ax, ay, bx, by) > 0;
}

// *****************************************************************************
//	segments and pixels
// *****************************************************************************

// A segment of the noding stage, with two payloads that the noder carries without looking at
// either. mask is the even-odd payload of the binary operators: bit 0 the parity contribution to
// operand A, bit 1 to B. weight is the coverage payload of a dissolve: the change of the number
// of elements covering a face when crossing the segment from the face on its right to the face
// on its left, RELATIVE TO THE STORED DIRECTION a -> b. That contract is what keeps it honest
// through the noder: MergeSegments negates the weight whenever it turns a segment round to its
// lexicographic orientation, and SnapSegments hands a parent's weight unchanged to the pieces it
// makes in the parent's travel direction, which a later MergeSegments turns round as needed.
// The mask is symmetric and needs neither. Identical fragments merge by XOR of the masks and by
// sum of the weights, and a fragment whose mask and weight are both zero affects no face and
// disappears.
struct Segment
{
	GPoint a, b;
	UInt8  mask   = 0;
	Int32  weight = 0;
};

inline bool ProperCrossing(const Segment& s, const Segment& t)
{
	int o1 = Orient(s.a, s.b, t.a), o2 = Orient(s.a, s.b, t.b);
	if (o1 == 0 || o2 == 0 || o1 == o2)
		return false;
	int o3 = Orient(t.a, t.b, s.a), o4 = Orient(t.a, t.b, s.b);
	return o3 != 0 && o4 != 0 && o3 != o4;
}

// The pixel that contains the crossing of the two segments p -> p2 and q -> q2 (a proper crossing,
// so the segments are not parallel). A pixel is the half-open unit square [c-1/2, c+1/2) in both
// axes around its integer centre c, so the crossing X maps to c = floor(X + 1/2).
//
// With t = cross(q - p, s) / cross(r, s), X = p + t r, so X.x = (p.x * den + r.x * num) / den and
// floor(X.x + 1/2) = floor((2 (p.x * den + r.x * num) + den) / (2 den)).
inline GPoint CrossingPixel(const GPoint& p, const GPoint& p2, const GPoint& q, const GPoint& q2)
{
	Int64 rx = p2.X() - p.X(), ry = p2.Y() - p.Y();
	Int64 sx = q2.X() - q.X(), sy = q2.Y() - q.Y();

	Int128 den = Cross128(rx, ry, sx, sy);
	Int128 num = Cross128(q.X() - p.X(), q.Y() - p.Y(), sx, sy);
	assert(den != 0);
	if (den < 0)
	{
		den = -den;
		num = -num;
	}
	Int128 nx = Int128(p.X()) * den + Int128(rx) * num;
	Int128 ny = Int128(p.Y()) * den + Int128(ry) * num;
	Int128 den2 = den * 2;
	return MakeGPoint(FloorDiv(nx * 2 + den, den2), FloorDiv(ny * 2 + den, den2));
}

// Does the closed segment a -> b meet the half-open pixel around c?
//
// The parameter interval of the segment inside the pixel is the intersection of [0, 1] with, per
// axis, the t for which 2c - 1 <= 2a + 2 t d < 2c + 1. Each bound is a fraction with denominator
// 2 d, inclusive on the lower pixel edge and exclusive on the upper one, so the interval is
// non-empty exactly when the tightest lower bound is below the tightest upper bound, or equal to it
// with both inclusive. The band test before it rejects the bulk of the candidates cheaply and
// exactly: a pixel meets the LINE through a and b only when |cross(d, c - a)| <= (|dx| + |dy|) / 2.
inline bool SegmentMeetsPixel(const GPoint& a, const GPoint& b, const GPoint& c)
{
	if (c == a || c == b)
		return true;

	Int64 dx = b.X() - a.X(), dy = b.Y() - a.Y();
	Int64 cx = c.X() - a.X(), cy = c.Y() - a.Y();

	Int128 cr = Cross128(dx, dy, cx, cy);
	if (cr < 0)
		cr = -cr;
	if (cr * 2 > Int128(std::llabs(dx)) + std::llabs(dy))
		return false;

	Fraction lo{ 0, 1 }, hi{ 1, 1 };
	bool loIncl = true, hiIncl = true;

	auto tightenLower = [&](const Fraction& f, bool incl)
	{
		int cmp = Compare(f, lo);
		if (cmp > 0) { lo = f; loIncl = incl; }
		else if (cmp == 0) loIncl = loIncl && incl;
	};
	auto tightenUpper = [&](const Fraction& f, bool incl)
	{
		int cmp = Compare(f, hi);
		if (cmp < 0) { hi = f; hiIncl = incl; }
		else if (cmp == 0) hiIncl = hiIncl && incl;
	};
	auto clip = [&](Int64 d, Int64 rel) -> bool // rel = c - a along this axis
	{
		if (d == 0)
			return rel == 0; // the segment stays in one column (row): it must be the pixel's

		Int64 n1 = 2 * rel - 1, n2 = 2 * rel + 1, den = 2 * d; // n1 <= t den < n2
		if (den > 0)
		{
			tightenLower(Fraction{ n1, den }, true);
			tightenUpper(Fraction{ n2, den }, false);
		}
		else
		{
			tightenLower(Fraction{ -n2, -den }, false);
			tightenUpper(Fraction{ -n1, -den }, true);
		}
		return true;
	};

	if (!clip(dx, cx) || !clip(dy, cy))
		return false;

	int cmp = Compare(lo, hi);
	return cmp < 0 || (cmp == 0 && loIncl && hiIncl);
}

inline void SortUnique(std::vector<GPoint>& pts)
{
	std::sort(pts.begin(), pts.end(), LexLess);
	pts.erase(std::unique(pts.begin(), pts.end()), pts.end());
}

// *****************************************************************************
//	the crossing sweep
// *****************************************************************************

// Bentley and Ottmann in exact arithmetic: the proper crossings of a set of segments, each
// reported once as the pixel that contains it. The sweep line is the vertical line with the
// symbolic tilt that orders points by (x, y). A segment enters the active set at its lower
// endpoint and leaves at its upper one; the active set is ordered by height at the sweep
// position, and whenever two segments become neighbours in it they are tested for a proper
// crossing, whose exact position joins an event heap. At a crossing event the segments through
// the crossing point reverse their order and the two new neighbourhoods are tested. Every
// crossing is found, because the two segments of a crossing are neighbours just before it; and
// every test is between two segments that could cross, there being no bounding box to filter by.
// Cost O((n + k) log n) for n segments and k crossings, whatever the shape of the segments.
//
// Two things keep the arithmetic exact. The active set only ever compares a segment that starts
// at the current position, an integer point, against active segments, which is a question of
// orientation: on which side of the active segment does the point lie, or, when it lies on it,
// which of the two directions is steeper. Nothing is compared at a rational position: at a
// crossing event the segments through the crossing point are not re-inserted but re-assigned to
// the tree nodes they occupy, which are consecutive, so the tree never sees the swap. The exact
// crossing position, a rational with numerators below 2^111 over a denominator below 2^74,
// orders the event heap; it is compared by its column first and with 256-bit products only when
// two events share a column, and whether a segment passes through such a point, which finds the
// ends of the run that reverses, is a 256-bit orientation as well.
//
// Degeneracies that need no case of their own: a segment starting on the interior of another
// (orientation zero, then the direction decides), two segments sharing an endpoint, and a
// crossing on an integer point where a third segment starts or ends. Collinear overlapping
// segments never cross properly; they are equal in height, ordered by their index, and a
// reversal keeps that order because it sorts the run by direction and index rather than
// reversing it.
struct CrossingSweep
{
	using Int256 = boost::multiprecision::int256_t;
	using SlotId = UInt32;
	static constexpr SlotId NO_SLOT = ~SlotId(0);

	// A point of the plane with rational coordinates (nx / d, ny / d), d > 0: an integer point
	// when d == 1, the crossing of two segments otherwise. fx is its column, floor(nx / d).
	struct ExactPoint
	{
		Int128 nx, ny, d;
		Int64  fx;
	};

	static ExactPoint FromGrid(const GPoint& p) { return ExactPoint{ p.X(), p.Y(), 1, p.X() }; }

	// The crossing of two properly crossing segments; CrossingPixel has the derivation.
	static ExactPoint CrossingOf(const Segment& s, const Segment& t)
	{
		Int64 rx = s.b.X() - s.a.X(), ry = s.b.Y() - s.a.Y();
		Int64 sx = t.b.X() - t.a.X(), sy = t.b.Y() - t.a.Y();
		Int128 den = Cross128(rx, ry, sx, sy);
		Int128 num = Cross128(t.a.X() - s.a.X(), t.a.Y() - s.a.Y(), sx, sy);
		assert(den != 0);
		if (den < 0)
		{
			den = -den;
			num = -num;
		}
		ExactPoint c;
		c.d  = den;
		c.nx = Int128(s.a.X()) * den + Int128(rx) * num;
		c.ny = Int128(s.a.Y()) * den + Int128(ry) * num;
		c.fx = FloorDiv(c.nx, den);
		return c;
	}

	static GPoint PixelOf(const ExactPoint& c)
	{
		Int128 den2 = c.d * 2;
		return MakeGPoint(FloorDiv(c.nx * 2 + c.d, den2), FloorDiv(c.ny * 2 + c.d, den2));
	}

	// The (x, y) order of two exact points. The columns differ for nearly every pair; within a
	// column the products need 256 bits unless one of the points is an integer point.
	static int Compare(const ExactPoint& p, const ExactPoint& q)
	{
		if (p.fx != q.fx)
			return p.fx < q.fx ? -1 : +1;
		if (p.d == 1 || q.d == 1)
		{
			Int128 l = p.nx * q.d, r = q.nx * p.d;
			if (l != r)
				return l < r ? -1 : +1;
			l = p.ny * q.d;
			r = q.ny * p.d;
			return (l > r) - (l < r);
		}
		Int256 l = Int256(p.nx) * Int256(q.d), r = Int256(q.nx) * Int256(p.d);
		if (l != r)
			return l < r ? -1 : +1;
		l = Int256(p.ny) * Int256(q.d);
		r = Int256(q.ny) * Int256(p.d);
		return (l > r) - (l < r);
	}

	// +1 when c lies to the left of the directed segment s, -1 to the right, 0 on its line.
	static int SideOf(const Segment& s, const ExactPoint& c)
	{
		Int64 dx = s.b.X() - s.a.X(), dy = s.b.Y() - s.a.Y();
		Int128 ex = c.nx - Int128(s.a.X()) * c.d;
		Int128 ey = c.ny - Int128(s.a.Y()) * c.d;
		Int256 v = Int256(dx) * Int256(ey) - Int256(dy) * Int256(ex);
		return (v > 0) - (v < 0);
	}

	// Of two segments through one point, s is below t just after it when t's direction is
	// counter-clockwise of s's. Equal directions order by index, so that two overlapping
	// segments are in the same order wherever the question is asked.
	static bool BelowByDirection(const Segment& s, EdgeId si, const Segment& t, EdgeId ti)
	{
		int c = CrossSign(s.b.X() - s.a.X(), s.b.Y() - s.a.Y(), t.b.X() - t.a.X(), t.b.Y() - t.a.Y());
		return c ? c > 0 : si < ti;
	}

	struct Below
	{
		const CrossingSweep* m_Sweep = nullptr;
		bool operator()(SlotId i, SlotId j) const { return i != j && m_Sweep->IsBelow(i, j); }
	};
	using ActiveSet  = std::set<SlotId, Below>;
	using ActiveIter = ActiveSet::iterator;

	struct Event
	{
		ExactPoint c;
		EdgeId     e; // one of the segments through c
	};
	struct EventAfter // the heap order: the front is the least position
	{
		bool operator()(const Event& l, const Event& r) const { return Compare(l.c, r.c) > 0; }
	};

	SizeT m_NrCrossings = 0; // crossing points found by the last Run

	// Appends the pixel of every proper crossing among segs to hot. The segments are oriented
	// from their lexicographically smaller endpoint and sorted by it, as MergeSegments leaves them.
	void Run(const std::vector<Segment>& segs, std::vector<GPoint>& hot)
	{
		assert(std::is_sorted(segs.begin(), segs.end(), [](const Segment& l, const Segment& r) { return LexLess(l.a, r.a); }));
		m_Segs = &segs;
		m_Hot  = &hot;
		m_NrCrossings = 0;
		EdgeId n = EdgeId(segs.size());

		m_ByHi.resize(n);
		for (EdgeId e = 0; e != n; ++e)
			m_ByHi[e] = e;
		std::sort(m_ByHi.begin(), m_ByHi.end(), [&segs](EdgeId i, EdgeId j) { return LexLess(segs[i].b, segs[j].b); });

		m_EdgeSlot.assign(n, NO_SLOT);
		m_SlotEdge.clear();
		m_SlotWhere.clear();
		m_FreeSlots.clear();
		m_Events.clear();

		ActiveSet active(Below{ this });
		m_Active = &active;

		EdgeId posLo = 0, posHi = 0;
		while (posLo != n || posHi != n || !m_Events.empty())
		{
			// the position: the least of the next start, the next end and the next crossing
			bool   haveGrid = false;
			GPoint g;
			if (posLo != n)
			{
				g = segs[posLo].a;
				haveGrid = true;
			}
			if (posHi != n)
			{
				const GPoint& h = segs[m_ByHi[posHi]].b;
				if (!haveGrid || LexLess(h, g))
					g = h;
				haveGrid = true;
			}
			if (!m_Events.empty() && (!haveGrid || Compare(m_Events.front().c, FromGrid(g)) < 0))
			{
				m_Pos    = m_Events.front().c;
				m_AtGrid = false;
			}
			else
			{
				m_Pos     = FromGrid(g);
				m_PosGrid = g;
				m_AtGrid  = true;
			}

			// the segments ending here leave; each departure makes two segments neighbours
			SlotId seed = NO_SLOT;
			if (m_AtGrid)
				for (; posHi != n && segs[m_ByHi[posHi]].b == g; ++posHi)
					Leave(m_ByHi[posHi], &seed);

			// the segments through a crossing here reverse; the heap holds the crossing as many
			// times as its pairs were neighbours, and one reversal serves them all
			while (!m_Events.empty() && Compare(m_Events.front().c, m_Pos) == 0)
			{
				EdgeId e = m_Events.front().e;
				std::pop_heap(m_Events.begin(), m_Events.end(), EventAfter());
				m_Events.pop_back();
				assert(m_EdgeSlot[e] != NO_SLOT); // a crossing is interior to both its segments
				seed = m_EdgeSlot[e];
			}
			if (seed != NO_SLOT)
				Reverse(seed);

			// the segments starting here enter
			if (m_AtGrid)
				for (; posLo != n && segs[posLo].a == g; ++posLo)
					Enter(posLo);
		}
		assert(active.empty());
		m_Active = nullptr;
	}

private:
	// The order of two active segments at the current position, an integer point that one of
	// them starts at: the point lies on one of them at least, and the side of the other decides,
	// or the direction when it lies on both.
	bool IsBelow(SlotId i, SlotId j) const
	{
		assert(m_AtGrid);
		EdgeId ei = m_SlotEdge[i], ej = m_SlotEdge[j];
		const Segment& s = (*m_Segs)[ei];
		const Segment& t = (*m_Segs)[ej];
		int os = s.a == m_PosGrid ? 0 : Orient(s.a, s.b, m_PosGrid);
		int ot = t.a == m_PosGrid ? 0 : Orient(t.a, t.b, m_PosGrid);
		if (os == 0 && ot == 0)
			return BelowByDirection(s, ei, t, ej);
		if (os == 0)
			return ot < 0; // the position, on s, lies below t
		if (ot == 0)
			return os > 0; // the position, on t, lies above s
		return HeightBelow(s, ei, t, ej);
	}

	// Two active segments neither of which passes through the position, by height at its
	// column. The tree never asks this, since every comparison involves the segment being
	// inserted, which starts at the position; it is here so that the order is total whatever is
	// asked. Both are non-vertical: a vertical segment active at this column passes through the
	// position.
	bool HeightBelow(const Segment& s, EdgeId si, const Segment& t, EdgeId ti) const
	{
		Int64 x = m_PosGrid.X();
		Int64 dsx = s.b.X() - s.a.X(), dsy = s.b.Y() - s.a.Y();
		Int64 dtx = t.b.X() - t.a.X(), dty = t.b.Y() - t.a.Y();
		assert(dsx > 0 && dtx > 0);
		Int128 hs = Int128(s.a.Y()) * dsx + Int128(x - s.a.X()) * dsy; // the height times dsx
		Int128 ht = Int128(t.a.Y()) * dtx + Int128(x - t.a.X()) * dty;
		Int128 l = hs * dtx, r = ht * dsx;
		if (l != r)
			return l < r;
		return BelowByDirection(s, si, t, ti);
	}

	bool Through(SlotId slot) const
	{
		const Segment& s = (*m_Segs)[m_SlotEdge[slot]];
		return m_AtGrid ? Orient(s.a, s.b, m_PosGrid) == 0 : SideOf(s, m_Pos) == 0;
	}

	SlotId NewSlot(EdgeId e)
	{
		SlotId slot;
		if (!m_FreeSlots.empty())
		{
			slot = m_FreeSlots.back();
			m_FreeSlots.pop_back();
		}
		else
		{
			slot = SlotId(m_SlotEdge.size());
			m_SlotEdge.push_back(e);
			m_SlotWhere.emplace_back();
		}
		m_SlotEdge[slot] = e;
		return slot;
	}

	void Enter(EdgeId e)
	{
		const Segment& s = (*m_Segs)[e];
		if (s.a == s.b)
			return; // no length, no crossing
		SlotId slot = NewSlot(e);
		auto ins = m_Active->insert(slot);
		assert(ins.second);
		ActiveIter it = ins.first;
		m_SlotWhere[slot] = it;
		m_EdgeSlot[e] = slot;
		if (it != m_Active->begin())
			TestPair(*std::prev(it), slot, nullptr);
		ActiveIter nx = std::next(it);
		if (nx != m_Active->end())
			TestPair(slot, *nx, nullptr);
	}

	void Leave(EdgeId e, SlotId* seed)
	{
		SlotId slot = m_EdgeSlot[e];
		if (slot == NO_SLOT)
			return; // no length, never entered
		ActiveIter it = m_SlotWhere[slot];
		ActiveIter nx = std::next(it);
		bool   hasPrev = it != m_Active->begin();
		SlotId prev = hasPrev ? *std::prev(it) : NO_SLOT;
		m_Active->erase(it);
		m_EdgeSlot[e] = NO_SLOT;
		m_FreeSlots.push_back(slot);
		if (hasPrev && nx != m_Active->end())
			TestPair(prev, *nx, seed);
	}

	// lo lies just below hi in the active set. Their proper crossing, if any, is ahead of the
	// sweep and joins the heap; or behind it, which happens when two segments that crossed
	// become neighbours again after whatever separated them has left, and then it was reversed
	// and reported when the sweep passed it; or at the current position, which only a departure
	// at an integer point can bring about, and then it is the seed of the reversal there.
	void TestPair(SlotId lo, SlotId hi, SlotId* seed)
	{
		const Segment& s = (*m_Segs)[m_SlotEdge[lo]];
		const Segment& t = (*m_Segs)[m_SlotEdge[hi]];
		if (!ProperCrossing(s, t))
			return;
		ExactPoint c = CrossingOf(s, t);
		int cmp = Compare(c, m_Pos);
		if (cmp < 0)
			return;
		if (cmp == 0)
		{
			MG_CHECK2(seed, "crossing sweep: a crossing at the sweep position where none can be");
			*seed = lo;
			return;
		}
		m_Events.push_back(Event{ c, m_SlotEdge[lo] });
		std::push_heap(m_Events.begin(), m_Events.end(), EventAfter());
	}

	// The run of active segments through the current position, a crossing, reverses: just after
	// the point they are ordered by direction, the steepest on top, and where directions coincide
	// by index. The tree keeps its nodes; only which segment sits in which node changes.
	void Reverse(SlotId seed)
	{
		ActiveIter first = m_SlotWhere[seed], last = first;
		while (first != m_Active->begin() && Through(*std::prev(first)))
			--first;
		for (ActiveIter nx = std::next(last); nx != m_Active->end() && Through(*nx); nx = std::next(last))
			last = nx;
		ActiveIter end = std::next(last);

		m_Run.clear();
		for (ActiveIter it = first; it != end; ++it)
			m_Run.push_back(m_SlotEdge[*it]);
		assert(m_Run.size() >= 2);
		const std::vector<Segment>& segs = *m_Segs;
		std::sort(m_Run.begin(), m_Run.end(), [&segs](EdgeId i, EdgeId j) { return BelowByDirection(segs[i], i, segs[j], j); });
		SizeT k = 0;
		for (ActiveIter it = first; it != end; ++it, ++k)
		{
			m_SlotEdge[*it] = m_Run[k];
			m_EdgeSlot[m_Run[k]] = *it;
		}

		m_Hot->push_back(PixelOf(m_Pos));
		++m_NrCrossings;

		if (first != m_Active->begin())
			TestPair(*std::prev(first), *first, nullptr);
		if (end != m_Active->end())
			TestPair(*last, *end, nullptr);
	}

	const std::vector<Segment>* m_Segs   = nullptr;
	std::vector<GPoint>*        m_Hot    = nullptr;
	ActiveSet*                  m_Active = nullptr;
	ExactPoint m_Pos{};
	GPoint     m_PosGrid;
	bool       m_AtGrid = false;

	std::vector<EdgeId>     m_ByHi;      // the segments by upper endpoint
	std::vector<SlotId>     m_EdgeSlot;  // per segment, its slot while active
	std::vector<EdgeId>     m_SlotEdge;  // per slot, its segment
	std::vector<ActiveIter> m_SlotWhere; // per slot, its node
	std::vector<SlotId>     m_FreeSlots;
	std::vector<Event>      m_Events;    // a heap by position, see EventAfter
	std::vector<EdgeId>     m_Run;
};

// *****************************************************************************
//	noding by iterated snap rounding
// *****************************************************************************

struct Noder
{
	SizeT m_NrCrossingPixels = 0; // pixels that a proper crossing was snapped to
	SizeT m_NrExtraRounds    = 0; // noding rounds beyond the first, i.e. snapping created new incidences

	// Orient every segment from its lexicographically smaller endpoint, merge identical segments,
	// keep their operand multiplicities modulo 2 and sum their coverage weights. A segment that
	// cancels (both parities zero and weight zero, such as the two directions of a corridor, or the
	// shared edge of two dissolved neighbours) disappears.
	//
	// The weight is relative to the stored direction a -> b (see Segment), so turning a segment
	// round negates it. The parity mask is symmetric and needs nothing. This is where the count
	// sweep first went wrong on real data: a steep edge whose snapped chain takes a vertical step
	// gets that step as a fragment travelling downward, which is then turned upward here, and
	// with its weight unchanged it claimed the wrong side of itself as covered.
	static void MergeSegments(std::vector<Segment>& segs)
	{
		for (auto& s : segs)
			if (LexLess(s.b, s.a))
			{
				std::swap(s.a, s.b);
				s.weight = -s.weight;
			}

		std::sort(segs.begin(), segs.end(), [](const Segment& l, const Segment& r)
			{
				return LexLess(l.a, r.a) || (l.a == r.a && LexLess(l.b, r.b));
			}
		);

		SizeT w = 0, n = segs.size();
		for (SizeT i = 0; i != n; )
		{
			SizeT j = i;
			UInt8 mask = 0;
			Int32 weight = 0;
			while (j != n && segs[j].a == segs[i].a && segs[j].b == segs[i].b)
			{
				mask   ^= segs[j].mask;
				weight += segs[j].weight;
				++j;
			}
			if (mask || weight)
			{
				segs[w] = segs[i];
				segs[w].mask   = mask;
				segs[w].weight = weight;
				++w;
			}
			i = j;
		}
		segs.resize(w);
	}

	void Run(std::vector<Segment>& segs)
	{
		MergeSegments(segs);
		m_Hot.clear();

		for (UInt32 round = 0; !segs.empty(); ++round)
		{
			MG_CHECK2(round < MAX_NODING_ROUNDS, "snap rounding did not reach a fixed point");

			for (const auto& s : segs)
			{
				m_Hot.push_back(s.a);
				m_Hot.push_back(s.b);
			}
			CollectCrossings(segs);
			SortUnique(m_Hot);

			bool anySplit = SnapSegments(segs);
			MergeSegments(segs);
			if (!anySplit)
				return; // the pass that confirms the fixed point is not an extra round
			if (round)
				++m_NrExtraRounds; // snapping created an incidence that a further pass had to node
		}
	}

private:
	using PointIndex = SpatialIndex<Int64, const GPoint*>;
	using PointIter  = PointIndex::iterator<GRect>;

	struct PixelKey
	{
		Int64  primary, secondary;
		GPoint c;
	};

	// The pixel of every proper crossing joins the hot set. Touching, T-junctions and collinear
	// overlaps need no crossing point: their incidences are at segment endpoints, which are hot
	// already, and the snapping pass splits the other segment there.
	void CollectCrossings(const std::vector<Segment>& segs)
	{
		m_Sweep.Run(segs, m_Hot);
		m_NrCrossingPixels += m_Sweep.m_NrCrossings;
	}

	// Replace every segment by the chain through the centres of the hot pixels it meets. The chain
	// is ordered by pixel column in the segment's x direction and by row within a column: pixels are
	// disjoint and a segment is monotone in both axes, so this is the order along the segment, and
	// the segment's own endpoints are its first and last pixel.
	bool SnapSegments(std::vector<Segment>& segs)
	{
		if (m_Hot.empty())
			return false;

		PointIndex index(m_Hot.data(), m_Hot.data() + m_Hot.size());

		m_Next.clear();
		m_Next.reserve(segs.size());
		bool anySplit = false;

		for (const auto& s : segs)
		{
			Int64 dx = s.b.X() - s.a.X(), dy = s.b.Y() - s.a.Y();
			int sx = Sign(dx), sy = Sign(dy);

			GRect box(s.a, s.b);
			// the index tests point leaves half-open against the search box; one more unit above
			GRect query(
				MakeGPoint(box.first.X() - 1, box.first.Y() - 1),
				MakeGPoint(box.second.X() + 2, box.second.Y() + 2)
			);

			m_Keys.clear();
			for (PointIter it = index.begin(query); it; ++it)
			{
				const GPoint& c = *(*it)->get_ptr();
				if (!SegmentMeetsPixel(s.a, s.b, c))
					continue;
				if (sx)
					m_Keys.push_back(PixelKey{ sx * c.X(), sy * c.Y(), c });
				else
					m_Keys.push_back(PixelKey{ sy * c.Y(), 0, c });
			}
			std::sort(m_Keys.begin(), m_Keys.end(), [](const PixelKey& l, const PixelKey& r)
				{
					return l.primary < r.primary || (l.primary == r.primary && l.secondary < r.secondary);
				}
			);
			MG_CHECK2(m_Keys.size() >= 2 && m_Keys.front().c == s.a && m_Keys.back().c == s.b
				, "snap rounding: a segment lost its endpoints");

			if (m_Keys.size() == 2)
			{
				m_Next.push_back(s);
				continue;
			}
			anySplit = true;
			for (SizeT k = 1, n = m_Keys.size(); k != n; ++k)
				m_Next.push_back(Segment{ m_Keys[k - 1].c, m_Keys[k].c, s.mask, s.weight });
		}
		segs.swap(m_Next);
		return anySplit;
	}

	std::vector<GPoint>   m_Hot;   // sorted and unique between rounds
	CrossingSweep         m_Sweep;
	std::vector<Segment>  m_Next;
	std::vector<PixelKey> m_Keys;
};

// *****************************************************************************
//	the plane sweep
// *****************************************************************************

// An edge of a sweep, directed from its lexicographically smaller endpoint: rightward, or upward
// when vertical. The sweep line is vertical with the symbolic tilt that orders points by (x, y);
// along it, "below" an edge is the face on the edge's right.
struct SweepEdge
{
	GPoint lo, hi;
};

inline bool IsVertical(const SweepEdge& e) { return e.lo.X() == e.hi.X(); }

// +1 when p is above the active edge e on the sweep line, -1 when below. p is lexicographically
// after e.lo, not e.hi (an edge is removed at its hi before that vertex's insertions) and, the edges
// being fully noded, not interior to e either.
inline int SideOfEdge(const SweepEdge& e, const GPoint& p)
{
	if (IsVertical(e))
		return p.Y() > e.hi.Y() ? +1 : -1;
	int o = Orient(e.lo, e.hi, p);
	assert(o != 0);
	return o > 0 ? +1 : -1;
}

// The order of two edges that are active at the same time. Two non-crossing edges keep one order
// over their common x range, so it can be decided where the later one starts. Two edges from one
// vertex order by angle in (-90, 90], which is what the orientation of their far ends gives, the
// vertical one on top.
inline bool IsBelow(const SweepEdge& e, const SweepEdge& f)
{
	if (e.lo == f.lo)
	{
		int o = Orient(e.lo, e.hi, f.hi);
		assert(o != 0); // overlapping edges are never both active
		return o > 0;
	}
	if (LexLess(e.lo, f.lo))
		return SideOfEdge(e, f.lo) > 0;
	return SideOfEdge(f, e.lo) < 0;
}

struct EdgeBelow
{
	const std::vector<SweepEdge>* m_Edges = nullptr;

	bool operator()(EdgeId i, EdgeId j) const
	{
		return i != j && IsBelow((*m_Edges)[i], (*m_Edges)[j]);
	}
};

struct SweepLine
{
	using ActiveSet  = std::set<EdgeId, EdgeBelow>;
	using ActiveIter = ActiveSet::iterator;

	explicit SweepLine(const std::vector<SweepEdge>& edges)
		: m_Edges(edges)
		, m_Active(EdgeBelow{ &edges })
		, m_Where(edges.size())
	{
		EdgeId n = EdgeId(edges.size());
		m_ByLo.resize(n);
		m_ByHi.resize(n);
		for (EdgeId e = 0; e != n; ++e)
			m_ByLo[e] = m_ByHi[e] = e;

		// starts by vertex, and per vertex bottom-up: the predecessor of an inserted edge is then
		// either an older edge or one just inserted, both with their faces already known.
		std::sort(m_ByLo.begin(), m_ByLo.end(), [&edges](EdgeId i, EdgeId j)
			{
				const SweepEdge& e = edges[i];
				const SweepEdge& f = edges[j];
				if (e.lo != f.lo)
					return LexLess(e.lo, f.lo);
				return IsBelow(e, f);
			}
		);
		std::sort(m_ByHi.begin(), m_ByHi.end(), [&edges](EdgeId i, EdgeId j)
			{
				return LexLess(edges[i].hi, edges[j].hi);
			}
		);
	}

	bool AtEnd() const { return m_PosLo == m_ByLo.size() && m_PosHi == m_ByHi.size(); }

	GPoint NextVertex() const
	{
		assert(!AtEnd());
		if (m_PosLo == m_ByLo.size())
			return m_Edges[m_ByHi[m_PosHi]].hi;
		if (m_PosHi == m_ByHi.size())
			return m_Edges[m_ByLo[m_PosLo]].lo;
		const GPoint& lo = m_Edges[m_ByLo[m_PosLo]].lo;
		const GPoint& hi = m_Edges[m_ByHi[m_PosHi]].hi;
		return LexLess(hi, lo) ? hi : lo;
	}

	// The event at v: the edges ending there leave the active set, then the edges starting there
	// enter it, bottom-up, and onNewEdge(edgeId, iterator) sees each of them once it is in place.
	template <typename OnNewEdge>
	void ProcessVertex(const GPoint& v, OnNewEdge&& onNewEdge)
	{
		for (; m_PosHi != m_ByHi.size() && m_Edges[m_ByHi[m_PosHi]].hi == v; ++m_PosHi)
			m_Active.erase(m_Where[m_ByHi[m_PosHi]]);

		for (; m_PosLo != m_ByLo.size() && m_Edges[m_ByLo[m_PosLo]].lo == v; ++m_PosLo)
		{
			EdgeId e = m_ByLo[m_PosLo];
			auto ins = m_Active.insert(e);
			assert(ins.second);
			m_Where[e] = ins.first;
			onNewEdge(e, ins.first);
		}
	}

	const std::vector<SweepEdge>& m_Edges;
	ActiveSet                     m_Active;
	std::vector<ActiveIter>       m_Where; // per edge, valid while it is active

private:
	std::vector<EdgeId> m_ByLo, m_ByHi;
	SizeT m_PosLo = 0, m_PosHi = 0;
};

// The face parities of every edge. rightMask is the parity of A (bit 0) and B (bit 1) in the face
// on the edge's right; the face on its left has rightMask ^ mask. Both operands are even-degree
// subgraphs (closed walks, merged modulo 2), so the parity of a face is well defined and equal to
// the parity below its lower boundary edge flipped by that edge: the sweep propagates it from the
// unbounded face upward through the predecessor of every newly active edge.
struct ParityEdge
{
	UInt8 mask;
	UInt8 rightMask;
};

inline void ComputeFaceParity(const std::vector<SweepEdge>& edges, std::vector<ParityEdge>& parity)
{
	assert(edges.size() == parity.size());
	SweepLine sweep(edges);
	while (!sweep.AtEnd())
	{
		GPoint v = sweep.NextVertex();
		sweep.ProcessVertex(v, [&](EdgeId e, SweepLine::ActiveIter it)
			{
				if (it == sweep.m_Active.begin())
					parity[e].rightMask = 0; // the unbounded face
				else
				{
					EdgeId p = *std::prev(it);
					parity[e].rightMask = parity[p].rightMask ^ parity[p].mask;
				}
			}
		);
	}
}

// The coverage counts of every edge, for a dissolve: below is the number of elements covering the
// face on the edge's right, and below + delta the number covering the face on its left, delta
// being the edge's summed weight (see Segment). The propagation is the parity sweep's with a sum
// in place of an XOR: the unbounded face has count 0, and the count on the right of an edge is
// the count on the left of the active edge just below it. Nothing here assumes the counts stay
// non-negative; a ring wound the wrong way subtracts, and the caller's membership test decides
// what that means.
struct CountEdge
{
	Int32 delta;
	Int32 below;
};

inline void ComputeFaceCounts(const std::vector<SweepEdge>& edges, std::vector<CountEdge>& counts)
{
	assert(edges.size() == counts.size());
	SweepLine sweep(edges);
	while (!sweep.AtEnd())
	{
		GPoint v = sweep.NextVertex();
		sweep.ProcessVertex(v, [&](EdgeId e, SweepLine::ActiveIter it)
			{
				if (it == sweep.m_Active.begin())
					counts[e].below = 0; // the unbounded face
				else
				{
					EdgeId p = *std::prev(it);
					counts[e].below = counts[p].below + counts[p].delta;
				}
			}
		);
	}
}

inline bool IsInside(BoolOp op, UInt8 mask)
{
	bool a = (mask & 1) != 0, b = (mask & 2) != 0;
	switch (op)
	{
	case BoolOp::Intersection: return a && b;
	case BoolOp::Union:        return a || b;
	case BoolOp::Xor:          return a != b;
	case BoolOp::Difference:   return a && !b;
	}
	return false;
}

// *****************************************************************************
//	rings
// *****************************************************************************

// A boundary edge of the result, directed with the result on its right.
struct DirEdge
{
	GPoint from, to;
};

struct Ring
{
	std::vector<GPoint> pts;    // open ring, starting at its lexicographically first vertex
	bool  isShell = false;      // clockwise (x right, y up), positive GeoDMS area
	SizeT parent  = NO_INDEX;   // for a hole: the index of its shell
};

// The sign of twice the signed area, standard orientation: +1 counter-clockwise, -1 clockwise.
inline int RingOrientation(const std::vector<GPoint>& pts)
{
	Int128 twiceArea = 0;
	const GPoint& o = pts[0];
	for (SizeT i = 1, n = pts.size(); i + 1 < n; ++i)
		twiceArea += Cross128(pts[i].X() - o.X(), pts[i].Y() - o.Y(), pts[i + 1].X() - o.X(), pts[i + 1].Y() - o.Y());
	return twiceArea.sign();
}

// Chains the directed edges into simple rings.
//
// At a vertex with several outgoing edges, an arriving walk continues along the first outgoing edge
// counter-clockwise from the direction it came from. Around a vertex the in- and outgoing edges
// alternate (each edge has the result on its right, so the wedges alternate inside and outside),
// which makes that a pairing of the incoming with the outgoing edges: the walks are closed and
// cover every edge once. A walk that reaches a vertex already on its path pinches the loop since
// that visit off as a ring of its own, so every ring is simple, and its orientation classifies it.
struct Polygonizer
{
	void Run(std::vector<DirEdge>& edges, std::vector<Ring>& rings)
	{
		rings.clear();
		if (edges.empty())
			return;

		// vertices
		m_Vertices.clear();
		m_Vertices.reserve(edges.size());
		for (const auto& e : edges)
			m_Vertices.push_back(e.from);
		SortUnique(m_Vertices);

		// edges grouped by their origin, and by angle within a group
		std::sort(edges.begin(), edges.end(), [](const DirEdge& l, const DirEdge& r)
			{
				return LexLess(l.from, r.from) || (l.from == r.from && LexLess(l.to, r.to));
			}
		);
		SizeT n = edges.size(), nv = m_Vertices.size();
		m_OutStart.assign(nv + 1, 0);
		m_From.resize(n);
		m_To.resize(n);
		for (SizeT e = 0; e != n; ++e)
		{
			m_From[e] = FindVertex(edges[e].from);
			m_To[e]   = FindVertex(edges[e].to);
			++m_OutStart[m_From[e] + 1];
		}
		for (SizeT v = 0; v != nv; ++v)
			m_OutStart[v + 1] += m_OutStart[v];
		for (SizeT v = 0; v != nv; ++v)
		{
			SizeT s = m_OutStart[v], t = m_OutStart[v + 1];
			MG_CHECK2(s != t, "dms overlay: a boundary vertex without an outgoing edge");
			if (t - s > 1)
				std::sort(edges.begin() + s, edges.begin() + t, [](const DirEdge& l, const DirEdge& r)
					{
						return AngleLess(l.to.X() - l.from.X(), l.to.Y() - l.from.Y(), r.to.X() - r.from.X(), r.to.Y() - r.from.Y());
					}
				);
		}
		for (SizeT e = 0; e != n; ++e) // the angular sort moved edges within their group
			m_To[e] = FindVertex(edges[e].to);

		// the walks
		m_Used.assign(n, false);
		m_PosOnPath.assign(nv, NO_INDEX);
		m_Path.clear();

		for (EdgeId e0 = 0; e0 != n; ++e0)
		{
			if (m_Used[e0])
				continue;
			EdgeId e = e0;
			while (true)
			{
				m_Used[e] = true;
				m_PosOnPath[m_From[e]] = m_Path.size();
				m_Path.push_back(e);

				UInt32 w = m_To[e];
				SizeT k = m_PosOnPath[w];
				if (k != NO_INDEX)
				{
					EmitRing(edges, k, rings);
					for (SizeT i = k, pe = m_Path.size(); i != pe; ++i)
						m_PosOnPath[m_From[m_Path[i]]] = NO_INDEX;
					m_Path.resize(k);
					if (m_Path.empty())
						break;
				}
				e = NextEdge(edges, e, w);
				MG_CHECK2(!m_Used[e], "dms overlay: the boundary walk re-used an edge");
			}
		}
		assert(m_Path.empty());
	}

private:
	UInt32 FindVertex(const GPoint& p) const
	{
		auto it = std::lower_bound(m_Vertices.begin(), m_Vertices.end(), p, LexLess);
		// In a Release build a miss here used to map the point to whatever vertex sorts next and
		// let the walk run on from the wrong place; the failures that followed named nothing.
		MG_CHECK2(it != m_Vertices.end() && *it == p, "dms overlay: a boundary edge ends at a vertex where no boundary edge begins");
		return UInt32(it - m_Vertices.begin());
	}

	// The first outgoing edge at w counter-clockwise from the ray back along the arriving edge e.
	EdgeId NextEdge(const std::vector<DirEdge>& edges, EdgeId e, UInt32 w) const
	{
		SizeT s = m_OutStart[w], t = m_OutStart[w + 1];
		assert(s != t);
		if (t - s == 1)
			return EdgeId(s);

		const DirEdge& in = edges[e];
		Int64 rx = in.from.X() - in.to.X(), ry = in.from.Y() - in.to.Y();

		SizeT lo = s, hi = t; // upper_bound in the angular order
		while (lo < hi)
		{
			SizeT mid = (lo + hi) / 2;
			const DirEdge& o = edges[mid];
			if (AngleLess(rx, ry, o.to.X() - o.from.X(), o.to.Y() - o.from.Y()))
				hi = mid;
			else
				lo = mid + 1;
		}
		return EdgeId(lo == t ? s : lo);
	}

	void EmitRing(const std::vector<DirEdge>& edges, SizeT k, std::vector<Ring>& rings) const
	{
		Ring ring;
		ring.pts.reserve(m_Path.size() - k);
		for (SizeT i = k, pe = m_Path.size(); i != pe; ++i)
			ring.pts.push_back(edges[m_Path[i]].from);
		MG_CHECK2(ring.pts.size() >= 3, "dms overlay: a ring with fewer than three vertices");

		int orientation = RingOrientation(ring.pts);
		MG_CHECK2(orientation != 0, "dms overlay: a ring without area");
		ring.isShell = orientation < 0;

		auto first = std::min_element(ring.pts.begin(), ring.pts.end(), LexLess);
		std::rotate(ring.pts.begin(), first, ring.pts.end());
		rings.push_back(std::move(ring));
	}

	std::vector<GPoint> m_Vertices;
	std::vector<UInt32> m_From, m_To;
	std::vector<SizeT>  m_OutStart;
	std::vector<bool>   m_Used;
	std::vector<EdgeId> m_Path;
	std::vector<SizeT>  m_PosOnPath;
};

// The shell of every hole. At a hole's first vertex v the polygon interior is on the left, so the
// wedge below the lower of the hole's two edges at v is interior, and the active edge directly below
// that edge bounds this interior from below: it belongs to the hole's shell, or to another hole of
// the same polygon, whose first vertex is before v (or at v and lower) and whose shell is then known.
struct HoleAssigner
{
	void Run(std::vector<Ring>& rings)
	{
		m_Edges.clear();
		m_RingOf.clear();
		m_EdgeStart.assign(rings.size() + 1, 0);
		m_Holes.clear();

		for (SizeT r = 0, nr = rings.size(); r != nr; ++r)
		{
			const auto& pts = rings[r].pts;
			m_EdgeStart[r] = m_Edges.size();
			for (SizeT i = 0, n = pts.size(); i != n; ++i)
			{
				const GPoint& p = pts[i];
				const GPoint& q = pts[i + 1 == n ? 0 : i + 1];
				m_Edges.push_back(LexLess(p, q) ? SweepEdge{ p, q } : SweepEdge{ q, p });
				m_RingOf.push_back(r);
			}
			if (!rings[r].isShell)
				m_Holes.push_back(r);
		}
		m_EdgeStart[rings.size()] = m_Edges.size();
		if (m_Holes.empty())
			return;

		std::sort(m_Holes.begin(), m_Holes.end(), [&rings](SizeT a, SizeT b)
			{
				return LexLess(rings[a].pts[0], rings[b].pts[0]);
			}
		);

		// a ring's two edges at its first vertex are its first and its last edge
		auto lowerEdgeOf = [this](SizeT r) -> EdgeId
		{
			EdgeId e0 = EdgeId(m_EdgeStart[r]), e1 = EdgeId(m_EdgeStart[r + 1] - 1);
			return IsBelow(m_Edges[e0], m_Edges[e1]) ? e0 : e1;
		};

		SweepLine sweep(m_Edges);
		SizeT hp = 0;
		while (!sweep.AtEnd())
		{
			GPoint v = sweep.NextVertex();
			sweep.ProcessVertex(v, [](EdgeId, SweepLine::ActiveIter) {});

			SizeT hb = hp;
			while (hp != m_Holes.size() && rings[m_Holes[hp]].pts[0] == v)
				++hp;
			if (hb == hp)
				continue;

			std::sort(m_Holes.begin() + hb, m_Holes.begin() + hp, [&](SizeT a, SizeT b)
				{
					return IsBelow(m_Edges[lowerEdgeOf(a)], m_Edges[lowerEdgeOf(b)]);
				}
			);
			for (SizeT k = hb; k != hp; ++k)
			{
				SizeT h = m_Holes[k];
				SweepLine::ActiveIter it = sweep.m_Where[lowerEdgeOf(h)];
				MG_CHECK2(it != sweep.m_Active.begin(), "dms overlay: a hole without an enclosing shell");
				SizeT r = m_RingOf[*std::prev(it)];
				SizeT parent = rings[r].isShell ? r : rings[r].parent;
				MG_CHECK2(parent != NO_INDEX, "dms overlay: a hole nested in a hole without a shell");
				rings[h].parent = parent;
			}
		}
	}

private:
	std::vector<SweepEdge> m_Edges;
	std::vector<SizeT>     m_RingOf;
	std::vector<SizeT>     m_EdgeStart;
	std::vector<SizeT>     m_Holes;
};

// *****************************************************************************
//	the engine: one element pair
// *****************************************************************************

template <typename P>
struct DmsOverlayEngine
{
	using Scalar = scalar_of_t<P>;
	static constexpr bool is_float = std::is_floating_point_v<Scalar>;

	DmsOverlayEngine(BoolOp op, CharPtr operName, Float64 explicitGrid = 0.0)
		: m_Op(op), m_OperName(operName), m_ExplicitGrid(explicitGrid)
	{
		if (explicitGrid == 0.0)
			return;
		if (!(explicitGrid > 0.0) || !std::isfinite(explicitGrid))
			throwErrorF(operName, "the grid size must be a positive number, not {}", explicitGrid);
		if constexpr (!is_float)
		{
			if (explicitGrid != std::floor(explicitGrid) || explicitGrid > 1e12)
				throwErrorF(operName, "the grid size for integer coordinates must be a whole number of at least 1, not {}", explicitGrid);
		}
	}

	SizeT m_NrUndefined = 0; // elements that had an undefined operand or an undefined point
	SizeT NrCrossingPixels() const { return m_Noder.m_NrCrossingPixels; }
	SizeT NrExtraRounds()    const { return m_Noder.m_NrExtraRounds; }

	// One lattice for a whole fold, see DeriveFoldCell. Zero, the default, derives the cell from
	// each operand pair, which is what the binary operators want.
	void SetFixedCell(Float64 cell) { m_FixedCell = cell; }

	// One frame for a whole fold, cell and origin both: every element and every intermediate of a
	// dissolve is quantized into the same integer lattice, so that the rings of one element can be
	// appended to those of another without any coordinate conversion, and so that the answer does
	// not depend on how the elements were grouped or tiled. The origin is a lattice line at or
	// below the extent the frame covers, and that extent must fit the 2^36 internal cells, which
	// DmsFrameFor checks when it makes the frame; SetFrame checks every operand against it. For
	// integer coordinates the cell is 1 and the origin integral. The current frame is set at once,
	// so that Store can dequantize after UnionBag, which frames nothing itself.
	void SetFixedFrame(Float64 cell, Float64 originX, Float64 originY)
	{
		MG_CHECK2(cell > 0.0, "dms overlay: a fixed frame needs a positive cell");
		m_FixedCell = cell;
		m_HasFixedOrigin = true;
		m_FixedOriginX = originX;
		m_FixedOriginY = originY;
		if constexpr (is_float)
		{
			m_Cell = cell;
			m_OriginX = originX;
			m_OriginY = originY;
		}
		else
		{
			m_ICell = Int64(cell);
			m_IOriginX = Int64(originX);
			m_IOriginY = Int64(originY);
		}
	}

	// The entry point of the binary operators: an undefined operand gives an undefined result.
	template <typename E>
	void Apply(E&& res, SA_ConstReference<P> a, SA_ConstReference<P> b)
	{
		if (!IsDefined(a) || !IsDefined(b) || !ApplyRanges(res, m_Op, a, b))
		{
			++m_NrUndefined;
			res.assign(Undefined());
		}
	}

	// The general entry point: any two ranges of P. False, and res untouched, when the operands
	// cannot be framed, which only an undefined or infinite coordinate causes.
	template <typename E, typename RA, typename RB>
	bool ApplyRanges(E&& res, BoolOp op, const RA& a, const RB& b)
	{
		if (!Compute(op, a, b) && !m_Framed)
			return false;
		Store(std::forward<E>(res));
		return true;
	}

	// The union of one operand with nothing: the even-odd reading of whatever it contains,
	// written as a valid polygon. This is the clean-up, and what dms_polygon does per element.
	template <typename E, typename RA>
	bool Clean(E&& res, const RA& a)
	{
		return ApplyRanges(std::forward<E>(res), BoolOp::Union, a, EmptyRange());
	}

	// Phase A of a dissolve: the even-odd reading of one element as rings on the lattice, without
	// writing them out. Empty when the element cannot be framed or encloses no area. The rings are
	// the engine's own and are valid until its next call; dms_append_rings copies what it needs.
	template <typename RA>
	const std::vector<Ring>& CleanToRings(const RA& a)
	{
		if (!Compute(BoolOp::Union, a, EmptyRange()))
			m_Rings.clear();
		return m_Rings;
	}

	// Phase B of a dissolve: the union of a bag of segments that already lie on this engine's fixed
	// frame (SetFixedFrame) and carry coverage weights (see Segment and dms_append_rings). Nodes
	// them once, sweeps once for the coverage counts, keeps every fragment where the count changes
	// between zero and nonzero, directed with the covered side on its right, and chains those into
	// rings, which Store then writes. The nonzero rule, rather than count > 0, keeps a ring that
	// arrived wound the wrong way on the inside instead of subtracting it; after phase A the two
	// agree, since every ring it produces is canonically wound. The bag is consumed. Returns whether
	// any area came out.
	bool UnionBag(std::vector<Segment>& bag)
	{
		MG_CHECK2(m_HasFixedOrigin, "dms overlay: UnionBag needs a fixed frame");
		m_Rings.clear();
		m_Framed = true;
		m_Segments.swap(bag);
		bag.clear();

		m_Noder.Run(m_Segments);

		SizeT n = m_Segments.size();
		m_Edges.resize(n);
		m_Counts.resize(n);
		for (SizeT i = 0; i != n; ++i)
		{
			m_Edges[i]  = SweepEdge{ m_Segments[i].a, m_Segments[i].b }; // merged: a is the lexicographic lower
			m_Counts[i] = CountEdge{ m_Segments[i].weight, 0 };
		}
		ComputeFaceCounts(m_Edges, m_Counts);

		m_Kept.clear();
		for (SizeT i = 0; i != n; ++i)
		{
			bool inRight = m_Counts[i].below != 0;
			bool inLeft  = (m_Counts[i].below + m_Counts[i].delta) != 0;
			if (inRight == inLeft)
				continue;
			if (inRight)
				m_Kept.push_back(DirEdge{ m_Edges[i].lo, m_Edges[i].hi });
			else
				m_Kept.push_back(DirEdge{ m_Edges[i].hi, m_Edges[i].lo });
		}
		CheckKeptClosed(n);

		m_Polygonizer.Run(m_Kept, m_Rings);
		m_HoleAssigner.Run(m_Rings);
		return !m_Rings.empty();
	}

	// The boundary of a region is closed: at every vertex as many kept edges leave as arrive. When
	// that fails, the counts around some vertex were inconsistent, and the polygonizer downstream
	// can only fail in ways that name nothing (a walk that re-uses an edge, a ring of two
	// vertices). So it is checked here, and the first offending vertex is reported with every
	// fragment that touches it: its endpoints, its weight, the count on its right and on its left,
	// and whether it was kept. That is the whole local configuration, enough to reason from.
	void CheckKeptClosed(SizeT nrFragments) const
	{
		std::vector<std::pair<GPoint, int>> ends;
		ends.reserve(2 * m_Kept.size());
		for (const auto& e : m_Kept)
		{
			ends.emplace_back(e.from, +1);
			ends.emplace_back(e.to,   -1);
		}
		std::sort(ends.begin(), ends.end(), [](const auto& l, const auto& r) { return LexLess(l.first, r.first); });
		for (SizeT i = 0, n = ends.size(); i != n; )
		{
			SizeT j = i;
			int balance = 0;
			while (j != n && ends[j].first == ends[i].first)
				balance += ends[j++].second;
			if (balance != 0)
			{
				const GPoint& v = ends[i].first;
				P w = Dequantize(v);
				std::string msg = std::format("dms overlay: the boundary is not closed at vertex ({}, {}), internal ({}, {}): {} more kept edges leave than arrive. Fragments at that vertex:"
					, w.X(), w.Y(), v.X(), v.Y(), balance);
				SizeT listed = 0;
				for (SizeT f = 0; f != nrFragments && listed != 24; ++f)
				{
					if (m_Edges[f].lo != v && m_Edges[f].hi != v)
						continue;
					++listed;
					bool inRight = m_Counts[f].below != 0;
					bool inLeft  = (m_Counts[f].below + m_Counts[f].delta) != 0;
					msg += std::format("\n  lo ({}, {}) hi ({}, {}) weight {} below {} above {} {}"
						, m_Edges[f].lo.X(), m_Edges[f].lo.Y(), m_Edges[f].hi.X(), m_Edges[f].hi.Y()
						, m_Counts[f].delta, m_Counts[f].below, m_Counts[f].below + m_Counts[f].delta
						, inRight == inLeft ? "dropped" : inRight ? "kept lo->hi" : "kept hi->lo");
				}
				throwErrorD(m_OperName, msg.c_str());
			}
			i = j;
		}
	}

	// Run the sweep and keep its rings. Returns whether the result encloses any area, which is
	// what dms_polygon_connectivity asks and what saves dms_overlay_polygon a store.
	template <typename RA, typename RB>
	bool Compute(BoolOp op, const RA& a, const RB& b)
	{
		m_Rings.clear();
		m_Framed = SetFrame(a, b);
		if (!m_Framed)
			return false;

		m_Segments.clear();
		BuildWalk(a, 1);
		BuildWalk(b, 2);

		m_Noder.Run(m_Segments);

		// face parities
		SizeT n = m_Segments.size();
		m_Edges.resize(n);
		m_Parity.resize(n);
		for (SizeT i = 0; i != n; ++i)
		{
			m_Edges[i] = SweepEdge{ m_Segments[i].a, m_Segments[i].b }; // merged: a is the lexicographic lower
			m_Parity[i] = ParityEdge{ m_Segments[i].mask, 0 };
		}
		ComputeFaceParity(m_Edges, m_Parity);

		// the boundary of the result, with the result on the right
		m_Kept.clear();
		for (SizeT i = 0; i != n; ++i)
		{
			bool inRight = IsInside(op, m_Parity[i].rightMask);
			bool inLeft  = IsInside(op, m_Parity[i].rightMask ^ m_Parity[i].mask);
			if (inRight == inLeft)
				continue;
			if (inRight)
				m_Kept.push_back(DirEdge{ m_Edges[i].lo, m_Edges[i].hi });
			else
				m_Kept.push_back(DirEdge{ m_Edges[i].hi, m_Edges[i].lo });
		}

		m_Polygonizer.Run(m_Kept, m_Rings);
		m_HoleAssigner.Run(m_Rings);
		return !m_Rings.empty();
	}

	// An empty operand, for the unary form: no segments, so its parity is even everywhere.
	struct EmptyRange
	{
		const P* begin() const { return nullptr; }
		const P* end()   const { return nullptr; }
	};

	// The coordinate statistics a frame is derived from: the joint bounding box and the largest
	// magnitude. An undefined or infinite coordinate makes it unusable.
	struct CoordStats
	{
		Float64 minX = std::numeric_limits<Float64>::infinity(), minY = minX;
		Float64 maxX = -minX, maxY = -minX, maxAbs = 0;
		bool any = false, usable = true;

		template <typename R>
		void Add(const R& range)
		{
			for (auto pi = range.begin(), pe = range.end(); pi != pe; ++pi)
			{
				const P& p = *pi;
				if (!IsDefined(p))
				{
					usable = false;
					return;
				}
				Float64 x = Float64(p.X()), y = Float64(p.Y());
				if constexpr (is_float)
				{
					if (!std::isfinite(x) || !std::isfinite(y))
					{
						usable = false;
						return;
					}
				}
				MakeMin(minX, x); MakeMin(minY, y);
				MakeMax(maxX, x); MakeMax(maxY, y);
				MakeMax(maxAbs, std::fabs(x)); MakeMax(maxAbs, std::fabs(y));
				any = true;
			}
		}
		Float64 Extent() const { return any ? Max<Float64>(maxX - minX, maxY - minY) : 0.0; }
		Float64 MaxAbs() const { return maxAbs; }
	};

	// The derived cell for float coordinates: the coarser of 2^36 cells over the extent and the
	// unit in the last place of the largest coordinate magnitude, below which the input carries
	// no information. A power of two, so that a coarser cell's lattice is a subset of a finer
	// one's and the origin rule keeps every derived frame on one lattice.
	static Float64 CellFor(Float64 extent, Float64 maxAbs)
	{
		int eExtent = extent > 0 ? std::ilogb(extent) + 1 - 36 : std::numeric_limits<int>::min();
		int eUlp = maxAbs > 0 ? std::ilogb(maxAbs) - (std::numeric_limits<Scalar>::digits - 1) : std::numeric_limits<int>::min();
		int e = Max<int>(eExtent, eUlp);
		return e == std::numeric_limits<int>::min() ? 1.0 : std::ldexp(1.0, e);
	}

private:
	// ---- the grid frame of one element pair ----

	// world = origin + cell * internal; for integer coordinates the origin and the cell are integers
	Float64 m_OriginX = 0, m_OriginY = 0, m_Cell = 1;
	Int64   m_IOriginX = 0, m_IOriginY = 0, m_ICell = 1;

	// The joint bounding box of both operands decides the frame. False when a point is undefined or
	// not finite, in which case the element is undefined.
	template <typename RA, typename RB>
	bool SetFrame(const RA& a, const RB& b)
	{
		CoordStats stats;
		stats.Add(a);
		if (stats.usable)
			stats.Add(b);
		if (!stats.usable)
			return false;

		Float64 minX = stats.any ? stats.minX : 0.0; // empty operands: any frame will do
		Float64 minY = stats.any ? stats.minY : 0.0;

		// The origin is the grid line at or below the joint minimum, so that the lattice is the same
		// for every element pair with the same cell: neighbours snap a shared boundary alike.
		if constexpr (is_float)
		{
			Float64 extent = stats.Extent();
			if (m_ExplicitGrid > 0.0)
			{
				m_Cell = m_ExplicitGrid;
				if (extent > m_Cell * Float64(COORD_LIMIT))
					throwErrorF(m_OperName, "the grid size {} is too fine for operands that span {}: at most 2^36 cells are supported in either direction; use a coarser grid", m_Cell, extent);
			}
			else if (m_FixedCell > 0.0)
				m_Cell = m_FixedCell; // one lattice for a whole fold, see DeriveFoldCell
			else
				m_Cell = CellFor(extent, stats.MaxAbs());

			if (m_HasFixedOrigin)
			{
				// one frame for a whole fold, see SetFixedFrame: the operands must lie inside it
				m_OriginX = m_FixedOriginX;
				m_OriginY = m_FixedOriginY;
				if (stats.any && !FitsFrame(stats.minX, stats.minY, stats.maxX, stats.maxY))
					throwErrorF(m_OperName, "an operand spanning [{}, {}] to [{}, {}] lies outside the fixed frame of this fold; its coordinates are not within the declared range of the values unit"
						, stats.minX, stats.minY, stats.maxX, stats.maxY);
			}
			else
			{
				m_OriginX = std::floor(minX / m_Cell) * m_Cell;
				m_OriginY = std::floor(minY / m_Cell) * m_Cell;
			}
		}
		else
		{
			m_ICell = m_ExplicitGrid > 0.0 ? Int64(m_ExplicitGrid) : (m_FixedCell > 0.0 ? Int64(m_FixedCell) : 1);
			if (m_HasFixedOrigin)
			{
				m_IOriginX = Int64(m_FixedOriginX); // integral by construction, see SetFixedFrame
				m_IOriginY = Int64(m_FixedOriginY);
				if (stats.any && !FitsFrame(stats.minX, stats.minY, stats.maxX, stats.maxY))
					throwErrorF(m_OperName, "an operand spanning [{}, {}] to [{}, {}] lies outside the fixed frame of this fold; its coordinates are not within the declared range of the values unit"
						, stats.minX, stats.minY, stats.maxX, stats.maxY);
			}
			else
			{
				m_IOriginX = FloorDivInt(Int64(minX), m_ICell) * m_ICell; // Int64(minX) is exact: 32-bit coordinates at most
				m_IOriginY = FloorDivInt(Int64(minY), m_ICell) * m_ICell;
				// the full Int32 range spans 2^32 cells at grid 1, within the budget of 2^36
			}
		}
		return true;
	}

	// Whether a box of world coordinates quantizes into the current frame's [0, 2^36] internal
	// range on both axes.
	bool FitsFrame(Float64 minX, Float64 minY, Float64 maxX, Float64 maxY) const
	{
		if constexpr (is_float)
		{
			Float64 span = m_Cell * Float64(COORD_LIMIT);
			return minX >= m_OriginX && minY >= m_OriginY && maxX <= m_OriginX + span && maxY <= m_OriginY + span;
		}
		else
		{
			Int64 span = m_ICell * COORD_LIMIT;
			return Int64(minX) >= m_IOriginX && Int64(minY) >= m_IOriginY && Int64(maxX) <= m_IOriginX + span && Int64(maxY) <= m_IOriginY + span;
		}
	}

	GPoint Quantize(const P& p) const
	{
		if constexpr (is_float)
		{
			Int64 x = Int64(std::floor((Float64(p.X()) - m_OriginX) / m_Cell + 0.5));
			Int64 y = Int64(std::floor((Float64(p.Y()) - m_OriginY) / m_Cell + 0.5));
			return MakeGPoint(x, y);
		}
		else
		{
			Int64 rx = Int64(p.X()) - m_IOriginX, ry = Int64(p.Y()) - m_IOriginY; // both >= 0
			if (m_ICell == 1)
				return MakeGPoint(rx, ry);
			return MakeGPoint((2 * rx + m_ICell) / (2 * m_ICell), (2 * ry + m_ICell) / (2 * m_ICell)); // round half up
		}
	}

	P Dequantize(const GPoint& g) const
	{
		if constexpr (is_float)
			return shp2dms_order<Scalar>(Scalar(m_OriginX + Float64(g.X()) * m_Cell), Scalar(m_OriginY + Float64(g.Y()) * m_Cell));
		else
			return shp2dms_order<Scalar>(Scalar(m_IOriginX + g.X() * m_ICell), Scalar(m_IOriginY + g.Y() * m_ICell));
	}

	// The closed walk of one operand: consecutive distinct grid points are segments, and the last
	// point connects back to the first. Rings, corridors and orientation are not looked at; the
	// corridors cancel in the merge modulo 2 and the even-odd rule does the rest.
	template <typename R>
	void BuildWalk(const R& seq, UInt8 mask)
	{
		GPoint first = MakeGPoint(0, 0), prev = first;
		bool any = false;
		for (auto pi = seq.begin(), pe = seq.end(); pi != pe; ++pi)
		{
			GPoint g = Quantize(*pi);
			if (!any)
			{
				first = prev = g;
				any = true;
				continue;
			}
			if (g != prev)
			{
				m_Segments.push_back(Segment{ prev, g, mask });
				prev = g;
			}
		}
		if (any && prev != first)
			m_Segments.push_back(Segment{ prev, first, mask });
	}

	// ---- output ----
public:

	template <typename E>
	void WriteRing(E&& ref, const std::vector<GPoint>& pts) const
	{
		for (const auto& g : pts)
			ref.emplace_back(MG_DEBUG_ALLOCATOR_FIRST("dms_overlay") Dequantize(g));
		ref.emplace_back(MG_DEBUG_ALLOCATOR_FIRST("dms_overlay") Dequantize(pts[0]));
	}

	// A vertex between two collinear edges came from noding (a vertex of the other operand on this
	// edge, or a crossing that snapped onto it) and says nothing about the result; leave it out. The
	// first vertex is the lexicographic minimum, a strict corner of the convex hull, so it stays
	// first. Done at write time only: the ring structures keep every node, because a touching ring
	// has a vertex there and the hole assignment must not see it as a T-junction.
	static void DropCollinear(const std::vector<GPoint>& in, std::vector<GPoint>& out)
	{
		out.clear();
		out.reserve(in.size());
		for (const auto& p : in)
		{
			out.push_back(p);
			while (out.size() >= 3 && Orient(out[out.size() - 3], out[out.size() - 2], out.back()) == 0)
				out.erase(out.end() - 2);
		}
		while (out.size() >= 3 && Orient(out[out.size() - 2], out.back(), out.front()) == 0)
			out.pop_back();
		while (out.size() >= 3 && Orient(out.back(), out.front(), out[1]) == 0)
			out.erase(out.begin());
		assert(out.size() >= 3 && out.front() == in.front());
	}

	// The layout of geos_write_mp: per polygon the shell, its holes, then the way back to the shell's
	// start through the holes' starts; the polygons chained the same way through their shells' starts.
	// Writes what the last Compute produced, an empty sequence when that enclosed no area.
	template <typename E>
	void Store(E&& res)
	{
		res.clear();
		if (m_Rings.empty())
			return;

		m_OutPts.resize(m_Rings.size());
		for (SizeT r = 0, n = m_Rings.size(); r != n; ++r)
			DropCollinear(m_Rings[r].pts, m_OutPts[r]);

		m_Shells.clear();
		m_HolesOf.assign(m_Rings.size(), std::vector<SizeT>());
		for (SizeT r = 0, n = m_Rings.size(); r != n; ++r)
		{
			if (m_Rings[r].isShell)
				m_Shells.push_back(r);
			else
			{
				MG_CHECK2(m_Rings[r].parent != NO_INDEX, "dms overlay: a hole was not assigned to a shell");
				m_HolesOf[m_Rings[r].parent].push_back(r);
			}
		}
		MG_CHECK2(!m_Shells.empty(), "dms overlay: rings without a shell");
		std::sort(m_Shells.begin(), m_Shells.end(), [this](SizeT a, SizeT b)
			{
				return LexLess(m_Rings[a].pts[0], m_Rings[b].pts[0]);
			}
		);

		SizeT count = m_Shells.size() - 1; // the way back through the shells' starts
		for (SizeT s : m_Shells)
		{
			auto& holes = m_HolesOf[s];
			std::sort(holes.begin(), holes.end(), [this](SizeT a, SizeT b)
				{
					return LexLess(m_Rings[a].pts[0], m_Rings[b].pts[0]);
				}
			);
			count += m_OutPts[s].size() + 1;
			for (SizeT h : holes)
				count += m_OutPts[h].size() + 1;
			count += holes.size(); // the way back: the holes' starts except the last, and the shell's
		}
		res.reserve(count MG_DEBUG_ALLOCATOR_SRC("dms_overlay"));

		std::vector<GPoint> shellStarts, holeStarts;
		for (SizeT s : m_Shells)
		{
			const auto& shell = m_OutPts[s];
			WriteRing(res, shell);

			holeStarts.clear();
			for (SizeT h : m_HolesOf[s])
			{
				WriteRing(res, m_OutPts[h]);
				holeStarts.push_back(m_OutPts[h][0]);
			}
			if (!holeStarts.empty())
			{
				holeStarts.pop_back();
				while (!holeStarts.empty())
				{
					res.emplace_back(MG_DEBUG_ALLOCATOR_FIRST("dms_overlay") Dequantize(holeStarts.back()));
					holeStarts.pop_back();
				}
				res.emplace_back(MG_DEBUG_ALLOCATOR_FIRST("dms_overlay") Dequantize(shell[0]));
			}
			shellStarts.push_back(shell[0]);
		}
		shellStarts.pop_back();
		while (!shellStarts.empty())
		{
			res.emplace_back(MG_DEBUG_ALLOCATOR_FIRST("dms_overlay") Dequantize(shellStarts.back()));
			shellStarts.pop_back();
		}
		assert(res.size() == count);
	}

private:
	BoolOp  m_Op;
	CharPtr m_OperName;
	Float64 m_ExplicitGrid;
	Float64 m_FixedCell = 0; // > 0: one lattice for a whole fold, see DeriveFoldCell
	bool    m_HasFixedOrigin = false; // one frame for a whole fold, see SetFixedFrame
	Float64 m_FixedOriginX = 0, m_FixedOriginY = 0;
	bool    m_Framed = false; // whether the last Compute could frame its operands

	std::vector<Segment>    m_Segments;
	Noder                   m_Noder;
	std::vector<SweepEdge>  m_Edges;
	std::vector<ParityEdge> m_Parity;
	std::vector<CountEdge>  m_Counts;
	std::vector<DirEdge>    m_Kept;
	Polygonizer             m_Polygonizer;
	std::vector<Ring>       m_Rings;
	HoleAssigner            m_HoleAssigner;
	std::vector<SizeT>      m_Shells;
	std::vector<std::vector<SizeT>>  m_HolesOf;
	std::vector<std::vector<GPoint>> m_OutPts; // the rings as written, without collinear vertices
};

// *****************************************************************************
//	the adapters the shared operator machinery uses
// *****************************************************************************

template <typename P> using dms_polygon_t = typename sequence_traits<P>::container_type;

// One cell for a whole fold. The binary form derives a cell per operand pair, which is right
// there and wrong in a fold: a dissolve that re-derives it after every reduction would snap the
// accumulated result onto a coarser lattice as it grows, and its answer would depend on the
// order the operands were folded in. A folding caller therefore derives the cell once, over all
// its operands, and gives it to every engine with SetFixedCell.
//
// Zero for integer coordinates, where the lattice is the integer grid whatever the extent, and
// zero when there is nothing to derive it from; SetFixedCell(0) means "derive per pair".
//
// A caller derives this per tile, since that is the data it has in hand; where a dissolve spans
// tiles, union_dms_polygons folds onto the coarser of the lattices it meets, which is associative
// and commutative and therefore leaves the answer independent of the fold order.
template <typename P, typename PolyRange>
Float64 DeriveFoldCell(const PolyRange& polys, Float64 grow = 0.0)
{
	if constexpr (!std::is_floating_point_v<scalar_of_t<P>>)
		return 0.0;
	else
	{
		typename DmsOverlayEngine<P>::CoordStats stats;
		for (auto pi = polys.begin(), pe = polys.end(); pi != pe; ++pi)
			stats.Add(*pi);
		if (!stats.usable || !stats.any)
			return 0.0;
		return DmsOverlayEngine<P>::CellFor(stats.Extent() + 2 * grow, stats.MaxAbs() + grow);
	}
}

// *****************************************************************************
//	the dissolve in one noding
// *****************************************************************************

// The frame of a dissolve: one cell and one origin for every element and every intermediate of
// one operator call, so that all of them share a single integer lattice. Derived once, before the
// first element is read, from the values unit's declared range or, failing that, from the extent
// of the data, by DmsFrameFor. A zero cell means none could be derived.
struct DmsFrame
{
	Float64 cell = 0, originX = 0, originY = 0;
	bool defined() const { return cell > 0.0; }
};

// The frame that covers the box [lo, hi] of world coordinates: the cell CellFor gives, so that the
// box spans at most 2^36 cells, and the origin on the lattice line at or below the box. Flooring
// the origin can push the far corner past the 2^36th cell by less than one cell, in which case the
// cell doubles once. For integer coordinates the cell is 1 and the origin the box's lower corner:
// the full Int32 range spans 2^32 cells, within the budget.
template <typename P>
DmsFrame DmsFrameFor(Float64 loX, Float64 loY, Float64 hiX, Float64 hiY)
{
	DmsFrame f;
	if (!std::isfinite(loX) || !std::isfinite(loY) || !std::isfinite(hiX) || !std::isfinite(hiY))
		return f;
	if (!(hiX >= loX) || !(hiY >= loY))
		return f;
	if constexpr (std::is_floating_point_v<scalar_of_t<P>>)
	{
		Float64 extent = Max<Float64>(hiX - loX, hiY - loY);
		Float64 maxAbs = Max<Float64>(Max<Float64>(std::fabs(loX), std::fabs(hiX)), Max<Float64>(std::fabs(loY), std::fabs(hiY)));
		if (!std::isfinite(extent))
			return f; // the type's own full range, not a declared one: nothing can be framed
		f.cell = DmsOverlayEngine<P>::CellFor(extent, maxAbs);
		// Never an unbounded loop on floating point: the first cell already spans the extent
		// within 2^36 cells, and flooring the origin adds less than one cell, so one doubling
		// settles it. Anything that does not settle in a few is unframeable and says so.
		for (int attempt = 0; attempt != 4; ++attempt)
		{
			if (!std::isfinite(f.cell) || !(f.cell > 0.0))
				return DmsFrame();
			f.originX = std::floor(loX / f.cell) * f.cell;
			f.originY = std::floor(loY / f.cell) * f.cell;
			Float64 span = f.cell * Float64(COORD_LIMIT);
			if (std::isfinite(f.originX) && std::isfinite(f.originY) && hiX - f.originX <= span && hiY - f.originY <= span)
				return f;
			f.cell *= 2.0;
		}
		return DmsFrame();
	}
	else
	{
		f.cell = 1.0;
		f.originX = std::floor(loX);
		f.originY = std::floor(loY);
	}
	return f;
}

// The accumulator of a dissolve: the ring edges of every element read so far, each with its
// coverage weight, all on the frame the bag carries. Filled by dms_append_rings as the tiles come
// in, consumed by DmsOverlayEngine::UnionBag at store time, so the dissolve nodes its input once
// whatever the number of elements or tiles it arrived in.
struct DmsSegmentBag
{
	DmsFrame             frame;
	std::vector<Segment> segs;
	SizeT                nrElements = 0;

	bool empty() const { return segs.empty(); }
};

// Phase A's output into the bag: every edge of every ring, in the direction the ring runs, with
// coverage weight -1. A ring the sweep wrote is canonically wound, shells clockwise with the
// covered side on the right of each edge and holes the other way round with the covered side
// again on the right, so crossing any ring edge from its right to its left leaves one element's
// coverage: -1, relative to the stored direction, which is what Segment's contract asks for. The
// noder turns the weight round with the fragment whenever it re-orients one, so no rule about
// lo and hi is needed here, and holes need no special case.
inline void dms_append_rings(DmsSegmentBag& bag, const std::vector<Ring>& rings)
{
	for (const auto& ring : rings)
	{
		const auto& pts = ring.pts; // open: the last point connects back to the first
		SizeT n = pts.size();
		if (n < 3)
			continue;
		// No reserve here: reserve(size + n) grows to exactly that, so with a bag that grows by a
		// few segments per element it reallocates and copies the whole bag on EVERY append, which
		// is quadratic in the number of elements: 124,662 buildings cost six minutes of memcpy.
		// push_back grows geometrically, which is what a bag that is appended to wants.
		for (SizeT i = 0; i != n; ++i)
		{
			const GPoint& from = pts[i];
			const GPoint& to   = pts[i + 1 == n ? 0 : i + 1];
			if (from == to)
				continue;
			bag.segs.push_back(Segment{ from, to, 0, -1 });
		}
	}
	++bag.nrElements;
}

// The accumulator of a fold: a polygon value plus the cell it was snapped to, so that the cell
// travels with the operands and every reduction of one operator call stays on one lattice.
template <typename P>
struct DmsPolySet
{
	dms_polygon_t<P> m_Poly;
	Float64          m_Cell = 0;

	bool empty() const { return m_Poly.empty(); }
};

// The reducer of an assoc_tower over DmsPolySet: the union of two partial results. Pairwise and
// associative, so the tower keeps the fold logarithmically deep, as union_geos_multi_polygon
// does for GEOS.
template <typename P>
struct union_dms_polygons
{
	void operator ()(DmsPolySet<P>& lhs, DmsPolySet<P>&& rhs) const
	{
		if (rhs.empty())
			return;
		if (lhs.empty())
		{
			lhs = std::move(rhs);
			return;
		}
		// The coarser of the two lattices, which is commutative and associative, so the lattice the
		// whole fold ends on is the same whatever order the tower reduced in. Within one tile every
		// operand carries the same cell anyway; the choice only bites where a dissolve spans tiles,
		// each of which derives its cell from its own extent.
		Float64 cell = Max<Float64>(lhs.m_Cell, rhs.m_Cell);

		DmsOverlayEngine<P> engine(BoolOp::Union, "dms_union_polygon");
		engine.SetFixedCell(cell);

		dms_polygon_t<P> result;
		engine.ApplyRanges(result, BoolOp::Union, lhs.m_Poly, rhs.m_Poly);
		lhs.m_Poly = std::move(result);
		lhs.m_Cell = cell;
	}
};

// Read one polygon value into the accumulator, cleaned: the sweep over the operand alone, which
// is the even-odd reading of whatever the source contains. This is what makes everything
// downstream (the fold, the split, the store) see canonical geometry, exactly as
// geos_create_polygons followed by normalize() does on the GEOS side.
template <typename P, typename R>
void dms_clean_into(DmsPolySet<P>& lhs, const R& poly, Float64 cell, CharPtr operName)
{
	DmsOverlayEngine<P> engine(BoolOp::Union, operName);
	engine.SetFixedCell(cell);

	lhs.m_Cell = cell;
	engine.Clean(lhs.m_Poly, poly);
}

// Copy a polygon value into a result reference. The sequence is already in the multi-polygon
// layout, so this is a copy, not a conversion.
template <typename E, typename R>
void dms_store_polygon(E&& ref, const R& poly)
{
	ref.clear();
	ref.reserve(poly.size() MG_DEBUG_ALLOCATOR_SRC("dms_store_polygon"));
	for (const auto& p : poly)
		ref.emplace_back(MG_DEBUG_ALLOCATOR_FIRST("dms_store_polygon") p);
}

// *****************************************************************************
//	splitting a value into its single polygons
// *****************************************************************************

// Only ever applied to a value this sweep wrote, where a ring of positive area opens a polygon
// and the counter-clockwise rings that follow it are its holes.

template <typename P>
using dms_ring_t = SA_ConstRing<P>;

template <typename P, typename R>
void dms_collect_rings(const R& poly, std::vector<dms_ring_t<P>>& rings)
{
	rings.clear();
	SA_ConstRingIterator<P> ri(poly, 0), re(poly, -1);
	for (; ri != re; ++ri)
		rings.push_back(*ri);
}

template <typename P>
bool dms_ring_is_shell(const dms_ring_t<P>& ring)
{
	return Area<Float64>(ring.begin(), ring.end()) > 0;
}

template <typename P, typename R>
SizeT dms_split_count(const R& poly)
{
	std::vector<dms_ring_t<P>> rings;
	dms_collect_rings<P>(poly, rings);

	SizeT result = 0;
	for (const auto& ring : rings)
		if (dms_ring_is_shell<P>(ring))
			++result;
	return result;
}

// One polygon in the multi-polygon layout: the shell, each hole, then the way back through the
// holes' first points to the shell's first point. The rings arrive closed, so they are copied
// as they are.
template <typename E, typename P>
void dms_write_single_polygon(E&& ref, const dms_ring_t<P>& shell, const std::vector<dms_ring_t<P>>& holes)
{
	SizeT count = shell.size();
	for (const auto& hole : holes)
		count += hole.size();
	if (!holes.empty())
		count += holes.size(); // the holes' starts except the last one, and the shell's start

	ref.clear();
	ref.reserve(count MG_DEBUG_ALLOCATOR_SRC("dms_write_single_polygon"));

	for (const auto& p : shell)
		ref.emplace_back(MG_DEBUG_ALLOCATOR_FIRST("dms_write_single_polygon") p);
	for (const auto& hole : holes)
		for (const auto& p : hole)
			ref.emplace_back(MG_DEBUG_ALLOCATOR_FIRST("dms_write_single_polygon") p);

	if (!holes.empty())
	{
		for (SizeT i = holes.size() - 1; i != 0; --i)
			ref.emplace_back(MG_DEBUG_ALLOCATOR_FIRST("dms_write_single_polygon") holes[i - 1].begin()[0]);
		ref.emplace_back(MG_DEBUG_ALLOCATOR_FIRST("dms_write_single_polygon") shell.begin()[0]);
	}
	assert(ref.size() == count);
}

template <typename P, typename RI, typename R>
RI dms_split_assign(RI resIter, const R& poly)
{
	std::vector<dms_ring_t<P>> rings;
	dms_collect_rings<P>(poly, rings);

	std::vector<dms_ring_t<P>> holes;
	for (SizeT i = 0, n = rings.size(); i != n; ++i)
	{
		if (!dms_ring_is_shell<P>(rings[i]))
			continue; // a hole before any shell cannot happen in what the sweep writes

		holes.clear();
		for (SizeT j = i + 1; j != n && !dms_ring_is_shell<P>(rings[j]); ++j)
			holes.push_back(rings[j]);

		dms_write_single_polygon(*resIter, rings[i], holes);
		++resIter;
	}
	return resIter;
}

} // namespace dms_overlay

#endif //!defined(DMS_GEO_DMS_TRAITS_H)
