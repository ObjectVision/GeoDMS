// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

#if !defined(__GEO_DMS_MINKOWSKI_H)
#define __GEO_DMS_MINKOWSKI_H

/*
 *  dms_minkowski_sum, dms_minkowski_difference and dms_buffer_multi_polygon on the lattice of the
 *  dms_ sweep (issues #1301 and #1302): per element one clean, then one noding and one sweep, or
 *  for a convex element none at all.
 */

// ==== The pieces ========================================================================
//
// For an element P (on the lattice, cleaned, its rings simple) and a CONVEX kernel K that
// contains the origin:
//
//     P (+) K  =  P  U  { e (+) conv(0, S(n_e)) : e an edge of P }  U  { v (+) conv(0, arc_v) : v a convex corner }
//
// with n_e the outward normal of edge e, S(n) the support set of K in direction n (the vertex of
// K that lies furthest in that direction, or the edge of K parallel to e), and arc_v the vertices
// of K whose normal cones lie between the normals of the two edges at v.
//
// Each piece lies within P (+) K, since K is convex and contains the origin. Conversely, a point
// x of P (+) K outside P is x = q + t k with q the point of P where the shrinking copy x - tK last
// touches it, t <= 1, and K's outward normal at k equal to P's at q: an edge normal when q is on
// an edge, so x lies in that edge's piece; a normal from the cone of a convex corner when q is
// one, so x lies in its fan. A reflex corner cannot be that point unless the support of both its
// edges is the same vertex of K, and then x lies in either edge's piece.
//
// Every piece is a simple counter-clockwise polygon, so the number of pieces covering a point is
// the winding number of the sum of their boundaries: never negative, and positive exactly on the
// union. That is the membership rule DmsOverlayEngine::ThresholdBag sweeps with. When the merge
// of identical fragments has cancelled the shared sides, what the sweep sees is the raw offset
// curve of GEOS and Clipper: every edge shifted by its support, an arc of K at every convex
// corner, and the way back through the vertex at every reflex one.
//
// Why not the convolution cycle of Guibas, Ramshaw and Stolfi, with backward arcs at the reflex
// vertices and the positive-winding rule, as #1301 first proposed: its winding number at x is the
// Euler characteristic of P n (x - K), which is zero when x - K holds a whole hole of P together
// with a loop of material round it. A hole smaller than the kernel would come back in the sum
// (Baram et al. 2015 fill such holes first). The pieces need no such step: a hole that the kernel
// covers is covered by the pieces of its own edges.
//
// A kernel that does not contain the origin is shifted by one of its lattice points c:
// P (+) K = (P + c) (+) (K - c).
//
// Only convex kernels: the four convex named variants (4HV, 4D, 8D, 16D) and a convex kernel
// polygon. The star-shaped XHV and XD, used by no configuration outside the regression tests of
// their bp_ operators, are refused, as is a kernel polygon that is not convex (#1301).
//
// ==== Erosion ===========================================================================
//
// dms_minkowski_difference keeps what the other backends keep, A n { x : x + K in A }, which they
// compute as A \ ((box \ A) (+) -K). With c a lattice point of K and K' = K - c,
// { x : x + K in A } = (A - c) eroded by K', and the complement of A - c grown by -K' is that
// complement together with the pieces of its boundary: A's rings in their stored orientation,
// which is the complement's on their left. The box contributes nothing near A. So the result is
// where   1_A + [c != 0] 1_{A - c} - (number of pieces)   reaches 1 + [c != 0].
//
// ==== Buffer ============================================================================
//
// dms_buffer_multi_polygon places its points the way GEOS does, so that switching a configuration
// between geos_buffer_multi_polygon and dms_buffer_multi_polygon with the same arguments gives
// comparable results: every edge is shifted by exactly the distance along its unit normal, and a
// convex corner gets an arc of round(turn / quantum) equal steps, the quantum being a quarter turn
// divided by the third argument, GEOS's quadrant segments (0: a straight cut, GEOS's bevel join).
// The pieces are then the rectangles of the edges and the fans of the corners, and the same
// argument, with the disc and the nearest point, makes their union the buffer. A negative
// distance erodes, through the complement as above.
//
// ==== The lattice =======================================================================
//
// Every element is framed on its own extent grown by the kernel's reach (DmsFrameFor), the frame
// the dms_ binary operators also derive per element pair, and cleaned (the single-ring shortcut,
// or the sweep). The kernel is snapped to the same lattice, and for a kernel the convex hull of
// the snapped vertices is taken, so that snapping cannot make it non-convex. Every vertex of every
// piece is then a lattice point, and the sweep's snap rounding is the only tolerance, as for the
// rest of the family.

#include "DMS_Traits.h"
#include "minkowski.h"
#include "ParallelTiles.h"
#include "ThreadScratch.h"

#include <atomic>
#include <numbers>

namespace dms_overlay
{

inline GPoint Add(const GPoint& a, const GPoint& b) { return MakeGPoint(a.X() + b.X(), a.Y() + b.Y()); }
inline GPoint Origin() { return MakeGPoint(0, 0); }

// The convex hull of lattice points, counter-clockwise, without collinear vertices; one point when
// they are all equal, the two extremes when they are all collinear. pts is sorted and made unique.
inline void LatticeHull(std::vector<GPoint>& pts, std::vector<GPoint>& hull)
{
	SortUnique(pts);
	hull.clear();
	if (pts.size() <= 2)
	{
		hull.assign(pts.begin(), pts.end());
		return;
	}
	hull.resize(2 * pts.size());
	SizeT k = 0;
	for (SizeT i = 0; i != pts.size(); ++i) // lower hull
	{
		while (k >= 2 && Orient(hull[k - 2], hull[k - 1], pts[i]) <= 0)
			--k;
		hull[k++] = pts[i];
	}
	for (SizeT i = pts.size() - 1, lower = k + 1; i != 0; --i) // upper hull
	{
		while (k >= lower && Orient(hull[k - 2], hull[k - 1], pts[i - 1]) <= 0)
			--k;
		hull[k++] = pts[i - 1];
	}
	hull.resize(k - 1); // the last point repeats the first
}

// *****************************************************************************
//	the kernel on the lattice
// *****************************************************************************

// A convex kernel on the lattice of one frame, as the pieces want it: K - c, counter-clockwise,
// starting at its lowest vertex (least y, then least x) so that its edge directions are in the
// angular order of AngleLess; and c, a lattice point of K, the origin itself when K contains it.
// A kernel that snapped to a segment has two vertices and two opposite directions, one that
// snapped to a point has one vertex and none; the pieces below handle both.
struct DmsLatticeKernel
{
	std::vector<GPoint> k;   // the vertices of K - c
	std::vector<GPoint> dir; // dir[j] = k[j + 1] - k[j], cyclically; empty for a single point
	GPoint c = Origin();

	SizeT size() const { return k.size(); }

	// A closed ring in world units, reflected through the origin when `reflect`, snapped to the
	// lattice of `cell` with llround, which keeps a symmetric kernel symmetric.
	void Build(const MinkowskiRing& ring, Float64 cell, bool reflect, std::vector<GPoint>& scratch)
	{
		scratch.clear();
		for (SizeT i = 0, n = ring.size() ? ring.size() - 1 : 0; i != n; ++i) // the closing point repeats the first
		{
			Float64 x = ring[i].X() / cell, y = ring[i].Y() / cell;
			if (reflect)
			{
				x = -x;
				y = -y;
			}
			scratch.push_back(MakeGPoint(std::llround(x), std::llround(y)));
		}
		LatticeHull(scratch, k);
		MG_CHECK2(!k.empty(), "dms minkowski: an empty kernel");

		SizeT m = k.size();
		GPoint o = Origin();
		bool containsOrigin = true;
		if (m == 1)
			containsOrigin = k[0] == o;
		else if (m == 2)
			containsOrigin = Orient(k[0], k[1], o) == 0 && !LexLess(o, k[0]) && !LexLess(k[1], o); // k[0] < k[1] after the sort
		else
			for (SizeT j = 0; j != m && containsOrigin; ++j)
				containsOrigin = Orient(k[j], k[j + 1 == m ? 0 : j + 1], o) >= 0;
		c = containsOrigin ? o : k[0];
		for (auto& p : k)
			p = MakeGPoint(p.X() - c.X(), p.Y() - c.Y());

		std::rotate(k.begin(), std::min_element(k.begin(), k.end(), [](const GPoint& a, const GPoint& b)
			{
				return a.Y() < b.Y() || (a.Y() == b.Y() && a.X() < b.X());
			}), k.end());
		dir.clear();
		if (m >= 2)
			for (SizeT j = 0; j != m; ++j)
			{
				const GPoint& a = k[j];
				const GPoint& b = k[j + 1 == m ? 0 : j + 1];
				dir.push_back(MakeGPoint(b.X() - a.X(), b.Y() - a.Y()));
			}
	}
};

// The support of a polygon kernel: for an edge of direction d, whose outward normal is d turned
// clockwise, the vertex of K furthest along that normal is k[s] with dir[s - 1] <= d < dir[s] in
// angular order, both rotated by the same quarter turn. When d runs along dir[s - 1] the support
// is the whole edge k[s - 1] .. k[s]: first and last differ, and the piece of the element's edge
// is a trapezoid.
struct DmsPolygonSupport
{
	const DmsLatticeKernel* m_K = nullptr;

	struct Edge
	{
		GPoint first, last;
		UInt32 iFirst = 0, iLast = 0;
	};

	Edge OfEdge(Int64 dx, Int64 dy) const
	{
		const auto& K = *m_K;
		SizeT m = K.size();
		if (m == 1)
			return Edge{ K.k[0], K.k[0], 0, 0 };
		auto it = std::upper_bound(K.dir.begin(), K.dir.end(), MakeGPoint(dx, dy), [](const GPoint& d, const GPoint& e)
			{
				return AngleLess(d.X(), d.Y(), e.X(), e.Y());
			});
		SizeT u = it - K.dir.begin(); // the number of directions at or before d
		SizeT s = u % m, f = s;
		if (u && !AngleLess(K.dir[u - 1].X(), K.dir[u - 1].Y(), dx, dy))
			f = (s + m - 1) % m; // d runs along an edge of K
		return Edge{ K.k[f], K.k[s], UInt32(f), UInt32(s) };
	}

	// The vertices of K from the support of the incoming edge to that of the outgoing one, for a
	// convex corner, where the normal turns counter-clockwise by less than half a turn. Empty when
	// both supports are the same vertex.
	void FanChain(const Edge& in, const Edge& out, std::vector<GPoint>& chain) const
	{
		chain.clear();
		const auto& K = *m_K;
		SizeT m = K.size();
		SizeT steps = (out.iFirst + m - in.iLast) % m;
		if (!steps)
			return;
		for (SizeT s = 0; s <= steps; ++s)
			chain.push_back(K.k[(in.iLast + s) % m]);
	}
};

// The support of a buffer, GEOS's placement: an edge is shifted by the distance along its unit
// outward normal, and a convex corner gets round(turn / quantum) equal steps of the circle between
// the two normals, the quantum being a quarter turn divided by the quadrant segments. Every point
// is snapped to the lattice; a point that snapping would put out of angular order is left out, so
// that every fan stays a simple counter-clockwise polygon.
struct DmsRoundSupport
{
	DmsRoundSupport(Float64 radiusInCells, UInt8 quadrantSegments)
		: m_Radius(radiusInCells)
		, m_Quantum(quadrantSegments ? (std::numbers::pi / 2.0) / quadrantSegments : 0.0)
	{}

	struct Edge
	{
		GPoint  first, last; // the same point: a buffer's support is never an edge
		Float64 angle = 0;   // of the outward normal
	};

	Edge OfEdge(Int64 dx, Int64 dy) const
	{
		Float64 len = std::hypot(Float64(dx), Float64(dy));
		Float64 nx = Float64(dy) / len, ny = -Float64(dx) / len; // the edge turned clockwise: outward
		GPoint s = MakeGPoint(std::llround(m_Radius * nx), std::llround(m_Radius * ny));
		if (CrossSign(dx, dy, s.X(), s.Y()) >= 0)
			s = Origin(); // a distance below the lattice's resolution: this edge gets no piece
		return Edge{ s, s, std::atan2(ny, nx) };
	}

	void FanChain(const Edge& in, const Edge& out, std::vector<GPoint>& chain) const
	{
		chain.clear();
		const GPoint& s0 = in.last;
		const GPoint& s1 = out.first;
		GPoint o = Origin();
		if (s0 == o || s1 == o || Orient(o, s0, s1) <= 0)
			return; // no fan at this resolution: the corner is left with its chord

		Float64 turn = out.angle - in.angle;
		if (turn <= 0)
			turn += 2.0 * std::numbers::pi; // a convex corner turns the normal counter-clockwise, by less than half a turn
		SizeT nrSteps = m_Quantum > 0 ? SizeT(turn / m_Quantum + 0.5) : 1; // GEOS: (int)(totalAngle / filletAngleQuantum + 0.5)

		chain.push_back(s0);
		for (SizeT i = 1; i < nrSteps; ++i)
		{
			Float64 a = in.angle + turn * Float64(i) / Float64(nrSteps);
			GPoint p = MakeGPoint(std::llround(m_Radius * std::cos(a)), std::llround(m_Radius * std::sin(a)));
			if (Orient(o, chain.back(), p) > 0 && Orient(o, p, s1) > 0)
				chain.push_back(p);
		}
		chain.push_back(s1);
	}

private:
	Float64 m_Radius;
	Float64 m_Quantum;
};

// *****************************************************************************
//	the pieces of one ring
// *****************************************************************************

// The edges of a ring as the engine stores it, shifted by `shift`, each with coverage `coverage`:
// the stored rings have their area on the right of every edge (shells clockwise, holes
// counter-clockwise), so crossing an edge from right to left leaves it, which is weight -coverage
// relative to the stored direction (see Segment and dms_append_rings).
inline void dms_append_ring(std::vector<Segment>& bag, const std::vector<GPoint>& pts, const GPoint& shift, Int32 coverage)
{
	SizeT n = pts.size();
	for (SizeT i = 0; i != n; ++i)
	{
		GPoint a = Add(pts[i], shift), b = Add(pts[i + 1 == n ? 0 : i + 1], shift);
		if (a != b)
			bag.push_back(Segment{ a, b, 0, -coverage });
	}
}

// The ring shifted, in its stored direction (`reversed` false) or the other way round.
inline void dms_shifted_ring(const std::vector<GPoint>& pts, const GPoint& shift, bool reversed, std::vector<GPoint>& out)
{
	out.clear();
	out.reserve(pts.size());
	if (reversed)
		for (auto it = pts.rbegin(); it != pts.rend(); ++it)
			out.push_back(Add(*it, shift));
	else
		for (const auto& p : pts)
			out.push_back(Add(p, shift));
}

// The pieces of one ring whose material lies on its LEFT: per edge e = a -> b the polygon
// b, a, a + first, b + last (counter-clockwise, the support lying on the right of e), and per
// convex corner b the fan b, b + chain[0], ..., b + chain[last]. Each with coverage `coverage`
// (+1 for a sum, -1 for the eroded strip), so weight +coverage along its counter-clockwise run.
template <typename Support>
void dms_append_pieces(std::vector<Segment>& bag, const std::vector<GPoint>& ring, const Support& support, Int32 coverage
	, std::vector<typename Support::Edge>& edges, std::vector<GPoint>& chain)
{
	SizeT n = ring.size();
	if (n < 3)
		return;
	auto next = [n](SizeT i) { return i + 1 == n ? 0 : i + 1; };

	edges.resize(n);
	for (SizeT j = 0; j != n; ++j)
	{
		const GPoint& a = ring[j];
		const GPoint& b = ring[next(j)];
		edges[j] = support.OfEdge(b.X() - a.X(), b.Y() - a.Y());
	}

	auto emit = [&bag, coverage](const GPoint& p, const GPoint& q)
	{
		if (p != q)
			bag.push_back(Segment{ p, q, 0, coverage });
	};
	for (SizeT j = 0; j != n; ++j)
	{
		SizeT j1 = next(j);
		const GPoint& a = ring[j];
		const GPoint& b = ring[j1];
		const GPoint& c = ring[next(j1)];
		const auto& e = edges[j];

		GPoint af = Add(a, e.first), bl = Add(b, e.last);
		emit(b, a);
		emit(a, af);
		emit(af, bl);
		emit(bl, b);

		if (CrossSign(b.X() - a.X(), b.Y() - a.Y(), c.X() - b.X(), c.Y() - b.Y()) <= 0)
			continue; // a reflex corner, or a straight one: the two sides above are the way back through b
		support.FanChain(e, edges[j1], chain);
		if (chain.size() < 2)
			continue;
		emit(b, Add(b, chain.front()));
		for (SizeT i = 1; i != chain.size(); ++i)
			emit(Add(b, chain[i - 1]), Add(b, chain[i]));
		emit(Add(b, chain.back()), b);
	}
}

// *****************************************************************************
//	a convex element: no sweep
// *****************************************************************************

// The shell, reversed to counter-clockwise and shifted, without its collinear vertices, into out;
// true when what is left is strictly convex.
inline bool dms_convex_ccw(const std::vector<GPoint>& cwShell, const GPoint& shift, std::vector<GPoint>& out)
{
	out.clear();
	for (auto it = cwShell.rbegin(); it != cwShell.rend(); ++it)
	{
		out.push_back(Add(*it, shift));
		while (out.size() >= 3 && Orient(out[out.size() - 3], out[out.size() - 2], out.back()) == 0)
			out.erase(out.end() - 2);
	}
	while (out.size() >= 3 && Orient(out[out.size() - 2], out.back(), out.front()) == 0)
		out.pop_back();
	while (out.size() >= 3 && Orient(out.back(), out.front(), out[1]) == 0)
		out.erase(out.begin());
	SizeT n = out.size();
	if (n < 3)
		return false;
	for (SizeT i = 0; i != n; ++i)
		if (Orient(out[i], out[i + 1 == n ? 0 : i + 1], out[i + 2 >= n ? i + 2 - n : i + 2]) <= 0)
			return false;
	return true;
}

// The boundary of the sum of a strictly convex counter-clockwise ring and a convex support: the
// edges shifted by their supports, joined at every corner by the fan's arc. That is the boundary
// of the union of the pieces, which for a convex ring do not overlap.
template <typename Support>
bool dms_convex_sum(const std::vector<GPoint>& ring, const Support& support
	, std::vector<typename Support::Edge>& edges, std::vector<GPoint>& chain, std::vector<GPoint>& out)
{
	SizeT n = ring.size();
	auto next = [n](SizeT i) { return i + 1 == n ? 0 : i + 1; };
	edges.resize(n);
	for (SizeT j = 0; j != n; ++j)
	{
		const GPoint& a = ring[j];
		const GPoint& b = ring[next(j)];
		edges[j] = support.OfEdge(b.X() - a.X(), b.Y() - a.Y());
	}

	out.clear();
	auto push = [&out](const GPoint& p)
	{
		if (out.empty() || out.back() != p)
			out.push_back(p);
	};
	for (SizeT j = 0; j != n; ++j)
	{
		SizeT j1 = next(j);
		const GPoint& b = ring[j1];
		push(Add(ring[j], edges[j].first));
		push(Add(b, edges[j].last));
		support.FanChain(edges[j], edges[j1], chain);
		for (SizeT i = 1; i + 1 < chain.size(); ++i) // its ends are the ends of the two shifted edges
			push(Add(b, chain[i]));
	}
	while (out.size() > 1 && out.back() == out.front())
		out.pop_back();
	return out.size() >= 3;
}

// *****************************************************************************
//	one element
// *****************************************************************************

// The scratch of one thread: its engine, its bag and ring buffers, and the kernel of the last
// element on the lattice of the last frame, which every element with the same kernel and the
// same cell reuses.
template <typename P>
struct DmsMinkowskiWorker
{
	explicit DmsMinkowskiWorker(CharPtr operName)
		: engine(BoolOp::Union, operName)
	{}

	DmsOverlayEngine<P>  engine;
	std::vector<Segment> bag;
	std::vector<GPoint>  ring, chain, outline, kernelScratch;
	std::vector<DmsPolygonSupport::Edge> polygonEdges;
	std::vector<DmsRoundSupport::Edge>   roundEdges;

	const DmsLatticeKernel& KernelFor(const MinkowskiRing& kernelRing, Float64 cell, bool reflect)
	{
		if (cell != m_KernelCell || reflect != m_KernelReflected || kernelRing.size() != m_KernelRing.size()
			|| !std::equal(kernelRing.begin(), kernelRing.end(), m_KernelRing.begin(), [](const DPoint& a, const DPoint& b) { return a.X() == b.X() && a.Y() == b.Y(); }))
		{
			m_Kernel.Build(kernelRing, cell, reflect, kernelScratch);
			m_KernelRing.assign(kernelRing.begin(), kernelRing.end());
			m_KernelCell = cell;
			m_KernelReflected = reflect;
		}
		return m_Kernel;
	}

private:
	DmsLatticeKernel m_Kernel;
	MinkowskiRing    m_KernelRing;
	Float64          m_KernelCell = 0;
	bool             m_KernelReflected = false;
};

// Frames the element on its own extent grown by `grow` world units, and cleans it into the
// engine's rings. False when it cannot be framed or encloses no area: its result stays empty, as
// for every dms_ operator that reads an element it cannot use.
template <typename P, typename R>
bool dms_frame_and_clean(DmsOverlayEngine<P>& engine, const R& geometry, Float64 grow, Float64& cell)
{
	typename DmsOverlayEngine<P>::CoordStats stats;
	stats.Add(geometry);
	if (!stats.usable || !stats.any || !std::isfinite(grow))
		return false;
	DmsFrame frame = DmsFrameFor<P>(stats.minX - grow, stats.minY - grow, stats.maxX + grow, stats.maxY + grow);
	if (!frame.defined())
		return false;
	engine.SetFixedFrame(frame.cell, frame.originX, frame.originY);
	cell = frame.cell;
	if (!engine.SingleRingToRings(geometry))
		engine.CleanToRings(geometry);
	return !engine.CurrRings().empty();
}

// The sum of the engine's rings (shifted by `shift`) and a support: the convex shortcut when the
// element is one convex shell and its outline comes out simple on the lattice, the sweep over all
// pieces otherwise. Stores into res.
template <typename P, typename E, typename Support>
void dms_sum_rings(DmsMinkowskiWorker<P>& w, E&& res, const Support& support, const GPoint& shift
	, std::vector<typename Support::Edge>& edges)
{
	const auto& rings = w.engine.CurrRings();
	if (rings.size() == 1 && rings[0].isShell && dms_convex_ccw(rings[0].pts, shift, w.ring)
		&& dms_convex_sum(w.ring, support, edges, w.chain, w.outline) && w.engine.IsSimpleRing(w.outline))
	{
		w.engine.SetSingleShell(w.outline);
		w.engine.Store(std::forward<E>(res));
		return;
	}

	w.bag.clear();
	for (const auto& ring : rings)
	{
		dms_append_ring(w.bag, ring.pts, shift, +1);         // P itself
		dms_shifted_ring(ring.pts, shift, true, w.ring);     // its material on the left
		dms_append_pieces(w.bag, w.ring, support, +1, edges, w.chain);
	}
	if (w.engine.ThresholdBag(w.bag, 1))
		w.engine.Store(std::forward<E>(res));
}

// The erosion of the engine's rings A by a support given for the reflected kernel and shifted by
// `shift` (the c of the header comment, reflected): A n (A + shift) minus the pieces of the
// complement of A + shift. Stores into res.
template <typename P, typename E, typename Support>
void dms_erode_rings(DmsMinkowskiWorker<P>& w, E&& res, const Support& support, const GPoint& shift
	, std::vector<typename Support::Edge>& edges)
{
	const auto& rings = w.engine.CurrRings();
	bool shifted = shift != Origin();

	w.bag.clear();
	for (const auto& ring : rings)
	{
		dms_append_ring(w.bag, ring.pts, Origin(), +1);      // A
		if (shifted)
			dms_append_ring(w.bag, ring.pts, shift, +1);     // A + shift
		dms_shifted_ring(ring.pts, shift, false, w.ring);    // the complement on the left
		dms_append_pieces(w.bag, w.ring, support, -1, edges, w.chain);
	}
	if (w.engine.ThresholdBag(w.bag, shifted ? 2 : 1))
		w.engine.Store(std::forward<E>(res));
}

// dms_minkowski_sum and dms_minkowski_difference of one element with a convex kernel ring (closed,
// world units) reaching `reach` from the origin.
// An undefined size, or an undefined point in a kernel polygon, leaves the result empty.
template <typename P, typename E, typename R>
void dms_minkowski(DmsMinkowskiWorker<P>& w, E&& res, const R& geometry, const MinkowskiRing& kernelRing, Float64 reach, bool erode)
{
	if (!std::isfinite(reach) || std::any_of(kernelRing.begin(), kernelRing.end(), [](const DPoint& p) { return !std::isfinite(p.X()) || !std::isfinite(p.Y()); }))
		return;
	Float64 cell;
	if (!dms_frame_and_clean(w.engine, geometry, reach, cell))
		return;
	const DmsLatticeKernel& K = w.KernelFor(kernelRing, cell, erode);
	DmsPolygonSupport support{ &K };
	if (erode)
		dms_erode_rings(w, std::forward<E>(res), support, K.c, w.polygonEdges);
	else
		dms_sum_rings(w, std::forward<E>(res), support, K.c, w.polygonEdges);
}

// dms_buffer_multi_polygon of one element. An undefined or infinite distance leaves the result
// empty, distance zero gives the cleaned element.
template <typename P, typename E, typename R>
void dms_buffer(DmsMinkowskiWorker<P>& w, E&& res, const R& geometry, Float64 distance, UInt8 quadrantSegments)
{
	if (!std::isfinite(distance))
		return;
	Float64 cell;
	if (!dms_frame_and_clean(w.engine, geometry, std::fabs(distance), cell))
		return;
	if (distance == 0)
	{
		w.engine.Store(std::forward<E>(res));
		return;
	}
	DmsRoundSupport support(std::fabs(distance) / cell, quadrantSegments);
	if (distance > 0)
		dms_sum_rings(w, std::forward<E>(res), support, Origin(), w.roundEdges);
	else
		dms_erode_rings(w, std::forward<E>(res), support, Origin(), w.roundEdges);
}

// *****************************************************************************
//	the elements of a tile
// *****************************************************************************

// Runs perElement(worker, i, result) for the n elements of a tile, in blocks over the threads of
// the pool, each block with its thread's worker, into a buffer per element; then copies the
// buffers into the tile in element order. The elements of a polygon tile share one value store,
// which two threads cannot write at once. onProgress(nrDone) follows every block, from whichever
// thread ran it, for the caller's progress report.
template <typename P, typename ResTile, typename PerElement, typename OnProgress>
void dms_minkowski_tile(ResTile&& resData, SizeT n, CharPtr operName, PerElement&& perElement, OnProgress&& onProgress)
{
	std::vector<dms_polygon_t<P>> results(n);
	thread_scratch<DmsMinkowskiWorker<P>> workers([operName] { return std::make_unique<DmsMinkowskiWorker<P>>(operName); });
	std::atomic<SizeT> nrDone = 0;

	constexpr SizeT blockSize = 64;
	parallel_for<SizeT>((n + blockSize - 1) / blockSize, [&](SizeT blockNr)
		{
			auto w = workers.local();
			SizeT i = blockNr * blockSize, ie = std::min(n, i + blockSize);
			SizeT nrInBlock = ie - i;
			for (; i != ie; ++i)
				perElement(*w, i, results[i]);
			onProgress(nrDone += nrInBlock);
		}
	);

	for (SizeT i = 0; i != n; ++i)
		if (!results[i].empty())
			dms_store_polygon(resData[i], results[i]);
}

} // namespace dms_overlay

#endif //!defined(__GEO_DMS_MINKOWSKI_H)
