#include <boost/polygon/polygon.hpp>

template <typename PointType>
void convolve_two_segments(
	std::vector<PointType>& figure,
	const std::pair<PointType, PointType >& a,
	const std::pair<PointType, PointType >& b)
{
	using namespace boost::polygon;
	figure.clear();
	figure.push_back(PointType(a.first));
	figure.push_back(PointType(a.first));
	figure.push_back(PointType(a.second));
	figure.push_back(PointType(a.second));
	convolve(figure[0], b.second);
	convolve(figure[1], b.first);
	convolve(figure[2], b.first);
	convolve(figure[3], b.second);
}

template <typename CT, typename itrT1, typename itrT2>
void convolve_two_point_sequences(boost::polygon::polygon_set_data<CT>& result, itrT1 ab, itrT1 ae, itrT2 bb, itrT2 be)
{
	using namespace boost::polygon;
	using point = point_data<CT>;
	using polygon = boost::polygon::polygon_with_holes_data<CT>;

	if (ab == ae || bb == be)
		return;

	typename std::iterator_traits<itrT1>::value_type prev_a = *ab++;
	std::vector<point> vec;
	polygon poly;

	for (; ab != ae; ++ab)
	{
		itrT2 tmpb = bb;
		typename std::iterator_traits<itrT2>::value_type prev_b = *tmpb++;

		for (; tmpb != be; ++tmpb)
		{
			convolve_two_segments(vec, std::make_pair(prev_b, *tmpb), std::make_pair(prev_a, *ab));
			set_points(poly, vec.begin(), vec.end());
			result.insert(poly, false);
			prev_b = *tmpb;
		}
		prev_a = *ab;
	}
}


int main(int argc, char** argv)
{
	typedef boost::polygon::point_data<int> point_type;
	typedef boost::polygon::polygon_with_holes_data<int> poly_type;
	point_type polyPoints[]  = 
		{
			{ 102785843, 443558000 },
			{ 102621648, 443753218 },
			{ 102681929, 443603250 },
			{ 102707796, 443579187 },
			{ 102719109, 443590500 },
			{ 102770015, 443542187 },
			{ 102785843, 443558000 },
	},
		kernelPoints[] = {
			{ 0, 10000 },
			{ 7071, 7071 },
			{ 10000, 0 },
			{ 7071, -7071 },
			{ 0, -10000 },
			{ -7071, -7071 },
			{ -10000, 0 },
			{ -7071, 7071 },
			{ 0, 10000 },
			};
/*	!!!!!! With the following 3 lines enabled, the program produces "geometry seems ok". Without, it produces  "geometry disappeared"
	point_type basePoint = polyPoints[0];
	for (auto& p : polyPoints) 
		boost::polygon::deconvolve(p, basePoint);
//*/

	poly_type poly, kernel;
	set_points(poly, polyPoints, polyPoints + 7);
	set_points(kernel, kernelPoints, kernelPoints + 9);

	boost::polygon::polygon_set_data<int> geometry;
	geometry.insert(poly);

	convolve_two_point_sequences(geometry, begin_points(kernel), end_points(kernel), begin_points(poly), end_points(poly));
	convolve(kernel, *begin_points(poly));
	geometry.insert(kernel);

	std::vector< poly_type > results;
	geometry.get(results);
	if (results.empty())
		std::cerr << "geometry disappeared";
	else
		std::cout << "geometry seems ok";
	return 0;
}