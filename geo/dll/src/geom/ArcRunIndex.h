// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

/*
 *  ArcRunIndex<T>: the run boxes (ArcRunBoxes, rtc/dll/src/geom/GeoDist.h) of every long arc of a set, built once beside the
 *  spatial index of the operators that search the nearest arc of each point, so that the nearest-segment walk over a
 *  candidate arc passes over the runs that cannot hold its nearest segment (#1334). Only read by the threads that query it.
 */

#ifndef __GEO_ARCRUNINDEX_H
#define __GEO_ARCRUNINDEX_H

#include "geom/GeoDist.h"
#include "mem/MyContainers.h"
#include "ParallelTiles.h"

#include <vector>

namespace ArcRunIndexParams
{
	// The long arcs get run boxes only when there are at least this many points per long arc. Building the boxes of an arc
	// reads its vertices once and costs less than one walk over it: for one ring of 16.8 million vertices on OVSRV10 (#1334),
	// a single point took no measurably longer with the boxes than without, and 4 points 0.08 to 0.11 s less.
	constexpr SizeT MinPointsPerArc = 1;
}

template <typename T>
struct ArcRunIndex
{
	using PointType = Point<T>;

	ArcRunIndex() = default;

	// the run boxes of the arcs [first, last) of at least ArcRunParams::MinVertices vertices, for nrPoints points to be
	// tested against them; isPolygon: the arcs are polygons, which ArcProjectionHandle walks per ring
	template <typename ArcIter>
	ArcRunIndex(ArcIter first, ArcIter last, SizeT nrPoints, bool isPolygon)
	{
		SizeT n = last - first;
		std::vector<SizeT> longArcs;
		for (SizeT i = 0; i != n; ++i)
			if ((*(first + i)).size() >= ArcRunParams::MinVertices)
				longArcs.emplace_back(i);
		if (longArcs.empty() || nrPoints < ArcRunIndexParams::MinPointsPerArc * longArcs.size())
			return;

		my_vec_t<ArcRunBoxes<T>> arcs(longArcs.size());
		parallel_for<SizeT>(longArcs.size(), [&arcs, &longArcs, first, isPolygon](SizeT c)
			{
				const auto& arc = *(first + longArcs[c]);
				arcs[c].Build(begin_ptr(arc), end_ptr(arc), isPolygon);
			}
		);

		m_RunsOf.assign(n, 0);
		for (SizeT c = 0; c != longArcs.size(); ++c)
			if (arcs[c].IsBuilt())
			{
				m_Arcs.emplace_back(std::move(arcs[c]));
				m_RunsOf[longArcs[c]] = UInt32(m_Arcs.size());
			}
		if (m_Arcs.empty())
			m_RunsOf.clear();
	}

	SizeT NrArcsWithRuns() const { return m_Arcs.size(); }

	// the run boxes of arc number arcIndex, or null when it has none
	const ArcRunBoxes<T>* Get(SizeT arcIndex) const
	{
		if (m_RunsOf.empty())
			return nullptr;
		UInt32 i = m_RunsOf[arcIndex];
		return i ? &m_Arcs[i - 1] : nullptr;
	}

private:
	my_vec_t<UInt32>         m_RunsOf; // per arc its position in m_Arcs plus one, 0 for none; empty when no arc has run boxes
	my_vec_t<ArcRunBoxes<T>> m_Arcs;
};

#endif // __GEO_ARCRUNINDEX_H
