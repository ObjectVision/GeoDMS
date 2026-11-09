// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

/*
 *  Distance kernels: MinDistToRange and point-to-segment/sequence square
 *  distances for nearest-object searches.
 */

#if !defined(__GEOM_GEODIST_H)
#define __GEOM_GEODIST_H

#include "vt/Conversions.h"
#include "geom/RingIterator.h"
#include "mem/MyContainers.h"

#include <cmath>
#include <limits>

using seq_elem_index_type = UInt32;

//----------------------------------------------------------------------
// Section      : Distance Bounds
//----------------------------------------------------------------------

template <typename T>
T MinDistToRange(T v, Range<T> r)
{
	assert(r.first <= r.second);
	if (v < r.first) return r.first - v;
	if (v > r.second) return v - r.second;
	return 0;
}

template <typename T>
T MaxDistToRange(T v, Range<T> r)
{
	return Max<T>(Dist(v, r.first), Dist(v, r.second));
}

template <typename T>
Point<T> MinDistToRange(Point<T> v, Range<Point<T>> r)
{
	return Point<T>(
		MinDistToRange(v.first, Range<T>(r.first.first, r.second.first)),
		MinDistToRange(v.second, Range<T>(r.first.second, r.second.second))
	);
}

//----------------------------------------------------------------------
// Section      : Distance Measures
//----------------------------------------------------------------------

// The segment vectors are taken in R (see Diff): in the coordinate type an arc drawn towards smaller unsigned
// coordinates had wrapped vectors, and its projection and distance were wrong (GEO-A34).
template <typename R, typename T>
inline Point<T> Project2Segm(const Point<T>& p, const Point<T>& a, const Point<T>& b)
{
	Point<R> ab = Diff<R>(b, a);
	Point<R> ap = Diff<R>(p, a);

	R c = InProduct<R>(ab, ap);
	if (c<=0)
		return a;
	R abSqrDist = Norm<R>(ab);
	if (c > abSqrDist)
		return b;

	if constexpr (std::is_unsigned_v<T>) // the offset from a can be negative; the projection itself lies between a and b
		return Convert<Point<T> >(Convert<Point<R> >(a) + ab * (InProduct<R>(ap, ab) / abSqrDist));
	else
		return a + Convert<Point<T> >(ab * (InProduct<R>(ap, ab) / abSqrDist));
}

template <typename R, typename T>
inline R SqrDist2Segm(const Point<T>& p, const Point<T>& a, const Point<T>& b)
{
	Point<R> ab = Diff<R>(b, a);
	Point<R> ap = Diff<R>(p, a);

	R c = InProduct<R>(ab, ap);
	if ( c<=0 )
		return MaxValue<R>();
	R abSqrDist = Norm<R>(ab);
	if (c > abSqrDist)
		return SqrDist<R>(p, b);
	return Sqr(OutProduct<R>(ap, ab)) / abSqrDist;
}

// The square of the distance from p to the nearest and to the farthest point of a box, in R as Diff takes differences, so that
// an unsigned or 16-bit coordinate neither wraps nor is cut back. An inverted box, which holds no point, is far away.
template <typename R, typename T>
R SqrDistToBox(const Point<T>& p, const Range<Point<T>>& box)
{
	R d1 = 0, d2 = 0;
	if (p.first < box.first.first)        d1 = R(box.first.first) - R(p.first);
	else if (box.second.first < p.first)  d1 = R(p.first) - R(box.second.first);
	if (p.second < box.first.second)      d2 = R(box.first.second) - R(p.second);
	else if (box.second.second < p.second) d2 = R(p.second) - R(box.second.second);
	return d1 * d1 + d2 * d2;
}

template <typename R, typename T>
R SqrMaxDistToBox(const Point<T>& p, const Range<Point<T>>& box)
{
	R d1 = Max<R>(std::abs(R(p.first)  - R(box.first.first )), std::abs(R(p.first)  - R(box.second.first )));
	R d2 = Max<R>(std::abs(R(p.second) - R(box.first.second)), std::abs(R(p.second) - R(box.second.second)));
	return d1 * d1 + d2 * d2;
}

// *****************************************************************************
//									ArcRunBoxes
// *****************************************************************************

// When an arc gets run boxes, and how long its runs are.
namespace ArcRunParams
{
	// An arc gets run boxes when it has at least this many vertices. Measured on OVSRV10 with dist_info of 1 million points to
	// 400 rings (#1334), per round: with 9 vertices the boxes cost 0.005 to 0.007 s of about 0.065 s, with 17 they made no
	// difference, with 33 they saved 0.007 to 0.012 s of about 0.11 s and with 65 0.07 to 0.09 s of about 0.18 s.
	constexpr SizeT MinVertices = 32;

	// A run holds the square root of the number of segments of its arc, so that a walk reads about as many boxes as it
	// reads segments of the runs it does not pass over, and at least this many segments.
	constexpr SizeT MinRunLength = 16;

	// A run is passed over only when its box lies farther from the point than the nearest distance by more than this many
	// units of the rounding of the distance type (or of a floating point coordinate type, when that is coarser) times the size
	// of the coordinates involved; see ArcRunBoxes.
	constexpr UInt32 MarginUlps = 256;
}

// The bounding boxes of consecutive runs of the segments of one arc: run r holds segments r * m_RunLength up to the next run,
// segment s running from vertex s to vertex s + 1 of the whole sequence, and its box holds those vertices, separators of a
// multi-linestring and the connecting edges of a multi-polygon included. ArcProjectionHandle::Project2LineString passes over a
// run when every segment in it is farther from the point than both the nearest distance found so far and the nearest of the
// farthest corners of the boxes of the runs it walks: some vertex of its own lies within that corner distance, and so does the
// nearest segment. A segment in a run that is passed over would thus have been refused by MakeSafeMin, which takes a segment
// only when it is nearer than or as near as the nearest so far, and the walk keeps its order, so the nearest segment, its index,
// the cut point and the tie rule stay exactly those of the walk over all segments (#1334). "Farther" allows for rounding: the
// computed distance to a segment and to a box differ from the exact ones by a few units of rounding times the size of the
// coordinates, which ArcRunParams::MarginUlps covers many times over.
template <typename T>
struct ArcRunBoxes
{
	using PointType = Point<T>;
	using RectType  = Range<PointType>;

	UInt32  m_RunLength = 0; // segments per run; 0: the arc has no run boxes
	Float64 m_Scale = 0;     // the largest absolute coordinate of the arc plus the longer side of its box
	my_vec_t<RectType> m_Boxes;

	// The linestrings of a multi-linestring, or the rings of a multi-polygon, as begin and end positions in the arc: what
	// Project2MultiLinestring and Project2MultiPolygon find by reading every vertex, which a walk with run boxes should not.
	my_vec_t<std::pair<UInt32, UInt32>> m_Parts;
	bool m_IsPolygon = false;

	bool IsBuilt() const { return m_RunLength; }

	void Build(const PointType* arcBegin, const PointType* arcEnd, bool isPolygon)
	{
		SizeT k = arcEnd - arcBegin;
		if (k < ArcRunParams::MinVertices || k > MAX_VALUE(UInt32))
			return;

		// the parts, as Project2MultiPolygon and Project2MultiLinestring walk them
		m_IsPolygon = isPolygon;
		if (isPolygon)
		{
			auto mpRef = IterRange<const PointType*>(arcBegin, arcEnd);
			auto ringIter = SA_ConstRingIterator<PointType>(mpRef, 0);
			auto ringEnd  = SA_ConstRingIterator<PointType>(mpRef, -1);
			for (; ringIter != ringEnd; ++ringIter)
			{
				SA_ConstRing<PointType> ring = *ringIter;
				m_Parts.emplace_back(UInt32(ring.begin() - arcBegin), UInt32(ring.end() - arcBegin));
			}
		}
		else
		{
			const PointType* cursor = arcBegin;
			while (cursor != arcEnd)
			{
				const PointType* next = cursor;
				while (next != arcEnd && IsDefined(*next))
					++next;
				m_Parts.emplace_back(UInt32(cursor - arcBegin), UInt32(next - arcBegin));
				if (next == arcEnd)
					break;
				cursor = next + 1;
			}
		}

		SizeT runLength = Max<SizeT>(ArcRunParams::MinRunLength, SizeT(std::sqrt(Float64(k - 1))));
		SizeT nrSegm = k - 1;
		SizeT nrRuns = (nrSegm + runLength - 1) / runLength;
		m_Boxes.resize(nrRuns);
		RectType arcBox;
		for (SizeT r = 0; r != nrRuns; ++r)
		{
			RectType box;
			for (SizeT v = r * runLength, ve = Min<SizeT>(nrSegm, (r + 1) * runLength); v <= ve; ++v)
				if (IsDefined(arcBegin[v]))
					box |= arcBegin[v];
			m_Boxes[r] = box;
			arcBox |= box;
		}
		if (arcBox.inverted())
		{
			m_Boxes.clear();
			return;
		}
		Float64 maxAbs = Max<Float64>(
			Max<Float64>(std::abs(Float64(arcBox.first.first )), std::abs(Float64(arcBox.second.first ))),
			Max<Float64>(std::abs(Float64(arcBox.first.second)), std::abs(Float64(arcBox.second.second))));
		Float64 maxSide = Max<Float64>(Float64(arcBox.second.first) - Float64(arcBox.first.first), Float64(arcBox.second.second) - Float64(arcBox.first.second));
		m_Scale = maxAbs + maxSide;
		m_RunLength = UInt32(runLength);
	}

	// how much farther than the nearest distance a run must lie to be passed over, for point p
	template <typename R>
	R Margin(const PointType& p) const
	{
		static_assert(std::is_floating_point_v<R>);
		R eps = std::numeric_limits<R>::epsilon();
		if constexpr (std::is_floating_point_v<T>)
			MakeMax(eps, R(std::numeric_limits<T>::epsilon()));
		R pointScale = Max<R>(std::abs(R(p.first)), std::abs(R(p.second)));
		return R(ArcRunParams::MarginUlps) * eps * (R(m_Scale) + pointScale);
	}
};

// *****************************************************************************
//									ArcProjectionHandle
// *****************************************************************************

template <typename R, typename T>
struct ArcProjectionHandle
{
	typedef Point<T>         PointType;
	typedef Range<PointType> RectType;
	typedef const PointType* ConstPointPtr;
	typedef R                sqrdist_type;

	PointType     m_Point;
	sqrdist_type  m_MinSqrDist = 0;

	// result vars
	bool         m_FoundAny = false, m_InArc = false, m_InSegm = false, m_IsPossiblyMultiPolygon = false;
	seq_elem_index_type m_SegmIndex = UNDEFINED_VALUE(seq_elem_index_type);
	PointType    m_CutPoint;

	// The number of linestrings or rings in which Project2Arc took a segment. Once a part has given a segment, a later part
	// must be strictly nearer, so with more than one such part, which of equally near segments is kept depends on the bound
	// the handle started with: whether an earlier part lay within it. IndexedArcProjectionHandle measures such an arc again.
	UInt32       m_NrFoundParts = 0;

	ArcProjectionHandle() {}

	ArcProjectionHandle(PointType p, sqrdist_type minSqrDist, bool isPossiblyMultiPolygon)
		:	m_Point(p)
		,	m_MinSqrDist(minSqrDist)
		, m_IsPossiblyMultiPolygon(isPossiblyMultiPolygon)
	{}

	bool CanSkip(const RectType& bb)
	{
		auto dist = MinDistToRange(m_Point, bb);
		return m_MinSqrDist < Norm<R>(dist);
	}

	bool MakeSafeMin(sqrdist_type newDist)
	{
		if (newDist < m_MinSqrDist)
		{
			m_MinSqrDist = newDist;
			return true;
		}
		return !m_FoundAny && newDist == m_MinSqrDist;
	}

	// runs: the run boxes of the whole arc, or null; runOffset: the position of arcBegin in the whole arc
	bool Project2LineString(ConstPointPtr arcBegin, ConstPointPtr arcEnd, const ArcRunBoxes<T>* runs = nullptr, SizeT runOffset = 0);
	bool Project2MultiLinestring(ConstPointPtr arcBegin, ConstPointPtr arcEnd);
	bool Project2MultiPolygon(ConstPointPtr arcBegin, ConstPointPtr arcEnd);

	bool Project2Arc(ConstPointPtr arcBegin, ConstPointPtr arcEnd, const ArcRunBoxes<T>* runs = nullptr)
	{
		if (runs)
		{
			// the parts that Project2MultiPolygon or Project2MultiLinestring would find, in the same order
			assert(runs->m_IsPolygon == m_IsPossiblyMultiPolygon);
			bool foundAny = false;
			for (const auto& part : runs->m_Parts)
				if (Project2LineString(arcBegin + part.first, arcBegin + part.second, runs, part.first))
				{
					m_SegmIndex += part.first;
					foundAny = true;
					++m_NrFoundParts;
				}
			return foundAny;
		}
		if (m_IsPossiblyMultiPolygon)
			return Project2MultiPolygon(arcBegin, arcEnd);
		else
			return Project2MultiLinestring(arcBegin, arcEnd);
	}
};

template <typename R, typename T>
bool ArcProjectionHandle<R, T>::Project2LineString(ConstPointPtr arcBegin, ConstPointPtr arcEnd, const ArcRunBoxes<T>* runs, SizeT runOffset)
{
	if (arcBegin == arcEnd)
		return false;

	assert(IsDefined(arcBegin[0]));

	ConstPointPtr
		last = arcEnd - 1,
		nearestSegm = nullptr;

	if (MakeSafeMin(SqrDist<sqrdist_type>(m_Point, *arcBegin)))
		nearestSegm = arcBegin;

	if (!runs)
	{
		for (ConstPointPtr i = arcBegin; i != last; ++i)
		{
			assert(IsDefined(i[1]));
			if (MakeSafeMin(SqrDist2Segm<sqrdist_type>(m_Point, i[0], i[1])))
				nearestSegm = i;
		}
	}
	else
	{
		// The runs that hold segments of this linestring are passed over when they lie farther than both the nearest so far
		// and the nearest farthest corner among them; see ArcRunBoxes for why that changes no result.
		SizeT nrSegm = last - arcBegin;
		SizeT runLength = runs->m_RunLength;
		assert(runLength);
		sqrdist_type margin = runs->template Margin<sqrdist_type>(m_Point);
		auto inflate = [margin](sqrdist_type sqrDist) { sqrdist_type d = std::sqrt(sqrDist) + margin; return d * d; };

		sqrdist_type nearestCorner = MaxValue<sqrdist_type>();
		if (nrSegm)
			for (SizeT r = runOffset / runLength, re = (runOffset + nrSegm - 1) / runLength; r <= re; ++r)
				MakeMin(nearestCorner, SqrMaxDistToBox<sqrdist_type>(m_Point, runs->m_Boxes[r]));
		sqrdist_type passBeyond = inflate(Min<sqrdist_type>(m_MinSqrDist, nearestCorner));

		for (SizeT s = 0; s != nrSegm; )
		{
			SizeT r = (runOffset + s) / runLength;
			SizeT runEnd = Min<SizeT>(nrSegm, (r + 1) * runLength - runOffset);
			if (SqrDistToBox<sqrdist_type>(m_Point, runs->m_Boxes[r]) > passBeyond)
			{
				s = runEnd;
				continue;
			}
			for (; s != runEnd; ++s)
			{
				assert(IsDefined(arcBegin[s + 1]));
				if (MakeSafeMin(SqrDist2Segm<sqrdist_type>(m_Point, arcBegin[s], arcBegin[s + 1])))
				{
					nearestSegm = arcBegin + s;
					MakeMin(passBeyond, inflate(m_MinSqrDist));
				}
			}
		}
	}

	if (nearestSegm == nullptr)
		return false;

	ConstPointPtr i = last;

	m_SegmIndex = nearestSegm - arcBegin;
	ConstPointPtr nearestSegmEnd = nearestSegm + 1;
	if (nearestSegmEnd == arcEnd) // only possible when linestring is only one point
	{
		assert(nearestSegm == arcBegin);
		assert(arcBegin + 1 == arcEnd);
		assert(m_InArc == false);
		m_CutPoint = *nearestSegm;
	}
	else
	{
		m_CutPoint = Project2Segm<R>(m_Point, *nearestSegm, *nearestSegmEnd);
		m_InArc = (nearestSegm != arcBegin || m_CutPoint != *arcBegin)
			&& (nearestSegm != i - 1 || m_CutPoint != *i);
	}
	m_InSegm = m_InArc && (*nearestSegmEnd != m_CutPoint);
	m_FoundAny = true;

	assert(m_InArc
		? arcBegin + m_SegmIndex + (m_InSegm ? 1 : 2) < arcEnd
		: (m_SegmIndex == 0 || arcBegin + m_SegmIndex + 2 == arcEnd)
	);
	return true;
};

template<typename ConstPointPtr>
auto FindLineStringSeparator(ConstPointPtr begin, ConstPointPtr end) -> ConstPointPtr
{
	while (begin != end && IsDefined(*begin))
		++begin;
	return begin;
}

template <typename R, typename T>
bool ArcProjectionHandle<R, T>::Project2MultiLinestring(ConstPointPtr arcBegin, ConstPointPtr arcEnd)
{
	if (arcBegin == arcEnd)
		return false;

	bool foundAny = false;
	auto arcCursor = arcBegin;
	while (arcCursor != arcEnd)
	{
		auto nextCursor = FindLineStringSeparator(arcCursor, arcEnd);
		if (Project2LineString(arcCursor, nextCursor))
		{
			m_SegmIndex += arcCursor - arcBegin;
			foundAny = true;
			++m_NrFoundParts;
		}
		if (nextCursor == arcEnd)
			break;
		arcCursor = nextCursor+1;
	}
	return foundAny;
}

template <typename R, typename T>
bool ArcProjectionHandle<R, T>::Project2MultiPolygon(ConstPointPtr arcBegin, ConstPointPtr arcEnd)
{
	if (arcBegin == arcEnd)
		return false;

	auto mpRef = IterRange<const PointType*>(arcBegin, arcEnd);
	auto ringIter = SA_ConstRingIterator<PointType>(mpRef,  0);
	auto ringEnd  = SA_ConstRingIterator<PointType>(mpRef, -1);

	bool foundAny = false;
	while (ringIter != ringEnd)
	{
		SA_ConstRing<PointType> ring = *ringIter;
		if (Project2LineString(ring.begin(), ring.end()))
		{
			m_SegmIndex += ring.begin() - arcBegin;
			foundAny = true;
			++m_NrFoundParts;
		}
		++ringIter;
	}
	return foundAny;
}

// *****************************************************************************
//									ArcProjectionHandleWithDist
// *****************************************************************************

template <typename T> inline
T SafeBet(T val)
{
	assert(val > 0);
	T result = val * 1.001;
	assert(val < result);
	return result;
}

template <typename R, typename T>
struct ArcProjectionHandleWithDist : ArcProjectionHandle<R, T>
{
	R m_Dist = R();

	ArcProjectionHandleWithDist() {}

	ArcProjectionHandleWithDist(typename ArcProjectionHandleWithDist::PointType p, typename ArcProjectionHandleWithDist::sqrdist_type minSqrDist, bool isPossiblyMultiPolygon)
		:	ArcProjectionHandle<R, T>(p, minSqrDist, isPossiblyMultiPolygon)
		,	m_Dist(CalcDist())
	{}

	bool Project2Arc(typename ArcProjectionHandleWithDist::ConstPointPtr arcBegin, typename ArcProjectionHandleWithDist::ConstPointPtr arcEnd, const ArcRunBoxes<T>* runs = nullptr)
	{
		if (!ArcProjectionHandle<R, T>::Project2Arc(arcBegin, arcEnd, runs))
			return false;
		m_Dist = sqrt(this->m_MinSqrDist);
		return true;
	}

private:
	R CalcDist() const {
		return SafeBet(sqrt(this->m_MinSqrDist));
	}
};

#endif // __GEOM_GEODIST_H


