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

#include "Projection.h"

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
	Float64    f;
};
DisplacementInfo displacement_info[9] = 
{
	{ 0,  0, Directions(),               Directions(),               0.0 },
	{-1, -1, Directions(D_North|D_West), Directions(D_South|D_East), 0.5 * 1.4142135623730950488016887242097},
	{ 0, -1, Directions(D_North),        Directions(D_South)       , 0.5},
	{ 1, -1, Directions(D_North|D_East), Directions(D_South|D_West), 0.5 * 1.4142135623730950488016887242097},
	{-1,  0, Directions(D_West),         Directions(D_East)       ,0.5},
	{ 1,  0, Directions(D_East),         Directions(D_West)       ,0.5},
	{-1,  1, Directions(D_South|D_West), Directions(D_North|D_East), 0.5 * 1.4142135623730950488016887242097},
	{ 0,  1, Directions(D_South),        Directions(D_North)       ,0.5},
	{ 1,  1, Directions(D_South|D_East), Directions(D_North|D_West), 0.5 * 1.4142135623730950488016887242097},
};

enum class GridDistFlags
{
	NoFlags = 0,
	HasLimitParameter = 1,
	HasInitialImpedance = 2,
	UseShadowTile = 4,
	HasLatitudeFactor = 8,
	HasBothParamters = HasLimitParameter | HasInitialImpedance,
	HasLimitParameterAndUseShadowTile = HasLimitParameter | UseShadowTile,
	HasInitialImpedanceAndUseShadowTile = HasInitialImpedance | UseShadowTile,
	HasAllParameters = HasLimitParameter | HasLatitudeFactor | HasInitialImpedance | UseShadowTile
};

constexpr auto operator |(GridDistFlags a, GridDistFlags b) { return GridDistFlags(int(a) | int(b)); }
constexpr bool operator &(GridDistFlags a, GridDistFlags b) { return int(a) & int(b); }


constexpr arg_index NrArguments(GridDistFlags flags)
{
	arg_index result = 2;
	if (flags & GridDistFlags::HasLimitParameter)
		++result;
//	if (flags & GridDistFlags::HasLatitudeFactor)
//		++result;
	if (flags & GridDistFlags::HasInitialImpedance)
		++result;
	return result;
}

template <typename Imp, typename Grid>
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

		if (m_Flags & GridDistFlags::HasLimitParameter)
			*argClasses++ = DataArray<ImpType>::GetStaticClass();

//		if (m_Flags & GridDistFlags::HasLatitudeFactor)
//			*argClasses++ = DataArray<ImpType>::GetStaticClass();

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
		const AbstrDataItem* adiLimitImp       = nullptr;
//		const AbstrDataItem* adiLatFactor      = nullptr;
		const AbstrDataItem* adiStartPointImp  = nullptr;

		assert(adiGridImp);
		assert(adiStartPointGrid);

		if (m_Flags & GridDistFlags::HasLimitParameter)
			adiLimitImp = AsDataItem(args[argIndex++]);
//		if (m_Flags & GridDistFlags::HasLatitudeFactor)
//			adiLatFactor = AsDataItem(args[argIndex++]);
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

		if (adiLimitImp)
		{
			MG_USERCHECK2(adiLimitImp->HasVoidDomainGuarantee(), "griddist: Limit parameter (3rd argument) must have void domain");
			impUnit->UnifyValues(adiLimitImp->GetAbstrValuesUnit(), "v1", "v3", UM_Throw);
		}

		if (adiStartPointImp)
		{
			startSet->UnifyDomain(adiStartPointImp->GetAbstrDomainUnit(), "e2", adiLimitImp ? "e4" : "e3", UM_Throw);
			impUnit ->UnifyValues(adiStartPointImp->GetAbstrValuesUnit(), "v1", adiLimitImp ? "v4" : "v3", UM_Throw);
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

		const Arg1Type* diGridImp = const_array_cast<ImpType>(arg1Lock);
		const Arg2Type* diStartPointGrid = const_array_cast<GridType>(arg2Lock);
		const Arg3bType* diStartPointImp = adiStartPointImp ? const_array_cast<ImpType>(arg3Lock) : nullptr;

		ImpType maxImp = MAX_VALUE(ImpType);

		if (adiLimitImp)
		{
			DataReadLock arg3aLock(adiLimitImp);
			maxImp = const_array_cast<ImpType>(adiLimitImp)->GetTile(0)[0];
		}

		std::vector<ImpType> latFactors;

		if (m_Flags & GridDistFlags::HasLatitudeFactor)
		{
			const AbstrUnit* baseUnit = gridSet;
			auto gridSetProjection = gridSet->GetProjection();
			if (gridSetProjection)
				baseUnit = gridSetProjection->GetBaseUnit();
			MG_CHECK(baseUnit);

			auto srToken = baseUnit->GetSpatialReference();


			OGRSpatialReference sr;
			auto errCode = sr.SetFromUserInput(SharedStr(srToken).c_str());
			if (errCode != OGRERR_NONE)
				throwErrorD(SharedStr(GetGroup()->GetNameID()).c_str(), "The spatial reference of the domain of the first argument cannot be interpreted");

			if (not(sr.IsGeographic()))
				throwErrorD(SharedStr(GetGroup()->GetNameID()).c_str(), "The spatial reference of the domain of the first argument must be geographic");
/*
			// Step 2: Export to PROJ String
			char* projString;
			spatialRef->exportToProjString(&projString);
			scoped_exit se([&projString] { CPLFree(projString); }); // Free the PROJ string memory when leaving the scope

			// Step 3: Create a PJ object
			auto C = std::unique_ptr<PJ_CONTEXT>(proj_context_create(), proj_context_destroy); // Create a PJ_CONTEXT object
			auto P = std::unique_ptr<PJ>        (proj_create(C, projString), proj_destroy); // Create a PJ object from the PROJ string

			if (P == nullptr) {
				// Handle error...
			}
*/

			auto base2gridSetTr = UnitProjection::GetCompositeTransform(gridSetProjection);
			auto gridSet2BaseTr = base2gridSetTr.Inverse();

			Range<GridType> range = gridSet->GetRange();
			auto yRange = Range<scalar_of_t<GridType>>(range.first.Y(), range.second.Y());

			for (auto y = range.first.Y(); y < range.second.Y()-0.5; ++y)
			{
				auto latitude = gridSet2BaseTr.Apply(shp2dms_order<Float64>(0, y)).Y();
				auto factor   = std::cos(latitude * (std::numbers::pi_v<Float64> / 180.0));
/*
* 
* 

				PJ_COORD crd = {0, latitude, 0, 0};
				PJ_FACTORS factors = proj_factors(sr.GetProjParm(), &crd);
				auto factor = factors.meridional_scale



				// Step 4: Call proj_factors for specific locations
				double latitude = 45.0; // Example latitude
				double longitude = -75.0; // Example longitude
				PJ_COORD coord = proj_coord(longitude, latitude, 0, 0); // Note the order is lon, lat
				PJ_FACTORS factors = proj_factors(P, coord);

				// Use factors...
				std::cout << "Meridian scale factor: " << factors.meridional_scale << std::endl;




				*/

				latFactors.push_back(factor);
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

		tile_id tn = useShadowTile ? 1 : adiGridImp->GetAbstrDomainUnit()->GetNrTiles();
		intertile_map itMap(useShadowTile ? 0 : tn);
		tile_id nrTilesRemaining = tn;

		int offsets[9] = {}; // TODO OPTIMIZE: memoize the offsets for the previous used nrC
		NodeType nrUsedC_for_offsets_memoization = 0;

		SizeT nrIterations = 0, nrPrevBorderCases = 0;
		while (nrTilesRemaining)
		{ 
			// iterate through the set of tiles
			for (tile_id t = 0; t!=tn; ++t) if (useShadowTile || !itMap[t].m_IsDone)
			{
				auto pseudoTile = useShadowTile ? no_tile : t;
				auto costData  = diGridImp->GetLockedDataRead(pseudoTile);

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
						deltaCost *= displacement_info[i].f;
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
							deltaCost *= displacement_info[i].f;
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
	static CommonOperGroup cogGD__LF("griddist_latfactor", oper_policy::better_not_in_meta_scripting);
	static CommonOperGroup cogGDM_LF("griddist_maximp_latfactor", oper_policy::better_not_in_meta_scripting);
	static CommonOperGroup cogGD_ULF("griddist_untiled_latfactor", oper_policy::better_not_in_meta_scripting);
	static CommonOperGroup cogGDMULF("griddist_maximp_untiled_latfactor", oper_policy::better_not_in_meta_scripting);

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
}
