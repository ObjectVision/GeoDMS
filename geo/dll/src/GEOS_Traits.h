// Copyright (C) 1998-2024 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

#if !defined(DMS_GEO_GEOS_TRAITS_H)
#define DMS_GEO_GEOS_TRAITS_H

#include "geo/RingIterator.h"


#include "RemoveAdjacentsAndSpikes.h"

//============================  GEOS  ============================
#include <geos/geom/GeometryFactory.h>
#include <geos/geom/Geometry.h>
#include <geos/geom/Polygon.h>
#include <geos/geom/LinearRing.h>
#include <geos/algorithm/Orientation.h>



template <typename DmsPointType>
auto geos_create_multi_polygon(SA_ConstReference<DmsPointType> polyRef, bool mustInsertInnerRings = true)
-> std::unique_ptr<geos::geom::MultiPolygon>
{
//	std::unique_ptr< geos::geom::MultiPolygon>  resMP.clear();

	SA_ConstRingIterator<DmsPointType> rb(polyRef, 0), re(polyRef, -1);
	if (rb == re)
		return;
	auto ri = rb;


	static auto factory = geos::geom::GeometryFactory::create();

	std::vector< geos::geom::Polygon> resPolygons;
	geos::geom::LinearRing helperRing, currRing;
	std::vector< geos::geom::LinearRing> currInnerRings;
	std::vector<geos::geom::Coordinate> helperRingCoords;

	SA_ConstRingIterator<DmsPointType>
		rb(polyRef, 0),
		re(polyRef, -1);
	auto ri = rb;
	//			dbg_assert(ri != re);
	if (ri == re)
		return;

	bool outerOrientationCW = true;
	for (; ri != re; ++ri)
	{
		assert((*ri).begin() != (*ri).end());
		assert((*ri).begin()[0] == (*ri).end()[-1]); // closed ?

		helperRingCoords.clear();
		for (const auto& p : *ri)
		{
			helperRingCoords.emplace_back(geos::geom::Coordinate(p.x, p.y));
		}
		helperRing = geos::geom::LinearRing(std::move(helperRingCoords), factory);
		MG_CHECK(helperRing.isClosed());

		bool currOrientationCW = !geos::algorithm::Orientation::isCCW(helperRing.getCoordinatesRO());
		MG_CHECK(ri != rb || currOrientationCW);

		if (ri == rb || currOrientationCW == outerOrientationCW)
		{
			if (ri != rb && !currRing.isEmpty())
			{
				resPolygons.emplace_back(factory->createPolygon(std::move(currRing), std::move(currInnerRings)));
				currInnerRings.clear();
			}
			currRing = helperRing;
			outerOrientationCW = currOrientationCW;

/* NYI
			// skip outer rings that intersect with a previous outer ring if innerRings are skipped
			if (!mustInsertInnerRings)
			{
				SizeT polygonIndex = 0;
				while (polygonIndex < resMP.size())
				{
					auto currPolygon = resMP.begin() + polygonIndex;
					if (boost::geometry::intersects(currPolygon->outer(), helperPolygon.outer()))
					{
						if (boost::geometry::within(currPolygon->outer(), helperPolygon.outer()))
						{
							resMP.erase(currPolygon);
							continue;
						}
						if (boost::geometry::within(helperPolygon.outer(), currPolygon->outer()))
						{
							helperPolygon.clear();
							assert(helperPolygon.outer().empty() && helperPolygon.inners().empty());
							break;
						}

						if (boost::geometry::overlaps(currPolygon->outer(), helperPolygon.outer()))
							throwDmsErrF("OuterPolygon: unexpected overlap of two outer rings in %s", AsString(polyRef).c_str());

						// a combination of touching outer rings such as in an 8 shape is 
					}
					polygonIndex++;
				}
			}
*/
		}
		else if (mustInsertInnerRings)
		{
			currInnerRings.emplace_back(helperRing);
		}
	}
	if (!currRing.isEmpty())
	{
		resPolygons.emplace_back(factory->createPolygon(std::move(currRing), std::move(currInnerRings)));
	}
	return factory->createMultiPolygon(std::move(resPolygons));
}


template <dms_sequence E>
void geos_assign(E&& ref, const geos::geom::Coordinate& c)
{
	ref.emplace_back(shp2dms_order(c.x, c.y));

}

template <dms_sequence E>
void geos_assign(E&& ref, const geos::geom::LinearRing& ringData)
{
	for (const auto& c : ringData.getCoordinatesRO())
		geos_assign(ref, c);
}

template <dms_sequence E>
void geos_assign(E&& ref, const geos::geom::Polygon& poly)
{
	geos_assign(ref, poly.getExteriorRing());
	SizeT irCount = poly.getNumInteriorRing();
	for (SizeT ir = 0; ir != irCount; ++ir)
		geos_assign(ref, poly.getInteriorRingN(ir));

	if (irCount)
	{
		--irCount;
		while (irCount)
			geos_assign(ref, poly.getInteriorRingN(--irCount).back());

		geos_assign(ref, poly.getExteriorRing().back());
	}
}


template <dms_sequence E>
void geos_assign_mp(E&& ref, const geos::geom::MultiPolygon& mpData)
{
	ref.clear();

	if (mpData.isEmpty())
		return;

	SizeT i = 0, n = mpData.getNumGeometries();
	assert(n != 0); // follows from poly.size()
	SizeT count = n - 1;

	for (; i != n; ++i)
	{
		const auto* poly = mpData.getGeometryN(i);
		count += poly->getExteriorRing()->getNumPoints();
		for (SizeT ir=0, irCount = poly->getNumInteriorRing(); ir != irCount; ++ir)
			count += poly->getInteriorRingN(ir)->getNumPoints() + 1;
	}

	ref.reserve(count);

	for (const auto& p : mpData)
	{
		geos_assign(ref, p);
	}
	while (--n)
	{ 
		geos_assign (ref, mpData.getGeometryN(n).getExteriorRing().back());
	}

	assert(ref.size() == count);
}


#endif //!defined(DMS_GEO_GEOS_TRAITS_H)
