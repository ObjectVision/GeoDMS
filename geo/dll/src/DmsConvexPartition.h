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

#include <algorithm>
#include <array>
#include <set>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

namespace dms_overlay
{

class ConvexPartitioner
{
public:
	using Ring  = std::vector<GPoint>;
	using Rings = std::vector<Ring>;

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
		Relink();
		MakeMonotone();
		ExtractFaces();
		m_Tri.clear();
		for (SizeT f = 0, nf = m_FaceStart.size() - 1; f != nf; ++f)
			TriangulateMonotone(m_FaceStart[f], m_FaceStart[f + 1]);
		RepairFlatTriangles();
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

		for (const auto& r : rings)
		{
			const Ring& pts = PtsOf(r);
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
			++m_NrRings;
		}
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
};

} // namespace dms_overlay

#endif //!defined(__GEO_DMS_CONVEX_PARTITION_H)
