// Copyright (C) 1998-2026 Object Vision b.v.
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

// SeparableMapping.h - the conversion state and grid-mapping dispatchers behind mapping(D, V)
// and mapping_count(D, V, C).
//
// These templates are instantiated by exactly one TU (OperConvMapping.cpp), so they live here
// rather than in OperConv.h, which 12 other TUs include and would otherwise parse them for
// nothing.
//
// Two things happen here, both from issue #298:
//
//  1. The conversion functor is built ONCE per operator invocation (MappingState, held by
//     AbstrMappingOperator::CreateResult) instead of once per tile. For a cross-CRS mapping that
//     construction is two proj.db lookups plus an OGRCreateCoordinateTransformation under a
//     global critical section, ~1.1 ms, which used to dominate the entire operator.
//
//  2. When the transformation is coordinate separable, the transformed x values (one per domain
//     column) and y values (one per domain row) are computed once for the WHOLE domain and every
//     raster cell (r, c) is then read off as (x[c], y[r]). A W x H domain costs W + H
//     transformations instead of W * H, and tile production calls PROJ zero times.

#pragma once

#if !defined(__CLC_SEPARABLEMAPPING_H)
#define __CLC_SEPARABLEMAPPING_H

#include "OperConv.h"
#include "CastedUnaryAttrOper.h"

#include <algorithm>
#include <map>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

// *****************************************************************************
//			TransformPointRun: the one blocked OGR transform loop
// *****************************************************************************
// The single place that pushes points through OGRCoordinateTransformation in PROJ_BLOCK_SIZE
// chunks. Both the generic mapping loop and the separable cross builder go through it, so their
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
//			MappingCross: the W + H transformed coordinates of a W x H domain
// *****************************************************************************

// Below this many cells the W + H + probe transformations and the structural gate are not
// repaid by the W * H they save.
constexpr SizeT g_SeparableMapping_MinCells = 4096;

// How many distinct rows / columns the verification lattice samples per axis (so up to 8 x 8
// probe points, plus at most one failing row and column: still one PROJ block).
constexpr SizeT g_SeparableMapping_ProbeLines = 8;

// Sanity cap on W + H. Never binds on a real domain (a square domain would have to exceed
// 500_000 cells per side), but keeps a pathological range from allocating without bound.
constexpr SizeT g_SeparableMapping_MaxCrossSize = 1000000;

// Sanity cap on the total size of the PER-SOURCE-TILE histogram pairs (an irregular tiling).
// Normally tiny -- a source tile lands on a small window of the destination -- but a tiling of
// many full-width slivers could make it nrTiles * destinationWidth.
constexpr SizeT g_SeparableMapping_MaxCountPartEntries = 4000000;

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

// Destination-INDEX decomposition of a cross, so that mapping_count's inner loop is one addition
// per raster cell instead of building a point and re-deriving its index from scratch.
//
// Range_GetIndex_naked is (Row - Top) * Width + (Col - Left), and Range_GetIndex_checked rejects
// on IsIncluding, which for a 2D range is the conjunction of the two INDEPENDENT per-ordinate
// tests (Range.h:215, Point.h:141/192). So the destination index splits exactly:
//
//     j = m_RowPart[r] + m_ColPart[c]      valid iff m_RowOk[r] && m_ColOk[c]
//
// one side carrying the (Row - Top) * Width term and the other the (Col - Left) term. Which side
// is which follows the cross pairing: under Swapped the destination Row comes from the source
// COLUMN and the destination Col from the source ROW.
struct CrossIndex
{
	// Per source-ROW-axis entry and per source-COLUMN-axis entry: the destination ORDINATE index
	// it lands on, and the same value already reduced to its term of the linear index.
	// m_RowAxisIsDstRow says which destination axis the source-row axis addresses -- under the
	// Swapped pairing the source rows drive the destination COLUMNS.
	std::vector<SizeT> m_RowAxisIdx, m_ColAxisIdx;
	std::vector<SizeT> m_RowPart, m_ColPart;
	std::vector<char>  m_RowOk, m_ColOk;
	bool               m_RowAxisIsDstRow = true;
	SizeT              m_DstWidth = 0, m_DstHeight = 0;
	bool               m_IsValid = false;
};

template<typename TR>
void BuildCrossIndex(const MappingCross<TR>& cross, typename Unit<TR>::range_t dstRange, CrossIndex& res)
{
	res.m_IsValid = false;
	if (!IsDefined(dstRange))
		return;

	auto top = Top(dstRange), bottom = Bottom(dstRange);
	auto left = Left(dstRange), right = Right(dstRange);
	if (!(left < right) || !(top < bottom))
		return;

	res.m_DstWidth = SizeT(Width(dstRange));
	res.m_DstHeight = SizeT(Height(dstRange));

	// The two ordinate reductions, each mirroring exactly what Range_GetIndex_checked would
	// decide for that ordinate alone.
	auto rowTerm = [top, bottom](const TR& v, SizeT& idx) -> bool
	{
		auto y = v.Row();
		if (!IsDefined(y) || y < top || !(y < bottom))
			return false;
		idx = SizeT(y - top);
		return true;
	};
	auto colTerm = [left, right](const TR& v, SizeT& idx) -> bool
	{
		auto x = v.Col();
		if (!IsDefined(x) || x < left || !(x < right))
			return false;
		idx = SizeT(x - left);
		return true;
	};

	bool straight = (cross.m_Pairing == CrossPairing::Straight);
	SizeT H = cross.m_RowVal.size(), W = cross.m_ColVal.size();

	res.m_RowAxisIsDstRow = straight;
	res.m_RowAxisIdx.assign(H, 0); res.m_RowPart.assign(H, 0); res.m_RowOk.assign(H, 0);
	res.m_ColAxisIdx.assign(W, 0); res.m_ColPart.assign(W, 0); res.m_ColOk.assign(W, 0);

	for (SizeT r = 0; r != H; ++r)
		if (cross.m_RowOk[r])
		{
			res.m_RowOk[r] = straight ? rowTerm(cross.m_RowVal[r], res.m_RowAxisIdx[r])
			                          : colTerm(cross.m_RowVal[r], res.m_RowAxisIdx[r]);
			res.m_RowPart[r] = straight ? res.m_RowAxisIdx[r] * res.m_DstWidth : res.m_RowAxisIdx[r];
		}

	for (SizeT c = 0; c != W; ++c)
		if (cross.m_ColOk[c])
		{
			res.m_ColOk[c] = straight ? colTerm(cross.m_ColVal[c], res.m_ColAxisIdx[c])
			                          : rowTerm(cross.m_ColVal[c], res.m_ColAxisIdx[c]);
			res.m_ColPart[c] = straight ? res.m_ColAxisIdx[c] : res.m_ColAxisIdx[c] * res.m_DstWidth;
		}

	res.m_IsValid = true;
}

// The two 1-D histograms whose outer product IS a mapping_count result, over ONE rectangle of
// source cells:
//
//     count(i, j) = m_DstRowCount[i] * m_DstColCount[j]
//
// because a source cell (r, c) lands in destination cell (i, j) iff row r lands on i AND column
// c lands on j, INDEPENDENTLY.
//
// THE RECTANGLE MATTERS. A single whole-domain product is only the answer when the source tiles
// actually cover the source domain's whole RANGE -- which regular (default) tiling does. An
// IRREGULAR tiling may cover a strict subset of its own bounding box (a sparse study area), and
// then the bounding box contains cells that do not exist; counting them would be wrong.
//
// So the general form is one CrossCounts PER SOURCE TILE: each source tile is its own rectangle
// of raster points, its own contribution factorises the same way, and a destination cell's count
// is the sum over every source tile whose image overlaps it. Tiles are disjoint, so those
// contributions simply add. A destination tile is still produced independently of every other --
// it just aggregates over the source tiles that overlap it.
//
// Only the nonzero window is stored (m_DstRowLo / m_DstColLo plus the spans), because a single
// source tile normally lands on a small part of the destination.
struct CrossCounts
{
	SizeT              m_DstRowLo = 0, m_DstColLo = 0;
	std::vector<SizeT> m_DstRowCount, m_DstColCount;
	bool               m_IsValid = false;

	SizeT DstRowEnd() const { return m_DstRowLo + m_DstRowCount.size(); }
	SizeT DstColEnd() const { return m_DstColLo + m_DstColCount.size(); }
	SizeT NrEntries() const { return m_DstRowCount.size() + m_DstColCount.size(); }
};

// Builds the pair of histograms for the source rectangle [r0, r1) x [c0, c1), in cross
// coordinates (i.e. relative to the source domain's range). Leaves m_IsValid false when nothing
// in that rectangle lands anywhere, which is a valid empty contribution.
inline void BuildCrossCounts(const CrossIndex& index, SizeT r0, SizeT r1, SizeT c0, SizeT c1, CrossCounts& res)
{
	res = CrossCounts{};
	if (!index.m_IsValid)
		return;

	// aXxx follows the source-ROW axis, bXxx the source-COLUMN axis. Which destination axis each
	// drives is decided at the end: under the Swapped pairing the source rows drive the
	// destination COLUMNS.
	SizeT aLo = UNDEFINED_VALUE(SizeT), aHi = 0, bLo = UNDEFINED_VALUE(SizeT), bHi = 0;

	for (SizeT r = r0; r != r1; ++r)
		if (index.m_RowOk[r])
		{
			SizeT v = index.m_RowAxisIdx[r];
			if (!IsDefined(aLo) || v < aLo) aLo = v;
			if (v > aHi) aHi = v;
		}
	for (SizeT c = c0; c != c1; ++c)
		if (index.m_ColOk[c])
		{
			SizeT v = index.m_ColAxisIdx[c];
			if (!IsDefined(bLo) || v < bLo) bLo = v;
			if (v > bHi) bHi = v;
		}
	if (!IsDefined(aLo) || !IsDefined(bLo))
		return; // this rectangle contributes nothing

	std::vector<SizeT> aCount(aHi - aLo + 1, 0), bCount(bHi - bLo + 1, 0);
	for (SizeT r = r0; r != r1; ++r)
		if (index.m_RowOk[r])
			aCount[index.m_RowAxisIdx[r] - aLo]++;
	for (SizeT c = c0; c != c1; ++c)
		if (index.m_ColOk[c])
			bCount[index.m_ColAxisIdx[c] - bLo]++;

	if (index.m_RowAxisIsDstRow)
	{
		res.m_DstRowLo = aLo; res.m_DstRowCount = std::move(aCount);
		res.m_DstColLo = bLo; res.m_DstColCount = std::move(bCount);
	}
	else
	{
		res.m_DstRowLo = bLo; res.m_DstRowCount = std::move(bCount);
		res.m_DstColLo = aLo; res.m_DstColCount = std::move(aCount);
	}
	res.m_IsValid = true;
}

// Produces one DESTINATION tile of a mapping_count result by aggregating over the source-tile
// contributions that overlap it. Reads no source data and touches no other destination tile, so
// tiles stay independent and the result can be a LazyTileFunctor.
template<typename Cardinal, typename TR, typename RIT>
void FillCountTileFromProduct(const std::vector<CrossCounts>& parts, RIT ri, typename Unit<TR>::range_t dstTileRange, typename Unit<TR>::range_t dstRange)
{
	SizeT iBase = SizeT(Top(dstTileRange) - Top(dstRange));
	SizeT jBase = SizeT(Left(dstTileRange) - Left(dstRange));
	SizeT W = SizeT(Width(dstTileRange)), H = SizeT(Height(dstTileRange));

	// The covering-tiling case is a single rectangle, so every cell can be written once and no
	// zeroing pass is needed. This is the regular/default tiling, i.e. the common one.
	if (parts.size() == 1 && parts[0].m_IsValid
		&& parts[0].m_DstRowLo <= iBase && iBase + H <= parts[0].DstRowEnd()
		&& parts[0].m_DstColLo <= jBase && jBase + W <= parts[0].DstColEnd())
	{
		const auto& cc = parts[0];
		for (SizeT i = 0; i != H; ++i)
		{
			SizeT rowCount = cc.m_DstRowCount[iBase + i - cc.m_DstRowLo];
			if (!rowCount)
			{
				for (SizeT j = 0; j != W; ++j, ++ri)
					Assign(*ri, Cardinal(0));
				continue;
			}
			const SizeT* colCount = cc.m_DstColCount.data() + (jBase - cc.m_DstColLo);
			for (SizeT j = 0; j != W; ++j, ++ri)
				Assign(*ri, Cardinal(rowCount * colCount[j])); // wraps exactly as repeated ++ would
		}
		return;
	}

	// General case: start from zero and add each overlapping source tile's contribution.
	for (SizeT k = 0, ke = W * H; k != ke; ++k)
		Assign(ri[k], Cardinal(0));

	for (const auto& cc : parts)
	{
		if (!cc.m_IsValid)
			continue;
		SizeT i0 = std::max(iBase, cc.m_DstRowLo), i1 = std::min(iBase + H, cc.DstRowEnd());
		if (i0 >= i1)
			continue;
		SizeT j0 = std::max(jBase, cc.m_DstColLo), j1 = std::min(jBase + W, cc.DstColEnd());
		if (j0 >= j1)
			continue;

		for (SizeT i = i0; i != i1; ++i)
		{
			SizeT rowCount = cc.m_DstRowCount[i - cc.m_DstRowLo];
			if (!rowCount)
				continue;
			RIT row = ri + (i - iBase) * W;
			for (SizeT j = j0; j != j1; ++j)
			{
				SizeT colCount = cc.m_DstColCount[j - cc.m_DstColLo];
				if (colCount)
					row[j - jBase] += Cardinal(rowCount * colCount);
			}
		}
	}
}

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

// Why a domain did or did not get the separable treatment. Reported at ST_MinorTrace, because a
// silent optimization that sometimes engages and sometimes does not is undiagnosable.
enum class CrossOutcome : UInt8
{
	used,                  // the fast path is on: W + H transformations instead of W * H
	domainTooSmall,        // below g_SeparableMapping_MinCells; the probes would not be repaid
	rescalerNotSeparable,  // a pre/post rescaler above AnisoScale mixes the axes
	crsNotSeparable,       // the structural CRS gate refused the pair
	notRectangular,        // W * H != n, or too large; not the shape this decomposition assumes
	transformFailed,       // OGR reported a whole-block failure; leave that case to the old code
	noUsableAnchor,        // no row or no column could be transformed at all
	verificationFailed,    // the pair looked separable but the probe lattice says otherwise
};

inline CharPtr AsString(CrossOutcome outcome)
{
	switch (outcome)
	{
	case CrossOutcome::used:                 return "used";
	case CrossOutcome::domainTooSmall:       return "skipped: domain too small";
	case CrossOutcome::rescalerNotSeparable: return "skipped: rescaler is not axis separable";
	case CrossOutcome::crsNotSeparable:      return "skipped: CRS pair is not separable";
	case CrossOutcome::notRectangular:       return "skipped: domain range is not the expected rectangle";
	case CrossOutcome::transformFailed:      return "skipped: a coordinate transformation block failed";
	case CrossOutcome::noUsableAnchor:       return "skipped: no transformable row or column";
	case CrossOutcome::verificationFailed:   return "skipped: verification found the transformation is not separable after all";
	}
	return "skipped";
}

// Builds the cross for a domain and PROVES it reproduces the generic per-point result on a probe
// lattice; any outcome other than `used` means "run the generic per-point loop".
//
// Cost when it succeeds: at most 2H + W + 64 transformed points for the whole domain.
template<typename TR, typename TA>
CrossOutcome BuildMappingCrossImpl(Type2DConversion<TR, TA>& functor, typename Unit<TA>::range_t domainRange, SizeT n, MappingCross<TR>& res, bool hasOgr)
{
	if (n < g_SeparableMapping_MinCells)
		return CrossOutcome::domainTooSmall;

	// Above AnisoScale a rescaler mixes the axes (rotation, shear, perspective) before or after
	// the projection, so the composite is not separable even when the CRS pair is.
	if (!functor.m_PreRescaler.IsAxisSeparable() || !functor.m_PostRescaler.IsAxisSeparable())
		return CrossOutcome::rescalerNotSeparable;
	if (hasOgr && !functor.m_OgrComponentHolder->IsAxisSeparableCrsPair())
		return CrossOutcome::crsNotSeparable;

	// Where the transformed coordinates come from. With a CRS conversion that is PROJ, in blocks.
	// WITHOUT one -- the two grids share a SpatialReference and differ only in their
	// UnitProjection -- the composite is just the axis-separable affine that Type2DConversion
	// already folded into m_PreRescaler, applied component-wise by SignedIntGridConvert. That is
	// separable by construction, costs no PROJ call and cannot fail, but it still goes through
	// the same probe verification below: the cross must reproduce the generic path exactly, and
	// checking that costs 64 points.
	auto runPoints = [&functor, hasOgr](auto&& gen, SizeT count, auto&& sink) -> bool
	{
		if (hasOgr)
			return TransformPointRun<TR, TA>(functor, gen, count, sink);

		if (functor.m_PreRescaler.IsIdentity())
			for (SizeT i = 0; i != count; ++i)
				sink(i, functor.ApplyDirect(gen(i)), true);
		else
			for (SizeT i = 0; i != count; ++i)
				sink(i, functor.ApplyScaled(gen(i)), true);
		return true;
	};

	if (!IsDefined(domainRange))
		return CrossOutcome::notRectangular;

	// A wrapped-around width (an empty or malformed range) fails the W * H == n check.
	SizeT W = SizeT(Width(domainRange)), H = SizeT(Height(domainRange));
	if (W < 2 || H < 2 || W * H != n || W + H > g_SeparableMapping_MaxCrossSize)
		return CrossOutcome::notRectangular;

	// Exactly the enumeration DispatchMapping uses, so the cross sees the same source values.
	auto valueAt = [&domainRange, W](SizeT r, SizeT c) { return Range_GetValue_naked(domainRange, r * W + c); };

	res.m_RowVal.resize(H); res.m_RowOk.assign(H, 0);
	res.m_ColVal.resize(W); res.m_ColOk.assign(W, 0);

	auto runRowsAtColumn = [&](SizeT atCol)
	{
		return runPoints(
			  [&valueAt, atCol](SizeT r) { return valueAt(r, atCol); }
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
	if (!runPoints(
			  [&valueAt, rStar](SizeT c) { return valueAt(rStar, c); }
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
	if (!runPoints(
			  [&valueAt, &probeRows, &probeCols, nc](SizeT k) { return valueAt(probeRows[k / nc], probeCols[k % nc]); }
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

// Reports the outcome once per invocation so that "did the #298 optimization engage, and if not
// why" is answerable from a trace log rather than from a profiler.
// Built for a plain same-SpatialReference affine too, not only for a CRS conversion. That case
// looked like it would be a wash -- the fill is per cell either way -- but measured on a 49M cell
// grid it is not: reading the two coordinate arrays costs nothing over merely enumerating the
// domain (10.5 vs 10.6 ms), while applying the affine per cell costs about 12 ms on top, because
// ApplyScaled goes through DPoint and SignedIntGridConvert's rounding and undefined checks rather
// than "two multiply-adds".
template<typename TR, typename TA>
bool BuildMappingCross(Type2DConversion<TR, TA>& functor, typename Unit<TA>::range_t domainRange, SizeT n, MappingCross<TR>& res)
{
	bool hasOgr = functor.m_OgrComponentHolder && functor.m_OgrComponentHolder->m_Transformer;

	auto outcome = BuildMappingCrossImpl<TR, TA>(functor, domainRange, n, res, hasOgr);

	reportF(MsgCategory::progress, SeverityTypeID::ST_MinorTrace
		, "mapping: separable fast path {}", AsString(outcome));

	return outcome == CrossOutcome::used;
}

// Writes one tile from the whole-domain cross, row-major, transforming nothing.
template<typename TA, typename TR, typename RIT>
void FillTileFromCross(const MappingCross<TR>& cross, RIT ri, typename Unit<TA>::range_t tileRange, typename Unit<TA>::range_t domainRange)
{
	assert(Top(tileRange) >= Top(domainRange) && Left(tileRange) >= Left(domainRange));

	SizeT rBase = SizeT(Top(tileRange) - Top(domainRange));
	SizeT cBase = SizeT(Left(tileRange) - Left(domainRange));
	SizeT W = SizeT(Width(tileRange)), H = SizeT(Height(tileRange));

	for (SizeT r = 0; r != H; ++r)
	{
		if (!cross.m_RowOk[rBase + r])
		{
			for (SizeT c = 0; c != W; ++c, ++ri)
				Assign(*ri, Undefined());
			continue;
		}
		if (cross.m_Pairing == CrossPairing::Straight)
		{
			auto rowPart = cross.m_RowVal[rBase + r].second;
			for (SizeT c = 0; c != W; ++c, ++ri)
				if (cross.m_ColOk[cBase + c])
					Assign(*ri, TR(cross.m_ColVal[cBase + c].first, rowPart));
				else
					Assign(*ri, Undefined());
		}
		else
		{
			auto rowPart = cross.m_RowVal[rBase + r].first;
			for (SizeT c = 0; c != W; ++c, ++ri)
				if (cross.m_ColOk[cBase + c])
					Assign(*ri, TR(rowPart, cross.m_ColVal[cBase + c].second));
				else
					Assign(*ri, Undefined());
		}
	}
}

// *****************************************************************************
//			MappingState: the shared per-invocation conversion state
// *****************************************************************************

template <typename F> struct is_2d_conversion : std::false_type {};
template <typename TR, typename TA> struct is_2d_conversion<Type2DConversion<TR, TA>> : std::true_type {};
template <typename F> constexpr bool is_2d_conversion_v = is_2d_conversion<F>::value;

template <typename TR, typename TA, typename TCF>
struct MappingState : AbstrMappingState
{
	using FunctorType = typename ConversionGenerator<TCF, TR, TA>::type;
	using range_t = typename Unit<TA>::range_t;

	// Only a 2D (coordinate) conversion can have a cross at all; for the numeric mapping
	// instantiations TA is a scalar, whose range_t has no Top/Left/Width/Height, so the cross
	// paths must not even be instantiated for them.
	static constexpr bool has_cross_support = is_2d_conversion_v<FunctorType>;

	MappingState(const Unit<TR>* dstUnit, const Unit<TA>* srcUnit)
		: m_DstUnit(make_shared_tree(dstUnit, existing_obj{}))
		, m_SrcUnit(make_shared_tree(srcUnit, existing_obj{}))
		, m_DomainRange(srcUnit->GetRange())
	{
		if constexpr (has_cross_support)
			m_HasCross = BuildMappingCross<TR, TA>(GetFunctor(), m_DomainRange, Cardinality(m_DomainRange), m_Cross);
	}

	// The functor for the calling thread. OGRCoordinateTransformation is NOT thread-safe, and a
	// shared one would be used concurrently by parallel_tileloop and by LazyTileFunctor refills,
	// so the state amortises construction per THREAD rather than per tile. When m_HasCross the
	// tile path never asks for one at all.
	FunctorType& GetFunctor() const
	{
		auto id = std::this_thread::get_id();
		std::lock_guard lock(m_FunctorMutex);
		auto& slot = m_PerThreadFunctor[id];
		if (!slot)
			slot = std::make_unique<FunctorType>(m_DstUnit.get(), m_SrcUnit.get());
		return *slot;
	}

	auto GetTileRange(tile_id t) const { return m_SrcUnit->GetTileRange(t); }
	auto GetDstTileRange(tile_id t) const { return m_DstUnit->GetTileRange(t); }
	auto GetDstRange() const { return m_DstUnit->GetRange(); }

	std::shared_ptr<const Unit<TR>> m_DstUnit;
	std::shared_ptr<const Unit<TA>> m_SrcUnit;
	range_t                         m_DomainRange;

	mutable std::mutex m_FunctorMutex;
	mutable std::map<std::thread::id, std::unique_ptr<FunctorType>> m_PerThreadFunctor;

	// Whole-domain separable cross. When present, tile production reads (x[c], y[r]) from it and
	// calls PROJ zero times per raster cell.
	MappingCross<TR> m_Cross;
	bool             m_HasCross = false;

};

// mapping_count's state: the cross, plus its reduction to destination indices and then to the
// histogram pairs whose outer product is the result. Separate from MappingState because mapping()
// needs none of it and should not pay for building it.
template <typename TR, typename TA, typename TCF>
struct MappingCountState : MappingState<TR, TA, TCF>
{
	using base_type = MappingState<TR, TA, TCF>;

	MappingCountState(const Unit<TR>* dstUnit, const Unit<TA>* srcUnit)
		: base_type(dstUnit, srcUnit)
	{
		if constexpr (base_type::has_cross_support)
			if (this->m_HasCross)
			{
				BuildCrossIndex<TR>(this->m_Cross, dstUnit->GetRange(), m_CrossIndex);
				BuildCountProduct();
			}
	}

	// Whether the source tiles exactly tile the source domain's RANGE, which regular (default)
	// tiling does. Tiles are disjoint by construction (every element has exactly one
	// GetTileDataLocation), so comparing the total element count settles it.
	bool SourceTilesCoverDomain() const
	{
		auto trd = this->m_SrcUnit->GetTiledRangeData();
		if (!trd)
			return false;
		SizeT covered = 0;
		for (tile_id t = 0, te = trd->GetNrTiles(); t != te; ++t)
			covered += trd->GetTileSize(t);
		return covered == Cardinality(this->m_DomainRange);
	}

	// Reduces the cross to the histogram pairs whose outer product is the result: ONE pair when
	// the source tiles cover the domain range, otherwise one pair PER SOURCE TILE, because an
	// irregular tiling's bounding box contains cells that do not exist and a single whole-domain
	// product would count them. See CrossCounts.
	void BuildCountProduct()
	{
		if (!m_CrossIndex.m_IsValid)
			return;

		SizeT H = m_CrossIndex.m_RowAxisIdx.size(), W = m_CrossIndex.m_ColAxisIdx.size();
		m_SourceTilesCoverDomain = SourceTilesCoverDomain();

		if (m_SourceTilesCoverDomain)
		{
			m_CrossCounts.resize(1);
			BuildCrossCounts(m_CrossIndex, 0, H, 0, W, m_CrossCounts[0]);
			m_HasCountProduct = m_CrossCounts[0].m_IsValid;
			reportF(MsgCategory::progress, SeverityTypeID::ST_MinorTrace
				, "mapping_count: outer product over the whole domain");
			return;
		}

		auto trd = this->m_SrcUnit->GetTiledRangeData();
		if (!trd)
			return;
		tile_id tn = trd->GetNrTiles();

		std::vector<CrossCounts> parts(tn);
		SizeT entries = 0;
		for (tile_id t = 0; t != tn; ++t)
		{
			auto tr = this->m_SrcUnit->GetTileRange(t);
			if (!IsDefined(tr))
				return;
			SizeT r0 = SizeT(Top(tr) - Top(this->m_DomainRange)), c0 = SizeT(Left(tr) - Left(this->m_DomainRange));
			SizeT r1 = r0 + SizeT(Height(tr)), c1 = c0 + SizeT(Width(tr));
			if (r1 > H || c1 > W)
				return; // a tile outside the domain range: not a shape this decomposition assumes

			BuildCrossCounts(m_CrossIndex, r0, r1, c0, c1, parts[t]);
			entries += parts[t].NrEntries();
			if (entries > g_SeparableMapping_MaxCountPartEntries)
				return; // pathological; the per-source-tile scatter is the safer bet
		}

		m_CrossCounts = std::move(parts);
		m_HasCountProduct = true;
		reportF(MsgCategory::progress, SeverityTypeID::ST_MinorTrace
			, "mapping_count: outer product per source tile ({} tiles, irregular tiling)", tn);
	}

	CrossIndex               m_CrossIndex;
	std::vector<CrossCounts> m_CrossCounts;
	bool                     m_HasCountProduct = false;
	bool                     m_SourceTilesCoverDomain = false;
};

// *****************************************************************************
//			DispatchMapping / DispatchMappingCount for Type2DConversion
// *****************************************************************************
// The generic per-point paths, used when there is no separable cross.

template<typename TR, typename TA>
void DispatchMapping(Type2DConversion<TR, TA>& functor, typename Type2DConversion<TR, TA>::iterator ri, typename Unit<TA>::range_t tileRange, SizeT n)
{
	if (functor.m_OgrComponentHolder)
	{
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

// Histograms a tile as an OUTER PRODUCT of two 1-D histograms, which is what separability buys
// mapping_count on top of the cheaper index arithmetic:
//
//   a source cell (r, c) lands in destination cell (i, j) iff row r lands on i AND column c
//   lands on j, independently -- so
//
//       count(i, j) = |{ r : r -> i }| * |{ c : c -> j }|
//
// Building those two 1-D histograms costs H + W, and the destination is then filled with ONE
// multiplication per DESTINATION cell, written in row-major order instead of scattered. The
// source cells are never visited at all: W * H disappears from the cost entirely.
//
// This is exact for any tiling: a tile is a rectangle of source cells, so its own contribution
// factorises the same way, and disjoint tiles sum.
//
// Returns false when the destination window a tile touches is bigger than the tile itself, in
// which case walking the tile's cells is the cheaper of the two; the caller then falls back.
template<typename TA, typename Cardinal, typename RI>
bool CountTileFromCrossProduct(const CrossIndex& index, RI ri, typename Unit<TA>::range_t srcTileRange, typename Unit<TA>::range_t domainRange, SizeT n)
{
	assert(index.m_IsValid);

	SizeT rBase = SizeT(Top(srcTileRange) - Top(domainRange));
	SizeT cBase = SizeT(Left(srcTileRange) - Left(domainRange));
	SizeT W = SizeT(Width(srcTileRange)), H = SizeT(Height(srcTileRange));

	// The destination window this tile touches, per axis.
	SizeT rowLo = UNDEFINED_VALUE(SizeT), rowHi = 0, colLo = UNDEFINED_VALUE(SizeT), colHi = 0;
	for (SizeT r = 0; r != H; ++r)
		if (index.m_RowOk[rBase + r])
		{
			SizeT v = index.m_RowAxisIdx[rBase + r];
			if (!IsDefined(rowLo) || v < rowLo) rowLo = v;
			if (v > rowHi) rowHi = v;
		}
	for (SizeT c = 0; c != W; ++c)
		if (index.m_ColOk[cBase + c])
		{
			SizeT v = index.m_ColAxisIdx[cBase + c];
			if (!IsDefined(colLo) || v < colLo) colLo = v;
			if (v > colHi) colHi = v;
		}
	if (!IsDefined(rowLo) || !IsDefined(colLo))
		return true; // nothing in this tile lands anywhere: no counts to add

	SizeT rowSpan = rowHi - rowLo + 1, colSpan = colHi - colLo + 1;
	if (rowSpan > W * H / colSpan)
		return false; // the destination window is larger than the tile; per-cell is cheaper

	std::vector<SizeT> rowHist(rowSpan, 0), colHist(colSpan, 0);
	for (SizeT r = 0; r != H; ++r)
		if (index.m_RowOk[rBase + r])
			rowHist[index.m_RowAxisIdx[rBase + r] - rowLo]++;
	for (SizeT c = 0; c != W; ++c)
		if (index.m_ColOk[cBase + c])
			colHist[index.m_ColAxisIdx[cBase + c] - colLo]++;

	// Put the two axes back onto (destination row, destination column).
	const auto& dstRowHist = index.m_RowAxisIsDstRow ? rowHist : colHist;
	const auto& dstColHist = index.m_RowAxisIsDstRow ? colHist : rowHist;
	SizeT dstRowLo = index.m_RowAxisIsDstRow ? rowLo : colLo;
	SizeT dstColLo = index.m_RowAxisIsDstRow ? colLo : rowLo;

	for (SizeT i = 0, ie = dstRowHist.size(); i != ie; ++i)
	{
		SizeT rowCount = dstRowHist[i];
		if (!rowCount)
			continue;
		SizeT base = (dstRowLo + i) * index.m_DstWidth + dstColLo;
		for (SizeT j = 0, je = dstColHist.size(); j != je; ++j)
			if (dstColHist[j])
			{
				SizeT k = base + j;
				if (k < n)
					ri[k] += Cardinal(rowCount * dstColHist[j]); // wraps exactly as ++ would
			}
	}
	return true;
}

// Histograms one tile straight from the whole-domain cross index: no coordinate is transformed,
// no point is built, and the destination index of a cell costs ONE addition. The per-row term is
// hoisted out of the inner loop, so what remains per cell is a flag test, an add and the
// (inherently scattered) increment.
template<typename TA, typename RI>
void CountTileFromCrossIndex(const CrossIndex& index, RI ri, typename Unit<TA>::range_t srcTileRange, typename Unit<TA>::range_t domainRange, SizeT n)
{
	assert(index.m_IsValid);

	SizeT rBase = SizeT(Top(srcTileRange) - Top(domainRange));
	SizeT cBase = SizeT(Left(srcTileRange) - Left(domainRange));
	SizeT W = SizeT(Width(srcTileRange)), H = SizeT(Height(srcTileRange));

	for (SizeT r = 0; r != H; ++r)
	{
		if (!index.m_RowOk[rBase + r])
			continue;
		SizeT rowPart = index.m_RowPart[rBase + r];

		const char*  colOk = index.m_ColOk.data() + cBase;
		const SizeT* colPart = index.m_ColPart.data() + cBase;
		for (SizeT c = 0; c != W; ++c)
			if (colOk[c])
			{
				SizeT j = rowPart + colPart[c];
				if (j < n)
					ri[j]++;
			}
	}
}

#endif // !defined(__CLC_SEPARABLEMAPPING_H)
