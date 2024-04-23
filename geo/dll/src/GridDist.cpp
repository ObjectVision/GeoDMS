// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "GeoPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

#include <numbers> // std::numbers
#include <proj.h>
#include <ogr_spatialref.h>

#include "Dijkstra.h"

#include "dbg/SeverityType.h"
#include "CheckedDomain.h"
#include "geo/RangeIndex.h"
#include "geo/PointOrder.h"
#include "utl/scoped_exit.h"

#include "Projection.h"

#include "gdal/gdal_base.h"

// *****************************************************************************
//									GridDist
// *****************************************************************************

#include "UnitClass.h"

enum Directions
{ 
	D_North = 1, D_East = 2, D_West = 4, D_South = 8
};

struct DisplacementInfo // { Start, NW, N, NE, W, E, SW, S, SE }
{
	int dx, dy;
	Directions d, r;
	Float64    factor;

	Float64 getFactor(bool useLatFactor, const std::vector<Couple<Float64>>& latFactors, UInt32 row) const
	{
		if (useLatFactor)
		{
			MG_CHECK(row < latFactors.size());
			if (dx)
				if (dy)
					return latFactors[row].second;
				else
					return latFactors[row].first;
		}
		return factor;
	}
};

DisplacementInfo displacement_info[9] = 
{
	{ 0,  0, Directions(),               Directions(),               0.0 },
	{-1, -1, Directions(D_North|D_West), Directions(D_South|D_East), 0.5 * std::numbers::sqrt2_v<Float64>},
	{ 0, -1, Directions(D_North),        Directions(D_South)       , 0.5},
	{ 1, -1, Directions(D_North|D_East), Directions(D_South|D_West), 0.5 * std::numbers::sqrt2_v<Float64>},
	{-1,  0, Directions(D_West),         Directions(D_East)       ,0.5},
	{ 1,  0, Directions(D_East),         Directions(D_West)       ,0.5},
	{-1,  1, Directions(D_South|D_West), Directions(D_North|D_East), 0.5 * std::numbers::sqrt2_v<Float64>},
	{ 0,  1, Directions(D_South),        Directions(D_North)       ,0.5},
	{ 1,  1, Directions(D_South|D_East), Directions(D_North|D_West), 0.5 * std::numbers::sqrt2_v<Float64>},
};

enum class GridDistFlags
{
	NoFlags = 0,
	HasLimitParameter = 1,
	HasInitialImpedance = 2,
	UseShadowTile = 4,
	HasLatitudeFactor = 8,
	HasZonalBoundaryImpedance = 16,

	HasBothParamters = HasLimitParameter | HasInitialImpedance,
	HasLimitParameterAndUseShadowTile = HasLimitParameter | UseShadowTile,
	HasInitialImpedanceAndUseShadowTile = HasInitialImpedance | UseShadowTile,

	HasAllParameters = HasLimitParameter | HasLatitudeFactor | HasInitialImpedance | UseShadowTile | HasZonalBoundaryImpedance
};

constexpr auto operator |(GridDistFlags a, GridDistFlags b) { return GridDistFlags(int(a) | int(b)); }
constexpr bool operator &(GridDistFlags a, GridDistFlags b) { return int(a) & int(b); }


constexpr arg_index NrArguments(GridDistFlags flags)
{
	arg_index result = 2;
	if (flags & GridDistFlags::HasZonalBoundaryImpedance)
		result += 2;
	if (flags & GridDistFlags::HasLimitParameter)
		++result;
	if (flags & GridDistFlags::HasInitialImpedance)
		++result;
	return result;
}

struct ProjContextDeleter {
	void operator()(PJ_CONTEXT* ctx) const {
		if (ctx != nullptr) {
			proj_context_destroy(ctx);
		}
	}
};
struct ProjDeleter {
	void operator()(PJ* p) const {
		if (p != nullptr) {
			proj_destroy(p);
		}
	}
};

template <typename Imp, typename Grid, typename ZoneID = UInt16>
class GridDistOperator : public Operator
{
	typedef Imp    ImpType;
	typedef Grid   GridType;
	typedef typename cardinality_type<Grid>::type NodeType;
	typedef UInt4  LinkType; 
	typedef UInt32 ZoneType; // TODO, REMOVE only required for non-used parts of DijkstraHeap
	typedef heapElemType<ImpType>    HeapElemType;
	typedef std::vector<HeapElemType> HeapType;
	typedef Range<GridType>          GridRangeType;

	typedef DataArray<ImpType> Arg1Type;  // Grid->Imp
	typedef DataArray<GridType> Arg2Type; // S->Grid
	typedef DataArray<ImpType> Arg3aType;  // {Void}->Imp
	typedef DataArray<ImpType> Arg3bType;  // S->Imp
	typedef DataArray<ImpType> ResultType;    // V->ImpType: resulting cost
	typedef DataArray<LinkType> ResultSubType; // V->E: TraceBack

	struct intertile_data
	{
		bool AddBorderCase(SizeT index, ImpType orgDist, ImpType newDist, LinkType src)
		{
			if (orgDist <= newDist)
				return false;
			if (!IsDefined(newDist))
				return false;

			m_DistData .push_back(newDist);
			if (m_DistData.back() >= orgDist) // prevoius compare could have been done without rouding off 80 bits registers to Float64, this check hopefully doesn't suffer from this false optimization
			{
				m_DistData.pop_back();
				return false;
			}
			m_IndexData.push_back(index);
			m_EdgeData .push_back(src);
			++m_NumBorderCases;
			return true;
		}

		// TODO OPTIMIZE: use a single container for all data, and use a struct {index, dist, edge} for the data
		typename sequence_traits<SizeT   >::container_type m_IndexData;
		typename sequence_traits<ImpType >::container_type m_DistData;
		typename sequence_traits<LinkType>::container_type m_EdgeData;

		bool                 m_IsDone = false;
		SizeT                m_NumBorderCases = 0;
	};

	using intertile_map = std::vector<intertile_data>;

public:
	GridDistFlags m_Flags = GridDistFlags::NoFlags;

	ClassCPtr m_ArgClasses[NrArguments(GridDistFlags::HasAllParameters)];

	GridDistOperator(AbstrOperGroup& og, GridDistFlags flags)
		: Operator(&og, ResultType::GetStaticClass()) 
		, m_Flags(flags)
	{
		arg_index currArg = 0;

		ClassCPtr* argClasses = m_ArgClasses;
		this->m_ArgClassesBegin = argClasses;

		*argClasses++ = DataArray<ImpType>::GetStaticClass(); // Arg1Type
		*argClasses++ = DataArray<GridType>::GetStaticClass(); // Arg2Type

		if (m_Flags & GridDistFlags::HasZonalBoundaryImpedance)
		{
			*argClasses++ = DataArray<ZoneID>::GetStaticClass();
			*argClasses++ = DataArray<ImpType>::GetStaticClass();
		}

		if (m_Flags & GridDistFlags::HasLimitParameter)
			*argClasses++ = DataArray<ImpType>::GetStaticClass();

		if (m_Flags & GridDistFlags::HasInitialImpedance)
			*argClasses++ = DataArray<ImpType>::GetStaticClass();

		assert(argClasses == m_ArgClassesBegin + NrArguments(m_Flags));
		this->m_ArgClassesEnd = argClasses;
	}

	// Override Operator
	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		assert(args.size() == NrArguments(m_Flags));

		arg_index argIndex = 0;
		const AbstrDataItem* adiGridImp        = AsDataItem(args[argIndex++]);
		const AbstrDataItem* adiStartPointGrid = AsDataItem(args[argIndex++]);
		const AbstrDataItem* adiZonalID        = nullptr;
		const AbstrDataItem* adiBoundaryImp    = nullptr;
		const AbstrDataItem* adiLimitImp       = nullptr;
		const AbstrDataItem* adiStartPointImp  = nullptr;

		assert(adiGridImp);
		assert(adiStartPointGrid);

		if (m_Flags & GridDistFlags::HasZonalBoundaryImpedance)
		{
			adiZonalID	   = AsDataItem(args[argIndex++]);
			adiBoundaryImp = AsDataItem(args[argIndex++]);
		}
		if (m_Flags & GridDistFlags::HasLimitParameter)
			adiLimitImp = AsDataItem(args[argIndex++]);
		if (m_Flags & GridDistFlags::HasInitialImpedance)
			adiStartPointImp = AsDataItem(args[argIndex++]);

		assert(argIndex == NrArguments(m_Flags));

		const Unit<GridType>* gridSet  = checked_domain<GridType>(adiGridImp, "a1");
		const Unit<ImpType>*  impUnit  = const_unit_cast<ImpType>(adiGridImp->GetAbstrValuesUnit());
		const AbstrUnit*      startSet = adiStartPointGrid->GetAbstrDomainUnit();

		assert(gridSet );
		assert(impUnit );
		assert(startSet);

		gridSet ->UnifyDomain(adiStartPointGrid->GetAbstrValuesUnit(), "e1", "v2", UM_Throw);

		if (adiZonalID)
		{
			gridSet->UnifyDomain(adiZonalID->GetAbstrDomainUnit(), "domain of grid impedances", "domain of zonal id's", UM_Throw);
			MG_CHECK(const_unit_cast<ZoneID>(adiZonalID->GetAbstrValuesUnit()));
			MG_USERCHECK2(adiBoundaryImp->HasVoidDomainGuarantee(), "griddist: Boundary impedance parameter (4th argument) must have void domain");
			impUnit->UnifyValues(adiBoundaryImp->GetAbstrValuesUnit(), "values of grid impedances", "zonal boundary impedance", UM_Throw);
		}
		if (adiLimitImp)
		{
			MG_USERCHECK2(adiLimitImp->HasVoidDomainGuarantee(), "griddist: maximum impedance parameter (3rd argument) must have void domain");
			impUnit->UnifyValues(adiLimitImp->GetAbstrValuesUnit(), "values of grid impedances", "maximum impedance", UM_Throw);
		}

		if (adiStartPointImp)
		{
			startSet->UnifyDomain(adiStartPointImp->GetAbstrDomainUnit(), "domain of startpoint locations", "domain of startpoint impedances", UM_Throw);
			impUnit ->UnifyValues(adiStartPointImp->GetAbstrValuesUnit(), "values of grid impedances", "values of startpoint impedances", UM_Throw);
		}

		if (!resultHolder)
			resultHolder = CreateCacheDataItem(gridSet, impUnit);

		AbstrDataItem* res = AsDataItem(resultHolder.GetNew());
		AbstrDataItem* trBck = CreateDataItem(res, GetTokenID_mt("TraceBack"), gridSet, Unit<LinkType>::GetStaticClass()->CreateDefault());
		MG_PRECONDITION(trBck);

		if (!mustCalc)
			return true;

//      ================================= Calculate Primary data results =================================

		DataReadLock arg1Lock(adiGridImp);
		DataReadLock arg2Lock(adiStartPointGrid);
		DataReadLock arg3Lock(adiStartPointImp);
		DataReadLock argZIDLock(adiZonalID);

		auto diGridImp = const_array_cast<ImpType>(arg1Lock);
		auto diStartPointGrid = const_array_cast<GridType>(arg2Lock);
		auto diStartPointImp = adiStartPointImp ? const_array_cast<ImpType>(arg3Lock) : nullptr;
		auto diZonalGrid = adiZonalID ? const_array_cast<ZoneID>(argZIDLock) : nullptr;

		ImpType maxImp = MAX_VALUE(ImpType);

		if (adiLimitImp)
		{
			DataReadLock arg3aLock(adiLimitImp);
			maxImp = const_array_cast<ImpType>(adiLimitImp)->GetTile(0)[0];
		}

		std::vector<Couple<Float64>> latFactors;
		Range<GridType> domainRange = gridSet->GetRange();
		auto yRange = Range<scalar_of_t<GridType>>(domainRange.first.Y(), domainRange.second.Y());
		ImpType boundaryFactor = 0.0;
		if (adiBoundaryImp)
		{
			DataReadLock argBILock(adiBoundaryImp);
			boundaryFactor = const_array_cast<ImpType>(adiBoundaryImp)->GetTile(0)[0];
		}


		if (m_Flags & GridDistFlags::HasLatitudeFactor)
		{
			const AbstrUnit* baseUnit = gridSet;
			auto gridSetProjection = gridSet->GetCurrProjection();
			if (gridSetProjection)
				baseUnit = gridSetProjection->GetBaseUnit();
			MG_CHECK(baseUnit);

			auto srToken = baseUnit->GetSpatialReference();

			GDAL_ErrorFrame errorFrame;

			OGRSpatialReference sr;
			auto errCode = sr.SetFromUserInput(SharedStr(srToken).c_str());
			errorFrame.ThrowUpWhateverCameUp();
			if (errCode != OGRERR_NONE)
				throwErrorF(SharedStr(GetGroup()->GetNameID()).c_str(), "The spatial reference of the domain of the first argument cannot be interpreted; ErrorCode=%d", errCode);

			if (not(sr.IsGeographic()))
				throwErrorD(SharedStr(GetGroup()->GetNameID()).c_str(), "The spatial reference of the domain of the first argument must be geographic");

			// Export to PROJ String
			char* projString = nullptr;
			sr.exportToProj4(&projString);
			scoped_exit se([&projString] { CPLFree(projString); }); // Free the PROJ string memory when leaving the scope

			// Step 3: Create a PJ object
			auto ctx  = std::unique_ptr<PJ_CONTEXT, ProjContextDeleter>(proj_context_create()); // Create a PJ_CONTEXT object
			errorFrame.m_ctx = ctx.get();

			auto proj = std::unique_ptr<PJ, ProjDeleter>               (proj_create(ctx.get(), projString)); // Create a PJ object from the PROJ string
			errorFrame.ThrowUpWhateverCameUp();

			if (!proj)
				throwErrorD(SharedStr(GetGroup()->GetNameID()).c_str(), "A projection for the domain of the first argument could not be created");

			auto gridSet2BaseTr = UnitProjection::GetCompositeTransform(gridSetProjection);

			latFactors.reserve(yRange.second - yRange.first);

			for (auto y = yRange.first; y < yRange.second; ++y)
			{
				auto latitude = gridSet2BaseTr.Apply(shp2dms_order<Float64>(0, y)).Y();
				auto latitudeInRadians = latitude * (std::numbers::pi_v<Float64> / 180.0);
				//auto factor   = std::cos(latitudeInRadians);

					// Call proj_factors for specific locations
					PJ_COORD crd = {0, latitudeInRadians, 0, 0};
					PJ_FACTORS factors = proj_factors(proj.get(), crd);

				errorFrame.ThrowUpWhateverCameUp();

				auto factor = 1.0 / factors.parallel_scale;

				latFactors.emplace_back(0.5 * factor, 0.5 * sqrt(1+factor*factor));
			}
		}

		DataWriteLock resLock(res, dms_rw_mode::write_only_mustzero);
		DataWriteLock tbLock (trBck, dms_rw_mode::write_only_mustzero);

		ResultType* result = mutable_array_cast<ImpType>(resLock);
		ResultSubType* traceBack = mutable_array_cast<LinkType>(tbLock);
			
		auto srcNodes = diStartPointGrid->GetLockedDataRead();
		typename DataArray<ImpType>::locked_cseq_t startCosts;
		if (diStartPointImp)
		{
			startCosts = diStartPointImp->GetLockedDataRead();
			assert(srcNodes.size() == startCosts.size());
		}
		bool useShadowTile = m_Flags & GridDistFlags::UseShadowTile;
		bool useLatFactor  = m_Flags & GridDistFlags::HasLatitudeFactor;

		tile_id tn = useShadowTile ? 1 : adiGridImp->GetAbstrDomainUnit()->GetNrTiles();
		intertile_map itMap(useShadowTile ? 0 : tn);
		tile_id nrTilesRemaining = tn;

		int offsets[9] = {}; // TODO OPTIMIZE: memoize the offsets for the previous used nrC
		NodeType nrUsedC_for_offsets_memoization = 0;
		ZoneID zonalID = 0;

		SizeT nrIterations = 0, nrPrevBorderCases = 0;
		while (nrTilesRemaining)
		{ 
			// iterate through the set of tiles
			for (tile_id t = 0; t!=tn; ++t) if (useShadowTile || !itMap[t].m_IsDone)
			{
				auto pseudoTile = useShadowTile ? no_tile : t;
				auto costData  = diGridImp->GetLockedDataRead(pseudoTile);
				auto zonalData = diZonalGrid ? diZonalGrid->GetLockedDataRead(pseudoTile) : typename DataArray<ZoneID>::locked_cseq_t();
				auto resultData    = result   ->GetLockedDataWrite(pseudoTile);
				auto traceBackData = traceBack->GetLockedDataWrite(pseudoTile);

				Range<GridType> range = useShadowTile ? gridSet->GetRange() : gridSet->GetTileRange(pseudoTile);
				SizeT nrV = Cardinality(range);

				DijkstraHeap<NodeType, LinkType, ZoneType, ImpType> dh(nrV, false);
				if (adiLimitImp)
					dh.m_MaxImp = maxImp;

				NodeType nrC = Width(range);
				if (nrC != nrUsedC_for_offsets_memoization)
				{
					for (UInt32 i = 1; i != 9; ++i) offsets[i] = displacement_info[i].dx + displacement_info[i].dy * nrC;
					nrUsedC_for_offsets_memoization = nrC;
				}
				assert(costData.size() == nrV); typename sequence_traits<ImpType>::cseq_t::const_iterator costDataPtr = costData.begin();

				assert(resultData   .size() == nrV); dh.m_ResultDataPtr    = resultData.begin();
				assert(traceBackData.size() == nrV); dh.m_TraceBackDataPtr = traceBackData.begin();

				fast_fill(resultData   .begin(), resultData   .end(), dh.m_MaxImp);
				fast_fill(traceBackData.begin(), traceBackData.end(), LinkType(0) );

				{
					// SetStartNodes from srcNodes
					const GridType* first = srcNodes.begin();
					const GridType* last  = srcNodes.end();
					if (diStartPointImp)
					{
						const ImpType* firstDist = startCosts.begin();
						MG_CHECK(firstDist);
						for (; first != last; ++firstDist, ++first)
						{
							SizeT index = Range_GetIndex_checked(range, *first);
							if (IsDefined(index) && ((!firstDist) || *firstDist >= ImpType()))
								dh.InsertNode(index, *firstDist, LinkType(0));
						}
					}
					else
					{
						for (; first != last; ++first)
						{
							SizeT index = Range_GetIndex_checked(range, *first);
							if (IsDefined(index))
								dh.InsertNode(index, ImpType(), LinkType(0));
						}
					}
				}

				if (!useShadowTile)
				{
					// SetStartNodes from interTileData for the current tile t
					const auto& currInterTileInfo = itMap[t];
					auto first     = currInterTileInfo.m_IndexData.begin(), last = currInterTileInfo.m_IndexData.end();
					auto firstEdge = currInterTileInfo.m_EdgeData.begin();
					auto firstDist = currInterTileInfo.m_DistData.begin();
					for  (; first != last; ++firstEdge, ++firstDist, ++first)
						dh.InsertNode(*first, *firstDist, *firstEdge);
				}

				// iterate through buffer until all destinations in the current tile are processed.
				while (!dh.Empty())
				{
					NodeType currNode = dh.Front().Value(); assert(currNode < nrV);
					ImpType currImp  = dh.Front().Imp();

					dh.PopNode();
					assert(currImp >= 0);

					if (!dh.MarkFinal(currNode, currImp))
						continue;

					ImpType cellDist = costData[currNode];
					if (!IsDefined(cellDist))
						continue;
					UInt32 latFactorRow = 0;
					if (useLatFactor)
						latFactorRow = Range_GetIndex_checked(yRange, Range_GetValue_naked(range, currNode).Y());

					if (diZonalGrid)
						zonalID = zonalData[currNode];

					MakeMax<ImpType>(cellDist, ImpType()); // raise negative values to zero.
					UInt32 forbiddenDirections = 0;
					if (currNode < nrC)             forbiddenDirections |= D_North;
					if (currNode >= nrV - nrC)      forbiddenDirections |= D_South;
					if ((currNode % nrC) == 0)      forbiddenDirections |= D_West;
					if ((currNode % nrC) == (nrC-1))forbiddenDirections |= D_East;

					for (UInt32 i=1; i!=9; ++i) if (!(displacement_info[i].d & forbiddenDirections))
					{
						NodeType otherNode = currNode + offsets[i]; assert(otherNode < nrV);
						ImpType deltaCost = costData[otherNode];
						if (!IsDefined(deltaCost))
							continue;
						MakeMax<ImpType>(deltaCost, ImpType()); // raise negative values to zero.
						deltaCost += cellDist;
						deltaCost *= displacement_info[i].getFactor(useLatFactor, latFactors, latFactorRow);

						if (diZonalGrid && zonalID != zonalData[otherNode])
							deltaCost += boundaryFactor;

						assert(deltaCost >= 0);
						auto newImp = currImp + deltaCost;
						dh.InsertNode(otherNode, newImp, displacement_info[i].d);
					}
				}

				if (!useShadowTile)
					itMap[t].m_IsDone = true;

				// set unreachable cells to UNDEFINED_VALUE
				for (auto& x : resultData)
					if (x >= dh.m_MaxImp)
						x = UNDEFINED_VALUE(ImpType);
			}
			

			if (useShadowTile)
				break;

			nrTilesRemaining = 0;

			// Store the border cases to other tiles for next iterations
			typename Arg1Type::locked_cseq_t costData1, costData2; // be lazy in locking and unlocking tiles
			typename DataArray<ZoneID>::locked_cseq_t zonalData1, zonalData2;

			for (tile_id t1=0; t1!=tn; ++t1)
			{
				Range<Grid> range1 = gridSet->GetTileRange(t1);
				Range<Grid> range1_inflated = Inflate(range1, Grid(1,1));

				for (tile_id t2=t1+1; t2!=tn; ++t2)
				{
					if (gridSet->IsCovered()) // regular and default tilings only
					{
						// limit the number of candidate tiles that could intersect with range1_inflated
						// enumerate the 4: t1+1, t1+nrTC - 1 ... t1+nrTC + 1
						// where nrTC is the number of tiles in a row, i.e.:RegularAdapter<Base>::tiling_extent()
						// for irregular tilings, the number of tiles in a row is not known, so all tiles have to be enumerated
						auto nrTilesInRow = gridSet->GetCurrSegmInfo()->GetTilingExtent().Col();

						if (t2 == t1 + 2)
							MakeMax(t2, t1 + nrTilesInRow - 1); // jump to previous column in the next row, if that is beyond t1+2
						else if (t2 == t1 + nrTilesInRow + 2)
							break; // exit processing t2's as they are beyond (+1, +1)
						if (t2 >= tn)
							break; // exit processing t2's as they are beyond the last tile
					}

					Range<Grid> range2 = gridSet->GetTileRange(t2);
					Range<Grid> intersect = (range1_inflated &  range2);
					if (intersect.empty()) 
						continue;

					costData1 = diGridImp->GetLockedDataRead(t1);
					costData2 = diGridImp->GetLockedDataRead(t2);

					if (diZonalGrid)
					{
						zonalData1 = diZonalGrid->GetLockedDataRead(t1);
						zonalData2 = diZonalGrid->GetLockedDataRead(t2);
					}

					auto resultData1 = result->GetLockedDataWrite(t1);
					auto resultData2 = result->GetLockedDataWrite(t2);

					SizeT intersectCount = Cardinality(intersect);
					for (SizeT ii=0; ii!=intersectCount; ++ii)
					{
						Grid p = Range_GetValue_naked(intersect, ii);
						SizeT i2 = Range_GetIndex_naked(range2, p);
						ImpType
							oldResData2 = resultData2[i2],
							deltaCost2 = costData2[i2];
						if (!IsDefined(deltaCost2))
							continue;
						MakeMax<ImpType>(deltaCost2, ImpType()); // raise negative values to zero.

						ZoneID zoneID = 0;
						if (diZonalGrid)
							zoneID = zonalData2[i2];

						UInt32 row = 0;
						if (useLatFactor)
							row = Range_GetIndex_checked(yRange, p.Y());
						for (UInt32 i=1; i!=9; ++i)
						{
							Grid q = p + shp2dms_order(Grid(displacement_info[i].dx, displacement_info[i].dy));
							if (!IsIncluding(range1, q))
								continue;
							SizeT i1 = Range_GetIndex_naked(range1, q);
							ImpType oldResData1 = resultData1[i1];
							if (!IsDefined(oldResData1) || oldResData1 >= maxImp)
								if (!IsDefined(oldResData2) || oldResData2 >= maxImp)
									continue;

							ImpType deltaCost = costData1[i1];
							if (!IsDefined(deltaCost))
								continue;
							MakeMax<ImpType>(deltaCost, ImpType()); // raise negative values to zero.
							deltaCost +=  deltaCost2;
							deltaCost *= displacement_info[i].getFactor(useLatFactor, latFactors, row);

							if (diZonalGrid && zoneID != zonalData1[i1])
								deltaCost += boundaryFactor;

							assert(deltaCost >= 0);
							if (deltaCost >= maxImp)
								continue;

							if (IsDefined(oldResData2))
							{
								ImpType newResData1 = oldResData2 + deltaCost;
								if (newResData1 < maxImp && itMap[t1].AddBorderCase(i1, oldResData1, newResData1, displacement_info[i].d))
								if (itMap[t1].m_IsDone)
								{
									itMap[t1].m_IsDone = false;
									++nrTilesRemaining;
								}
							}
							if (IsDefined(oldResData1))
							{
								ImpType newResData2 = oldResData1 + deltaCost;
								if (newResData2 < maxImp && itMap[t2].AddBorderCase(i2, oldResData2, newResData2, displacement_info[i].r))
								if (itMap[t2].m_IsDone)
								{
									itMap[t2].m_IsDone = false;
									++nrTilesRemaining;
								}
							}
						}	// next direction
					}	// next boundary
				}	// next t2
			}	// next t1

			SizeT numBorderCases = 0;
			for (auto i=itMap.begin(), e=itMap.end(); i!=e; ++i)
				numBorderCases += i->m_NumBorderCases;

			reportF(SeverityTypeID::ST_MajorTrace, "GridDist completed iteration %d; %d of the %d tiles need reprocessing for %d border cases (%d extra)", ++nrIterations, nrTilesRemaining, tn, numBorderCases, numBorderCases - nrPrevBorderCases);
			nrPrevBorderCases = numBorderCases;
		}	// next iteration 
		resLock.Commit();
		tbLock.Commit();

		return true;
	}
};

// *****************************************************************************
//											INSTANTIATION
// *****************************************************************************

#include "RtcTypeLists.h"
#include "utl/TypeListOper.h"

namespace {
	static CommonOperGroup cogGD__  ("griddist", oper_policy::better_not_in_meta_scripting);
	static CommonOperGroup cogGDM_  ("griddist_maximp", oper_policy::better_not_in_meta_scripting);
	static CommonOperGroup cogGD_U  ("griddist_untiled", oper_policy::better_not_in_meta_scripting);
	static CommonOperGroup cogGDMU  ("griddist_maximp_untiled", oper_policy::better_not_in_meta_scripting);
	static CommonOperGroup cogGD__LF("griddist_latitude_specific", oper_policy::better_not_in_meta_scripting);
	static CommonOperGroup cogGDM_LF("griddist_maximp_latitude_specific", oper_policy::better_not_in_meta_scripting);
	static CommonOperGroup cogGD_ULF("griddist_untiled_latitude_specific", oper_policy::better_not_in_meta_scripting);
	static CommonOperGroup cogGDMULF("griddist_maximp_untiled_latitude_specific", oper_policy::better_not_in_meta_scripting);

	static CommonOperGroup cogGD__Z("griddist_zonal", oper_policy::better_not_in_meta_scripting);
	static CommonOperGroup cogGDM_Z("griddist_zonal_maximp", oper_policy::better_not_in_meta_scripting);
	static CommonOperGroup cogGD_UZ("griddist_zonal_untiled", oper_policy::better_not_in_meta_scripting);
	static CommonOperGroup cogGDMUZ("griddist_zonal_maximp_untiled", oper_policy::better_not_in_meta_scripting);
	static CommonOperGroup cogGD__LFZ("griddist_zonal_latitude_specific", oper_policy::better_not_in_meta_scripting);
	static CommonOperGroup cogGDM_LFZ("griddist_zonal_maximp_latitude_specific", oper_policy::better_not_in_meta_scripting);
	static CommonOperGroup cogGD_ULFZ("griddist_zonal_untiled_latitude_specific", oper_policy::better_not_in_meta_scripting);
	static CommonOperGroup cogGDMULFZ("griddist_zonal_maximp_untiled_latitude_specific", oper_policy::better_not_in_meta_scripting);

	template <typename Imp>
	struct GridDistOperSet
	{
		GridDistOperSet(AbstrOperGroup& og, GridDistFlags flags)
			: m_OpersWithoutInitialImpedance(og, flags)
			, m_OpersWithInitialImpedance(og, flags | GridDistFlags::HasInitialImpedance)
		{}

		tl_oper::inst_tuple<typelists::domain_points, GridDistOperator<Imp, _>, AbstrOperGroup&, GridDistFlags>
			m_OpersWithoutInitialImpedance, m_OpersWithInitialImpedance;

	};

	tl_oper::inst_tuple_templ<typelists::floats, GridDistOperSet, AbstrOperGroup&, GridDistFlags> gridDistOperSets__(cogGD__, GridDistFlags::NoFlags);
	tl_oper::inst_tuple_templ<typelists::floats, GridDistOperSet, AbstrOperGroup&, GridDistFlags> gridDistOperSetsM_(cogGDM_, GridDistFlags::HasLimitParameter);
	tl_oper::inst_tuple_templ<typelists::floats, GridDistOperSet, AbstrOperGroup&, GridDistFlags> gridDistOperSets_U(cogGD_U, GridDistFlags::UseShadowTile);
	tl_oper::inst_tuple_templ<typelists::floats, GridDistOperSet, AbstrOperGroup&, GridDistFlags> gridDistOperSetsMU(cogGDMU, GridDistFlags::HasLimitParameterAndUseShadowTile);

	tl_oper::inst_tuple_templ<typelists::floats, GridDistOperSet, AbstrOperGroup&, GridDistFlags> gridDistOperSets__LF(cogGD__LF, GridDistFlags::HasLatitudeFactor);
	tl_oper::inst_tuple_templ<typelists::floats, GridDistOperSet, AbstrOperGroup&, GridDistFlags> gridDistOperSetsM_LF(cogGDM_LF, GridDistFlags::HasLimitParameter| GridDistFlags::HasLatitudeFactor);
	tl_oper::inst_tuple_templ<typelists::floats, GridDistOperSet, AbstrOperGroup&, GridDistFlags> gridDistOperSets_ULF(cogGD_ULF, GridDistFlags::UseShadowTile| GridDistFlags::HasLatitudeFactor);
	tl_oper::inst_tuple_templ<typelists::floats, GridDistOperSet, AbstrOperGroup&, GridDistFlags> gridDistOperSetsMULF(cogGDMULF, GridDistFlags::HasLimitParameterAndUseShadowTile| GridDistFlags::HasLatitudeFactor);

	tl_oper::inst_tuple_templ<typelists::floats, GridDistOperSet, AbstrOperGroup&, GridDistFlags> gridDistOperSets__Z(cogGD__Z, GridDistFlags::HasZonalBoundaryImpedance);
	tl_oper::inst_tuple_templ<typelists::floats, GridDistOperSet, AbstrOperGroup&, GridDistFlags> gridDistOperSetsM_Z(cogGDM_Z, GridDistFlags::HasLimitParameter| GridDistFlags::HasZonalBoundaryImpedance);
	tl_oper::inst_tuple_templ<typelists::floats, GridDistOperSet, AbstrOperGroup&, GridDistFlags> gridDistOperSets_UZ(cogGD_UZ, GridDistFlags::UseShadowTile| GridDistFlags::HasZonalBoundaryImpedance);
	tl_oper::inst_tuple_templ<typelists::floats, GridDistOperSet, AbstrOperGroup&, GridDistFlags> gridDistOperSetsMUZ(cogGDMUZ, GridDistFlags::HasLimitParameterAndUseShadowTile| GridDistFlags::HasZonalBoundaryImpedance);

	tl_oper::inst_tuple_templ<typelists::floats, GridDistOperSet, AbstrOperGroup&, GridDistFlags> gridDistOperSets__LFZ(cogGD__LFZ, GridDistFlags::HasLatitudeFactor| GridDistFlags::HasZonalBoundaryImpedance);
	tl_oper::inst_tuple_templ<typelists::floats, GridDistOperSet, AbstrOperGroup&, GridDistFlags> gridDistOperSetsM_LFZ(cogGDM_LFZ, GridDistFlags::HasLimitParameter | GridDistFlags::HasLatitudeFactor| GridDistFlags::HasZonalBoundaryImpedance);
	tl_oper::inst_tuple_templ<typelists::floats, GridDistOperSet, AbstrOperGroup&, GridDistFlags> gridDistOperSets_ULFZ(cogGD_ULFZ, GridDistFlags::UseShadowTile | GridDistFlags::HasLatitudeFactor| GridDistFlags::HasZonalBoundaryImpedance);
	tl_oper::inst_tuple_templ<typelists::floats, GridDistOperSet, AbstrOperGroup&, GridDistFlags> gridDistOperSetsMULFZ(cogGDMULFZ, GridDistFlags::HasLimitParameterAndUseShadowTile | GridDistFlags::HasLatitudeFactor| GridDistFlags::HasZonalBoundaryImpedance);

}
