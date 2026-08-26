// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

#if !defined(__GEO_MINKOWSKI_H)
#define __GEO_MINKOWSKI_H

#include "GeoBase.h"

#include "dbg/Diagnostics.h"
#include "geom/Geometry.h"
#include "ptr/SharedStr.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <vector>

// *****************************************************************************
//	Minkowski kernels
// *****************************************************************************
//
// The twelve kernel rings that the bp_polygon_i4HV / bp_split_union_polygon_dXD / ... operator
// families select through their NAME SUFFIX, and that the xx_minkowski_sum / xx_minkowski_difference
// operators select through their `variant` argument (issue #917).
//
// One definition, shared by both, so that the named overload provably reproduces what the
// deprecated suffixed operator produced: SetKernel in BoostPolygon.cpp is a thin adapter over
// MakeMinkowskiKernel below.
//
// Suffix grammar (see the [[Boost polygon functions]] wiki page): _<a><X><y>
//   a = i (inflate) or d (deflate). NOT part of the shape: the direction is carried by the
//       operator (xx_minkowski_sum vs xx_minkowski_difference), since the ring itself is the same.
//   X = degree of rounding: 4, 8, 16, or X (spiked / star-shaped)
//   y = kernel orientation: HV (axis-aligned, square) or D (diagonal, diamond)

enum class MinkowskiKernelShape { k4HV, k4D, k8D, k16D, kXHV, kXD };

// Order-safe DPoint construction: Point<T>'s two-argument constructor takes (first, second), whose
// mapping onto (x, y) flips with DMS_POINT_ROWCOL. Everything below means x and y literally.
inline auto MinkowskiKernelPoint(Float64 x, Float64 y) -> DPoint
{
	DPoint result;
	result.X() = x;
	result.Y() = y;
	return result;
}

// The kernel ring for `shape`, scaled by `size` and centred on the origin, closed (the first point
// is repeated as the last). The arithmetic is the one the twelve SetKernel cases used, unchanged:
// callers that need integer coordinates convert on insertion, exactly as before.
inline auto MakeMinkowskiKernel(MinkowskiKernelShape shape, Float64 size) -> std::vector<DPoint>
{
	std::vector<DPoint> kernel;

	const Float64 c0 = 0;
	const Float64 csqrtHalf = std::numbers::sqrt2 / 2.0;

	switch (shape) {
	case MinkowskiKernelShape::k4HV:
		kernel.reserve(5);
		kernel.push_back(MinkowskiKernelPoint(size, size));
		kernel.push_back(MinkowskiKernelPoint(size, -size));
		kernel.push_back(MinkowskiKernelPoint(-size, -size));
		kernel.push_back(MinkowskiKernelPoint(-size, size));
		break;

	case MinkowskiKernelShape::k4D:
		kernel.reserve(5);
		kernel.push_back(MinkowskiKernelPoint(c0, size));
		kernel.push_back(MinkowskiKernelPoint(size, c0));
		kernel.push_back(MinkowskiKernelPoint(c0, -size));
		kernel.push_back(MinkowskiKernelPoint(-size, c0));
		break;

	case MinkowskiKernelShape::k8D:
		kernel.reserve(9);
		kernel.push_back(MinkowskiKernelPoint(c0, size));
		kernel.push_back(MinkowskiKernelPoint(csqrtHalf * size, csqrtHalf * size));
		kernel.push_back(MinkowskiKernelPoint(size, c0));
		kernel.push_back(MinkowskiKernelPoint(csqrtHalf * size, -csqrtHalf * size));
		kernel.push_back(MinkowskiKernelPoint(c0, -size));
		kernel.push_back(MinkowskiKernelPoint(-csqrtHalf * size, -csqrtHalf * size));
		kernel.push_back(MinkowskiKernelPoint(-size, c0));
		kernel.push_back(MinkowskiKernelPoint(-csqrtHalf * size, csqrtHalf * size));
		break;

	case MinkowskiKernelShape::k16D:
		kernel.reserve(17);
		kernel.push_back(MinkowskiKernelPoint(c0, size));
		kernel.push_back(MinkowskiKernelPoint(0.3826834 * size, 0.9238795 * size));
		kernel.push_back(MinkowskiKernelPoint(csqrtHalf * size, csqrtHalf * size));
		kernel.push_back(MinkowskiKernelPoint(0.9238795 * size, 0.3826834 * size));
		kernel.push_back(MinkowskiKernelPoint(size, c0));
		kernel.push_back(MinkowskiKernelPoint(0.9238795 * size, -0.3826834 * size));
		kernel.push_back(MinkowskiKernelPoint(csqrtHalf * size, -csqrtHalf * size));
		kernel.push_back(MinkowskiKernelPoint(0.3826834 * size, -0.9238795 * size));
		kernel.push_back(MinkowskiKernelPoint(c0, -size));
		kernel.push_back(MinkowskiKernelPoint(-0.3826834 * size, -0.9238795 * size));
		kernel.push_back(MinkowskiKernelPoint(-csqrtHalf * size, -csqrtHalf * size));
		kernel.push_back(MinkowskiKernelPoint(-0.9238795 * size, -0.3826834 * size));
		kernel.push_back(MinkowskiKernelPoint(-size, c0));
		kernel.push_back(MinkowskiKernelPoint(-0.9238795 * size, 0.3826834 * size));
		kernel.push_back(MinkowskiKernelPoint(-csqrtHalf * size, csqrtHalf * size));
		kernel.push_back(MinkowskiKernelPoint(-0.3826834 * size, 0.9238795 * size));
		break;

	case MinkowskiKernelShape::kXHV:
		kernel.reserve(9);
		kernel.push_back(MinkowskiKernelPoint(0.1 * size, 0.1 * size));
		kernel.push_back(MinkowskiKernelPoint(+size, c0));
		kernel.push_back(MinkowskiKernelPoint(0.1 * size, -0.1 * size));
		kernel.push_back(MinkowskiKernelPoint(c0, -size));
		kernel.push_back(MinkowskiKernelPoint(-0.1 * size, -0.1 * size));
		kernel.push_back(MinkowskiKernelPoint(-size, c0));
		kernel.push_back(MinkowskiKernelPoint(-0.1 * size, 0.1 * size));
		kernel.push_back(MinkowskiKernelPoint(c0, +size));
		break;

	case MinkowskiKernelShape::kXD:
		kernel.reserve(9);
		kernel.push_back(MinkowskiKernelPoint(size, size));
		kernel.push_back(MinkowskiKernelPoint(0.1 * size, c0));
		kernel.push_back(MinkowskiKernelPoint(size, -size));
		kernel.push_back(MinkowskiKernelPoint(c0, -0.1 * size));
		kernel.push_back(MinkowskiKernelPoint(-size, -size));
		kernel.push_back(MinkowskiKernelPoint(-0.1 * size, c0));
		kernel.push_back(MinkowskiKernelPoint(-size, size));
		kernel.push_back(MinkowskiKernelPoint(c0, 0.1 * size));
		break;
	}
	kernel.push_back(kernel.front());
	return kernel;
}

// The spellings accepted by the `variant` argument, listed in the order a user is most likely to
// look for them. The bare form is canonical; the i-/d-prefixed forms are accepted so that a
// configuration migrating away from bp_polygon_i4HV can keep its literal.
inline auto MinkowskiKernelShapeNames() -> CharPtr { return "'4HV', '4D', '8D', '16D', 'XHV' or 'XD'"; }

// Parse a `variant` argument. `wantErode` says which operator is asking: xx_minkowski_difference
// erodes and accepts the 'd' prefix, xx_minkowski_sum dilates and accepts 'i'. A prefix that
// disagrees with the operator is a configuration error naming `siblingOperName`, since silently
// honouring it would compute the opposite of what the operator name promises.
inline auto ParseMinkowskiKernelShape(CharPtr spec, bool wantErode, CharPtr operName, CharPtr siblingOperName) -> MinkowskiKernelShape
{
	if (!spec || !*spec)
		throwErrorF(operName, "empty kernel variant; expected {}", MinkowskiKernelShapeNames());

	CharPtr shapeSpec = spec;
	if (*shapeSpec == 'i' || *shapeSpec == 'I' || *shapeSpec == 'd' || *shapeSpec == 'D')
	{
		// 'D' alone is not a prefix but the start of no accepted shape, so a leading D/d is only
		// ambiguous for the empty remainder, which the length check below rejects anyway.
		bool specErodes = (*shapeSpec == 'd' || *shapeSpec == 'D');
		if (shapeSpec[1] != '\0')
		{
			if (specErodes != wantErode)
				throwErrorF(operName, "kernel variant '{}' requests {}; use {} for that, or drop the '{}' prefix"
					, spec
					, specErodes ? "deflation" : "inflation"
					, siblingOperName
					, specErodes ? "d" : "i"
				);
			++shapeSpec;
		}
	}

	if (!stricmp(shapeSpec, "4HV")) return MinkowskiKernelShape::k4HV;
	if (!stricmp(shapeSpec, "4D" )) return MinkowskiKernelShape::k4D;
	if (!stricmp(shapeSpec, "8D" )) return MinkowskiKernelShape::k8D;
	if (!stricmp(shapeSpec, "16D")) return MinkowskiKernelShape::k16D;
	if (!stricmp(shapeSpec, "XHV")) return MinkowskiKernelShape::kXHV;
	if (!stricmp(shapeSpec, "XD" )) return MinkowskiKernelShape::kXD;

	throwErrorF(operName, "unknown kernel variant '{}'; expected {}", spec, MinkowskiKernelShapeNames());
}

// *****************************************************************************
//	Minkowski sum as a union of convex cells
// *****************************************************************************
//
// A (+) K is computed here as a union of CONVEX cells. That shape of answer is what makes it
// expressible in libraries with no Minkowski primitive of their own (boost.geometry, GEOS):
// boost.polygon's boundary convolution accumulates oriented quads under a WINDING rule, and an
// OR-union of those same quads is a strictly smaller set -- it drops, for instance, the region
// where a kernel bridges a notch narrower than itself.
//
// The identity used, for a CONVEX part P of the kernel and any vertex c of P:
//
//     A (+) P  =  (A + c)  U  { conv(e (+) P) : e an edge of the boundary of A }
//
// Proof: P-c is convex and contains the origin, so for x = a+k outside A the segment [a, x] lies
// in a + (P-c) and leaves A at some a' = a + t*k; the remaining (1-t)*k is still in P-c by
// convexity, so x = a' + (1-t)*k with a' on the boundary. Every term above is convex, or is A
// itself, so an OR-union of them is exact rather than an approximation.
//
// The kernel is therefore split into convex parts first. The twelve named kernels are star-shaped
// about the origin (the X-shaped two are not convex), and an arbitrary user kernel is ear-clipped.
//
// Rings are CLOSED throughout this section: the first point is repeated as the last, as
// MakeMinkowskiKernel produces them and as the DMS polygon representation stores them.

using MinkowskiRing = std::vector<DPoint>;

inline auto MinkowskiCross(DPoint o, DPoint a, DPoint b) -> Float64
{
	return (a.X() - o.X()) * (b.Y() - o.Y()) - (a.Y() - o.Y()) * (b.X() - o.X());
}

// Twice the signed area of a closed ring; positive for counter-clockwise.
inline auto MinkowskiSignedArea2(const MinkowskiRing& ring) -> Float64
{
	Float64 result = 0;
	if (ring.size() < 3)
		return result;
	for (SizeT i = 0, n = ring.size() - 1; i != n; ++i)
		result += ring[i].X() * ring[i + 1].Y() - ring[i + 1].X() * ring[i].Y();
	return result;
}

// Monotone chain. Returns a closed counter-clockwise ring, or an empty one when the input is
// degenerate (fewer than 3 distinct, non-collinear points).
inline auto MinkowskiConvexHull(std::vector<DPoint> points) -> MinkowskiRing
{
	std::sort(points.begin(), points.end(), [](DPoint a, DPoint b)
		{ return a.X() != b.X() ? a.X() < b.X() : a.Y() < b.Y(); });
	points.erase(std::unique(points.begin(), points.end()
		, [](DPoint a, DPoint b) { return a.X() == b.X() && a.Y() == b.Y(); }), points.end());

	MinkowskiRing result;
	if (points.size() < 3)
		return result;

	result.resize(2 * points.size());
	SizeT k = 0;
	for (SizeT i = 0; i != points.size(); ++i) // lower hull
	{
		while (k >= 2 && MinkowskiCross(result[k - 2], result[k - 1], points[i]) <= 0)
			--k;
		result[k++] = points[i];
	}
	for (SizeT i = points.size() - 1, lower = k + 1; i != 0; --i) // upper hull
	{
		while (k >= lower && MinkowskiCross(result[k - 2], result[k - 1], points[i - 1]) <= 0)
			--k;
		result[k++] = points[i - 1];
	}
	result.resize(k); // k >= 3 and result[k-1] == result[0]: the ring is already closed
	if (result.size() < 4)
		result.clear();
	return result;
}

inline bool MinkowskiPointInTriangle(DPoint p, DPoint a, DPoint b, DPoint c)
{
	// a, b, c counter-clockwise; boundary counts as inside, which is what keeps ear clipping from
	// cutting an ear across a vertex that merely touches the diagonal.
	return MinkowskiCross(a, b, p) >= 0 && MinkowskiCross(b, c, p) >= 0 && MinkowskiCross(c, a, p) >= 0;
}

// Split a closed ring into convex parts. Returns the ring itself when it is already convex, a fan
// of triangles otherwise, and an EMPTY vector when the ring is degenerate or cannot be clipped
// (self-intersecting) -- callers report that as a configuration error rather than guessing.
inline auto MinkowskiConvexParts(const MinkowskiRing& closedRing) -> std::vector<MinkowskiRing>
{
	std::vector<MinkowskiRing> result;

	std::vector<DPoint> v;
	v.reserve(closedRing.size());
	for (auto p : closedRing) // drop the closing duplicate and any repeated point
		if (v.empty() || p.X() != v.back().X() || p.Y() != v.back().Y())
			v.push_back(p);
	if (v.size() > 1 && v.front().X() == v.back().X() && v.front().Y() == v.back().Y())
		v.pop_back();
	if (v.size() < 3)
		return result;

	auto closeRing = [](MinkowskiRing r) { r.push_back(r.front()); return r; };

	MinkowskiRing asClosed(v.begin(), v.end());
	asClosed.push_back(asClosed.front());
	if (MinkowskiSignedArea2(asClosed) < 0)
		std::reverse(v.begin(), v.end()); // ear clipping below assumes counter-clockwise

	bool isConvex = true;
	for (SizeT i = 0, n = v.size(); i != n && isConvex; ++i)
		if (MinkowskiCross(v[(i + n - 1) % n], v[i], v[(i + 1) % n]) < 0)
			isConvex = false;
	if (isConvex)
	{
		result.push_back(closeRing(MinkowskiRing(v.begin(), v.end())));
		return result;
	}

	while (v.size() > 3)
	{
		bool clipped = false;
		for (SizeT i = 0, n = v.size(); i != n; ++i)
		{
			auto prev = v[(i + n - 1) % n], curr = v[i], next = v[(i + 1) % n];
			if (MinkowskiCross(prev, curr, next) <= 0)
				continue; // reflex or collinear: not an ear

			bool isEar = true;
			for (SizeT j = 0; j != n && isEar; ++j)
			{
				if (j == i || j == (i + n - 1) % n || j == (i + 1) % n)
					continue;
				if (MinkowskiPointInTriangle(v[j], prev, curr, next))
					isEar = false;
			}
			if (!isEar)
				continue;

			result.push_back(closeRing(MinkowskiRing{ prev, curr, next }));
			v.erase(v.begin() + i);
			clipped = true;
			break;
		}
		if (!clipped)
			return {}; // no ear in a polygon with more than 3 vertices: the ring is not simple
	}
	result.push_back(closeRing(MinkowskiRing{ v[0], v[1], v[2] }));
	return result;
}

// conv(segment(p, q) (+) convexPart), the cell swept by one convex kernel part along one edge.
inline auto MinkowskiEdgeCell(DPoint p, DPoint q, const MinkowskiRing& convexPart, std::vector<DPoint>& scratch) -> MinkowskiRing
{
	scratch.clear();
	scratch.reserve(2 * convexPart.size());
	for (SizeT i = 0, n = convexPart.size() - 1; i != n; ++i) // skip the closing duplicate
	{
		scratch.push_back(MinkowskiKernelPoint(p.X() + convexPart[i].X(), p.Y() + convexPart[i].Y()));
		scratch.push_back(MinkowskiKernelPoint(q.X() + convexPart[i].X(), q.Y() + convexPart[i].Y()));
	}
	return MinkowskiConvexHull(scratch);
}

// -K, the kernel reflected through the origin. Point reflection is a rotation by pi, so the
// winding order survives and the ring stays closed.
inline auto MinkowskiReflect(const MinkowskiRing& ring) -> MinkowskiRing
{
	MinkowskiRing result;
	result.reserve(ring.size());
	for (auto p : ring)
		result.push_back(MinkowskiKernelPoint(-p.X(), -p.Y()));
	return result;
}

// The largest coordinate magnitude in the kernel: how far a sum can reach beyond its input, and
// hence how much room the erosion path must leave around the bounding box it inverts within.
inline auto MinkowskiReach(const MinkowskiRing& ring) -> Float64
{
	Float64 result = 0;
	for (auto p : ring)
	{
		result = std::max(result, std::abs(p.X()));
		result = std::max(result, std::abs(p.Y()));
	}
	return result;
}

// A kernel worked out once per operator call: its convex parts and its reach. `parts` is empty
// only for a degenerate kernel, which every caller reports rather than silently treating as a
// no-op, since an empty kernel would make the whole result empty.
struct PreparedMinkowskiKernel
{
	std::vector<MinkowskiRing> parts;
	Float64 reach = 0;
	bool empty() const { return parts.empty(); }
};

inline auto PrepareMinkowskiKernel(const MinkowskiRing& ring) -> PreparedMinkowskiKernel
{
	PreparedMinkowskiKernel result;
	result.parts = MinkowskiConvexParts(ring);
	result.reach = MinkowskiReach(ring);
	return result;
}

#endif //!defined(__GEO_MINKOWSKI_H)
