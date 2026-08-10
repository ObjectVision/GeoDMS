// Copyright (C) 1998-2026 Object Vision b.v.
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

// SeparableMapping.cpp - the structural half of the #298 separability gate.
//
// Decides whether a (src, dst) CRS pair *may* be coordinate separable, i.e. whether the
// transformed x can depend on the source x alone and the transformed y on the source y alone.
// Non-templated, so it compiles once for all value types; the templated fast path that consumes
// this verdict lives in SeparableMapping.h.
//
// This is a NECESSARY condition, never a sufficient one. The caller must still verify
// numerically -- exactly, never with a tolerance -- before using the fast path, because a
// structural check classifies the declared CRSs and not the coordinate operation GDAL actually
// selected (we ask for OGR_CT_OP_SELECTION=BEST_ACCURACY, and GDAL may choose among candidate
// operations by area of use).

#include "ClcPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif

#include "OperConv.h"

#include <string_view>

namespace {

// *****************************************************************************
//			PROJ.4 definition scanning
// *****************************************************************************

// Calls f(key, value) for each whitespace-separated "+key=value" token of a PROJ.4 definition
// string. A bare "+key" yields an empty value; anything not starting with '+' is skipped.
template <typename Func>
void ScanProjParams(std::string_view def, Func&& f)
{
	size_t i = 0, e = def.size();
	while (i != e)
	{
		while (i != e && std::isspace(static_cast<unsigned char>(def[i])))
			++i;
		size_t b = i;
		while (i != e && !std::isspace(static_cast<unsigned char>(def[i])))
			++i;
		if (b == i)
			break;

		auto token = def.substr(b, i - b);
		if (token.front() != '+')
			continue;
		token.remove_prefix(1);

		auto eqPos = token.find('=');
		if (eqPos == std::string_view::npos)
			f(token, std::string_view{});
		else
			f(token.substr(0, eqPos), token.substr(eqPos + 1));
	}
}

// *****************************************************************************
//			The whitelist
// *****************************************************************************

// Geographic and NORMAL-ASPECT CYLINDRICAL projections: their easting formula reads only the
// longitude and their northing formula only the latitude, so the image of a rectangular grid is
// the product of the images of its two coordinate ranges. That is precisely the property #298
// exploits.
//
// Deliberately a whitelist, not a blacklist: anything unlisted falls back to the generic
// per-point loop, which is correct but slow. Adding a method here is a correctness claim about
// its formulas -- do not add one without checking that neither ordinate reads the other.
//
// Notably NOT separable, and easy to mistake for separable:
//   tmerc / utm / etmerc  - transverse aspect: easting depends on the latitude too
//   omerc                 - oblique aspect
//   lcc / aea / laea      - conic / azimuthal: both ordinates read both inputs
//   stere / sterea        - azimuthal
//   moll / sinu / robin   - pseudo-cylindrical: the parallels' length varies, so x reads phi
bool IsSeparableProjMethod(std::string_view method)
{
	return method == "longlat"   // geographic, any axis order or angular unit
		|| method == "latlong"   // PROJ's alias for the same
		|| method == "merc"      // Mercator 1SP / 2SP, incl. EPSG:3395
		|| method == "webmerc"   // Pseudo-Mercator, EPSG:3857
		|| method == "eqc"       // Equirectangular / Plate Carree
		|| method == "mill"      // Miller cylindrical
		|| method == "cea";      // Cylindrical equal area
}

// Whether a single CRS, as PROJ.4 sees it, is one of the separable families.
// +lon_0 +lat_ts +lat_0 +k_0 +x_0 +y_0 +units= +a= +b= are per-axis constants and are fine.
bool ProjDefIsSeparable(std::string_view def)
{
	bool sawProj = false, methodIsSeparable = false, rest = true;

	ScanProjParams(def, [&](std::string_view key, std::string_view value)
		{
			if (key == "proj")
			{
				sawProj = true;
				methodIsSeparable = IsSeparableProjMethod(value);
			}
			else if (key == "alpha" || key == "gamma")
				rest = false; // an oblique parameterisation of an otherwise separable method
			else if (key == "nadgrids")
			{
				// EPSG:3857 exports with "+nadgrids=@null", the explicit no-op grid -- rejecting
				// every +nadgrids would kill the very case #298 targets. Any real grid file is a
				// per-point datum shift and is not separable.
				rest = rest && (value == "@null");
			}
			else if (key == "geoidgrids")
				rest = false; // a vertical component, i.e. not a plain 2D map
		}
	);

	return sawProj && methodIsSeparable && rest;
}

// Reads the PROJ.4 form of srs and classifies it. Returns false when GDAL cannot express the
// CRS that way, which is itself a reason to refuse the fast path.
bool SpatialRefIsSeparable(const OGRSpatialReference& srs)
{
	CplString buffer; // frees m_Text with CPLFree
	if (srs.exportToProj4(&buffer.m_Text) != OGRERR_NONE || !buffer.m_Text)
		return false;
	return ProjDefIsSeparable(std::string_view(buffer.m_Text));
}

bool SpatialRefsAreAxisSeparable(const OGRSpatialReference& src, const OGRSpatialReference& dst)
{
	// No datum change. This is the load-bearing condition, not a nicety: a datum shift is a
	// Helmert or grid-shift step that routes through geocentric XYZ, where every output ordinate
	// reads every input ordinate. It also rules out GDAL selecting among candidate operations
	// per point by area of use.
	if (!src.IsSameGeogCS(&dst))
		return false;

	return SpatialRefIsSeparable(src) && SpatialRefIsSeparable(dst);
}

} // anonymous namespace

// *****************************************************************************
//			SpatialRefBlock::IsAxisSeparableCrsPair
// *****************************************************************************

bool SpatialRefBlock::IsAxisSeparableCrsPair() const
{
	if (m_IsAxisSeparable.has_value())
		return *m_IsAxisSeparable;

	bool result = false;
	{
		// A CRS that GDAL cannot cleanly express in PROJ.4 form makes noise we do not want in the
		// event log: this is an optimization probe, and its only consequence is which of two
		// equivalent code paths runs. Swallow whatever came up and answer "not separable".
		GDAL_ErrorFrame frame;
		result = SpatialRefsAreAxisSeparable(m_Src, m_Dst);
		if (frame.HasError())
		{
			frame.ReleaseError();
			result = false;
		}
	}

	m_IsAxisSeparable = result;
	return result;
}
