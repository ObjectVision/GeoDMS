//<HEADER> 
/*
Data & Model Server (DMS) is a server written in C++ for DSS applications. 
Version: see srv/dms/rtc/dll/src/RtcVersion.h for version info.

Copyright (C) 1998-2004  YUSE GSO Object Vision BV. 

Documentation on using the Data & Model Server software can be found at:
http://www.ObjectVision.nl/DMS/

See additional guidelines and notes in srv/dms/Readme-srv.txt 

This library is free software; you can use, redistribute, and/or
modify it under the terms of the GNU General Public License version 2 
(the License) as published by the Free Software Foundation,
provided that this entire header notice and readme-srv.txt is preserved.

See LICENSE.TXT for terms of distribution or look at our web site:
http://www.objectvision.nl/DMS/License.txt
or alternatively at: http://www.gnu.org/copyleft/gpl.html

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
General Public License for more details. However, specific warranties might be
granted by an additional written contract for support, assistance and/or development
*/
//</HEADER>

#include "GeoPCH.h"
#pragma hdrstop

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
namespace {
	static CommonOperGroup cogGD("griddist");
	}

template <typename Imp, typename Grid>
class GridDistOperator : public TernaryOperator
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
	typedef DataArray<ImpType> Arg3Type;  // S->Imp
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

	typedef std::vector<intertile_data> intertile_map;

public:
	GridDistOperator():
		TernaryOperator(&cogGD, ResultType::GetStaticClass(),
			Arg1Type::GetStaticClass(),
			Arg2Type::GetStaticClass(),
			Arg3Type::GetStaticClass()
		) 
	{}

	// Override Operator
	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		dms_assert(args.size() == 3);

		const AbstrDataItem* adiGridImp       = debug_valcast<const AbstrDataItem*>(args[0]);
		const AbstrDataItem* adiStartPointGrid= debug_valcast<const AbstrDataItem*>(args[1]);
		const AbstrDataItem* adiStartPointImp = debug_valcast<const AbstrDataItem*>(args[2]);

		dms_assert(adiGridImp && adiStartPointGrid && adiStartPointImp);

		const Unit<GridType>* gridSet  = checked_domain<GridType>(adiGridImp);
		const Unit<ImpType>*  impUnit  = const_unit_cast<ImpType>(adiGridImp->GetAbstrValuesUnit());
		const AbstrUnit*      startSet = adiStartPointGrid->GetAbstrDomainUnit();

		dms_assert(gridSet );
		dms_assert(impUnit );
		dms_assert(startSet);

		gridSet->UnifyDomain(adiStartPointGrid->GetAbstrValuesUnit(), UM_Throw);
		startSet->UnifyDomain(adiStartPointImp->GetAbstrDomainUnit(), UM_Throw);
		impUnit->UnifyValues(adiStartPointImp->GetAbstrValuesUnit(), UM_Throw);

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
			const Arg3Type* diStartPointImp = const_array_cast<ImpType>(arg3Lock);

			DataWriteLock resLock(res, dms_rw_mode::write_only_mustzero);
			DataWriteLock tbLock (trBck, dms_rw_mode::write_only_mustzero);

			ResultType* result = mutable_array_cast<ImpType>(resLock);
			ResultSubType* traceBack = mutable_array_cast<LinkType>(tbLock);

			tile_id tn = adiGridImp->GetAbstrDomainUnit()->GetNrTiles();
			intertile_map itMap(tn);
			tile_id nrTilesRemaining = tn;

			SizeT nrIterations = 0, nrPrevBorderCases = 0;
			while (nrTilesRemaining)
			{ // iterate through the set of tiles until 

				for (tile_id t = 0; t!=tn; ++t) if (!itMap[t].m_IsDone)
				{
					auto costData  = diGridImp->GetLockedDataRead(t);
					auto srcNodes = diStartPointGrid->GetLockedDataRead();
					auto startCosts = diStartPointImp->GetLockedDataRead();

					auto resultData    = result   ->GetLockedDataWrite(t);
					auto traceBackData = traceBack->GetLockedDataWrite(t);

					Range<GridType> range = gridSet->GetTileRange(t);
					SizeT nrV = Cardinality(range);

					DijkstraHeap<NodeType, LinkType, ZoneType, ImpType> dh(nrV, false);

					UInt32 nrC = _Width(range);
					int offsets[9]; for (UInt32 i=1; i!=9; ++i) offsets[i] = displacement_info[i].dx + displacement_info[i].dy * nrC;

					dms_assert(costData.size() == nrV); typename sequence_traits<ImpType>::cseq_t::const_iterator costDataPtr = costData.begin();
					dms_assert(srcNodes.size() == startCosts.size());


					dms_assert(resultData   .size() == nrV); dh.m_ResultDataPtr    = resultData.begin();
					dms_assert(traceBackData.size() == nrV); dh.m_TraceBackDataPtr = traceBackData.begin();

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
							dms_assert(deltaCost >= 0);
							if (deltaCost < dh.m_MaxImp)
								dh.InsertNode(otherNode, currImp + deltaCost, displacement_info[i].d);
						}
					}
					itMap[t].m_IsDone = true;
				}
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

				reportF(ST_MajorTrace, "GridDist completed iteration %d; %d of the %d tiles need reprocessing for %d border cases (%d extra)", ++nrIterations, nrTilesRemaining, tn, numBorderCases, numBorderCases - nrPrevBorderCases);
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

namespace 
{
	template <typename Imp>
	struct GridDistOperSet : tl_oper::inst_tuple<typelists::domain_points, GridDistOperator<Imp, _>>
	{};

	tl_oper::inst_tuple<typelists::floats, GridDistOperSet<_>> gridDistOperSets;

}
