#include "SystemTest.h"

/*
#include "dbg/Debug.h"
#include "geo/BaseBounds.h"
#include "ptr/OwningPtr.h"

#include <iostream>
#include <algorithm>
*/
//#include <vector>
/*
#include <boost/random/mersenne_twister.hpp>
#include <boost/random/uniform_real.hpp>
#include <boost/random/uniform_01.hpp>
*/

// ============== TestConv

#include "SpatialAnalyzer.h"

template <typename value_type, typename FIter>
value_type sum(FIter b, FIter e)
{
	value_type t = value_type();
	while (b!=e)
	{
		t += *b;
		++b;
	}
	return t;
}

void testConv()
{
	typedef double value_type;
	const unsigned GRID_X = 2700;
	const unsigned GRID_Y = 3250;
	const unsigned FP_S   = 201;
	const unsigned GRID_X2 = GRID_X + FP_S-1;

	DBG_START("TestConv", "All", true);


	TOwnedGrid<value_type> 
		xxx(GridPoint(GRID_X2,GRID_Y)), 
		yyy(GridPoint(FP_S,FP_S)); 

	random_uniform_fill(xxx.begin(), xxx.end(), 1);
	random_uniform_fill(yyy.begin(), yyy.end(), 2);

	Float64 sx = sum<Float64>(xxx.begin(), xxx.end());
	Float64 sy = sum<Float64>(yyy.begin(), yyy.end());

	DBG_TRACE(("xxx.size() = %d sum = %lf", xxx.size(), sx));
	DBG_TRACE(("yyy.size() = %d sum = %lf", yyy.size(), sy));

	TOwnedGrid<value_type>  zzz(xxx.GetSize());

	Potential(AT_PotentialIpps,
		TGrid<const value_type>(xxx.GetSize(), xxx.begin()),
		zzz,
		TGrid<const value_type>(yyy.GetSize(), yyy.begin()),
		GridPoint(FP_S/2, FP_S/2)
	);
	Float64 sz = sum<Float64>(zzz.begin(), zzz.end());
	DBG_TRACE(("sx*sy = %lf, sz = %lf", sx*sy, sz));
}

