#include "boost/geometry.hpp"
#include "boost/geometry/algorithms/union.hpp"

namespace bg = boost::geometry;

constexpr bool ClockWise = true;
constexpr bool Closed    = true; // each ring is closed, i.e. last point == first point

using Point        = bg::model::point<double, 2, bg::cs::cartesian>;
using Ring         = bg::model::ring   <Point, ClockWise, Closed>;
using Polygon      = bg::model::polygon<Point, ClockWise, Closed>;
using MultiPolygon = bg::model::multi_polygon<Polygon>;

int main(int argc, char** argv)
{
	Ring s1{
		{ 173904.25160630842, 604340     }, // A
		{ 173803.203125     , 604351.875 }, // B
		{ 173802.59375      , 604351.9375}, // C
		{ 173802.578125     , 604351.9375}, // D
		{ 173797.5588235294 , 604360     }, // E
		{ 174000            , 604360     }, // F
		{ 174000            , 604340     }, // G
		{ 173904.25160630842, 604340     }  // A
	};
	std::cout << "is_valid(s1) = " << bg::is_valid(s1) << std::endl;
	std::cout << "area(s1)     = " << bg::area(s1) << std::endl;

	Ring s2{
		{ 173797.5588235294 , 604360     }, // E
		{ 173802.578125     , 604351.9375}, // D
		{ 173802.59375      , 604351.9375}, // C
		{ 173794.765625     , 604352.875 }, // H
		{ 173735.58701923076, 604360     }, // I
		{ 173797.5588235294 , 604360     }  // E
	};
	std::cout << "is_valid(s2) = " << bg::is_valid(s2) << std::endl;
	std::cout << "area(s2)     = " << bg::area(s2) << std::endl;

	MultiPolygon result;
	bg::union_(s1, s2, result);

	if (result.empty())
		std::cout << "result of union(s1, s2) is empty" << std::endl;
	else for (auto r: result)
	{
		std::cout << "outer" << std::endl;
		for (auto p : r.outer())
		{
			std::cout << bg::get<0>(p) << " " << bg::get<1>(p) << std::endl;
		}
	}
}
