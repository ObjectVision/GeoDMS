// Copyright (C) 1998-2024 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

// OperConvTransform.cpp - RD2LatLong transformation operator instantiations
// Split from OperConv.cpp for parallel compilation

#include "ClcPch.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif

#include "OperConv.h"

namespace {

// *****************************************************************************
//			COORDINATE CONVERSION FUNCTIONS (local to this unit)
// *****************************************************************************

DPoint rd2LatLong(DPoint rd)
{
	double
		x = rd.first,
		y = rd.second;

	if (x < 0 || x > 290000)
		throwErrorD("Argument Error", "RD2LatLong requires x to be between 0 and 290000");
	if (y < 290000 || y > 630000)
		throwErrorD("Argument Error", "RD2LatLong requires y to be between 290000 and 630000");

	const double
		x0 = 155000.000,
		y0 = 463000.000,

		l0 = 5.387638889,
		f0 = 52.156160556,

		a01 = 3236.0331637, b10 = 5261.3028966,
		a20 = -32.5915821, b11 = 105.9780241,
		a02 = -0.2472814, b12 = 2.4576469,
		a21 = -0.8501341, b30 = -0.8192156,
		a03 = -0.0655238, b31 = -0.0560092,
		a22 = -0.0171137, b13 = 0.0560089,
		a40 = 0.0052771, b32 = -0.0025614,
		a23 = -0.0003859, b14 = 0.0012770,
		a41 = 0.0003314, b50 = 0.0002574,
		a04 = 0.0000371, b33 = -0.0000973,
		a42 = 0.0000143, b51 = 0.0000293,
		a24 = -0.0000090, b15 = 0.0000291;

	double
		dx = (x - x0) / 100000,
		dy = (y - y0) / 100000;

	double
		dx2 = dx * dx,
		dx3 = dx2 * dx,
		dx4 = dx2 * dx2,
		dx5 = dx4 * dx,

		dy2 = dy * dy,
		dy3 = dy2 * dy,
		dy4 = dy2 * dy2,
		dy5 = dy4 * dy;

	double
		df = a01 * dy
		+ a20 * dx2
		+ a02 * dy2
		+ a21 * dx2 * dy
		+ a03 * dy3
		+ a40 * dx4
		+ a22 * dx2 * dy2
		+ a04 * dy4
		+ a41 * dx4 * dy
		+ a23 * dx2 * dy3
		+ a42 * dx4 * dy2
		+ a24 * dx2 * dy4,

		dl = b10 * dx
		+ b11 * dx * dy
		+ b30 * dx3
		+ b12 * dx * dy2
		+ b31 * dx3 * dy
		+ b13 * dx * dy3
		+ b50 * dx5
		+ b32 * dx3 * dy2
		+ b14 * dx * dy4
		+ b51 * dx5 * dy
		+ b33 * dx3 * dy3
		+ b15 * dx * dy5;

	return DPoint(l0 + dl / 3600, f0 + df / 3600);
}

DPoint rd2LatLongEd50(DPoint rd)
{
	DPoint lf = rd2LatLong(rd);
	double
		l = lf.first,
		f = lf.second;

	return DPoint(
		l + (+89.120 + 3.708 * (f - 52) - 17.176 * (l - 5)) / 100000
		, f + (-18.00 - 14.723 * (f - 52) - 1.029 * (l - 5)) / 100000
	);
}

DPoint rd2LatLongWgs84(DPoint rd)
{
	DPoint lf = rd2LatLong(rd);
	double
		l = lf.first,
		f = lf.second;

	return DPoint(
		l + (-37.902 + 0.329 * (f - 52) - 14.667 * (l - 5)) / 100000
		, f + (-96.862 - 11.714 * (f - 52) - 0.125 * (l - 5)) / 100000
	);
}

DPoint latLongWgs842rd(DPoint ll)
{
	Float64
		longitude = ll.first,
		latitude = ll.second;
	Float64
		dL = 0.36 * (longitude - 5.38720621),
		dF = 0.36 * (latitude - 52.15517440);
	Float64
		dF2 = dF * dF,
		dF3 = dF * dF2,
		dL2 = dL * dL,
		dL3 = dL * dL2,
		dL4 = dL2 * dL2;
	Float64
		somX
		= 190094.945 * dL
		+ -11832.228 * dF * dL
		+ -144.221 * dF2 * dL
		+ -32.391 * dL3
		+ -0.705 * dF
		+ -2.340 * dF3 * dL
		+ -0.608 * dF * dL3
		+ -0.008 * dL2
		+ 0.148 * dF2 * dL3,
		somY
		= 309056.544 * dF
		+ 3638.893 * dL2
		+ 73.077 * dF2
		+ -157.984 * dF * dL2
		+ 59.788 * dF3
		+ 0.433 * dL
		+ -6.439 * dF2 * dL2
		+ -0.032 * dF * dL
		+ 0.092 * dL4
		+ -0.054 * dF * dL4;

	return DPoint(155000 + somX, 463000 + somY);
}

DPoint rd2LatLongGE(DPoint rd)
{
	const double Xc = 17.78369991;
	const double Yc = 40.18290427;

	const double Xx = -0.0000749081;
	const double Xy = -0.0000128123;
	const double Yx = -0.0000064425;
	const double Yy = -0.0000846220;

	return rd2LatLongWgs84(
		DPoint(
			rd.first + Xc + Xx * rd.first + Xy * rd.second
			, rd.second + Yc + Yx * rd.first + Yy * rd.second
		)
	);
}

// *****************************************************************************
//			COORDINATE CONVERSION FUNCTORS
// *****************************************************************************

template<typename TR, typename TA, DPoint CF(DPoint)>
struct ConvertFunc : unary_func<TR, TA>
{
	TR operator() (const TA& p) const
	{
		if (!IsDefined(p))
			return UNDEFINED_VALUE(TR);
		DPoint res = CF(DPoint(p.Col(), p.Row()));
		return shp2dms_order<scalar_of_t<TR>>(res.first, res.second);
	}
};

template<typename TR, typename TA> struct RD2LatLong      : ConvertFunc<TR, TA, rd2LatLong     > { RD2LatLong     (const AbstrUnit*, const AbstrUnit*) {} };
template<typename TR, typename TA> struct RD2LatLongEd50  : ConvertFunc<TR, TA, rd2LatLongEd50 > { RD2LatLongEd50 (const AbstrUnit*, const AbstrUnit*) {} };
template<typename TR, typename TA> struct RD2LatLongWgs84 : ConvertFunc<TR, TA, rd2LatLongWgs84> { RD2LatLongWgs84(const AbstrUnit*, const AbstrUnit*) {} };
template<typename TR, typename TA> struct RD2LatLongGE    : ConvertFunc<TR, TA, rd2LatLongGE   > { RD2LatLongGE   (const AbstrUnit*, const AbstrUnit*) {} };
template<typename TR, typename TA> struct LatLongWgs842RD : ConvertFunc<TR, TA, latLongWgs842rd> { LatLongWgs842RD(const AbstrUnit*, const AbstrUnit*) {} };

// *****************************************************************************
//			Operator groups and instantiations
// *****************************************************************************

CommonOperGroup cog_RD2LatLong("RD2LatLong");
CommonOperGroup cog_RD2LatLongEd50("RD2LatLongEd50");
CommonOperGroup cog_RD2LatLongWgs84("RD2LatLongWgs84");
CommonOperGroup cog_RD2LatLongGE("RD2LatLongGE");
CommonOperGroup cog_LatLongWgs842RD("LatLongWgs842RD");

template <typename TR, typename TA>
struct Rd2LlOpers
{
	Rd2LlOpers()
		: cRD2LL(&cog_RD2LatLong)
		, cRD2LLEd50(&cog_RD2LatLongEd50)
		, cRD2LLWgs84(&cog_RD2LatLongWgs84)
		, cRD2LLGE(&cog_RD2LatLongGE)
		, cLLWgs842RD(&cog_LatLongWgs842RD)
	{}

	TransformAttrOperator<TR, TA, tl::bind_placeholders<RD2LatLong, ph::_1, ph::_2> > cRD2LL;
	TransformAttrOperator<TR, TA, tl::bind_placeholders<RD2LatLongEd50, ph::_1, ph::_2> > cRD2LLEd50;
	TransformAttrOperator<TR, TA, tl::bind_placeholders<RD2LatLongWgs84, ph::_1, ph::_2> > cRD2LLWgs84;
	TransformAttrOperator<TR, TA, tl::bind_placeholders<RD2LatLongGE, ph::_1, ph::_2> > cRD2LLGE;
	TransformAttrOperator<TR, TA, tl::bind_placeholders<LatLongWgs842RD, ph::_1, ph::_2> > cLLWgs842RD;
};

template <typename TR, typename TA>
struct Rd2LlOpersWithSeq : Rd2LlOpers<TR, TA>
{
	Rd2LlOpers<
		typename sequence_traits<TR>::container_type
		, typename sequence_traits<TA>::container_type
	>	m_SequenceOpers;
};

// RD to LatLong transformations for points and sequences
Rd2LlOpersWithSeq<DPoint, DPoint> rd2llDD;
Rd2LlOpersWithSeq<DPoint, FPoint> rd2llDF;
Rd2LlOpersWithSeq<FPoint, FPoint> rd2llFF;

} // end anonymous namespace
