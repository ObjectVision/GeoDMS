// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

#if !defined(__GEO_DMS_CONVEX_PARTITION_H)
#define __GEO_DMS_CONVEX_PARTITION_H

/*
 *  dms_split_convex_polygon (issue #1300): the rings of one element, on the lattice of the dms_
 *  sweep, split into strictly convex parts whose union is the element and which meet along whole
 *  shared edges. Every vertex of a part is a vertex of the element, so nothing is rounded.
 */

// ==== The algorithm =====================================================================
//
// 1. The rings, as the engine stores them (the material on the right of every edge), are turned
//    round so that the material lies on the left, and a vertex between two collinear edges is
//    left out: it would make a straight angle in whichever part it ended up in.
// 2. Where rings touch, at a point that is a vertex of more than one ring (a hole touching its
//    shell, two holes touching each other), the ring links are redone so that every vertex record
//    there bounds one wedge of material: an arriving edge continues along the first leaving edge
//    clockwise from the way back. The boundary is then weakly simple, and each record at such a
//    point is moved, infinitesimally, into its own wedge (step 3), which makes it simple.
// 3. Every predicate below is exact and works on a symbolically perturbed copy of the vertices,
//    so that no three are collinear and no two coincide or share an x: first the move into the
//    wedge of step 2 (only at touching points), then Simulation of Simplicity (Edelsbrunner and
//    Muecke 1990), each vertex i moved by (e^(2^(2i)), e^(2^(2i+1))) for an e infinitely smaller
//    than the first move. The algorithm of steps 4 and 5 then runs as in general position.
// 4. A sweep in the lexicographic order of the perturbed vertices splits the material into
//    x-monotone pieces (de Berg et al., Computational Geometry, section 3.2, with x and y in the
//    roles of y and x): a diagonal at every split and merge vertex.
// 5. Each monotone piece is triangulated with the stack algorithm of section 3.3.
// 6. A triangle that has no area in the real coordinates, which the perturbation can produce
//    where three vertices are collinear, is removed by flipping its long edge, which is always a
//    diagonal: a vertex never lies inside an edge of the input, since the sweep that cleaned it
//    noded every edge.
// 7. Hertel and Mehlhorn: every diagonal whose removal leaves both its ends strictly convex in the
//    real coordinates is removed. At most four times the minimum number of convex parts that
//    diagonals between vertices can give; a rectangle stays one part, an L becomes two.
//
// A single ring that is already strictly convex is returned as it is, without any of this.
//
// dms_split_convex_polygon_keilsnoeyink (#1305, m_UseMinimum): a shell without a hole that touches
// no other ring is taken apart from the other rings, triangulated on its own by steps 4 to 6, and
// split into the minimum number of strictly convex parts by MinConvexPartition below; only when it
// exceeds that class's caps does it get step 7. The other rings, the shells with holes and the rings
// that touch, go through steps 2 to 7 together; with holes the minimum is NP-hard. The minimum costs
// 4 times the time of Hertel and Mehlhorn on building outlines and 29 times on CBS buurten, so it is
// a separate operator and dms_split_convex_polygon keeps step 7 alone.

#include <algorithm>
#include <array>
#include <cmath>
#include <set>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

namespace dms_overlay
{

// ==== #1305: the minimum for a ring without holes =======================================
//
// The minimum number of strictly convex parts of a simple ring by diagonals, with the dynamic
// programme of Keil (1985) in the form that Keil and Snoeyink (2002) bound:
//
// - Only chords with a reflex end are used. In a partition from which no diagonal can be removed,
//   every diagonal has a reflex end, and every part can be triangulated by a fan from one of its
//   reflex corners; so the chords of such fans and the diagonals together have reflex ends.
// - The chord (i, j), i < j, closes the sub-polygon Q(i, j) of the vertices i..j. It is the
//   triangle (i, k, j) on the chord, with Q(i, k) and Q(k, j) beside it, and the part on the chord
//   is that triangle, merged across (i, k) and across (k, j) with the parts on those chords where
//   the corners stay strictly convex. The apex k is a vertex that both ends see, or is next to one.
// - Per chord only the partitions of Q(i, j) with the fewest parts are kept: a merge across the
//   chord later saves one part, never more, so one part more is never worth a better shape. Of
//   those, only the ones whose part on the chord is narrowest at a reflex end of the chord are
//   kept: a narrower part merges wherever a wider one does. At a convex end every merge stays
//   convex, so there the shape does not matter, and a chord with one reflex end keeps one shape.
//
// The diagonals with a reflex end are found by triangular expansion (Bungiu, Hemmer, Hershberger,
// Huang and Kroeller 2014) over a triangulation of the ring: from each reflex vertex the triangles
// are entered through the edges that the view still passes, and the view narrows to the vertices it
// passes. A vertex is seen only strictly inside the view, so a diagonal never runs through a vertex.
//
// The root is the ring itself, closed by its edge from the last vertex to the first, the first
// being the lexicographically smallest vertex: the result does not depend on where the ring starts,
// and ties are broken by the order of the vertices. Every test is exact. A ring is declined, and
// left to Hertel and Mehlhorn, when r * n exceeds m_MaxPairs or the work counted on the way
// (triangles entered, apices tried, shapes combined) exceeds m_MaxWork.

class MinConvexPartition
{
public:
	using Ring  = std::vector<GPoint>;
	using Rings = std::vector<Ring>;
	using Tri   = std::array<UInt32, 3>;

	// The caps, from the CBS buurten of 2025 (14,638 rings without holes, median n 93 and r 44, the
	// largest n 30,426): a unit of work took about 9 ns, so a ring costs at most about 0.1 s, and the
	// rings within both caps (14,061) hold 70% of what the minimum saves over Hertel-Mehlhorn there.
	// r * n bounds the chord table (4 bytes each) before any work is done.
	double m_MaxPairs = 2.0e6;
	double m_MaxWork  = 1.0e7;

	// pts[0..n): counter-clockwise (the material on the left), simple, without straight vertices.
	// tris: a triangulation of it, each triangle counter-clockwise, as indices into pts. Appends the
	// parts, each counter-clockwise and strictly convex, and returns true; returns false, appending
	// nothing, when the ring exceeds a cap.
	bool Run(const GPoint* pts, UInt32 n, const std::vector<Tri>& tris, Rings& parts)
	{
		UInt32 i0 = 0;
		for (UInt32 i = 1; i != n; ++i)
			if (PosLess(pts[i], pts[i0]))
				i0 = i;
		m_N = n;
		m_P.resize(n);
		Int64 x0 = pts[0].X(), x1 = x0, y0 = pts[0].Y(), y1 = y0;
		for (UInt32 i = 0; i != n; ++i)
		{
			m_P[i] = pts[(i0 + i) % n];
			x0 = std::min(x0, m_P[i].X()); x1 = std::max(x1, m_P[i].X());
			y0 = std::min(y0, m_P[i].Y()); y1 = std::max(y1, m_P[i].Y());
		}
		m_Small = UInt64(x1 - x0) < (UInt64(1) << 31) && UInt64(y1 - y0) < (UInt64(1) << 31);

		m_IsReflex.assign(n, 0);
		m_RIdx.assign(n, NONE);
		m_Reflex.clear();
		for (UInt32 i = 0; i != n; ++i)
			if (Orient(Prev(i), i, Next(i)) < 0)
			{
				m_IsReflex[i] = 1;
				m_RIdx[i] = UInt32(m_Reflex.size());
				m_Reflex.push_back(i);
			}
		if (m_Reflex.empty())
		{
			parts.emplace_back(m_P.begin(), m_P.end());
			return true;
		}
		if (double(m_Reflex.size()) * n > m_MaxPairs)
			return false;
		m_Work = 0;
		LoadTriangles(tris, i0);
		if (!FindChords() || !Solve())
			return false;
		Emit(parts);
		return true;
	}

private:
	static constexpr UInt32 NONE = UInt32(-1);
	static constexpr UInt32 EDGE = UInt32(-2);

	struct State { UInt32 a, b, k, sA, sB; bool mA, mB; }; // a, b: the neighbours of i and j in the part on the chord
	struct Chord { UInt32 i, j, w = 0, first = 0, count = 0; };
	struct Partner { UInt32 v, chord; };
	struct View { UInt32 t, e, r, l; }; // entering across edge e of triangle t, seeing between the rays to r and to l

	static Int128 Cross(const GPoint& o, const GPoint& a, const GPoint& b)
	{
		return MulFull(a.X() - o.X(), b.Y() - o.Y()) - MulFull(a.Y() - o.Y(), b.X() - o.X());
	}
	static bool PosLess(const GPoint& a, const GPoint& b) { return a.X() < b.X() || (a.X() == b.X() && a.Y() < b.Y()); }
	static UInt64 Key(UInt32 a, UInt32 b) { return (UInt64(a) << 32) | b; }
	int Orient(UInt32 a, UInt32 b, UInt32 c) const
	{
		const GPoint& A = m_P[a];
		const GPoint& B = m_P[b];
		const GPoint& C = m_P[c];
		if (m_Small) // every difference below 2^31, so both products below 2^62
		{
			Int64 v = (B.X() - A.X()) * (C.Y() - A.Y()) - (B.Y() - A.Y()) * (C.X() - A.X());
			return (v > 0) - (v < 0);
		}
		return Sign(Cross(A, B, C));
	}
	UInt32 Prev(UInt32 i) const { return i ? i - 1 : m_N - 1; }
	UInt32 Next(UInt32 i) const { return i + 1 == m_N ? 0 : i + 1; }

	// ---- the diagonals with a reflex end ----

	void LoadTriangles(const std::vector<Tri>& tris, UInt32 i0)
	{
		UInt32 n = m_N, nt = UInt32(tris.size());
		m_T.resize(nt);
		for (UInt32 t = 0; t != nt; ++t)
			for (UInt32 k = 0; k != 3; ++k)
				m_T[t][k] = (tris[t][k] + n - i0) % n;
		// the neighbour across every edge, NONE at the ring
		m_EdgeKeys.clear();
		for (UInt32 t = 0; t != nt; ++t)
			for (UInt32 k = 0; k != 3; ++k)
				m_EdgeKeys.emplace_back(Key(m_T[t][k], m_T[t][(k + 1) % 3]), t);
		std::sort(m_EdgeKeys.begin(), m_EdgeKeys.end());
		m_Nb.assign(nt, Tri{ NONE, NONE, NONE });
		for (UInt32 t = 0; t != nt; ++t)
			for (UInt32 k = 0; k != 3; ++k)
			{
				UInt64 twin = Key(m_T[t][(k + 1) % 3], m_T[t][k]);
				auto it = std::lower_bound(m_EdgeKeys.begin(), m_EdgeKeys.end(), std::pair<UInt64, UInt32>(twin, 0));
				if (it != m_EdgeKeys.end() && it->first == twin)
					m_Nb[t][k] = it->second;
			}
		// the triangles at every vertex
		m_VTriStart.assign(n + 1, 0);
		for (const auto& T : m_T)
			for (UInt32 v : T)
				++m_VTriStart[v + 1];
		for (UInt32 v = 0; v != n; ++v)
			m_VTriStart[v + 1] += m_VTriStart[v];
		m_VTri.resize(m_VTriStart[n]);
		m_Fill.assign(m_VTriStart.begin(), m_VTriStart.end() - 1);
		for (UInt32 t = 0; t != nt; ++t)
			for (UInt32 v : m_T[t])
				m_VTri[m_Fill[v]++] = t;
	}

	// every diagonal with a reflex end, ordered by the length of its chain, and per vertex its partners
	bool FindChords()
	{
		UInt32 n = m_N;
		m_Chords.clear();
		m_Seen.assign(n, NONE);
		for (UInt32 u : m_Reflex)
		{
			auto see = [&](UInt32 v)
			{
				if (m_Seen[v] == u)
					return;
				m_Seen[v] = u;
				if (v == Prev(u) || v == Next(u) || (m_IsReflex[v] && v < u)) // an edge, or found from v
					return;
				m_Chords.push_back({ std::min(u, v), std::max(u, v) });
			};
			m_Views.clear();
			for (UInt32 q = m_VTriStart[u]; q != m_VTriStart[u + 1]; ++q)
			{
				UInt32 t = m_VTri[q];
				UInt32 e = m_T[t][0] == u ? 0 : m_T[t][1] == u ? 1 : 2;
				UInt32 a = m_T[t][(e + 1) % 3], b = m_T[t][(e + 2) % 3];
				see(a);
				see(b);
				UInt32 eab = (e + 1) % 3;
				if (m_Nb[t][eab] != NONE)
					m_Views.push_back({ t, eab, a, b });
			}
			// the edge crossed runs from the right of the view to its left
			while (!m_Views.empty())
			{
				if (++m_Work > m_MaxWork)
					return false;
				View w = m_Views.back();
				m_Views.pop_back();
				UInt32 x = m_T[w.t][w.e], y = m_T[w.t][(w.e + 1) % 3];
				UInt32 s = m_Nb[w.t][w.e];
				UInt32 f = m_T[s][0] == y ? 0 : m_T[s][1] == y ? 1 : 2;
				MG_CHECK2(m_T[s][(f + 1) % 3] == x, "dms convex split: the triangles of a ring do not match");
				UInt32 c = m_T[s][(f + 2) % 3];
				UInt32 exc = (f + 1) % 3, ecy = (f + 2) % 3; // x -> c on the right, c -> y on the left
				bool rightOk = Orient(u, w.r, c) > 0, leftOk = Orient(u, w.l, c) < 0;
				if (rightOk && leftOk)
				{
					see(c);
					if (m_Nb[s][exc] != NONE) m_Views.push_back({ s, exc, w.r, c });
					if (m_Nb[s][ecy] != NONE) m_Views.push_back({ s, ecy, c, w.l });
				}
				else if (!rightOk)
				{
					if (m_Nb[s][ecy] != NONE) m_Views.push_back({ s, ecy, w.r, w.l });
				}
				else if (m_Nb[s][exc] != NONE)
					m_Views.push_back({ s, exc, w.r, w.l });
			}
		}
		std::sort(m_Chords.begin(), m_Chords.end(), [](const Chord& x, const Chord& y)
			{
				return x.j - x.i < y.j - y.i || (x.j - x.i == y.j - y.i && x.i < y.i);
			});

		m_PartStart.assign(n + 1, 0);
		for (const auto& c : m_Chords)
		{
			++m_PartStart[c.i + 1];
			++m_PartStart[c.j + 1];
		}
		for (UInt32 v = 0; v != n; ++v)
			m_PartStart[v + 1] += m_PartStart[v];
		m_Part.resize(m_PartStart[n]);
		m_Fill.assign(m_PartStart.begin(), m_PartStart.end() - 1);
		for (UInt32 c = 0, nc = UInt32(m_Chords.size()); c != nc; ++c)
		{
			m_Part[m_Fill[m_Chords[c].i]++] = { m_Chords[c].j, c };
			m_Part[m_Fill[m_Chords[c].j]++] = { m_Chords[c].i, c };
		}
		for (UInt32 v = 0; v != n; ++v)
			std::sort(m_Part.begin() + m_PartStart[v], m_Part.begin() + m_PartStart[v + 1], [](const Partner& p, const Partner& q) { return p.v < q.v; });

		// the apices that the dynamic programme will walk: per chord (i, j) the shorter of the partners
		// of i after i + 1 and those of j before j - 1, both counted from the place of the other end in
		// the sorted list. A ring above the cap is declined now rather than halfway.
		m_LenA.resize(m_Chords.size());
		m_LenB.resize(m_Chords.size());
		for (UInt32 v = 0; v != n; ++v)
		{
			UInt32 s = m_PartStart[v], e = m_PartStart[v + 1];
			UInt32 afterNext = s, beforePrev = s;
			while (afterNext != e && m_Part[afterNext].v <= v + 1) ++afterNext;
			while (beforePrev != e && m_Part[beforePrev].v + 1 < v) ++beforePrev;
			for (UInt32 q = s; q != e; ++q)
			{
				if (m_Part[q].v > v)
					m_LenA[m_Part[q].chord] = q - afterNext;
				else
					m_LenB[m_Part[q].chord] = beforePrev - q - 1;
			}
		}
		double walk = 0;
		for (UInt32 c = 0, nc = UInt32(m_Chords.size()); c != nc; ++c)
			walk += std::min(m_LenA[c], m_LenB[c]) + 1;
		if (m_Work + walk > m_MaxWork)
			return false;

		// the chord of a reflex vertex and any other, in a table of r rows of n
		m_ChordOf.assign(m_Reflex.size() * SizeT(n), NONE);
		for (UInt32 c = 0, nc = UInt32(m_Chords.size()); c != nc; ++c)
		{
			UInt32 i = m_Chords[c].i, j = m_Chords[c].j;
			if (m_IsReflex[i]) m_ChordOf[SizeT(m_RIdx[i]) * n + j] = c;
			if (m_IsReflex[j]) m_ChordOf[SizeT(m_RIdx[j]) * n + i] = c;
		}
		return true;
	}

	// the chord (u, v), u < v: EDGE for an edge of the ring, NONE when it is not a diagonal with a reflex end
	UInt32 ChordId(UInt32 u, UInt32 v) const
	{
		if (v == u + 1)
			return EDGE;
		if (m_IsReflex[u]) return m_ChordOf[SizeT(m_RIdx[u]) * m_N + v];
		if (m_IsReflex[v]) return m_ChordOf[SizeT(m_RIdx[v]) * m_N + u];
		return NONE;
	}

	// the partners of v strictly between lo and hi
	std::pair<UInt32, UInt32> PartnersBetween(UInt32 v, UInt32 lo, UInt32 hi) const
	{
		auto b = m_Part.begin() + m_PartStart[v], e = m_Part.begin() + m_PartStart[v + 1];
		auto f = std::upper_bound(b, e, lo, [](UInt32 x, const Partner& p) { return x < p.v; });
		auto l = std::lower_bound(f, e, hi, [](const Partner& p, UInt32 x) { return p.v < x; });
		return { UInt32(f - m_Part.begin()), UInt32(l - m_Part.begin()) };
	}

	// ---- the dynamic programme ----

	// a1 narrower than a2 as the neighbour of i in the part on (i, j): its edge closer to the chord
	bool NarrowerAtI(UInt32 i, UInt32 a1, UInt32 a2) const { return Orient(i, a2, a1) > 0; }
	bool NarrowerAtJ(UInt32 j, UInt32 b1, UInt32 b2) const { return Orient(j, b2, b1) < 0; }

	// The part on (i, j) can always be fanned from a reflex end: with i reflex, its triangle on the
	// chord has as apex the vertex before j in the part, and then the part does not continue across
	// (k, j); with i convex and j reflex, mirrored. Only the root, whose ends may both be convex,
	// tries both merges.
	enum Merges : UInt8 { BOTH, ONLY_A, ONLY_B };

	void Consider(UInt32 i, UInt32 j, UInt32 k, UInt32 cA, UInt32 cB, bool needA, bool needB, Merges merges)
	{
		UInt32 wA = cA == EDGE ? 0 : m_Chords[cA].w;
		UInt32 wB = cB == EDGE ? 0 : m_Chords[cB].w;
		UInt32 w0 = wA + wB + 1;
		if (m_BestW != NONE && w0 > m_BestW + 2)
			return;
		if (Orient(i, k, j) <= 0)
			return;
		UInt32 firstA = cA == EDGE ? NONE : m_Chords[cA].first, nA = cA == EDGE ? 0 : m_Chords[cA].count;
		UInt32 firstB = cB == EDGE ? NONE : m_Chords[cB].first, nB = cB == EDGE ? 0 : m_Chords[cB].count;
		UInt32 sA0 = firstA, sB0 = firstB; // the shapes kept when the part does not continue
		if (merges == ONLY_A) nB = 0;
		if (merges == ONLY_B) nA = 0;
		m_Work += 1 + nA + nB + (merges == BOTH ? double(nA) * nB : 0.0);

		Add(w0, { k, k, k, sA0, sB0, false, false });

		// merged across (i, k) only: the narrowest neighbour of i among the shapes that stay convex at i and k
		UInt32 best = NONE;
		for (UInt32 s = firstA; s != firstA + nA; ++s)
		{
			const State& sa = m_States[s];
			if (Orient(j, i, sa.a) > 0 && Orient(sa.b, k, j) > 0
				&& (best == NONE || (needA && NarrowerAtI(i, sa.a, m_States[best].a))))
				best = s;
		}
		if (best != NONE)
			Add(w0 - 1, { m_States[best].a, k, k, best, sB0, true, false });

		// merged across (k, j) only
		best = NONE;
		for (UInt32 s = firstB; s != firstB + nB; ++s)
		{
			const State& sb = m_States[s];
			if (Orient(i, k, sb.a) > 0 && Orient(sb.b, j, i) > 0
				&& (best == NONE || (needB && NarrowerAtJ(j, sb.b, m_States[best].b))))
				best = s;
		}
		if (best != NONE)
			Add(w0 - 1, { k, m_States[best].b, k, sA0, best, false, true });
		if (merges != BOTH)
			return;

		// merged across both: per shape of A, the narrowest shape of B that keeps k convex
		for (UInt32 s = firstA; s != firstA + nA; ++s)
		{
			const State& sa = m_States[s];
			if (Orient(j, i, sa.a) <= 0)
				continue;
			UInt32 bestB = NONE;
			for (UInt32 t = firstB; t != firstB + nB; ++t)
			{
				const State& sb = m_States[t];
				if (Orient(sb.b, j, i) > 0 && Orient(sa.b, k, sb.a) > 0
					&& (bestB == NONE || (needB && NarrowerAtJ(j, sb.b, m_States[bestB].b))))
					bestB = t;
			}
			if (bestB != NONE)
				Add(w0 - 2, { sa.a, m_States[bestB].b, k, s, bestB, true, true });
		}
	}

	void Add(UInt32 w, const State& s)
	{
		if (m_BestW == NONE || w < m_BestW)
		{
			m_BestW = w;
			m_Cand.clear();
		}
		if (w == m_BestW)
			m_Cand.push_back(s);
	}

	// the partitions of Q(i, j) with the fewest parts, and of those the narrowest at a reflex end
	bool SolveChord(Chord& ch, bool needA, bool needB, bool isRoot = false)
	{
		UInt32 i = ch.i, j = ch.j;
		m_BestW = NONE;
		m_Cand.clear();

		// the apices, in increasing order: next to i or seen from it, and next to j or seen from it.
		// The shorter of the two lists is walked, and the other chord looked up in the table.
		auto [ai, ae] = PartnersBetween(i, i + 1, j);
		auto [bi, be] = PartnersBetween(j, i, j - 1);
		Merges merges = isRoot ? BOTH : m_IsReflex[i] ? ONLY_A : ONLY_B;
		auto tryApex = [&](UInt32 k, UInt32 cA, UInt32 cB)
		{
			if (cA != NONE && cB != NONE)
				Consider(i, j, k, cA, cB, needA, needB, merges);
		};
		UInt32 nWalk;
		if (ae - ai <= be - bi)
		{
			nWalk = ae - ai + 1;
			tryApex(i + 1, EDGE, ChordId(i + 1, j));
			for (UInt32 p = ai; p != ae; ++p)
				tryApex(m_Part[p].v, m_Part[p].chord, ChordId(m_Part[p].v, j));
		}
		else
		{
			nWalk = be - bi + 1;
			for (UInt32 q = bi; q != be; ++q)
				tryApex(m_Part[q].v, ChordId(i, m_Part[q].v), m_Part[q].chord);
			tryApex(j - 1, ChordId(i, j - 1), EDGE);
		}
		m_Work += nWalk;
		if (m_Cand.empty() || m_Work > m_MaxWork)
			return false;

		ch.w = m_BestW;
		ch.first = UInt32(m_States.size());
		if (!needA && !needB)
			m_States.push_back(m_Cand[0]);
		else if (!needB)
		{
			UInt32 b = 0;
			for (UInt32 c = 1; c != m_Cand.size(); ++c)
				if (NarrowerAtI(i, m_Cand[c].a, m_Cand[b].a))
					b = c;
			m_States.push_back(m_Cand[b]);
		}
		else if (!needA)
		{
			UInt32 b = 0;
			for (UInt32 c = 1; c != m_Cand.size(); ++c)
				if (NarrowerAtJ(j, m_Cand[c].b, m_Cand[b].b))
					b = c;
			m_States.push_back(m_Cand[b]);
		}
		else
		{
			// the staircase: narrowest at i first, and kept only when also narrower at j than all before
			std::stable_sort(m_Cand.begin(), m_Cand.end(), [this, i, j](const State& x, const State& y)
				{
					if (NarrowerAtI(i, x.a, y.a)) return true;
					if (NarrowerAtI(i, y.a, x.a)) return false;
					return NarrowerAtJ(j, x.b, y.b);
				});
			UInt32 last = NONE;
			for (const State& s : m_Cand)
				if (last == NONE || NarrowerAtJ(j, s.b, m_States[last].b))
				{
					last = UInt32(m_States.size());
					m_States.push_back(s);
				}
		}
		ch.count = UInt32(m_States.size()) - ch.first;
		return true;
	}

	bool Solve()
	{
		m_States.clear();
		for (auto& ch : m_Chords)
			if (!SolveChord(ch, m_IsReflex[ch.i], m_IsReflex[ch.j]))
				return false;
		m_Root = { 0, m_N - 1 };
		return SolveChord(m_Root, false, false, true);
	}

	// ---- the parts ----

	void Emit(Rings& parts)
	{
		UInt32 n = m_N;
		// the diagonals that the root's partition keeps, from its derivation
		m_Diag.clear();
		m_Todo.clear();
		m_Todo.push_back({ 0, n - 1, m_Root.first });
		while (!m_Todo.empty())
		{
			Todo td = m_Todo.back();
			m_Todo.pop_back();
			const State& st = m_States[td.s];
			if (st.k > td.i + 1)
			{
				if (!st.mA)
					m_Diag.emplace_back(td.i, st.k);
				m_Todo.push_back({ td.i, st.k, st.sA });
			}
			if (td.j > st.k + 1)
			{
				if (!st.mB)
					m_Diag.emplace_back(st.k, td.j);
				m_Todo.push_back({ st.k, td.j, st.sB });
			}
		}
		// per vertex its neighbours further on: the next vertex and the kept diagonals
		m_UpStart.assign(n + 1, 0);
		for (UInt32 v = 0; v + 1 < n; ++v)
			++m_UpStart[v + 1];
		for (const auto& [u, v] : m_Diag)
			++m_UpStart[u + 1];
		for (UInt32 v = 0; v != n; ++v)
			m_UpStart[v + 1] += m_UpStart[v];
		m_Up.resize(m_UpStart[n]);
		m_Fill.assign(m_UpStart.begin(), m_UpStart.end() - 1);
		for (UInt32 v = 0; v + 1 < n; ++v)
			m_Up[m_Fill[v]++] = v + 1;
		for (const auto& [u, v] : m_Diag)
			m_Up[m_Fill[u]++] = v;
		for (UInt32 v = 0; v != n; ++v)
			std::sort(m_Up.begin() + m_UpStart[v], m_Up.begin() + m_UpStart[v + 1]);

		// every part is closed by the root edge or by a kept diagonal (i, j), and its vertices are the
		// farthest steps from i towards j
		auto emitPart = [&](UInt32 i, UInt32 j)
		{
			Ring part;
			part.push_back(m_P[i]);
			for (UInt32 c = i; c != j; )
			{
				auto b = m_Up.begin() + m_UpStart[c], e = m_Up.begin() + m_UpStart[c + 1];
				auto it = std::upper_bound(b, e, c == i ? j - 1 : j);
				MG_CHECK2(it != b, "dms convex split: a part of the minimum does not close");
				c = *(it - 1);
				part.push_back(m_P[c]);
			}
			parts.push_back(std::move(part));
		};
		emitPart(0, n - 1);
		for (const auto& [u, v] : m_Diag)
			emitPart(u, v);
	}

	struct Todo { UInt32 i, j, s; };

	UInt32 m_N = 0;
	bool m_Small = false;
	std::vector<GPoint> m_P;
	std::vector<UInt8>  m_IsReflex;
	std::vector<UInt32> m_RIdx, m_Reflex, m_Seen, m_Fill, m_ChordOf;
	std::vector<Tri>    m_T, m_Nb;
	std::vector<std::pair<UInt64, UInt32>> m_EdgeKeys;
	std::vector<UInt32> m_VTriStart, m_VTri;
	std::vector<View>   m_Views;
	std::vector<Chord>  m_Chords;
	std::vector<UInt32> m_PartStart, m_LenA, m_LenB;
	std::vector<Partner> m_Part;
	Chord m_Root{ 0, 0 };
	std::vector<State> m_States, m_Cand;
	UInt32 m_BestW = NONE;
	double m_Work = 0;
	std::vector<std::pair<UInt32, UInt32>> m_Diag;
	std::vector<Todo> m_Todo;
	std::vector<UInt32> m_UpStart, m_Up;
};

class ConvexPartitioner
{
public:
	using Ring  = std::vector<GPoint>;
	using Rings = std::vector<Ring>;

	// true: the minimum for a ring without holes (dms_split_convex_polygon_keilsnoeyink, #1305);
	// false: Hertel and Mehlhorn for every ring (dms_split_convex_polygon)
	bool m_UseMinimum = false;
	MinConvexPartition m_Min;

	// rings: every ring of one element, open, with the material on the right of every edge (shells
	// clockwise, holes counter-clockwise), as DmsOverlayEngine keeps them. parts: appended, each
	// open, counter-clockwise and strictly convex.
	template <typename RingRef>
	void Run(const std::vector<RingRef>& rings, Rings& parts)
	{
		Load(rings);
		if (m_V.empty())
			return;
		if (m_NrRings == 1 && IsStrictlyConvexRing())
		{
			Ring part;
			for (UInt32 v = 0, n = UInt32(m_V.size()); v != n; ++v)
				part.push_back(m_V[v].p);
			parts.push_back(std::move(part));
			return;
		}

		// #1305: a shell without a hole that touches no other ring gets the minimum, on its own
		if (m_UseMinimum)
			SelectMinimalRings();
		if (m_UseMinimum && !m_MinRings.empty())
		{
			bool reload = m_NrRings != 1;
			std::vector<RingRef> sub;
			for (UInt32 src : m_MinRings)
			{
				if (reload)
				{
					sub.assign(1, rings[src]);
					Load(sub);
				}
				Triangulate();
				m_RingPts.clear();
				for (const auto& v : m_V)
					m_RingPts.push_back(v.p);
				if (!m_Min.Run(m_RingPts.data(), UInt32(m_RingPts.size()), m_Tri, parts))
					HertelMehlhorn(parts);
			}
			if (m_RestRings.empty())
				return;
			sub.clear();
			for (UInt32 src : m_RestRings)
				sub.push_back(rings[src]);
			Load(sub);
		}
		Relink();
		Triangulate();
		HertelMehlhorn(parts);
	}

private:
	enum : UInt8 { START, SPLIT, END, MERGE, REGULAR };
	static constexpr UInt32 NONE = UInt32(-1);

	struct Vtx
	{
		GPoint p;
		Int64  wx = 0, wy = 0;         // the first-order move into its own wedge, at a touching point only
		UInt32 prev = 0, next = 0;     // along the boundary, the material on the left
		UInt8  type = REGULAR;
	};

	// ---- step 1: the rings, the material on the left, without straight vertices ----

	static Int128 Cross(const GPoint& o, const GPoint& a, const GPoint& b)
	{
		return MulFull(a.X() - o.X(), b.Y() - o.Y()) - MulFull(a.Y() - o.Y(), b.X() - o.X());
	}
	static int RealOrient(const GPoint& a, const GPoint& b, const GPoint& c) { return Sign(Cross(a, b, c)); }
	static bool PosLess(const GPoint& a, const GPoint& b) { return a.X() < b.X() || (a.X() == b.X() && a.Y() < b.Y()); }

	template <typename RingRef>
	static const Ring& PtsOf(const RingRef& r)
	{
		if constexpr (std::is_pointer_v<RingRef>)
			return *r;
		else
			return r;
	}

	template <typename RingRef>
	void Load(const std::vector<RingRef>& rings)
	{
		m_V.clear();
		m_NrRings = 0;

		// the positions that more than one ring has as a vertex: a ring is simple, so a position
		// occurring twice is a touching point, and there a straight vertex must stay
		m_Pos.clear();
		for (const auto& r : rings)
			for (const auto& p : PtsOf(r))
				m_Pos.push_back(p);
		std::sort(m_Pos.begin(), m_Pos.end(), PosLess);
		m_Touch.clear();
		for (SizeT i = 1; i < m_Pos.size(); ++i)
			if (m_Pos[i] == m_Pos[i - 1] && (m_Touch.empty() || !(m_Touch.back() == m_Pos[i])))
				m_Touch.push_back(m_Pos[i]);
		auto isTouch = [this](const GPoint& p) { return std::binary_search(m_Touch.begin(), m_Touch.end(), p, PosLess); };

		m_RingFirst.clear();
		m_RingSrc.clear();
		for (UInt32 src = 0, nSrc = UInt32(rings.size()); src != nSrc; ++src)
		{
			const Ring& pts = PtsOf(rings[src]);
			// reversed, and without straight vertices: a stack, as DropCollinear does
			m_Ring.clear();
			for (auto it = pts.rbegin(); it != pts.rend(); ++it)
			{
				if (!m_Ring.empty() && m_Ring.back() == *it)
					continue;
				m_Ring.push_back(*it);
				while (m_Ring.size() >= 3 && !isTouch(m_Ring[m_Ring.size() - 2])
					&& RealOrient(m_Ring[m_Ring.size() - 3], m_Ring[m_Ring.size() - 2], m_Ring.back()) == 0)
					m_Ring.erase(m_Ring.end() - 2);
			}
			for (bool changed = true; changed && m_Ring.size() >= 3; )
			{
				changed = false;
				SizeT n = m_Ring.size();
				if (!isTouch(m_Ring[n - 1]) && RealOrient(m_Ring[n - 2], m_Ring[n - 1], m_Ring[0]) == 0)
				{
					m_Ring.pop_back();
					changed = true;
				}
				else if (!isTouch(m_Ring[0]) && RealOrient(m_Ring[n - 1], m_Ring[0], m_Ring[1]) == 0)
				{
					m_Ring.erase(m_Ring.begin());
					changed = true;
				}
			}
			if (m_Ring.size() < 3)
				continue;

			UInt32 base = UInt32(m_V.size()), n = UInt32(m_Ring.size());
			for (UInt32 i = 0; i != n; ++i)
			{
				Vtx v;
				v.p = m_Ring[i];
				v.prev = base + (i ? i - 1 : n - 1);
				v.next = base + (i + 1 == n ? 0 : i + 1);
				m_V.push_back(v);
			}
			m_RingFirst.push_back(base);
			m_RingSrc.push_back(src);
			++m_NrRings;
		}
		m_RingFirst.push_back(UInt32(m_V.size()));
	}

	// #1305: the rings that get the minimum: shells (the material on the left, so counter-clockwise)
	// that touch no other ring and have no hole inside them. A hole inside a shell's ring is a hole of
	// that shell, or lies in an island inside one of its holes; either way the shell has a hole.
	void SelectMinimalRings()
	{
		m_MinRings.clear();
		m_RestRings.clear();
		UInt32 nr = m_NrRings;
		m_RingOk.assign(nr, 0);
		m_RingBox.resize(nr);
		m_Holes.clear();
		for (UInt32 g = 0; g != nr; ++g)
		{
			UInt32 b = m_RingFirst[g], e = m_RingFirst[g + 1];
			Int128 area2 = 0;
			bool touch = false;
			auto& box = m_RingBox[g];
			box = { m_V[b].p.X(), m_V[b].p.Y(), m_V[b].p.X(), m_V[b].p.Y() };
			for (UInt32 v = b; v != e; ++v)
			{
				const GPoint& p = m_V[v].p;
				area2 += Cross(m_V[b].p, p, m_V[m_V[v].next].p);
				if (!m_Touch.empty() && std::binary_search(m_Touch.begin(), m_Touch.end(), p, PosLess))
					touch = true;
				box[0] = std::min(box[0], p.X()); box[1] = std::min(box[1], p.Y());
				box[2] = std::max(box[2], p.X()); box[3] = std::max(box[3], p.Y());
			}
			if (area2 > 0)
				m_RingOk[g] = !touch;
			else
				m_Holes.push_back(g);
		}
		for (UInt32 h : m_Holes)
		{
			const GPoint& q = m_V[m_RingFirst[h]].p;
			for (UInt32 g = 0; g != nr; ++g)
			{
				const auto& box = m_RingBox[g];
				if (m_RingOk[g] && box[0] <= q.X() && q.X() <= box[2] && box[1] <= q.Y() && q.Y() <= box[3] && InsideRing(q, g))
					m_RingOk[g] = 0;
			}
		}
		for (UInt32 g = 0; g != nr; ++g)
			(m_RingOk[g] ? m_MinRings : m_RestRings).push_back(m_RingSrc[g]);
	}

	// q inside ring g, by the crossings of a ray to the right; q is not on the ring
	bool InsideRing(const GPoint& q, UInt32 g) const
	{
		bool inside = false;
		for (UInt32 v = m_RingFirst[g], e = m_RingFirst[g + 1]; v != e; ++v)
		{
			const GPoint& a = m_V[v].p;
			const GPoint& b = m_V[m_V[v].next].p;
			if ((a.Y() > q.Y()) != (b.Y() > q.Y()))
			{
				int o = RealOrient(a, b, q);
				if (b.Y() > a.Y() ? o > 0 : o < 0)
					inside = !inside;
			}
		}
		return inside;
	}

	void Triangulate()
	{
		MakeMonotone();
		ExtractFaces();
		m_Tri.clear();
		for (SizeT f = 0, nf = m_FaceStart.size() - 1; f != nf; ++f)
			TriangulateMonotone(m_FaceStart[f], m_FaceStart[f + 1]);
		RepairFlatTriangles();
	}

	bool IsStrictlyConvexRing() const
	{
		for (const auto& v : m_V)
			if (RealOrient(m_V[v.prev].p, v.p, m_V[v.next].p) <= 0)
				return false;
		return true;
	}

	// ---- step 2: touching points ----

	// The angular order of directions, counter-clockwise from the positive x axis.
	static bool DirLess(Int64 ax, Int64 ay, Int64 bx, Int64 by)
	{
		int ha = (ay > 0 || (ay == 0 && ax > 0)) ? 0 : 1, hb = (by > 0 || (by == 0 && bx > 0)) ? 0 : 1;
		if (ha != hb)
			return ha < hb;
		return Sign(MulFull(ax, by) - MulFull(ay, bx)) > 0;
	}

	void Relink()
	{
		if (m_Touch.empty())
			return;
		UInt32 n = UInt32(m_V.size());
		m_Order.resize(n);
		for (UInt32 v = 0; v != n; ++v)
			m_Order[v] = v;
		std::sort(m_Order.begin(), m_Order.end(), [this](UInt32 a, UInt32 b) { return PosLess(m_V[a].p, m_V[b].p) || (m_V[a].p == m_V[b].p && a < b); });

		for (UInt32 i = 0; i != n; )
		{
			UInt32 j = i + 1;
			while (j != n && m_V[m_Order[j]].p == m_V[m_Order[i]].p)
				++j;
			if (j - i >= 2)
				RelinkAt(i, j);
			i = j;
		}
		// each record at a touching point moves into its own wedge
		for (const auto& p : m_Touch)
		{
			auto it = std::lower_bound(m_Order.begin(), m_Order.end(), p, [this](UInt32 a, const GPoint& q) { return PosLess(m_V[a].p, q); });
			for (; it != m_Order.end() && m_V[*it].p == p; ++it)
			{
				Vtx& v = m_V[*it];
				const GPoint& u = m_V[v.prev].p;
				const GPoint& w = m_V[v.next].p;
				Int64 ax = u.X() - p.X(), ay = u.Y() - p.Y(), bx = w.X() - p.X(), by = w.Y() - p.Y();
				int o = RealOrient(u, p, w);
				if (o > 0)      { v.wx =  ax + bx; v.wy =  ay + by; }
				else if (o < 0) { v.wx = -ax - bx; v.wy = -ay - by; }
				else            { v.wx = -by;      v.wy =  bx;      } // straight: the left of the leaving edge
			}
		}
	}

	// The records m_Order[i..j) share a position. An arriving edge continues along the first leaving
	// edge clockwise from its way back, which is the edge that bounds the same wedge of material.
	void RelinkAt(UInt32 i, UInt32 j)
	{
		const GPoint p = m_V[m_Order[i]].p;
		m_Outs.clear();
		for (UInt32 k = i; k != j; ++k)
			m_Outs.push_back(m_Order[k]);
		auto dirOut = [this, &p](UInt32 r) { const GPoint& q = m_V[m_V[r].next].p; return std::pair<Int64, Int64>(q.X() - p.X(), q.Y() - p.Y()); };
		std::sort(m_Outs.begin(), m_Outs.end(), [&](UInt32 a, UInt32 b)
			{
				auto da = dirOut(a), db = dirOut(b);
				return DirLess(da.first, da.second, db.first, db.second);
			});
		m_NewNext.clear();
		for (UInt32 k = i; k != j; ++k)
		{
			UInt32 a = m_Order[k];
			const GPoint& u = m_V[m_V[a].prev].p;
			Int64 rx = u.X() - p.X(), ry = u.Y() - p.Y();
			auto it = std::lower_bound(m_Outs.begin(), m_Outs.end(), 0, [&](UInt32 r, int)
				{
					auto d = dirOut(r);
					return DirLess(d.first, d.second, rx, ry);
				});
			UInt32 b = (it == m_Outs.begin()) ? m_Outs.back() : *(it - 1);
			m_NewNext.emplace_back(a, m_V[b].next);
		}
		for (const auto& [a, nx] : m_NewNext)
		{
			m_V[a].next = nx;
			m_V[nx].prev = a;
		}
	}

	// ---- step 3: the perturbed predicates ----

	// The sweep order: real x, then the move into the wedge, then the index, a smaller index lying
	// further to the right. The perturbed x of two vertices never ties.
	bool Before(UInt32 a, UInt32 b) const
	{
		const Vtx& va = m_V[a];
		const Vtx& vb = m_V[b];
		if (va.p.X() != vb.p.X())
			return va.p.X() < vb.p.X();
		if (va.wx != vb.wx)
			return va.wx < vb.wx;
		return a > b;
	}

	// +1 when c lies to the left of a -> b, -1 to the right, in the perturbed coordinates: never 0.
	int OrientP(UInt32 a, UInt32 b, UInt32 c) const
	{
		const Vtx& A = m_V[a];
		const Vtx& B = m_V[b];
		const Vtx& C = m_V[c];
		Int128 d = Cross(A.p, B.p, C.p);
		if (d != 0)
			return Sign(d);
		if (A.wx | A.wy | B.wx | B.wy | C.wx | C.wy)
		{
			Int64 bx = B.p.X() - A.p.X(), by = B.p.Y() - A.p.Y(), cx = C.p.X() - A.p.X(), cy = C.p.Y() - A.p.Y();
			Int64 dbx = B.wx - A.wx, dby = B.wy - A.wy, dcx = C.wx - A.wx, dcy = C.wy - A.wy;
			Int128 l1 = MulFull(dbx, cy) - MulFull(dby, cx) + MulFull(bx, dcy) - MulFull(by, dcx);
			if (l1 != 0)
				return Sign(l1);
			Int128 l2 = MulFull(dbx, dcy) - MulFull(dby, dcx);
			if (l2 != 0)
				return Sign(l2);
		}
		// Simulation of Simplicity: the three by index, the first-order cofactors in the order of
		// their infinitesimals, each with its own wedge-move part, then the constant term.
		UInt32 i0 = a, i1 = b, i2 = c;
		int parity = 1;
		if (i0 > i1) { std::swap(i0, i1); parity = -parity; }
		if (i1 > i2) { std::swap(i1, i2); parity = -parity; }
		if (i0 > i1) { std::swap(i0, i1); parity = -parity; }
		const Vtx& P0 = m_V[i0];
		const Vtx& P1 = m_V[i1];
		const Vtx& P2 = m_V[i2];
		auto lin = [](Int64 realPart, Int64 movePart) { return realPart ? (realPart > 0 ? 1 : -1) : (movePart > 0) - (movePart < 0); };
		if (int s = lin(P1.p.Y() - P2.p.Y(), P1.wy - P2.wy)) return parity * s; // d/dx0
		if (int s = lin(P2.p.X() - P1.p.X(), P2.wx - P1.wx)) return parity * s; // d/dy0
		if (int s = lin(P2.p.Y() - P0.p.Y(), P2.wy - P0.wy)) return parity * s; // d/dx1
		return -parity;                                                          // d2/dy0dx1
	}

	// The angular order of the directions from v to a and from v to b in the perturbed coordinates.
	bool DirLessP(UInt32 v, UInt32 a, UInt32 b) const
	{
		auto half = [this, v](UInt32 t)
		{
			Int64 dy = m_V[t].p.Y() - m_V[v].p.Y();
			if (dy)
				return dy > 0 ? 0 : 1;
			Int64 wy = m_V[t].wy - m_V[v].wy;
			if (wy)
				return wy > 0 ? 0 : 1;
			return t < v ? 0 : 1; // a smaller index lies higher
		};
		int ha = half(a), hb = half(b);
		if (ha != hb)
			return ha < hb;
		return OrientP(v, a, b) > 0;
	}

	// ---- step 4: x-monotone pieces ----

	UInt32 Lo(UInt32 e) const { UInt32 n = m_V[e].next; return Before(e, n) ? e : n; }
	UInt32 Hi(UInt32 e) const { UInt32 n = m_V[e].next; return Before(e, n) ? n : e; }

	bool EdgeBelowEdge(UInt32 e, UInt32 f) const
	{
		if (e == f)
			return false;
		UInt32 el = Lo(e), eh = Hi(e), fl = Lo(f), fh = Hi(f);
		if (el == fl)
			return OrientP(el, eh, fh) > 0;
		if (Before(el, fl))
			return OrientP(el, eh, fl) > 0;
		return OrientP(fl, fh, el) < 0;
	}

	struct VKey { UInt32 v; };
	struct EdgeOrder
	{
		const ConvexPartitioner* m_P;
		using is_transparent = void;
		bool operator()(UInt32 e, UInt32 f) const { return m_P->EdgeBelowEdge(e, f); }
		bool operator()(UInt32 e, VKey k) const { return m_P->OrientP(m_P->Lo(e), m_P->Hi(e), k.v) > 0; }
		bool operator()(VKey k, UInt32 e) const { return m_P->OrientP(m_P->Lo(e), m_P->Hi(e), k.v) < 0; }
	};
	using Status = std::set<UInt32, EdgeOrder>;

	void Classify()
	{
		for (UInt32 v = 0, n = UInt32(m_V.size()); v != n; ++v)
		{
			Vtx& x = m_V[v];
			bool prevAfter = Before(v, x.prev), nextAfter = Before(v, x.next);
			bool convex = OrientP(x.prev, v, x.next) > 0;
			if (prevAfter && nextAfter)
				x.type = convex ? START : SPLIT;
			else if (!prevAfter && !nextAfter)
				x.type = convex ? END : MERGE;
			else
				x.type = REGULAR;
		}
	}

	void MakeMonotone()
	{
		UInt32 n = UInt32(m_V.size());
		Classify();
		m_Order.resize(n);
		for (UInt32 v = 0; v != n; ++v)
			m_Order[v] = v;
		std::sort(m_Order.begin(), m_Order.end(), [this](UInt32 a, UInt32 b) { return Before(a, b); });

		m_Diag.clear();
		m_Helper.assign(n, NONE);
		Status status(EdgeOrder{ this });
		m_Where.assign(n, status.end());

		auto insertEdge = [&](UInt32 e, UInt32 helper)
		{
			auto ins = status.insert(e);
			MG_CHECK2(ins.second, "dms convex split: an edge entered the sweep twice");
			m_Where[e] = ins.first;
			m_Helper[e] = helper;
		};
		auto eraseEdge = [&](UInt32 e)
		{
			MG_CHECK2(m_Where[e] != status.end(), "dms convex split: an edge left the sweep that was not in it");
			status.erase(m_Where[e]);
			m_Where[e] = status.end();
		};
		auto below = [&](UInt32 v) -> UInt32
		{
			auto it = status.lower_bound(VKey{ v });
			MG_CHECK2(it != status.begin(), "dms convex split: no edge below a vertex that needs one");
			return *std::prev(it);
		};
		auto diagonalToMergeHelper = [&](UInt32 v, UInt32 e)
		{
			UInt32 h = m_Helper[e];
			if (h != NONE && m_V[h].type == MERGE)
				m_Diag.emplace_back(v, h);
		};

		for (UInt32 v : m_Order)
		{
			const Vtx& x = m_V[v];
			UInt32 ePrev = x.prev; // the edge prev -> v has id prev
			switch (x.type)
			{
			case START:
				insertEdge(v, v);
				break;
			case END:
				diagonalToMergeHelper(v, ePrev);
				eraseEdge(ePrev);
				break;
			case SPLIT:
			{
				UInt32 ej = below(v);
				m_Diag.emplace_back(v, m_Helper[ej]);
				m_Helper[ej] = v;
				insertEdge(v, v);
				break;
			}
			case MERGE:
			{
				diagonalToMergeHelper(v, ePrev);
				eraseEdge(ePrev);
				UInt32 ej = below(v);
				diagonalToMergeHelper(v, ej);
				m_Helper[ej] = v;
				break;
			}
			default: // REGULAR
				if (Before(x.prev, v)) // the material lies above v
				{
					diagonalToMergeHelper(v, ePrev);
					eraseEdge(ePrev);
					insertEdge(v, v);
				}
				else
				{
					UInt32 ej = below(v);
					diagonalToMergeHelper(v, ej);
					m_Helper[ej] = v;
				}
			}
		}
		MG_CHECK2(status.empty(), "dms convex split: the sweep ended with edges in it");
	}

	// ---- the pieces as cycles of vertex records ----

	void ExtractFaces()
	{
		UInt32 n = UInt32(m_V.size());
		// half-edges: v -> next(v) is half-edge v; the diagonals follow, both ways
		m_HOrg.resize(n);
		m_HDst.resize(n);
		for (UInt32 v = 0; v != n; ++v)
		{
			m_HOrg[v] = v;
			m_HDst[v] = m_V[v].next;
		}
		for (const auto& [a, b] : m_Diag)
		{
			m_HOrg.push_back(a); m_HDst.push_back(b);
			m_HOrg.push_back(b); m_HDst.push_back(a);
		}
		UInt32 nh = UInt32(m_HOrg.size());

		// the leaving half-edges of every vertex, counter-clockwise
		m_OutStart.assign(n + 1, 0);
		for (UInt32 h = 0; h != nh; ++h)
			++m_OutStart[m_HOrg[h] + 1];
		for (UInt32 v = 0; v != n; ++v)
			m_OutStart[v + 1] += m_OutStart[v];
		m_Out.resize(nh);
		m_Fill.assign(m_OutStart.begin(), m_OutStart.end() - 1);
		for (UInt32 h = 0; h != nh; ++h)
			m_Out[m_Fill[m_HOrg[h]]++] = h;
		for (UInt32 v = 0; v != n; ++v)
			std::sort(m_Out.begin() + m_OutStart[v], m_Out.begin() + m_OutStart[v + 1], [this, v](UInt32 g, UInt32 h)
				{
					return DirLessP(v, m_HDst[g], m_HDst[h]);
				});

		// the pieces: an arriving half-edge u -> v continues along the first half-edge leaving v
		// clockwise from the way back to u, which keeps the piece on the left
		auto nextOf = [this](UInt32 h) -> UInt32
		{
			UInt32 u = m_HOrg[h], v = m_HDst[h];
			auto first = m_Out.begin() + m_OutStart[v], last = m_Out.begin() + m_OutStart[v + 1];
			auto it = std::lower_bound(first, last, u, [this, v](UInt32 g, UInt32 t) { return m_HDst[g] != t && DirLessP(v, m_HDst[g], t); });
			return (it == first) ? *(last - 1) : *(it - 1);
		};
		m_Used.assign(nh, false);
		m_FacePts.clear();
		m_FaceStart.assign(1, 0);
		for (UInt32 h0 = 0; h0 != nh; ++h0)
		{
			if (m_Used[h0])
				continue;
			for (UInt32 h = h0; !m_Used[h]; h = nextOf(h))
			{
				m_Used[h] = true;
				m_FacePts.push_back(m_HOrg[h]);
			}
			m_FaceStart.push_back(UInt32(m_FacePts.size()));
		}
	}

	// ---- step 5: a monotone piece into triangles ----

	void Emit(UInt32 a, UInt32 b, UInt32 c)
	{
		if (OrientP(a, b, c) < 0)
			std::swap(b, c);
		m_Tri.push_back({ a, b, c });
	}

	void TriangulateMonotone(UInt32 first, UInt32 last)
	{
		UInt32 n = last - first;
		const UInt32* f = m_FacePts.data() + first;
		MG_CHECK2(n >= 3, "dms convex split: a piece with fewer than three vertices");
		if (n == 3)
		{
			Emit(f[0], f[1], f[2]);
			return;
		}
		UInt32 iMin = 0, iMax = 0;
		for (UInt32 i = 1; i != n; ++i)
		{
			if (Before(f[i], f[iMin])) iMin = i;
			if (Before(f[iMax], f[i])) iMax = i;
		}
		// counter-clockwise from the leftmost vertex: the lower chain to the rightmost, then the upper
		// chain back; both ascend, and merged they give the order of the sweep
		auto nextI = [n](UInt32 i) { return i + 1 == n ? 0 : i + 1; };
		auto prevI = [n](UInt32 i) { return i ? i - 1 : n - 1; };
		m_Sorted.clear();
		m_Chain.clear();
		m_Sorted.push_back(f[iMin]);
		m_Chain.push_back(0);
		UInt32 lo = nextI(iMin), up = prevI(iMin), lastLo = f[iMin], lastUp = f[iMin];
		while (lo != iMax || up != iMax)
		{
			bool takeLower = (up == iMax) || (lo != iMax && Before(f[lo], f[up]));
			if (takeLower)
			{
				MG_CHECK2(Before(lastLo, f[lo]), "dms convex split: a piece is not monotone");
				lastLo = f[lo];
				m_Sorted.push_back(f[lo]);
				m_Chain.push_back(0);
				lo = nextI(lo);
			}
			else
			{
				MG_CHECK2(Before(lastUp, f[up]), "dms convex split: a piece is not monotone");
				lastUp = f[up];
				m_Sorted.push_back(f[up]);
				m_Chain.push_back(1);
				up = prevI(up);
			}
		}
		m_Sorted.push_back(f[iMax]);
		m_Chain.push_back(1);

		// the stack algorithm
		m_Stack.clear();
		m_Stack.push_back(0);
		m_Stack.push_back(1);
		for (UInt32 j = 2; j + 1 < n; ++j)
		{
			if (m_Chain[j] != m_Chain[m_Stack.back()])
			{
				for (SizeT k = 0; k + 1 < m_Stack.size(); ++k)
					Emit(m_Sorted[j], m_Sorted[m_Stack[k]], m_Sorted[m_Stack[k + 1]]);
				UInt32 top = m_Stack.back();
				m_Stack.clear();
				m_Stack.push_back(top);
				m_Stack.push_back(j);
			}
			else
			{
				UInt32 q = m_Stack.back();
				m_Stack.pop_back();
				while (!m_Stack.empty())
				{
					UInt32 r = m_Stack.back();
					int o = OrientP(m_Sorted[r], m_Sorted[q], m_Sorted[j]);
					bool inside = (m_Chain[j] == 0) ? (o > 0) : (o < 0);
					if (!inside)
						break;
					Emit(m_Sorted[j], m_Sorted[q], m_Sorted[r]);
					q = r;
					m_Stack.pop_back();
				}
				m_Stack.push_back(q);
				m_Stack.push_back(j);
			}
		}
		for (SizeT k = 0; k + 1 < m_Stack.size(); ++k)
			Emit(m_Sorted[n - 1], m_Sorted[m_Stack[k]], m_Sorted[m_Stack[k + 1]]);
	}

	// ---- step 6: triangles without area ----

	static UInt64 Key(UInt32 a, UInt32 b) { return (UInt64(a) << 32) | b; }

	void RegisterTri(UInt32 t)
	{
		const auto& T = m_Tri[t];
		for (int k = 0; k != 3; ++k)
			m_EdgeTri[Key(T[k], T[(k + 1) % 3])] = t;
	}
	void UnregisterTri(UInt32 t)
	{
		const auto& T = m_Tri[t];
		for (int k = 0; k != 3; ++k)
			m_EdgeTri.erase(Key(T[k], T[(k + 1) % 3]));
	}
	bool IsFlat(UInt32 t) const
	{
		const auto& T = m_Tri[t];
		return RealOrient(m_V[T[0]].p, m_V[T[1]].p, m_V[T[2]].p) == 0;
	}

	void RepairFlatTriangles()
	{
		m_Queue.clear();
		for (UInt32 t = 0, nt = UInt32(m_Tri.size()); t != nt; ++t)
			if (IsFlat(t))
				m_Queue.push_back(t);
		if (m_Queue.empty())
			return;

		m_EdgeTri.clear();
		for (UInt32 t = 0, nt = UInt32(m_Tri.size()); t != nt; ++t)
			RegisterTri(t);

		SizeT budget = 8 * m_Tri.size() + 64;
		while (!m_Queue.empty())
		{
			MG_CHECK2(budget--, "dms convex split: the triangles without area could not be flipped away");
			UInt32 t = m_Queue.back();
			m_Queue.pop_back();
			if (!IsFlat(t))
				continue;
			auto T = m_Tri[t];
			// the middle one of three collinear points; the edge across from it is the long one
			int m = 0;
			for (int k = 0; k != 3; ++k)
			{
				const GPoint& a = m_V[T[(k + 1) % 3]].p;
				const GPoint& b = m_V[T[(k + 2) % 3]].p;
				const GPoint& c = m_V[T[k]].p;
				if ((PosLess(a, c) && PosLess(c, b)) || (PosLess(b, c) && PosLess(c, a)))
				{
					m = k;
					break;
				}
			}
			UInt32 mid = T[m], e1 = T[(m + 1) % 3], e2 = T[(m + 2) % 3]; // the triangle runs e1 -> e2 -> mid
			auto it = m_EdgeTri.find(Key(e2, e1));
			MG_CHECK2(it != m_EdgeTri.end(), "dms convex split: the long edge of a triangle without area is not a diagonal");
			UInt32 u = it->second;
			auto U = m_Tri[u];
			UInt32 d = U[0] + U[1] + U[2] - e1 - e2;
			UnregisterTri(t);
			UnregisterTri(u);
			m_Tri[t] = { e1, d, mid };
			m_Tri[u] = { d, e2, mid };
			RegisterTri(t);
			RegisterTri(u);
			if (IsFlat(t))
				m_Queue.push_back(t);
			if (IsFlat(u))
				m_Queue.push_back(u);
		}
	}

	// ---- step 7: Hertel and Mehlhorn ----

	void HertelMehlhorn(Rings& parts)
	{
		UInt32 nt = UInt32(m_Tri.size());
		UInt32 nh = 3 * nt;
		m_Org.resize(nh);
		m_Nxt.resize(nh);
		m_Prv.resize(nh);
		m_Twin.assign(nh, NONE);
		m_Gone.assign(nh, false);
		m_EdgeTri.clear();
		for (UInt32 t = 0; t != nt; ++t)
			for (UInt32 k = 0; k != 3; ++k)
			{
				UInt32 h = 3 * t + k;
				m_Org[h] = m_Tri[t][k];
				m_Nxt[h] = 3 * t + (k + 1) % 3;
				m_Prv[h] = 3 * t + (k + 2) % 3;
				m_EdgeTri[Key(m_Tri[t][k], m_Tri[t][(k + 1) % 3])] = h;
			}
		for (UInt32 h = 0; h != nh; ++h)
		{
			auto it = m_EdgeTri.find(Key(m_Org[m_Nxt[h]], m_Org[h]));
			if (it != m_EdgeTri.end())
				m_Twin[h] = it->second;
		}

		for (UInt32 h = 0; h != nh; ++h)
		{
			UInt32 t = m_Twin[h];
			if (t == NONE || t < h || m_Gone[h])
				continue;
			// h runs a -> b in one part, t runs b -> a in the other
			UInt32 a = m_Org[h], b = m_Org[t];
			UInt32 x = m_Org[m_Prv[h]], y = m_Org[m_Nxt[m_Nxt[h]]];
			UInt32 u = m_Org[m_Prv[t]], v = m_Org[m_Nxt[m_Nxt[t]]];
			if (RealOrient(m_V[x].p, m_V[a].p, m_V[v].p) <= 0 || RealOrient(m_V[u].p, m_V[b].p, m_V[y].p) <= 0)
				continue;
			UInt32 hp = m_Prv[h], hn = m_Nxt[h], tp = m_Prv[t], tn = m_Nxt[t];
			m_Nxt[hp] = tn; m_Prv[tn] = hp;
			m_Nxt[tp] = hn; m_Prv[hn] = tp;
			m_Gone[h] = m_Gone[t] = true;
		}

		m_Used.assign(nh, false);
		for (UInt32 h0 = 0; h0 != nh; ++h0)
		{
			if (m_Gone[h0] || m_Used[h0])
				continue;
			Ring part;
			for (UInt32 h = h0; !m_Used[h]; h = m_Nxt[h])
			{
				m_Used[h] = true;
				part.push_back(m_V[m_Org[h]].p);
			}
			parts.push_back(std::move(part));
		}
	}

	std::vector<Vtx>    m_V;
	UInt32              m_NrRings = 0;
	std::vector<GPoint> m_Pos, m_Touch, m_Ring;
	std::vector<UInt32> m_Order, m_Outs, m_Helper;
	std::vector<std::pair<UInt32, UInt32>> m_NewNext, m_Diag;
	std::vector<typename Status::iterator> m_Where;
	std::vector<UInt32> m_HOrg, m_HDst, m_OutStart, m_Out, m_Fill, m_FacePts, m_FaceStart;
	std::vector<bool>   m_Used;
	std::vector<UInt32> m_Sorted, m_Stack, m_Queue;
	std::vector<UInt8>  m_Chain;
	std::vector<std::array<UInt32, 3>> m_Tri;
	std::unordered_map<UInt64, UInt32> m_EdgeTri;
	std::vector<UInt32> m_Org, m_Nxt, m_Prv, m_Twin;
	std::vector<bool>   m_Gone;

	// #1305
	std::vector<UInt32> m_RingFirst, m_RingSrc, m_MinRings, m_RestRings, m_Holes;
	std::vector<UInt8>  m_RingOk;
	std::vector<std::array<Int64, 4>> m_RingBox;
	std::vector<GPoint> m_RingPts;
};

} // namespace dms_overlay

#endif //!defined(__GEO_DMS_CONVEX_PARTITION_H)
