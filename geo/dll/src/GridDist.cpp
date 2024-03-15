// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "GeoPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

#include "Dijkstra.h"

#include "dbg/SeverityType.h"
#include "CheckedDomain.h"

// *****************************************************************************
//									GridDist
// *****************************************************************************

#include "geo/RangeIndex.h"
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

	HasBothParamters = HasLimitParameter | HasInitialImpedance,
	HasLimitParameterAndUseShadowTile = HasLimitParameter | UseShadowTile,
	HasInitialImpedanceAndUseShadowTile = HasInitialImpedance | UseShadowTile,
	HasAllParameters = HasLimitParameter | HasInitialImpedance | UseShadowTile
};

constexpr auto operator |(GridDistFlags a, GridDistFlags b) { return GridDistFlags(int(a) | int(b)); }
constexpr bool operator &(GridDistFlags a, GridDistFlags b) { return int(a) & int(b); }


constexpr arg_index NrArguments(GridDistFlags flags)
{
	arg_index result = 2;
	if (flags & GridDistFlags::HasLimitParameter)
		++result;
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
		intertile_data()
			:	m_IsDone(false) 
			,	m_NumBorderCases(0)
		{}

		bool AddBorderCase(SizeT index, ImpType orgDist, ImpType newDist, LinkType src)
		{
			if (orgDist <= newDist)
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

		typename sequence_traits<SizeT   >::container_type m_IndexData;
		typename sequence_traits<ImpType>::container_type m_DistData;
		typename sequence_traits<LinkType>::container_type m_EdgeData;
		bool                 m_IsDone;
		SizeT                m_NumBorderCases;
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
		const AbstrDataItem* adiLimitImp       =  nullptr;
		const AbstrDataItem* adiStartPointImp  = nullptr;

		assert(adiGridImp);
		assert(adiStartPointGrid);

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

		if (mustCalc)
		{
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
				maxImp = GetTheValue<ImpType>(adiLimitImp);
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

			SizeT nrIterations = 0, nrPrevBorderCases = 0;
			while (nrTilesRemaining)
			{ // iterate through the set of tiles until 

				for (tile_id t = 0; t!=tn; ++t) if (!itMap[t].m_IsDone)
				{
					auto pseudoTile = useShadowTile ? no_tile : t;
					auto costData  = diGridImp->GetLockedDataRead(pseudoTile);

					auto resultData    = result   ->GetLockedDataWrite(pseudoTile);
					auto traceBackData = traceBack->GetLockedDataWrite(pseudoTile);

					Range<GridType> range = gridSet->GetTileRange(pseudoTile);
					SizeT nrV = Cardinality(range);

					DijkstraHeap<NodeType, LinkType, ZoneType, ImpType> dh(nrV, false);

					auto nrC = Width(range);
					int offsets[9]; for (UInt32 i=1; i!=9; ++i) offsets[i] = displacement_info[i].dx + displacement_info[i].dy * nrC;

					assert(costData.size() == nrV); typename sequence_traits<ImpType>::cseq_t::const_iterator costDataPtr = costData.begin();


					assert(resultData   .size() == nrV); dh.m_ResultDataPtr    = resultData.begin();
					assert(traceBackData.size() == nrV); dh.m_TraceBackDataPtr = traceBackData.begin();

					fast_fill(resultData   .begin(), resultData   .end(), dh.m_MaxImp);
					fast_fill(traceBackData.begin(), traceBackData.end(), LinkType(0) );

		//			SetStartNodes(range, srcNodes.begin(), srcNodes.end(), startCosts.begin());
					{
						const GridType* first = srcNodes.begin();
						const GridType* last  = srcNodes.end();
						const ImpType* firstDist = startCosts.begin();
						for  (; first != last; ++firstDist, ++first)
						{
							SizeT index = Range_GetIndex_checked(range, *first);
							if (IsDefined(index) && ((!firstDist) || *firstDist >= ImpType()))
								dh.InsertNode(index, firstDist ? *firstDist : ImpType(), LinkType(0));
						}
					}
					{
						auto first     = itMap[t].m_IndexData.begin(), last = itMap[t].m_IndexData.end();
						auto firstEdge = itMap[t].m_EdgeData.begin();
						auto firstDist = itMap[t].m_DistData.begin();
						for  (; first != last; ++firstEdge, ++firstDist, ++first)
						{
//							SizeT index = Range_GetIndex_checked(range, *first);
//							if (IsDefined(index))
								dh.InsertNode(*first, *firstDist, *firstEdge);
						}
					}

					// iterate through buffer until destination is processed.
					while (!dh.Empty())
					{
						NodeType currNode = dh.Front().Value(); dms_assert(currNode < nrV);
						ImpType currImp  = dh.Front().Imp();

						dh.PopNode();
						dms_assert(currImp >= 0);

						if (!dh.MarkFinal(currNode, currImp))
							continue;

						ImpType cellDist = costData[currNode];
						if (!IsDefined(cellDist))
							continue;
						if (adiLimitImp && cellDist >= maxImp)
							continue;

						MakeMax<ImpType>(cellDist, ImpType()); // raise negative values to zero.
						UInt32 forbiddenDirections = 0;
						if (currNode < nrC)             forbiddenDirections |= D_North;
						if (currNode >= nrV - nrC)      forbiddenDirections |= D_South;
						if ((currNode % nrC) == 0)      forbiddenDirections |= D_West;
						if ((currNode % nrC) == (nrC-1))forbiddenDirections |= D_East;

						for (UInt32 i=1; i!=9; ++i) if (!(displacement_info[i].d & forbiddenDirections))
						{
							NodeType otherNode = currNode + offsets[i]; dms_assert(otherNode < nrV);
							ImpType deltaCost = costData[otherNode];
							if (!IsDefined(deltaCost))
								continue;
							MakeMax<ImpType>(deltaCost, ImpType()); // raise negative values to zero.
							deltaCost += cellDist;
							deltaCost *= displacement_info[i].f;
							assert(deltaCost >= 0);
							if (deltaCost < dh.m_MaxImp)
								dh.InsertNode(otherNode, currImp + deltaCost, displacement_info[i].d);
						}
					}
					itMap[t].m_IsDone = true;
				}
				if (useShadowTile)
					break;

				nrTilesRemaining = 0;

				typename Arg1Type::locked_cseq_t costData1, costData2; // be lazy in locking and unlocking tiles

				for (tile_id t1=0; t1!=tn; ++t1)
				{
					Range<Grid> range1 = gridSet->GetTileRange(t1);
					Range<Grid> range1_inflated = Inflate(range1, Grid(1,1));
					for (tile_id t2=t1+1; t2!=tn; ++t2)
					{
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
								if (oldResData1 >= MaxValue<ImpType>() && oldResData2 >= MaxValue<ImpType>())
									continue;

								ImpType deltaCost = costData1[i1];
								if (!IsDefined(deltaCost))
									continue;
								MakeMax<ImpType>(deltaCost, ImpType()); // raise negative values to zero.
								deltaCost +=  deltaCost2;
								deltaCost *= displacement_info[i].f;
								dms_assert(deltaCost >= 0);
								if (deltaCost >= MaxValue<ImpType>())
									continue;

								ImpType newResData1 = oldResData2 + deltaCost;
								ImpType newResData2 = oldResData1 + deltaCost;
								if (itMap[t1].AddBorderCase(i1, oldResData1, newResData1, displacement_info[i].d))
								{
									if (itMap[t1].m_IsDone)
									{
										itMap[t1].m_IsDone = false;
										++nrTilesRemaining;
									}
								}
								else if (itMap[t2].AddBorderCase(i2, oldResData2, newResData2, displacement_info[i].r))
								{
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
		}
		return true;
	}
};

// *****************************************************************************
//											INSTANTIATION
// *****************************************************************************

#include "RtcTypeLists.h"
#include "utl/TypeListOper.h"

namespace {
	static CommonOperGroup cogGD__("griddist", oper_policy::better_not_in_meta_scripting);
	static CommonOperGroup cogGDM_("griddist_maximp", oper_policy::better_not_in_meta_scripting);
	static CommonOperGroup cogGD_U("griddist_untiled", oper_policy::better_not_in_meta_scripting);
	static CommonOperGroup cogGDMU("griddist_maximp_untiled", oper_policy::better_not_in_meta_scripting);

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
}
