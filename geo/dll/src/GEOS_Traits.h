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
#include <geos/operation/polygonize/Polygonizer.h>


inline auto geos_factory() -> const geos::geom::GeometryFactory*
{
	// Create a GeometryFactory with the precision model
//	static auto pm = std::make_unique<geos::geom::PrecisionModel>(1024.0);
//	static auto geometryFactory = geos::geom::GeometryFactory::create(pm.get());
//	return geometryFactory.get();
	return geos::geom::GeometryFactory::getDefaultInstance();
}

template <typename P>
struct geos_union_poly_traits
{
	//	using coordinate_type = scalar_of_t<P>;
	using coordinate_type = Float64;
	using point_type = Point<coordinate_type>;
	using ring_type = std::vector<point_type>;

	using polygon_with_holes_type = geos::geom::Polygon;

	using multi_polygon_type = geos::geom::MultiPolygon;
	using polygon_result_type = multi_polygon_type;
};

template <typename DmsPointType>
auto geos_Coordinate(const DmsPointType& p)
{
	return geos::geom::Coordinate(p.X(), p.Y());
}

template <typename DmsPointType>
auto geos_create_multi_linestring(const DmsPointType* begin, const DmsPointType* beyond)
-> std::unique_ptr<geos::geom::Geometry>
{
	auto curr = begin;
	std::vector< geos::geom::Coordinate> lineStringCoords;
	std::vector< std::unique_ptr<geos::geom::LineString>> resLineStrings;
	while (curr!= beyond)
	{
		lineStringCoords.clear();
		while (curr != beyond && IsDefined(*curr))
		{
			lineStringCoords.emplace_back(geos_Coordinate(*curr));
			++curr;
		}
		if (!lineStringCoords.empty())
		{
			auto ls = geos_factory()->createLineString(std::move(lineStringCoords));
			resLineStrings.emplace_back(std::move(ls));
		}
		if (curr == beyond)
			break;
		assert(!IsDefined(*curr));
		++curr;
	}
	if (resLineStrings.empty())
		return {};
	if (resLineStrings.size() == 1)
		return std::unique_ptr<geos::geom::Geometry>( resLineStrings[0].release() );
	return std::unique_ptr<geos::geom::Geometry>(geos_factory()->createGeometryCollection(std::move(resLineStrings)).release() );
}

template <typename CoordType>
auto geos_circle(double radius, int pointsPerCircle) -> std::unique_ptr<geos::geom::LinearRing>
{
	if (pointsPerCircle < 3)
		pointsPerCircle = 3;
	auto anglePerPoint = 2.0 * std::numbers::pi_v<double> / pointsPerCircle;
//	auto points = create_circle_points<CoordType>(radius, pointsPerCircle);
	std::vector<geos::geom::Coordinate> points;
	points.reserve(pointsPerCircle+1);
	for (int i = 0; i < pointsPerCircle; ++i) {
		double angle = i * anglePerPoint;
		int x = static_cast<int>(radius * std::cos(angle));
		int y = static_cast<int>(radius * std::sin(angle));
		points.emplace_back(x, y);
	}
	points.emplace_back(points.front());
	return geos_factory()->createLinearRing(std::move(points));
}


template <typename DmsPointType>
struct geos_create_linear_ring_helper_data
{
	std::vector<DmsPointType> helperRingPoints;
	std::vector<geos::geom::Coordinate> helperRingCoords;
};

template <typename DmsPointType>
auto geos_create_linear_ring(const DmsPointType* begin, const DmsPointType* beyond, geos_create_linear_ring_helper_data<DmsPointType>& tmp)
-> std::unique_ptr<geos::geom::LinearRing>
{
	assert(begin != beyond);
	assert(begin[0] == beyond[-1]); // closed ?

	tmp.helperRingPoints.clear();
	tmp.helperRingPoints.reserve(beyond - begin);
	for (auto ptr = begin; ptr != beyond; ++ptr)
		tmp.helperRingPoints.emplace_back(*ptr);
	remove_adjacents_and_spikes(tmp.helperRingPoints);
	if (tmp.helperRingPoints.size() < 3)
		return {};

	tmp.helperRingCoords.clear();
	tmp.helperRingCoords.reserve(tmp.helperRingPoints.size() + 1);
	for (const auto& p : tmp.helperRingPoints)
		tmp.helperRingCoords.emplace_back(geos_Coordinate(p));
	tmp.helperRingCoords.emplace_back(geos_Coordinate(tmp.helperRingPoints[0])); // close ring.

	auto result = geos_factory()->createLinearRing(std::move(tmp.helperRingCoords));
	MG_CHECK(result->isClosed());
	return result;
}

template <typename DmsPointType>
auto geos_create_polygons(SA_ConstReference<DmsPointType> polyRef, bool mustInsertInnerRings = true)
-> std::unique_ptr<geos::geom::Geometry>
{
	assert(mustInsertInnerRings);

	SA_ConstRingIterator<DmsPointType> rb(polyRef, 0), re(polyRef, -1);
	if (rb == re)
		return {};
	std::sort(rb.m_IndexBuffer.begin(), rb.m_IndexBuffer.end());

	std::vector< std::unique_ptr<geos::geom::Polygon>> resPolygons;
	std::unique_ptr<geos::geom::LinearRing> currRing;
	std::vector< std::unique_ptr<geos::geom::LinearRing>> currInnerRings;

	geos_create_linear_ring_helper_data<DmsPointType> tmpRingData;

	auto ri = rb;

	bool outerOrientationCW = true;
	for (; ri != re; ++ri)
	{
		auto helperRing = geos_create_linear_ring((*ri).begin(), (*ri).end(), tmpRingData);
		if (helperRing.get() == nullptr)
			continue;

		bool currOrientationCW = !geos::algorithm::Orientation::isCCW(helperRing->getCoordinatesRO());
		MG_CHECK(ri != rb || currOrientationCW);

		if (ri == rb || currOrientationCW == outerOrientationCW)
		{
			if (ri != rb && currRing && !currRing->isEmpty())
			{
				resPolygons.emplace_back(geos_factory()->createPolygon(std::move(currRing), std::move(currInnerRings)));
				currInnerRings.clear();
			}
			currRing = std::move( helperRing );
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
			currInnerRings.emplace_back(std::move(helperRing));
		}
	}
	if (currRing && !currRing->isEmpty())
	{
		resPolygons.emplace_back(geos_factory()->createPolygon(std::move(currRing), std::move(currInnerRings)));
	}
	if (resPolygons.empty())
		return {};

//	geos::operation::polygonize::Polygonizer polygonizer;
//	for (const auto& p : resPolygons)
//		polygonizer.add(p.get());
//	resPolygons = polygonizer.getPolygons();

	if (resPolygons.size() == 1)
	{
		auto r = std::move(resPolygons.front());
		r->normalize();
		return r;
	}

	auto r = geos_factory()->createMultiPolygon(std::move(resPolygons));
	r->normalize();
	return r;
}


template <dms_sequence E>
void geos_write_point(E&& ref, const geos::geom::Coordinate& c)
{
	ref.emplace_back(shp2dms_order(c.x, c.y));

}

template <dms_sequence E>
auto geos_write_lr(E&& ref, const geos::geom::LinearRing* lr) -> std::optional<geos::geom::Coordinate>
{
	assert(lr);
	const auto* coords = lr->getCoordinatesRO();
	assert(coords);
	auto s = coords->getSize();
	if (s <= 3)
		return {};

	MG_CHECK(coords->getAt(0) == coords->getAt(s - 1));
	for (SizeT i = 0; i != s; ++i)
		geos_write_point(ref, coords->getAt(i));
	return coords->getAt(0);
}

template <dms_sequence E>
void geos_write_back(E&& ref, const geos::geom::LinearRing* lr)
{
	assert(lr);
	geos_write_point(ref, lr->getCoordinatesRO()->back());
}

template <dms_sequence E>
auto geos_write_polygon_with_holes(E&& ref, const geos::geom::Polygon* poly)  -> std::optional<geos::geom::Coordinate>
{
	assert(poly);
	auto outerBackTrackPoint = geos_write_lr(std::forward<E>(ref), poly->getExteriorRing());
	if (!outerBackTrackPoint)
		return {};

	std::vector<geos::geom::Coordinate> backTrackPoints;
	SizeT irCount = poly->getNumInteriorRing();
	for (SizeT ir = 0; ir != irCount; ++ir)
	{
		auto backTrackPoint = geos_write_lr(std::forward<E>(ref), poly->getInteriorRingN(ir));
		if (backTrackPoint)
			backTrackPoints.emplace_back(*backTrackPoint);
	}

	if (!backTrackPoints.empty())
	{
		backTrackPoints.pop_back();
		while (!backTrackPoints.empty())
		{
			geos_write_point(std::forward<E>(ref), backTrackPoints.back());
			backTrackPoints.pop_back();
		}
		geos_write_point(std::forward<E>(ref), *outerBackTrackPoint);
	}
	return outerBackTrackPoint;
}

template <dms_sequence E>
void geos_assign_polygon_with_holes(E&& ref, const geos::geom::Polygon* poly)
{
	SizeT count = poly->getExteriorRing()->getNumPoints();
	assert(count);
	for (auto irCount = poly->getNumInteriorRing(); irCount;)
		count += poly->getInteriorRingN(--irCount)->getCoordinatesRO()->getSize() + 1;

	ref.clear();
	ref.reserve(count);

	geos_write_polygon_with_holes(std::forward<E>(ref), poly);
	assert(ref.size() == count);
}

template <dms_sequence E>
auto geos_write_mp(E&& ref, const geos::geom::MultiPolygon* mp) -> std::optional<geos::geom::Coordinate>
{
	assert(mp);
	if (mp->isEmpty())
		return {};

	SizeT polygonCount = mp->getNumGeometries();
	assert(polygonCount != 0); // follows from poly.size()

	std::optional<geos::geom::Coordinate> firstBackTrackPoint;
	std::vector<geos::geom::Coordinate> backTrackPoints;

	for (SizeT i = 0; i != polygonCount; ++i)
	{
		const auto* poly = debug_cast<const geos::geom::Polygon*>(mp->getGeometryN(i));
		auto backTrackPoint = geos_write_polygon_with_holes(std::forward<E>(ref), poly);
		if (backTrackPoint)
		{
			if (!firstBackTrackPoint)
				firstBackTrackPoint = *backTrackPoint;
			backTrackPoints.emplace_back(*backTrackPoint);
		}
	}
	if (!backTrackPoints.empty())
	{ 
		backTrackPoints.pop_back();
		while (!backTrackPoints.empty())
		{
			geos_write_point(std::forward<E>(ref), backTrackPoints.back());
			backTrackPoints.pop_back();
		}
	}
	return firstBackTrackPoint;
}

template <dms_sequence E>
void geos_assign_mp(E&& ref, const geos::geom::MultiPolygon* mp)
{
	assert(mp);
	ref.clear();

	if (mp->isEmpty())
		return;

	SizeT polygonCount = mp->getNumGeometries();
	assert(polygonCount != 0); // follows from poly.size()
	SizeT count = polygonCount - 1;

	for (SizeT i = 0; i != polygonCount; ++i)
	{
		const auto* poly = mp->getGeometryN(i);
		auto nrOuterPoints = poly->getExteriorRing()->getNumPoints();
		if (nrOuterPoints > 3)
		{
			count += nrOuterPoints;
			for (SizeT ir = 0, irCount = poly->getNumInteriorRing(); ir != irCount; ++ir)
			{
				auto nrInnerPoints = poly->getInteriorRingN(ir)->getNumPoints();
				if (nrInnerPoints > 3)
					count += nrInnerPoints + 1;
			}
		}
	}

	ref.reserve(count);
	geos_write_mp(std::move(ref), mp);
}

/*
inline auto getPolygonsFromGeometryCollection(const geos::geom::GeometryCollection* gc) -> std::unique_ptr<geos::geom::MultiPolygon>
{
	// Use Polygonizer to form polygons from the GeometryCollection
	geos::operation::polygonize::Polygonizer polygonizer;
	polygonizer.add(gc);

	auto pwh = polygonizer.getPolygons();
	// Get the polygons formed by the Polygonizer
	return geos_factory()->createMultiPolygon(std::move(pwh));
}
*/

inline void cleanupPolygons(std::unique_ptr<geos::geom::Geometry>& r)
{
	if (!r)
		return;
	r->normalize();
//	if (auto gc = dynamic_cast<geos::geom::GeometryCollection*>(r.get()))
//		r.reset(getPolygonsFromGeometryCollection(gc).release());
}

template <typename E>
auto geos_write_geometry(E&& ref, const geos::geom::Geometry* geometry) -> std::optional<geos::geom::Coordinate>
{
	if (!geometry || geometry->isEmpty())
		return {};

	if (auto mp = dynamic_cast<const geos::geom::MultiPolygon*>(geometry))
		return geos_write_mp(std::forward<E>(ref), mp);

	if (auto gc = dynamic_cast<const geos::geom::GeometryCollection*>(geometry))
	{
		std::vector<geos::geom::Coordinate> backTrackPoints;

		std::optional<geos::geom::Coordinate> firstBackTrackPoint;
		for (SizeT i = 0, n = gc->getNumGeometries(); i != n; ++i)
		{
			auto backTrackPoint = geos_write_geometry(std::forward<E>(ref), gc->getGeometryN(i));
			if (backTrackPoint)
			{
				if (!firstBackTrackPoint)
					firstBackTrackPoint = *backTrackPoint;
				backTrackPoints.emplace_back(*backTrackPoint);
			}
		}
		if (!backTrackPoints.empty())
		{
			backTrackPoints.pop_back();
			while (!backTrackPoints.empty())
			{
				geos_write_point(std::forward<E>(ref), backTrackPoints.back());
				backTrackPoints.pop_back();
			}
		}
		return firstBackTrackPoint;
	}

	if (auto poly = dynamic_cast<const geos::geom::Point*>(geometry))
		return {};

	if (auto poly = dynamic_cast<const geos::geom::Polygon*>(geometry))
		return geos_write_polygon_with_holes(std::forward<E>(ref), poly);

	if (auto lr = dynamic_cast<const geos::geom::LinearRing*>(geometry))
		return geos_write_lr(std::forward<E>(ref), lr);

	if (auto lr = dynamic_cast<const geos::geom::LineString*>(geometry))
		return {};

	throwDmsErrF("geos_write_geometry: unsupported geometry type: %s", geometry->toText().c_str());
}

template <typename E>
void geos_assign_geometry(E&& ref, const geos::geom::Geometry* geometry)
{
	ref.clear();
	if (!geometry || geometry->isEmpty())
		return;

	if (auto mp = dynamic_cast<const geos::geom::MultiPolygon*>(geometry))
	{
		geos_assign_mp(std::forward<E>(ref), mp);
		return;
	}
	if (auto poly = dynamic_cast<const geos::geom::Polygon*>(geometry))
	{
		geos_assign_polygon_with_holes(std::forward<E>(ref), poly);
		return;
	}
	geos_write_geometry(std::forward<E>(ref), geometry);
}

template <typename RI>
auto geos_split_write_mp(RI resIter, const geos::geom::MultiPolygon* mp) -> RI
{
	for (SizeT i = 0, np = mp->getNumGeometries(); i != np; ++i)
	{
		auto backTrackPtr = geos_write_polygon_with_holes(*resIter, mp->getGeometryN(i));
		if (backTrackPtr)
			++resIter;
	}
	return resIter;
}

template <typename RI>
auto geos_split_write_geometry(RI resIter, const geos::geom::Geometry* geometry) -> RI
{
	if (auto mp = dynamic_cast<const geos::geom::MultiPolygon*>(geometry))
		return geos_split_write_mp(resIter, mp);

	if (auto poly = dynamic_cast<const geos::geom::Polygon*>(geometry))
	{
		geos_write_polygon_with_holes(*resIter, poly);
		return ++resIter;
	}
	if (auto gc = dynamic_cast<const geos::geom::GeometryCollection*>(geometry))
	{
		for (SizeT i = 0, n = gc->getNumGeometries(); i != n; ++i)
			resIter = geos_split_write_geometry(resIter, gc->getGeometryN(i));

		return resIter;
	}
	if (auto poly = dynamic_cast<const geos::geom::LinearRing*>(geometry))
	{
		auto backTrackPoint = geos_write_lr(*resIter, poly);
		if (backTrackPoint)
			return ++resIter;
	}
	if (auto gc = dynamic_cast<const geos::geom::Point*>(geometry))
		return resIter;

	if (geometry)
		throwDmsErrF("geos_split_write_geometry: unsupported geometry type: %s", geometry->toText().c_str());

	return resIter;
}

template <typename RI>
auto geos_split_assign_geometry(RI resIter, const geos::geom::Geometry* geometry) -> RI
{
	resIter->clear();
	if (!geometry || geometry->isEmpty())
		return resIter;

	if (auto mp = dynamic_cast<const geos::geom::MultiPolygon*>(geometry))
		return geos_split_write_mp(resIter, mp);

	if (auto poly = dynamic_cast<const geos::geom::Polygon*>(geometry))
	{
		geos_assign_polygon_with_holes(*resIter, poly);
		return ++resIter;
	}
	return geos_split_write_geometry(resIter, geometry);
}

struct geos_intersection {
	auto operator ()(const geos::geom::Geometry* a, const geos::geom::Geometry* b) const -> std::unique_ptr<geos::geom::Geometry>
	{
		auto r = a->intersection(b);
		cleanupPolygons(r);
		return r;
	}
};

struct geos_union {
	auto operator ()(const geos::geom::Geometry* a, const geos::geom::Geometry* b) const -> std::unique_ptr<geos::geom::Geometry>
	{
		auto  r = a->Union(b);
		cleanupPolygons(r);
		return r;
	}
};

struct geos_difference {
	auto operator ()(const geos::geom::Geometry* a, const geos::geom::Geometry* b) const -> std::unique_ptr<geos::geom::Geometry>
	{
		auto r = a->difference(b);
		cleanupPolygons(r);
		return r;
	}
};

struct geos_sym_difference {
	auto operator ()(const geos::geom::Geometry* a, const geos::geom::Geometry* b) const -> std::unique_ptr<geos::geom::Geometry>
	{
		auto r = a->symDifference(b);
		cleanupPolygons(r);
		return r;
	}
};

struct union_geos_multi_polygon
{
	using mp_ptr = std::unique_ptr<geos::geom::Geometry>;

	void operator()(mp_ptr& lhs, mp_ptr&& rhs) const
	{
		if (!lhs)
			lhs = std::move(rhs);
		else if (rhs)
			lhs = union_(lhs.get(), rhs.get());
	}
	geos_union union_;
};

template <typename E>
void dms_assign(std::unique_ptr<geos::geom::Geometry>& lhs, E&& ref)
{
	lhs = geos_create_polygons(std::forward<E>(ref));
}

template <typename E>
void dms_insert(std::unique_ptr<geos::geom::Geometry>& lhs, E&& ref)
{
	std::unique_ptr<geos::geom::Geometry> res; 
	dms_assign(res, std::forward<E>(ref));

	union_geos_multi_polygon union_;
	union_(lhs, std::move(res));
}



#endif //!defined(DMS_GEO_GEOS_TRAITS_H)
