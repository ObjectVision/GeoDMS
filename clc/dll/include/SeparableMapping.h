// Copyright (C) 1998-2026 Object Vision b.v.
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

// SeparableMapping.h - the grid-mapping dispatchers for Type2DConversion.
//
// mapping(D, V) enumerates a grid domain row-major and reprojects every cell into V's
// coordinate space; mapping_count(D, V, C) histograms the same enumeration. Both are
// instantiated by exactly one TU (OperConvMapping.cpp), so they live here rather than in
// OperConv.h, which 12 other TUs include and would otherwise parse them for nothing.
//
// See issue #298: for a coordinate-separable transformation a W x H tile needs only W + H
// transformations instead of W * H, because the tile's domain values contain only W distinct
// x's and H distinct y's. That fast path is added on top of the generic dispatchers here.

#pragma once

#if !defined(__CLC_SEPARABLEMAPPING_H)
#define __CLC_SEPARABLEMAPPING_H

#include "OperConv.h"

#include <algorithm>
#include <vector>

// *****************************************************************************
//			TransformPointRun: the one blocked OGR transform loop
// *****************************************************************************
// The single place that pushes points through OGRCoordinateTransformation in PROJ_BLOCK_SIZE
// chunks. Both the generic mapping loop and the separable fast path go through it, so their
// pre-rescaling, axis-order handling, post-rescaling and failure policy cannot drift apart.
//
//	gen(k)               -> the k-th source value (TA), k counted over the whole run
//	sink(k, value, ok)   <- the k-th result; value is meaningless when !ok
//
// Returns false iff some block's Transform() returned FALSE. Note that the caller cannot
// distinguish which points those were: like the loop this replaces, a FALSE return marks the
// whole block as failed. Callers that need a per-point answer must treat a false return as
// "no usable information" rather than as "these points are undefined".
template<typename TR, typename TA, typename SrcGen, typename Sink>
bool TransformPointRun(Type2DConversion<TR, TA>& functor, SrcGen&& gen, SizeT n, Sink&& sink)
{
	assert(functor.m_OgrComponentHolder);
	assert(functor.m_OgrComponentHolder->m_Transformer);

	Float64 resX[PROJ_BLOCK_SIZE];
	Float64 resY[PROJ_BLOCK_SIZE];
	int     successFlags[PROJ_BLOCK_SIZE];
	bool    source_is_expected_to_be_col_first = functor.m_Source_is_expected_to_be_col_first;
	bool    projection_is_col_first = functor.m_Projection_is_col_first;

	bool  everyBlockTransformed = true;
	SizeT base = 0;
	while (n)
	{
		auto s = n;
		MakeMin(s, PROJ_BLOCK_SIZE);
		for (SizeT i = 0; i != s; ++i)
		{
			DPoint rescaledA = functor.m_PreRescaler.Apply(DPoint(gen(base + i)));
			rescaledA = prj2dms_order(rescaledA, source_is_expected_to_be_col_first);
			resX[i] = rescaledA.first;
			resY[i] = rescaledA.second;
		}
		if (!functor.m_OgrComponentHolder->m_Transformer->Transform(s, resX, resY, nullptr, successFlags))
		{
			fast_fill(successFlags, successFlags + s, 0);
			everyBlockTransformed = false;
		}
		for (SizeT i = 0; i != s; ++i)
		{
			if (successFlags[i])
			{
				auto reprojectedPoint = prj2dms_order(resX[i], resY[i], projection_is_col_first);
				auto rescaledPoint = functor.m_PostRescaler.Apply(reprojectedPoint);
				sink(base + i, SignedIntGridConvert<TR>(rescaledPoint), true);
			}
			else
				sink(base + i, TR(), false);
		}
		base += s;
		n -= s;
	}
	return everyBlockTransformed;
}

// *****************************************************************************
//			MappingCross: the W + H transformed coordinates of a W x H tile
// *****************************************************************************

// Below this many cells the 2H + W + 64 probe transformations and the structural gate are not
// repaid by the W * H they save.
constexpr SizeT g_SeparableMapping_MinCells = 4096;

// How many distinct rows / columns the verification lattice samples per axis (so up to 8 x 8
// probe points, plus at most one failing row and column: still one PROJ block).
constexpr SizeT g_SeparableMapping_ProbeLines = 8;

// Which source component the transformed column values carry. EPSG:4326 is authority order
// (lat, lon) while EPSG:3857 is (easting, northing), so LatLong -> Mercator is separable *with a
// swap*: result component 0 is a function of source component 1. The pairing is therefore
// DETECTED from the data rather than derived from the axis-order flags.
enum class CrossPairing : UInt8 { Straight, Swapped };

template<typename TR>
struct MappingCross
{
	std::vector<TR>   m_ColVal, m_RowVal; // W and H entries
	std::vector<char> m_ColOk, m_RowOk;
	CrossPairing      m_Pairing = CrossPairing::Straight;

	bool Ok(SizeT r, SizeT c) const { return m_RowOk[r] && m_ColOk[c]; }

	// Addresses the raw storage components, so this is independent of dms_order_tag and of both
	// col-first flags: whatever the pairing means physically, it was measured, not assumed.
	TR Combine(SizeT r, SizeT c) const
	{
		return m_Pairing == CrossPairing::Straight
			? TR(m_ColVal[c].first, m_RowVal[r].second)
			: TR(m_RowVal[r].first, m_ColVal[c].second);
	}
};

// The first index whose flag is set, or UNDEFINED_VALUE(SizeT) when there is none.
inline SizeT FirstOkIndex(const std::vector<char>& okFlags)
{
	for (SizeT i = 0, e = okFlags.size(); i != e; ++i)
		if (okFlags[i])
			return i;
	return UNDEFINED_VALUE(SizeT);
}

// Up to g_SeparableMapping_ProbeLines evenly spread indices in [0, okFlags.size()), always
// including the first and the last, plus -- when one exists -- one index whose flag is clear, so
// that the verification also checks that the UNDEFINED set is separable instead of assuming it.
inline std::vector<SizeT> PickProbeLines(const std::vector<char>& okFlags)
{
	SizeT n = okFlags.size();
	assert(n >= 2);

	SizeT k = g_SeparableMapping_ProbeLines;
	MakeMin(k, n);

	std::vector<SizeT> res;
	res.reserve(k + 1);
	for (SizeT i = 0; i != k; ++i)
		res.push_back((n - 1) * i / (k - 1));

	for (SizeT i = 0; i != n; ++i)
		if (!okFlags[i])
		{
			res.push_back(i);
			break;
		}

	std::sort(res.begin(), res.end());
	res.erase(std::unique(res.begin(), res.end()), res.end());
	return res;
}

// Why a tile did or did not get the separable treatment. Reported at ST_MinorTrace, because a
// silent optimization that sometimes engages and sometimes does not is undiagnosable.
enum class CrossOutcome : UInt8
{
	used,                  // the fast path ran: W + H transformations instead of W * H
	tileTooSmall,          // below g_SeparableMapping_MinCells; the probes would not be repaid
	rescalerNotSeparable,  // a pre/post rescaler above AnisoScale mixes the axes
	crsNotSeparable,       // the structural CRS gate refused the pair
	notRectangular,        // W * H != n; not the enumeration this decomposition assumes
	transformFailed,       // OGR reported a whole-block failure; leave that case to the old code
	noUsableAnchor,        // no row or no column could be transformed at all
	verificationFailed,    // the pair looked separable but the probe lattice says otherwise
};

inline CharPtr AsString(CrossOutcome outcome)
{
	switch (outcome)
	{
	case CrossOutcome::used:                 return "used";
	case CrossOutcome::tileTooSmall:         return "skipped: tile too small";
	case CrossOutcome::rescalerNotSeparable: return "skipped: rescaler is not axis separable";
	case CrossOutcome::crsNotSeparable:      return "skipped: CRS pair is not separable";
	case CrossOutcome::notRectangular:       return "skipped: tile range is not the expected rectangle";
	case CrossOutcome::transformFailed:      return "skipped: a coordinate transformation block failed";
	case CrossOutcome::noUsableAnchor:       return "skipped: no transformable row or column";
	case CrossOutcome::verificationFailed:   return "skipped: verification found the transformation is not separable after all";
	}
	return "skipped";
}

// Builds the cross for a tile and PROVES it reproduces the generic per-point result on a probe
// lattice; any outcome other than `used` means "run the generic loop".
//
// Cost when it succeeds: at most 2H + W + 64 transformed points instead of W * H.
template<typename TR, typename TA>
CrossOutcome BuildMappingCrossImpl(Type2DConversion<TR, TA>& functor, typename Unit<TA>::range_t tileRange, SizeT n, MappingCross<TR>& res)
{
	assert(functor.m_OgrComponentHolder && functor.m_OgrComponentHolder->m_Transformer);

	if (n < g_SeparableMapping_MinCells)
		return CrossOutcome::tileTooSmall;

	// Above AnisoScale a rescaler mixes the axes (rotation, shear, perspective) before or after
	// the projection, so the composite is not separable even when the CRS pair is.
	if (!functor.m_PreRescaler.IsAxisSeparable() || !functor.m_PostRescaler.IsAxisSeparable())
		return CrossOutcome::rescalerNotSeparable;
	if (!functor.m_OgrComponentHolder->IsAxisSeparableCrsPair())
		return CrossOutcome::crsNotSeparable;

	// A wrapped-around width (an empty or malformed range) fails the W * H == n check.
	SizeT W = SizeT(Width(tileRange)), H = SizeT(Height(tileRange));
	if (W < 2 || H < 2 || W * H != n)
		return CrossOutcome::notRectangular;

	// Exactly the enumeration DispatchMapping uses, so the fast path sees the same source values.
	auto valueAt = [&tileRange, W](SizeT r, SizeT c) { return Range_GetValue_naked(tileRange, r * W + c); };

	res.m_RowVal.resize(H); res.m_RowOk.assign(H, 0);
	res.m_ColVal.resize(W); res.m_ColOk.assign(W, 0);

	auto runRowsAtColumn = [&](SizeT atCol)
	{
		return TransformPointRun<TR, TA>(functor
			, [&valueAt, atCol](SizeT r) { return valueAt(r, atCol); }
			, H
			, [&res](SizeT r, const TR& v, bool ok) { res.m_RowVal[r] = v; res.m_RowOk[r] = ok; }
		);
	};

	// 1. the vertical cross, provisionally down column 0.
	SizeT cStar = 0;
	if (!runRowsAtColumn(cStar))
		return CrossOutcome::transformFailed;
	SizeT rStar = FirstOkIndex(res.m_RowOk);
	if (!IsDefined(rStar))
		return CrossOutcome::noUsableAnchor;

	// 2. the horizontal cross along a row that column cStar could transform.
	if (!TransformPointRun<TR, TA>(functor
			, [&valueAt, rStar](SizeT c) { return valueAt(rStar, c); }
			, W
			, [&res](SizeT c, const TR& v, bool ok) { res.m_ColVal[c] = v; res.m_ColOk[c] = ok; }))
		return CrossOutcome::transformFailed;
	SizeT firstOkCol = FirstOkIndex(res.m_ColOk);
	if (!IsDefined(firstOkCol))
		return CrossOutcome::noUsableAnchor;

	// 3. re-anchor the vertical cross when column 0 turned out to be a bad column. Without this a
	// global EPSG:4326 grid whose first column or row is degenerate would lose the fast path.
	if (firstOkCol != cStar)
	{
		cStar = firstOkCol;
		if (!runRowsAtColumn(cStar))
			return CrossOutcome::transformFailed;
		if (!res.m_RowOk[rStar])
			return CrossOutcome::noUsableAnchor;
	}

	// 4. the verification lattice.
	auto probeRows = PickProbeLines(res.m_RowOk);
	auto probeCols = PickProbeLines(res.m_ColOk);
	SizeT nc = probeCols.size(), np = probeRows.size() * nc;

	std::vector<TR>   probeVal(np);
	std::vector<char> probeOk(np, 0);
	if (!TransformPointRun<TR, TA>(functor
			, [&valueAt, &probeRows, &probeCols, nc](SizeT k) { return valueAt(probeRows[k / nc], probeCols[k % nc]); }
			, np
			, [&probeVal, &probeOk](SizeT k, const TR& v, bool ok) { probeVal[k] = v; probeOk[k] = ok; }))
		return CrossOutcome::transformFailed;

	// 4a. adopt the pairing from the first probe that can tell the two apart. A probe on the
	// anchor row or column is degenerate -- both pairings produce the same point there.
	bool pairingKnown = false;
	for (SizeT k = 0; k != np && !pairingKnown; ++k)
	{
		SizeT r = probeRows[k / nc], c = probeCols[k % nc];
		if (!probeOk[k] || !res.Ok(r, c))
			continue;

		res.m_Pairing = CrossPairing::Straight; auto straight = res.Combine(r, c);
		res.m_Pairing = CrossPairing::Swapped;  auto swapped = res.Combine(r, c);
		if (straight == swapped)
			continue;

		if (probeVal[k] == straight)
			res.m_Pairing = CrossPairing::Straight;
		else if (!(probeVal[k] == swapped))
			return CrossOutcome::verificationFailed;
		pairingKnown = true;
	}
	if (!pairingKnown)
		return CrossOutcome::verificationFailed; // nothing discriminating; refuse rather than guess

	// 4b. every probe must agree, EXACTLY. A whitelisted projection's easting formula does not
	// read the other ordinate, so the fast path's results are bit-identical or the hypothesis is
	// wrong -- comparing with a tolerance here is what would let a merely near-separable
	// transformation (UTM over a narrow bbox) through and silently change output.
	for (SizeT k = 0; k != np; ++k)
	{
		SizeT r = probeRows[k / nc], c = probeCols[k % nc];
		bool expectedOk = res.Ok(r, c);
		if (bool(probeOk[k]) != expectedOk)
			return CrossOutcome::verificationFailed;
		if (expectedOk && !(probeVal[k] == res.Combine(r, c)))
			return CrossOutcome::verificationFailed;
	}
	return CrossOutcome::used;
}

// Reports the outcome once per tile so that "did the #298 optimization engage, and if not why"
// is answerable from a trace log rather than from a profiler.
template<typename TR, typename TA>
bool BuildMappingCross(Type2DConversion<TR, TA>& functor, typename Unit<TA>::range_t tileRange, SizeT n, MappingCross<TR>& res)
{
	if (!functor.m_OgrComponentHolder || !functor.m_OgrComponentHolder->m_Transformer)
		return false;

	auto outcome = BuildMappingCrossImpl<TR, TA>(functor, tileRange, n, res);

	// A tile below the threshold is the uninteresting majority; narrating those would bury the
	// rest. Everything else says something about either the data or the gate.
	if (outcome != CrossOutcome::tileTooSmall)
		reportF(MsgCategory::progress, SeverityTypeID::ST_MinorTrace
			, "mapping: separable fast path {}", AsString(outcome));

	return outcome == CrossOutcome::used;
}

// Writes the W * H cells of a tile from its cross, row-major, without transforming anything.
template<typename TR, typename RIT>
void FillFromCross(const MappingCross<TR>& cross, RIT ri, SizeT W, SizeT H)
{
	for (SizeT r = 0; r != H; ++r)
	{
		if (!cross.m_RowOk[r])
		{
			for (SizeT c = 0; c != W; ++c, ++ri)
				Assign(*ri, Undefined());
			continue;
		}
		if (cross.m_Pairing == CrossPairing::Straight)
		{
			auto rowPart = cross.m_RowVal[r].second;
			for (SizeT c = 0; c != W; ++c, ++ri)
				if (cross.m_ColOk[c])
					Assign(*ri, TR(cross.m_ColVal[c].first, rowPart));
				else
					Assign(*ri, Undefined());
		}
		else
		{
			auto rowPart = cross.m_RowVal[r].first;
			for (SizeT c = 0; c != W; ++c, ++ri)
				if (cross.m_ColOk[c])
					Assign(*ri, TR(rowPart, cross.m_ColVal[c].second));
				else
					Assign(*ri, Undefined());
		}
	}
}

// *****************************************************************************
//			DispatchMapping / DispatchMappingCount for Type2DConversion
// *****************************************************************************

template<typename TR, typename TA>
void DispatchMapping(Type2DConversion<TR, TA>& functor, typename Type2DConversion<TR, TA>::iterator ri, typename Unit<TA>::range_t tileRange, SizeT n)
{
	if (functor.m_OgrComponentHolder)
	{
		MappingCross<TR> cross;
		if (BuildMappingCross<TR, TA>(functor, tileRange, n, cross))
		{
			FillFromCross(cross, ri, SizeT(Width(tileRange)), SizeT(Height(tileRange)));
			return;
		}

		TransformPointRun<TR, TA>(functor
			, [&tileRange](SizeT k) { return Range_GetValue_naked(tileRange, k); }
			, n
			, [&ri](SizeT, const TR& v, bool ok)
			{
				if (ok)
					Assign(*ri, v);
				else
					Assign(*ri, Undefined());
				++ri;
			}
		);
	}
	else
		if (functor.m_PreRescaler.IsIdentity())
			for (SizeT i = 0; i != n; ++ri, ++i)
				Assign(*ri, functor.ApplyDirect(Range_GetValue_naked(tileRange, i)));
		else
			for (SizeT i = 0; i != n; ++ri, ++i)
				Assign(*ri, functor.ApplyScaled(Range_GetValue_naked(tileRange, i)));
}

template<typename TR, typename TA, typename RI>
void DispatchMappingCount(Type2DConversion<TR, TA>& functor, RI ri, typename Unit<TA>::range_t srcTileRange, typename Unit<TR>::range_t dstRange, SizeT n)
{
	SizeT k = Cardinality(srcTileRange);
	if (functor.m_OgrComponentHolder)
	{
		// The histogram itself stays O(W * H) -- unavoidable -- but the transformations do not.
		MappingCross<TR> cross;
		if (BuildMappingCross<TR, TA>(functor, srcTileRange, k, cross))
		{
			SizeT W = SizeT(Width(srcTileRange)), H = SizeT(Height(srcTileRange));
			for (SizeT r = 0; r != H; ++r)
			{
				if (!cross.m_RowOk[r])
					continue;
				for (SizeT c = 0; c != W; ++c)
				{
					if (!cross.m_ColOk[c])
						continue;
					auto j = Range_GetIndex_checked(dstRange, cross.Combine(r, c));
					if (j < n)
						ri[j]++;
				}
			}
			return;
		}

		// Batched, unlike the ApplyProjection / ApplyScaledProjection loop this replaces, which
		// issued one Transform(1, ...) call per point.
		TransformPointRun<TR, TA>(functor
			, [&srcTileRange](SizeT i) { return Range_GetValue_naked(srcTileRange, i); }
			, k
			, [&ri, &dstRange, n](SizeT, const TR& v, bool ok)
			{
				if (!ok)
					return;
				auto j = Range_GetIndex_checked(dstRange, v);
				if (j < n)
					ri[j]++;
			}
		);
	}
	else
		if (functor.m_PreRescaler.IsIdentity())
			for (SizeT i = 0; i != k; ++i)
			{
				auto j = Range_GetIndex_checked(dstRange, functor.ApplyDirect(Range_GetValue_naked(srcTileRange, i)));
				if (j < n)
					ri[j]++;
			}
		else
			for (SizeT i = 0; i != k; ++i)
			{
				auto j = Range_GetIndex_checked(dstRange, functor.ApplyScaled(Range_GetValue_naked(srcTileRange, i)));
				if (j < n)
					ri[j]++;
			}
}

#endif // !defined(__CLC_SEPARABLEMAPPING_H)
