#include "GeoPCH.h"
#pragma hdrstop

#include "Dijkstra.h"

#include "dbg/SeverityType.h"
#include "dbg/Timer.h"
#include "utl/scoped_exit.h"

#include "LockLevels.h"

#include "CheckedDomain.h"
#include "DataCheckMode.h"
#include "DataItemClass.h"
#include "OperationContext.h"
#include "ParallelTiles.h"
#include "UnitClass.h"

#include "makeCululative.h"

#include "TreeBuilder.h"
#include "DijkstraFlags.h"
#include "InvertedRel.h"
#include <agents.h>
#include <concrtrm.h>
#include <semaphore>

template <class T>
inline T absolute(const T& x)
{
	return (x<0) ? -x : x;
}

void CheckNoneMode(const AbstrDataItem* adi, CharPtr role)
{
	if (adi && adi->DetermineActualCheckMode() != DCM_None)
		adi->throwItemError(SharedStr(role) + ": out of range or undefined values not allowed");
}

void CheckDefineMode(const AbstrDataItem* adi, CharPtr role)
{
	if (adi && adi->DetermineActualCheckMode() & DCM_CheckDefined)
		adi->throwItemError(SharedStr(role) + ": undefined values not allowed");
}
bool HasVoidDomainGuarantee(const AbstrDataItem* adi)
{
	return adi && adi->HasVoidDomainGuarantee();
}

void CheckFlags(DijkstraFlag df)
{
	MG_CHECK(!flags(df & DijkstraFlag::OD) || !flags(df & DijkstraFlag::ProdTraceBack));
	MG_CHECK(flags(df & DijkstraFlag::OD) || flags(df & DijkstraFlag::OrgNode));
	MG_CHECK(flags(df & DijkstraFlag::OD) || !flags(df & DijkstraFlag::OrgZone));
	MG_CHECK(flags(df & DijkstraFlag::OD) || !flags(df & DijkstraFlag::UInt64_Od));
	MG_CHECK(flags(df & DijkstraFlag::OD) || !flags(df & DijkstraFlag::OD_Data));
	MG_CHECK(flags(df & DijkstraFlag::Interaction) || !flags(df & DijkstraFlag::InteractionAlpha));
	MG_CHECK(flags(df & DijkstraFlag::Interaction) || !flags(df & DijkstraFlag::OrgMinImp));
	MG_CHECK(flags(df & DijkstraFlag::Interaction) || !flags(df & DijkstraFlag::DstMinImp));
	MG_CHECK(!flags(df & DijkstraFlag::Bidirectional) || !flags(df & DijkstraFlag::BidirFlag));
	MG_CHECK(flags(df & DijkstraFlag::UseAltLinkImp) || !flags(df & DijkstraFlag::ProdOdAltImpedance));
	MG_CHECK(((df & DijkstraFlag::EuclidFlags) == DijkstraFlag::None) || ((df & DijkstraFlag::EuclidFlags) == DijkstraFlag::EuclidFlags));
}
// *****************************************************************************
//									Helper structs
// *****************************************************************************

template <typename NodeType, typename ZoneType, typename ImpType>
struct ZoneInfo {
	const NodeType* Node_rel;
	const ImpType * Impedances;
	const ZoneType* Zone_rel;
};

template <typename NodeType, typename ZoneType, typename ImpType>
struct NetworkInfo {
	typedef ZoneInfo<NodeType, ZoneType, ImpType> zone_info;
	

	NetworkInfo(NodeType nrV_, LinkType nrE_,
		ZoneType nrX_, ZoneType nrY_, ZoneType nrSrcZones_, ZoneType nrDstZones_,
		zone_info&& startPoints_, zone_info&& endPoints_)
	: nrV(nrV_)
	, nrE(nrE_)
	, nrX(nrX_)
	, nrY(nrY_)
	, nrOrgZones(nrSrcZones_)
	, nrDstZones(nrDstZones_)
	, startPoints(std::move(startPoints_))
	, endPoints(std::move(endPoints_))
	{
//		dms_assert(!endPoints.Impedances || endPoints.Zone_rel);
		if (startPoints.Zone_rel)
			orgZone_startPoint_inv.Init(startPoints.Zone_rel, nrX, nrOrgZones);
		else
		{
			if (nrOrgZones == 1)
			{
				dms_assert(nrOrgZones == 1);
				orgZone_startPoint_inv.InitAllToOne(nrX);
			}
			else
			{
				dms_assert(nrOrgZones == nrX); // DijkstraFlags::OD and no Zone_rel => each x is considered as a separate orgZone
			}
		}
		if (!nrV)
			reportD(SeverityTypeID::ST_Warning, "no nodes in network");
		if (!nrE)
			reportD(SeverityTypeID::ST_Warning, "no edges in network");

		if (!nrOrgZones)
			reportD(SeverityTypeID::ST_Warning, "no OrgZones related to network");
		if (!nrDstZones)
			reportD(SeverityTypeID::ST_Warning, "no DstZones related to network");

		if (!nrX)
			reportD(SeverityTypeID::ST_Warning, "no startpoints to relate OrgZones to network nodes");
		if (!nrY)
			reportD(SeverityTypeID::ST_Warning, "no endpoints to relate network nodes to DstZones");
	}

	NodeType  nrV;
	LinkType  nrE;
	ZoneType nrX, nrY, nrOrgZones, nrDstZones;

	ZoneInfo<NodeType, ZoneType, ImpType> startPoints, endPoints;
	Inverted_rel<ZoneType> orgZone_startPoint_inv;
};

template <typename NodeType, typename LinkType, typename ImpType>
struct GraphInfo {

	const NodeType* linkF1Data;
	const NodeType* linkF2Data;
	const ImpType * linkImpDataPtr;

	Inverted_rel<LinkType> node_link1_inv, node_link2_inv;
};

// *****************************************************************************
//									NodeZoneConnector
// *****************************************************************************

//template <typename ImpType>
template <typename NodeType, typename LinkType, typename ZoneType, typename ImpType>
struct NodeZoneConnector 
{
	typedef NetworkInfo<NodeType, ZoneType, ImpType> network_info;

	NodeZoneConnector() {}
//	NodeZoneConnector(const NodeZoneConnector&) {}

	void Init(const network_info& networkInfo, DijkstraFlag df)
	{
		m_NetworkInfoPtr = &networkInfo;
		m_CurrSrcZoneTick = UNDEFINED_VALUE(ZoneType);
		if (!m_ResImpPerDstZone) // init wasn't already done?
		{

			m_ResImpPerDstZone = OwningPtrSizedArray<ImpType>( m_NetworkInfoPtr->nrDstZones MG_DEBUG_ALLOCATOR_SRC("dijkstra: m_ResImpPerDstZone"));

			if (flags(df & DijkstraFlag::OD) && flags(df & DijkstraFlag::SparseResult))
			{
				dms_assert(m_NetworkInfoPtr->nrDstZones);
				m_LastCommittedSrcZone = OwningPtrSizedArray<ZoneType>(m_NetworkInfoPtr->nrDstZones, Undefined() MG_DEBUG_ALLOCATOR_SRC("dijkstra: m_LastCommittedSrcZone"));
				if (flags(df & DijkstraFlag::ProdLinkFlow))
					m_FoundResPerY = OwningPtrSizedArray<ZoneType>(m_NetworkInfoPtr->nrY MG_DEBUG_ALLOCATOR_SRC("dijkstra: m_FoundResPerY"));
			}
			if (m_NetworkInfoPtr->endPoints.Zone_rel)
				m_FoundYPerDstZone = OwningPtrSizedArray<ZoneType>(m_NetworkInfoPtr->nrDstZones MG_DEBUG_ALLOCATOR_SRC("dijkstra: m_FoundYPerDstZone"));
		}
	}

	bool IsDense() const
	{
		return !m_LastCommittedSrcZone;
	}

	void ResetSrc()
	{
		++m_CurrSrcZoneTick;
		if (IsDense())
		{
			fast_undefine(m_ResImpPerDstZone.begin(), m_ResImpPerDstZone.begin() + m_NetworkInfoPtr->nrDstZones);
			if (m_FoundYPerDstZone)
				fast_undefine(m_FoundYPerDstZone.begin(), m_FoundYPerDstZone.begin() + m_NetworkInfoPtr->nrDstZones);
		}
		else
			m_FoundYPerRes.clear();
	}
	bool CommitY(ZoneType y, ImpType dstImp)
	{
		dms_assert(IsDefined(m_CurrSrcZoneTick));
		dms_assert(y < m_NetworkInfoPtr->nrY);
		ZoneType dstZone = m_NetworkInfoPtr->endPoints.Zone_rel ? m_NetworkInfoPtr->endPoints.Zone_rel[y] : y;
		dms_assert(dstZone < m_NetworkInfoPtr->nrDstZones);
		if (IsDense())
		{
			dms_assert(!m_LastCommittedSrcZone);
			dms_assert(!m_FoundResPerY);
			dms_assert(m_FoundYPerRes.empty());
			if (IsDefined(m_ResImpPerDstZone[dstZone]))
			{
				dms_assert(m_ResImpPerDstZone[dstZone] <= dstImp); // Commits come in ascending order
				return false;
			}
		}
		else
		{
			dms_assert(m_LastCommittedSrcZone);
			if (m_LastCommittedSrcZone[dstZone] == m_CurrSrcZoneTick)
			{
				dms_assert(IsDefined(m_ResImpPerDstZone[dstZone]));
				dms_assert(m_ResImpPerDstZone[dstZone] <= dstImp); // Commits come in ascending order
				return false;
			}
			m_LastCommittedSrcZone[dstZone] = m_CurrSrcZoneTick;

			dms_assert(m_FoundYPerRes.size() < m_NetworkInfoPtr->nrDstZones);
			ZoneType resIndex = m_FoundYPerRes.size();
			if (m_FoundResPerY)
				m_FoundResPerY[y] = resIndex;
			m_FoundYPerRes.push_back(y);
		}
		m_ResImpPerDstZone[dstZone] = dstImp;
		if (m_FoundYPerDstZone)
			m_FoundYPerDstZone[dstZone] = y;
		return true;
	}

	ZoneType Y2Res(ZoneType y) const
	{
		dms_assert(IsDefined(m_CurrSrcZoneTick));
		dms_assert(y < m_NetworkInfoPtr->nrY);
		ZoneType dstZone = LookupOrSame(m_NetworkInfoPtr->endPoints.Zone_rel, y);
		dms_assert(dstZone < m_NetworkInfoPtr->nrDstZones);
		if (IsDense())
		{
			dms_assert(!m_LastCommittedSrcZone);
			dms_assert(!m_FoundResPerY);
			if (!IsDefined(m_ResImpPerDstZone[dstZone]))
				return UNDEFINED_VALUE(ZoneType);
			return dstZone;
		}

		if (m_LastCommittedSrcZone[dstZone] != m_CurrSrcZoneTick)
			return UNDEFINED_VALUE(ZoneType);

		dms_assert(m_FoundResPerY);
		dms_assert(IsDefined(m_FoundResPerY[y]));
		return m_FoundResPerY[y];
	}

	ZoneType ZonalResCount() const
	{
		dms_assert(IsDefined(m_CurrSrcZoneTick));

		if (IsDense())
		{
			dms_assert(!m_LastCommittedSrcZone);
			return m_NetworkInfoPtr->nrDstZones;
		}
		else
		{
			dms_assert(m_LastCommittedSrcZone);
			return m_FoundYPerRes.size();
		}
	}

	bool IsConnected(ZoneType zoneID)
	{
		dms_assert(zoneID < m_NetworkInfoPtr->nrDstZones);
		if (!IsDense() && m_LastCommittedSrcZone[zoneID] != m_CurrSrcZoneTick)
			return false;
		return IsDefined(m_ResImpPerDstZone[zoneID]);
	}

	ZoneType Res2EndPoint(ZoneType resIndex)
	{
		dms_assert(resIndex < ZonalResCount());
		dms_assert(m_FoundYPerRes.size() || resIndex < m_NetworkInfoPtr->nrDstZones);
		dms_assert(m_FoundYPerRes.empty() || resIndex < m_FoundYPerRes.size());
		return LookupOrSame(begin_ptr(m_FoundYPerRes), resIndex);
	}

	ZoneType Res2DstZone(ZoneType resIndex)
	{
		ZoneType y = Res2EndPoint(resIndex);
		return LookupOrSame(m_NetworkInfoPtr->endPoints.Zone_rel, y);
	}
	ZoneType DstZone2EndPoint(ZoneType dstZone)
	{
		dms_assert(dstZone < m_NetworkInfoPtr->nrDstZones);

		dms_assert(IsDense() || IsConnected(dstZone));
		if (IsDense() && !IsConnected(dstZone))
			return UNDEFINED_VALUE(NodeType);

		ZoneType y = LookupOrSame(m_FoundYPerDstZone.begin(), dstZone);
		dms_assert(IsDefined(y) || !m_LastCommittedSrcZone);
		return y;
	}

	NodeType DstZone2EndNode(ZoneType dstZone)
	{
		auto y = DstZone2EndPoint(dstZone);
		if (!IsDefined(y))
			return UNDEFINED_VALUE(NodeType);

		NodeType node = LookupOrSame(m_NetworkInfoPtr->endPoints.Node_rel, y);
		dms_assert(node < m_NetworkInfoPtr->nrV);
		return node;
	}
	NodeType Res2EndNode(ZoneType resIndex)
	{
		ZoneType dstZone = Res2DstZone(resIndex);
		NodeType node = DstZone2EndNode(dstZone);
		dms_assert(IsDefined(node) || IsDense());
		dms_assert(IsConnected(dstZone) || !IsDefined(node));
		return node;
	}

	ImpType Res2Imp(ZoneType resIndex)
	{
		ZoneType zoneId = Res2DstZone(resIndex);
		dbg_assert(IsConnected(zoneId));
		ImpType d = m_ResImpPerDstZone[zoneId];
		dms_assert(IsDefined(d) && d < MAX_VALUE(ImpType));
		return d;
	}

	const network_info* m_NetworkInfoPtr = nullptr;

	ZoneType m_CurrSrcZoneTick = UNDEFINED_VALUE(ZoneType);

	// Found: result for current orgZone at the current state of network heap processing
	OwningPtrSizedArray<ImpType>  m_ResImpPerDstZone;   // always                 DstZone -> Found Imp
	OwningPtrSizedArray<ZoneType> m_FoundYPerDstZone;   // optional(DstZone_rel): DstZone -> Found Y; TODO?: replace by m_FoundEndNodePerDstZone
	std::vector<ZoneType>         m_FoundYPerRes;       // optional(sparse): Res -> Found Y
	OwningPtrSizedArray<ZoneType> m_FoundResPerY;       // optional(sparse && ProdLinkFlow): Found Y -> Res

	OwningPtrSizedArray<ZoneType> m_LastCommittedSrcZone;
};

// *****************************************************************************
//									ResultInfo
// *****************************************************************************

template <typename ZoneType, typename ImpType, typename MassType>
struct ResultInfo {
	typedef ImpType * ImpTypeArray;
	typedef ZoneType* ZoneTypeArray;
	typedef MassType* MassTypeArray;
	typedef typename sequence_traits<std::vector<LinkType>>::seq_t::iterator LinkSeqArray;

	ImpTypeArray  od_ImpData, od_AltLinkImp;
	ZoneTypeArray od_SrcZoneIds, od_DstZoneIds;
	ZoneTypeArray od_StartPointIds, od_EndPointIds;
	LinkSeqArray od_LS; // link set
	LinkType* node_TB;

	MassTypeArray OrgFactor, OrgDemand;
	ImpType*      OrgMaxImp;
	MassTypeArray DstFactor, DstSupply;
	MassTypeArray LinkFlow;
};

// *****************************************************************************
//									bunch of locks struct
// *****************************************************************************

struct WriteBlock {
	WriteBlock()
		: od_LS(item_level_type(0), ord_level_type::SpecificOperator, "Dijkstra.LS")
		, dstFactor(item_level_type(0), ord_level_type::SpecificOperator, "Dijkstra.DstFactor")
		, dstSupply(item_level_type(0), ord_level_type::SpecificOperator, "Dijkstra.DstSupply")
	{}
	leveled_critical_section od_LS, dstFactor, dstSupply;
};


// *****************************************************************************
//									ProcessDijkstra
// *****************************************************************************

template <typename NodeType, typename LinkType, typename ZoneType, typename ImpType, typename MassType, typename ParamType>
SizeT ProcessDijkstra(TreeItemDualRef& resultHolder
,	const NetworkInfo<NodeType, ZoneType, ImpType>& ni
,	const ImpType * orgMaxImpedances, bool orgMaxImpedancesHasVoidDomain
,	const MassType* srcMassLimitPtr, bool srcMassLimitHasVoidDomain
,	const MassType* dstMassPtr, bool dstMassHasVoidDomain

,	const GraphInfo<NodeType, LinkType, ImpType>& graph
,	const Inverted_rel<ZoneType>& node_endPoint_inv

,	DijkstraFlag df

// stuff not required for counting
,	const ImpType * altLinkWeights, bool altLinkWeightsHasVoidDomain
,	const ImpType * orgMinImp, bool orgMinImpHasVoidDomain
,	const ImpType * dstMinImp, bool dstMinImpHasVoidDomain
,	const MassType* tgOrgMass, bool tgOrgMassHasVoidDomain
,	const MassType* tgDstMass, bool tdDstMassHasVoidDomain
,   const ParamType tgBetaDecay // Beta Decay Parameter
,	const ParamType tgAlphaLogit
,	const ParamType tgBetaLogit
,	const ParamType tgGammaLogit
,   const ParamType* tgOrgAlpha, bool tgOrgAlphaHasVoidDomain
,	const SizeT*   resCumulCount

// result area
,	SizeT* resCount
,	ResultInfo<ZoneType, ImpType, MassType>&& res
,   tile_id numResultTiles
,	CharPtr   actionMsg
)
{
	DBG_START("ProcessDijkstra", actionMsg, MG_DEBUG_DIJKSTRA);

	Timer processTimer;
	std::atomic<SizeT> resultCount = 0, zoneCount = 0;

	dms_assert(flags(df & DijkstraFlag::OD) || !bool(ni.startPoints.Zone_rel));

	dms_assert(altLinkWeights || !res.od_AltLinkImp);

	bool tgBetaDecayIsZero = (tgBetaDecay == 0.0);
	bool tgBetaDecayIsOne  = (tgBetaDecay == 1.0);
	bool tgBetaDecayIsZeroOrOne = tgBetaDecayIsZero || tgBetaDecayIsOne;
	bool useSrcZoneStamps = flags(df & DijkstraFlag::SparseResult) && flags(df & DijkstraFlag::OD);
	bool useTraceBack = (altLinkWeights || res.od_LS || res.LinkFlow || flags(df & DijkstraFlag::VerboseLogging) && !res.node_TB);

	WriteBlock writeBlocks;

	concurrency::combinable<NodeZoneConnector<NodeType, LinkType, ZoneType, ImpType>> nzcC;
	concurrency::combinable<OwningDijkstraHeap<NodeType, LinkType, ZoneType, ImpType>> dhC;
	concurrency::combinable<TreeRelations> trC;
	concurrency::combinable< std::vector<ImpType>> pot_ijC;

	concurrency::combinable<std::vector<MassType> >  resLinkFlowC;

	// ===================== start looping through all orgZones
	concurrency::task_group tasks;

	unsigned int max_concurrent_reads = IsMultiThreaded1() ? concurrency::GetProcessorCount() : 1;
	auto available_tasks = std::counting_semaphore(max_concurrent_reads);

	for (ZoneType orgZone = 0; orgZone != ni.nrOrgZones; ++orgZone)
	{
		// Wait for a task to release an available slot.
		available_tasks.acquire();
		auto returnTokenOnExit = make_shared_exit([&available_tasks]() { available_tasks.release(); }); // given to task by value

		tasks.run(CreateTaskWithContext(
			[=,
//			returnTokenOnExit = make_any_scoped_exit([&available_tasks]() { available_tasks.release(); }), // given to task by value
			&resultHolder,
			&writeBlocks,
			&ni, &graph, &node_endPoint_inv, &zoneCount, &resultCount, &processTimer,
			&nzcC, &dhC, &trC, &pot_ijC, &resLinkFlowC, // combinables with thread locals to reuse per orgZone
			&res
			]()
			{
				DSM::CancelIfOutOfInterest(resultHolder.GetNew());
				if (OperationContext::CancelableFrame::CurrActiveCanceled())
					return;

				auto& nzc = nzcC.local();
				auto& dh = dhC.local();
				TreeRelations& tr = trC.local();
				auto& nodeALW = dh.m_AltLinkWeight;
				auto& resLinkFlow = resLinkFlowC.local();

				dms_assert(orgZone < ni.nrOrgZones);
				dms_assert(dh.Empty());

				if (!nzc.m_ResImpPerDstZone) // initialized ?
				{
					nzc.Init(ni, df);
					dh.Init(ni.nrV, useSrcZoneStamps, useTraceBack);
				}

				bool trIsUsed = false;
				if (altLinkWeights || res.LinkFlow)
				{
					tr.InitNodes(ni.nrV);
					trIsUsed = true;
					if (!nodeALW)
						nodeALW = OwningPtrSizedArray<ImpType>(ni.nrV MG_DEBUG_ALLOCATOR_SRC("dijkstra: nodeALW"));
					if (res.LinkFlow)
						resLinkFlow.resize(ni.nrE, 0);
				}

				
				if (res.node_TB)
				{
					dms_assert(!useTraceBack);
					dms_assert(!flags(df & DijkstraFlag::OD));
					dms_assert(!dh.m_TraceBackData);
					dh.m_TraceBackDataPtr = res.node_TB;
					fast_undefine(dh.m_TraceBackDataPtr, dh.m_TraceBackDataPtr + ni.nrV); // REMOVE WHEN new Tree is implemented
				}

				// ===================== initialization of a new orgZone
				dh.m_MaxImp = (orgMaxImpedances) ? orgMaxImpedances[orgMaxImpedancesHasVoidDomain ? 0 : orgZone] : MAX_VALUE(ImpType);

				dh.ResetImpedances();
				nzc.ResetSrc();

				// ===================== InsertNode for all statPoints of current orgZone
				for (ZoneType startPointIndex = ni.orgZone_startPoint_inv.FirstOrSame(orgZone); IsDefined(startPointIndex); startPointIndex = ni.orgZone_startPoint_inv.NextOrNone(startPointIndex))
				{
					dms_assert(startPointIndex < ni.nrX); // guaranteed by InvertedRel
					NodeType startNode = ni.startPoints.Node_rel ? ni.startPoints.Node_rel[startPointIndex] : startPointIndex;
					dh.InsertNode(startNode, ni.startPoints.Impedances ? ni.startPoints.Impedances[startPointIndex] : ImpType(0), UNDEFINED_VALUE(LinkType));
					if (trIsUsed)
						tr.InitRootNode(startNode);
				};
				MassType
					cumulativeMass = 0,
					maxSrcMass = (srcMassLimitPtr) ? srcMassLimitPtr[srcMassLimitHasVoidDomain ? 0 : orgZone] : MAX_VALUE(MassType);

				// ===================== iterate through buffer until all destinations are processed or cut or limit has been reached.
				typedef heapElemType<ImpType, ZoneType> EndPointHeapElemType;
				std::vector<EndPointHeapElemType> endPointHeap;

				// iterate through buffer until destinations are processed.
				while (!dh.Empty())
				{
					NodeType currNode = dh.Front().Value(); dms_assert(currNode < ni.nrV);
					ImpType currImp = dh.Front().Imp();

					dh.PopNode();
					dms_assert(currImp >= 0);

					// If alternative route from heap was faster: accept that when we are certain of it 
					if (!dh.MarkFinal(currNode, currImp))
						continue;

					if (flags(df & DijkstraFlag::VerboseLogging))
					{
						reportF(SeverityTypeID::ST_MajorTrace, "Node %1% accepted at impedance %2% from link %3%"
							, currNode, currImp
							, dh.m_TraceBackDataPtr[currNode]
						);
					}

					if (trIsUsed)
					{
						dms_assert(dh.m_TraceBackDataPtr);
						LinkType backLink = dh.m_TraceBackDataPtr[currNode];
						if (IsDefined(backLink))
						{
							dms_assert(backLink < ni.nrE);
							NodeType parentNode;
							if (graph.linkF2Data[backLink] == currNode)
								parentNode = graph.linkF1Data[backLink];
							else
							{
								dms_assert(graph.linkF1Data[backLink] == currNode);
								parentNode = graph.linkF2Data[backLink];
							}
							dms_assert(parentNode < ni.nrV);
							tr.InitChildNode(currNode, parentNode);
						}
					}
					ZoneType y = node_endPoint_inv.FirstOrSame(currNode);
					if (ni.endPoints.Impedances) // check if max cumulative mass has been reached.
					{
//						dms_assert(ni.endPoints.Zone_rel);
						while (IsDefined(y))
						{
							dms_assert(y < ni.nrY);
							dms_assert(ni.endPoints.Impedances[y] >= 0);
							ImpType endpointImpData = currImp + ni.endPoints.Impedances[y];

							if (flags(df & DijkstraFlag::VerboseLogging))
							{
								ZoneType dstZone = ni.endPoints.Zone_rel ? ni.endPoints.Zone_rel[y] : y;
								dms_assert(dstZone < ni.nrDstZones);
								reportF(SeverityTypeID::ST_MajorTrace, "Node %1% connects to DstZone %2% at impedance %3% through endPoint %4%"
									, currNode, dstZone, endpointImpData, y
								);
							}

							endPointHeap.push_back(EndPointHeapElemType(y, endpointImpData));
							std::push_heap(endPointHeap.begin(), endPointHeap.end());

							y = node_endPoint_inv.NextOrNone(y);
						}
						while (!endPointHeap.empty() && endPointHeap.front().Imp() <= currImp)
						{
							ZoneType yy = endPointHeap.front().Value();
							ImpType dstImp = endPointHeap.front().Imp();

							if (nzc.CommitY(yy, dstImp))
							{
								if (flags(df & DijkstraFlag::VerboseLogging))
								{
									ZoneType dstZone = ni.endPoints.Zone_rel ? ni.endPoints.Zone_rel[yy] : yy;
									dms_assert(dstZone < ni.nrDstZones);
									reportF(SeverityTypeID::ST_MajorTrace, "Committed to DstZone %1% at impedance %2% through endPoint %3%"
										, dstZone, dstImp, yy
									);
								}

								if (dstMassPtr)
								{
									ZoneType dstZone = ni.endPoints.Zone_rel ? ni.endPoints.Zone_rel[yy] : yy;
									dms_assert(dstZone < ni.nrDstZones);
									cumulativeMass += dstMassPtr[dstMassHasVoidDomain ? 0 : dstZone];
									if (cumulativeMass >= maxSrcMass)
										MakeMin(dh.m_MaxImp, dstImp); // no new zones will be finalized that are beyond this zone. 
								}
							}

							std::pop_heap(endPointHeap.begin(), endPointHeap.end());
							endPointHeap.pop_back();
						}
					}
					else
					{
						while (IsDefined(y))
						{
							dms_assert(y < ni.nrY);
							if (nzc.CommitY(y, currImp) && dstMassPtr)
							{
								ZoneType dstZone = ni.endPoints.Zone_rel ? ni.endPoints.Zone_rel[y] : y;
								dms_assert(dstZone < ni.nrDstZones);
								cumulativeMass += dstMassPtr[dstMassHasVoidDomain ? 0 : dstZone];
								if (cumulativeMass >= maxSrcMass)
									MakeMin(dh.m_MaxImp, currImp); // no new zones will be finalized that are beyond this zone. 
							}

							if (flags(df & DijkstraFlag::VerboseLogging))
							{
								ZoneType dstZone = ni.endPoints.Zone_rel ? ni.endPoints.Zone_rel[y] : y;
								dms_assert(dstZone < ni.nrDstZones);
								reportF(SeverityTypeID::ST_MajorTrace, "Node %1% identifies with DstZone %2% at impedance %3% through endPoint %4%"
									, currNode, dstZone, currImp, y
								);
							}

							y = node_endPoint_inv.NextOrNone(y);
						}
					}

					// traverse neighbouring nodes
					LinkType currLink = graph.node_link1_inv.First(currNode);
					while (currLink != UNDEFINED_VALUE(LinkType))
					{
						dms_assert(currLink < ni.nrE);
						NodeType otherNode = graph.linkF2Data[currLink]; dms_assert(otherNode < ni.nrV);
						ImpType deltaCost = graph.linkImpDataPtr[currLink];
						dms_assert(deltaCost >= 0);
						if (deltaCost < dh.m_MaxImp)
							dh.InsertNode(otherNode, currImp + deltaCost, currLink);
						currLink = graph.node_link1_inv.Next(currLink);
					}
					if (!flags(df & (DijkstraFlag::Bidirectional | DijkstraFlag::BidirFlag)))
						continue;

					currLink = graph.node_link2_inv.First(currNode);
					while (currLink != UNDEFINED_VALUE(LinkType))
					{
						dms_assert(currLink < ni.nrE);
						NodeType otherNode = graph.linkF1Data[currLink]; dms_assert(otherNode < ni.nrV);
						ImpType deltaCost = graph.linkImpDataPtr[currLink];
						dms_assert(deltaCost >= 0);
						if (deltaCost < dh.m_MaxImp)
							dh.InsertNode(otherNode, currImp + deltaCost, currLink);
						currLink = graph.node_link2_inv.Next(currLink);
					}
				}
				if (ni.endPoints.Impedances)
				{
					// postprocess remaining endPoints.
					while (!endPointHeap.empty() && endPointHeap.front().Imp() < dh.m_MaxImp)
					{
						ZoneType yy = endPointHeap.front().Value();
						ImpType dstImp = endPointHeap.front().Imp();

						if (nzc.CommitY(yy, dstImp))
						{
							if (flags(df & DijkstraFlag::VerboseLogging))
							{
								ZoneType dstZone = ni.endPoints.Zone_rel ? ni.endPoints.Zone_rel[yy] : yy;
								dms_assert(dstZone < ni.nrDstZones);
								reportF(SeverityTypeID::ST_MajorTrace, "Finally committed to dstzone %1% at impedance %2% through endpoint %3%"
									, dstZone, dstImp, yy
								);
							}

							if (dstMassPtr)
							{
								ZoneType dstZone = ni.endPoints.Zone_rel ? ni.endPoints.Zone_rel[yy] : yy;
								dms_assert(dstZone < ni.nrDstZones);
								cumulativeMass += dstMassPtr[dstMassHasVoidDomain ? 0 : dstZone];
								if (cumulativeMass >= maxSrcMass)
									MakeMin(dh.m_MaxImp, dstImp); // no new zones will be finalized that are beyond this zone. 
							}
						}

						std::pop_heap(endPointHeap.begin(), endPointHeap.end());
						endPointHeap.pop_back();
					}
				}
				// ===================== count number of reached/processed dstZones j from this orgZone i 
				SizeT zonalResultCount = nzc.ZonalResCount();
				SizeT resultCountBase = nzc.IsDense() ? SizeT(ni.nrDstZones) * SizeT(orgZone) : resCumulCount ? resCumulCount[orgZone] : SizeT(0);

				// ===================== write optional od related res.od_ImpData, res.od_DstZoneIds, and res.od_SrcZoneIds

				resultCount += zonalResultCount;

				if (nzc.IsDense())
				{
					dms_assert(zonalResultCount == ni.nrDstZones);

					if (res.od_ImpData)
						fast_copy(nzc.m_ResImpPerDstZone.begin(), nzc.m_ResImpPerDstZone.begin() + zonalResultCount, res.od_ImpData + resultCountBase);
					if (res.od_DstZoneIds)
					{
						auto dstZonePtr = res.od_DstZoneIds + resultCountBase + zonalResultCount;
						ZoneType c = zonalResultCount;
						while (c)
							*--dstZonePtr = --c;
					}
					if (res.od_EndPointIds)
					{
						auto endPointPtr = res.od_EndPointIds + resultCountBase + zonalResultCount;
						ZoneType c = zonalResultCount;
						while (c)
							*--endPointPtr = nzc.DstZone2EndPoint(--c);
					}
				}
				else
				{
					if (flags(df & DijkstraFlag::Counting))
					{
						dms_assert(resCount);
						dms_assert(!res.od_ImpData);
						dms_assert(!res.od_DstZoneIds);
						dms_assert(orgZone < ni.nrOrgZones);
						resCount[orgZone] = zonalResultCount;
					}
					else
					{
						if (res.od_ImpData)
						{
							auto currPtr = res.od_ImpData + resultCountBase;
							for (ZoneType resIndex = 0; resIndex != zonalResultCount; ++resIndex)
								*currPtr++ = nzc.Res2Imp(resIndex);
						}
						if (res.od_DstZoneIds) {
							auto currPtr = res.od_DstZoneIds + resultCountBase;
							for (ZoneType resIndex = 0; resIndex != zonalResultCount; ++resIndex)
								*currPtr++ = nzc.Res2DstZone(resIndex);
						}
						if (res.od_EndPointIds) {
							auto currPtr = res.od_EndPointIds + resultCountBase;
							for (ZoneType resIndex = 0; resIndex != zonalResultCount; ++resIndex)
								*currPtr++ = nzc.Res2EndPoint(resIndex);
						}
					}
				}
				if (res.od_SrcZoneIds)
				{
					auto currPtr = res.od_SrcZoneIds + resultCountBase;
					fast_fill(currPtr, currPtr + zonalResultCount, orgZone);
				}
				// ===================== calculate optional alternative impedances, and write to resAltLinkImp if requested
				const ImpType* d_vj = dh.m_ResultDataPtr;
				dms_assert(nodeALW || !res.od_AltLinkImp);
				if (altLinkWeights)
				{
					dms_assert(nodeALW);
					dms_assert(dh.m_TraceBackDataPtr);
					dms_assert(trIsUsed);

					for (ZoneType startPointIndex = ni.orgZone_startPoint_inv.FirstOrSame(orgZone); IsDefined(startPointIndex); startPointIndex = ni.orgZone_startPoint_inv.NextOrNone(startPointIndex)) // walk through each tree of all startPoints
					{
						NodeType node = LookupOrSame(ni.startPoints.Node_rel, startPointIndex);
						dms_assert(node < ni.nrV);
						nodeALW[node] = 0;

						treenode_pointer currNodePtr = &tr.m_TreeNodes[node];
						dms_assert(!currNodePtr->m_ParentNode);
						while (currNodePtr = tr.WalkDepthFirst_TopDown(currNodePtr))
						{
							dms_assert(currNodePtr && currNodePtr->m_ParentNode);
							node = tr.NrOfNode(currNodePtr);
							dms_assert(node < ni.nrV);

							LinkType currLink = dh.m_TraceBackDataPtr[node];
							NodeType prevNode = tr.NrOfNode(currNodePtr->m_ParentNode);
							dms_assert(currLink < ni.nrE);
							nodeALW[node] = altLinkWeights[altLinkWeightsHasVoidDomain ? 0 : currLink] + nodeALW[prevNode];
						}
					}

					// copy nodeALW -> resAltLinkImp
					if (res.od_AltLinkImp) {
						auto currPtr = res.od_AltLinkImp + resultCountBase;
						for (SizeT j = 0; j != zonalResultCount; ++j)
						{
							NodeType node = nzc.Res2EndNode(j);
							dms_assert(IsDefined(node) || nzc.IsDense());
							dms_assert(!IsDefined(node) || !dh.IsStale(node));// guaranteed by Res2EndNode(j);

							*currPtr++ = IsDefined(node) ? nodeALW[node] : UNDEFINED_VALUE(ImpType);
						}
					}
					d_vj = nodeALW.begin();
				}

				// ===================== interaction: calculate t[i, j] etc.
				if (flags(df & DijkstraFlag::InteractionOrMaxImp))
				{
					std::vector<ImpType>& pot_ij = pot_ijC.local();
					Float64 totalPotential = 0;
					ImpType maxImp = 0;

					// t[i, j] := impedance[i, j] ^ -beta;
					// pot[i, j] : = w[j] * t[i, j]
					// OrgZone_factor[i] : = SUM j: pot[i, j] = D_i

					// flow[i, j] : = v[i] * pot[i, j] * D_i ^ (alpha - 1);
					// OrgZone_demand[i] : = SUM j: flow[i, j] 
					// DstZone_supply[j] : = SUM i: flow[i, j]
					// flow[l] : = SUM l in (i->j): flow[i, j]
					// see: http://wiki.objectvision.nl/index.php/Accessibility#Attraction_Potential
					// and: http://wiki.objectvision.nl/index.php/Dijkstra_functions

					dms_assert(flags(df & DijkstraFlag::Calc_pot_ij) == (res.DstFactor || res.DstSupply || res.LinkFlow));
					if (flags(df & DijkstraFlag::Calc_pot_ij))
						vector_zero_n_reuse(pot_ij, zonalResultCount);
					for (SizeT j = 0; j != zonalResultCount; ++j)
					{
						ZoneType dstZone = nzc.Res2DstZone(j);
						dms_assert(dstZone < ni.nrDstZones);
						dms_assert(!flags(df & DijkstraFlag::SparseResult) || nzc.IsConnected(dstZone));

						NodeType node = nzc.DstZone2EndNode(dstZone);
						if (!IsDefined(node))
						{
							dms_assert(!flags(df & DijkstraFlag::SparseResult) || !flags(df & DijkstraFlag::OD));
							dbg_assert(!nzc.IsConnected(dstZone));
							continue;
						}
						Float64 impedance = d_vj[node];
						dms_assert(IsDefined(impedance));
						dms_assert(impedance >= 0.0);
						if (ni.endPoints.Impedances)
							impedance += ni.endPoints.Impedances[dstZone]; // TODO: check of dit niet dubbel werk is

						if (orgMinImp) MakeMax(impedance, orgMinImp[orgMinImpHasVoidDomain ? 0 : orgZone]);
						if (dstMinImp) MakeMax(impedance, dstMinImp[dstMinImpHasVoidDomain ? 0 : dstZone]);
						dms_assert(impedance >= 0.0);

						if (impedance <= 0 && !tgBetaDecayIsZero)
							continue;
						dms_assert(impedance > 0 || tgBetaDecayIsZero);
						MakeMax(maxImp, impedance);

						if (!flags(df & DijkstraFlag::Interaction))
							continue; // next j.

						// t_ij
						Float64 potential;

						dms_assert(flags(df & (DijkstraFlag::DistDecay | DijkstraFlag::DistLogit)));

						if (flags(df & DijkstraFlag::DistDecay))
							potential = (tgBetaDecayIsZeroOrOne)
							? tgBetaDecayIsZero ? 1.0 : 1.0 / impedance
							: exp(log(impedance)*-tgBetaDecay);
						else
						{
							if (impedance > 0)
								potential = 1.0 / (1.0 + exp(tgAlphaLogit + tgBetaLogit * log(impedance) + tgGammaLogit * impedance));
							else if (tgBetaLogit == 0)
								potential = 1.0 / (1.0 + exp(tgAlphaLogit + tgGammaLogit * impedance));
							else if (tgBetaLogit < 0)
								potential = 0;
						}
						if (flags(df & DijkstraFlag::Calc_pot_ij))
							pot_ij[j] = potential;

						if (tgDstMass)
							potential *= tgDstMass[tdDstMassHasVoidDomain ? 0 : dstZone];  // w_j * t_ij
						totalPotential += potential; // SUM w_j * t_ij = D_i
					}
					if (res.OrgMaxImp)
						res.OrgMaxImp[orgZone] = maxImp;
					if (!flags(df & DijkstraFlag::Interaction))
						goto afterInteraction;
					if (res.OrgFactor)
						res.OrgFactor[orgZone] = totalPotential; // D_i

					MassType orgMass = (tgOrgMass) ? tgOrgMass[tgOrgMassHasVoidDomain ? 0 : orgZone] : 1.0;
					Float64 orgAlpha = (tgOrgAlpha) ? tgOrgAlpha[tgOrgAlphaHasVoidDomain ? 0 : orgZone] : 0.0;
					dms_assert(orgAlpha >= 0.0);
					if (totalPotential)
					{
						dms_assert(totalPotential >= 0.0);
						Float64 balancingFactor = orgMass;
						if (orgAlpha != 0.0)
							balancingFactor *= (orgAlpha == 1.0) ? totalPotential : exp(log(totalPotential) * orgAlpha);

						if (res.OrgDemand)
							res.OrgDemand[orgZone] = balancingFactor; // v_i * D_i^alpha
						balancingFactor /= totalPotential; // v_i * D_i^(alpha-1)

						for (SizeT j = 0; j != zonalResultCount; ++j)
						{
							if (flags(df & DijkstraFlag::Calc_pot_ij))
								pot_ij[j] *= balancingFactor; // v_i * D_i^(alpha-1) * t_ij

							ZoneType dstZone = nzc.Res2DstZone(j);
							dms_assert(dstZone < ni.nrDstZones);
							dms_assert(!flags(df & DijkstraFlag::SparseResult) || nzc.IsConnected(dstZone));
							dms_assert(nzc.IsConnected(dstZone) || pot_ij.empty() || (pot_ij[j] == 0));

							if (res.DstFactor)
							{
								dms_assert(!pot_ij.empty());

								leveled_critical_section::scoped_lock lock(writeBlocks.dstFactor);
								res.DstFactor[dstZone] += pot_ij[j];
							}
							if (res.DstSupply)
							{
								dms_assert(!pot_ij.empty());
								if (tgDstMass)
									pot_ij[j] *= tgDstMass[tdDstMassHasVoidDomain ? 0 : dstZone];  // v_i * D_i^(alpha-1) * w_j * t_ij
								leveled_critical_section::scoped_lock lock(writeBlocks.dstSupply);
								res.DstSupply[dstZone] += pot_ij[j];
							}
						}
					}
					else
					{
						if (res.OrgDemand)
							res.OrgDemand[orgZone] = 0.0; // v_i * D_i^alpha with D_i is zero.
					}
					// ===================== Write Link_flow, WARNING: nodeALW is reused and overwritten.
					if (res.LinkFlow && totalPotential)
					{
						dms_assert(dh.m_TraceBackDataPtr);
						dms_assert(nodeALW);
						dms_assert(tr.IsUsed());
						dms_assert(flags(df & DijkstraFlag::Interaction));

						// init nodeALW for used nodes to zero.
						for (ZoneType startPointIndex = ni.orgZone_startPoint_inv.FirstOrSame(orgZone); IsDefined(startPointIndex); startPointIndex = ni.orgZone_startPoint_inv.NextOrNone(startPointIndex))
						{
							NodeType currNode = ni.startPoints.Node_rel ? ni.startPoints.Node_rel[startPointIndex] : startPointIndex;
							dms_assert(currNode < ni.nrV);

							treenode_pointer currNodePtr = &tr.m_TreeNodes[currNode];

							for (currNodePtr = tr.MostDown(currNodePtr); currNodePtr->m_ParentNode; currNodePtr = tr.WalkDepthFirst_BottomUp(currNodePtr))
							{
								dms_assert(currNodePtr && currNodePtr->m_ParentNode);
								currNode = tr.NrOfNode(currNodePtr);
								dms_assert(currNode < ni.nrV);

								nodeALW[currNode] = 0;
							}
						}

						for (ZoneType startPointIndex = ni.orgZone_startPoint_inv.FirstOrSame(orgZone); IsDefined(startPointIndex); startPointIndex = ni.orgZone_startPoint_inv.NextOrNone(startPointIndex))
						{
							NodeType currNode = ni.startPoints.Node_rel ? ni.startPoints.Node_rel[startPointIndex] : startPointIndex;
							dms_assert(currNode < ni.nrV);

							treenode_pointer currNodePtr = &tr.m_TreeNodes[currNode];

							for (currNodePtr = tr.MostDown(currNodePtr); currNodePtr->m_ParentNode; currNodePtr = tr.WalkDepthFirst_BottomUp(currNodePtr))
							{
								dms_assert(currNodePtr && currNodePtr->m_ParentNode);
								currNode = tr.NrOfNode(currNodePtr);
								dms_assert(currNode < ni.nrV);

								MassType* flowPtr = &nodeALW[currNode];
								ZoneType y = node_endPoint_inv.FirstOrSame(currNode);
								while (IsDefined(y))
								{
									ZoneType j = nzc.Y2Res(y);
									if (IsDefined(j))
										*flowPtr += pot_ij[j];
									y = node_endPoint_inv.NextOrNone(y);
								}
								NodeType prevNode = tr.NrOfNode(currNodePtr->m_ParentNode);
								dms_assert(prevNode < ni.nrV);
								LinkType currLink = dh.m_TraceBackDataPtr[currNode];
								dms_assert(currLink < ni.nrE);

								MassType flow = *flowPtr;
								nodeALW[prevNode] += flow;

								resLinkFlow[currLink] += flow;
							}
						}
					}
				}

				// ===================== Write resulting Link Set per found OD pair
			afterInteraction:
				if (res.od_LS)
				{
					dms_assert(dh.m_TraceBackDataPtr);
					for (SizeT j = 0; j != zonalResultCount; ++j)
					{
						NodeType node = nzc.Res2EndNode(j);
						dms_assert(IsDefined(node) || nzc.IsDense());
						dms_assert(!IsDefined(node) || !dh.IsStale(node)); // guaranteed by IsConnected(zoneId);
						if (!IsDefined(node))
							continue;
						leveled_critical_section::scoped_lock lock(writeBlocks.od_LS);
						auto resLinkSetRef = res.od_LS[resultCountBase + j];
						while (true)
						{
							dms_assert(node < ni.nrV);
							LinkType currLink = dh.m_TraceBackDataPtr[node];
							dms_assert(!dh.IsStale(node));
							if (!IsDefined(currLink))
								break;
							dms_assert(currLink < ni.nrE);

							if (graph.linkF2Data[currLink] == node)
								node = graph.linkF1Data[currLink];
							else
							{
								dms_assert(graph.linkF1Data[currLink] == node);
								node = graph.linkF2Data[currLink];
							}
							dms_assert(IsDefined(node));

							resLinkSetRef.push_back(currLink);
						}
					}
				}

				// ===================== report nrOrgZones every 5 seconds
				zoneCount++;
				if (processTimer.PassedSecs(5))
					reportF(SeverityTypeID::ST_MajorTrace, "DijkstraMatr %s %d of %d sources: resulted in %u od-pairs", actionMsg, zoneCount, ni.nrOrgZones, resultCount);
				&returnTokenOnExit;
			}
		));
	}
	tasks.wait(); // Wait for all tasks to finish.
	if (res.LinkFlow)
		resLinkFlowC.combine_each(
			[&res](std::vector<MassType>& localLinkFlow){
				auto linkFlowPtr = res.LinkFlow;
				for (auto flowPtr = localLinkFlow.begin(), flowEnd = localLinkFlow.end(); flowPtr != flowEnd; ++flowPtr, ++linkFlowPtr)
					*linkFlowPtr += *flowPtr;
		});
	

	if (OperationContext::CancelableFrame::CurrActiveCanceled())
		return UNDEFINED_VALUE(SizeT);

	reportF(SeverityTypeID::ST_MajorTrace, "DijkstraMatr %s all %d sources: resulted in %u od-pairs", actionMsg, ni.nrOrgZones, resultCount);

	return resultCount;
}


// *****************************************************************************
//									DijkstraMatr
// *****************************************************************************

template <class T>
class DijkstraMatrOperator : public VariadicOperator
{
	typedef T       ImpType;
	typedef T       MassType;
	typedef typename div_type<T>::type ParamType;
	typedef UInt32 LinkType;
	typedef UInt32 ZoneType;
	typedef std::vector<LinkType> EdgeSeq;

	typedef DataArray<ImpType>  ArgImpType;  // link_imp, alt_imp, (optional) impCut: E->Imp
	typedef DataArray<NodeType> ArgNodeType; // link_f1, link_f2: E->V
	typedef DataArray<ZoneType> ArgZoneType; // link_f1, link_f2: E->V
	typedef DataArray<Bool>     ArgFlagType; // E->Bool

	typedef ArgNodeType SrcNodeType; // x->V:       Node bij iedere x uit de instapset 
	typedef ArgImpType  SrcDistType; // x->Imp:    impedance vanuit SrcZone tot aan iedere startmode
	typedef ArgZoneType SrcZoneType; // x->SrcZone: SrcZone waar deze instap toe behoort

	typedef ArgNodeType DstNodeType; // y->V:       Node bij iedere y uit de uitstapeset
	typedef ArgImpType  DstDistType; // y->Imp:    impedance van endnode totaan de EndZone
	typedef ArgZoneType DstZoneType; // y->DstZone: EndZone waar deze uitstap toe leidt

	// Optional DijkstraFlag::DstLimit
	typedef DataArray<MassType> ArgMassType; // OrgZone->Mass, DstZone->Mass: limit mass die voor SrcZone bereikt moet worden

	// Optional DijkstraFlag::Interaction or DijkstraFlag::NetworkAssignment
	typedef DataArray<MassType> ArgSMType; // SrcZone->Mass: Mass that will be distributed among Destibations, resulting in Accessibility per destination
	typedef DataArray<MassType> ArgDMType; // DstZone->Mass: Attractiveness of Destination for network assignment: E->Flow
	typedef DataArray<ParamType> ArgParamType; //  0 - no decay, 1 = 1/d, 2=1/d^2 etc.


	typedef Unit<ZoneType>      ResultUnitType; // SrcZone * DstZone (or a subset thereof if a maximum dist is provided as 10th argument)
	typedef DataArray<ImpType>  ResultImpType; // ResultType->ImpType: resulting cost
	typedef DataArray<ZoneType> ResultSubType; // ResultType->ZoneType (subitems SrcZone and DstZone) in case of a sparse result (MaxDist or LimitMass)
	typedef DataArray<EdgeSeq > ResultLinkSetType; // DijkstraFlag::ProdOdLinkSet

	static UInt32 CalcNrArgs(DijkstraFlag df)
	{
		UInt32 nrArgs = 4;
		if (flags(df & DijkstraFlag::BidirFlag)) ++nrArgs;
		if (flags(df & DijkstraFlag::OrgNode)) ++nrArgs;
		if (flags(df & DijkstraFlag::OrgImp)) ++nrArgs;
		if (flags(df & DijkstraFlag::OrgZone)) ++nrArgs;
		if (flags(df & DijkstraFlag::OrgZoneLoc)) ++nrArgs;
		if (flags(df & DijkstraFlag::OrgMinImp)) ++nrArgs;
		if (flags(df & DijkstraFlag::DstNode)) ++nrArgs;
		if (flags(df & DijkstraFlag::DstImp)) ++nrArgs;
		if (flags(df & DijkstraFlag::DstZone)) ++nrArgs;
		if (flags(df & DijkstraFlag::DstZoneLoc)) ++nrArgs;
		if (flags(df & DijkstraFlag::DstMinImp)) ++nrArgs;
		if (flags(df & DijkstraFlag::ImpCut)) ++nrArgs;
		if (flags(df & DijkstraFlag::DstLimit)) nrArgs += 2;
		if (flags(df & DijkstraFlag::UseEuclidicFilter)) nrArgs += 1; // requires OrgZoneLoc and DstZoneLoc
		if (flags(df & DijkstraFlag::UseAltLinkImp)) ++nrArgs;
		if (flags(df & DijkstraFlag::Interaction)) nrArgs += 2;
		if (flags(df & DijkstraFlag::DistDecay)) nrArgs += 1;
		if (flags(df & DijkstraFlag::DistLogit)) nrArgs += 3;
		if (flags(df & DijkstraFlag::InteractionAlpha)) ++nrArgs;

		dms_assert(nrArgs >= 3 && nrArgs <= 24);
		return nrArgs;
	}

	oper_arg_policy GetArgPolicy(arg_index argNr, CharPtr firstArgValue) const override
	{
		dms_assert(firstArgValue == nullptr || *firstArgValue == char(0));
		return (argNr) 
			? oper_arg_policy::calc_as_result
			: oper_arg_policy::calc_always;
	}
	static const UnitClass* GetResultUnitClass(DijkstraFlag df)
	{
		dms_assert(flags(df & DijkstraFlag::OD)); // PRECONDTION;
		return flags(df & DijkstraFlag::UInt64_Od)
			?	Unit<UInt64>::GetStaticClass()
			:	Unit<UInt32>::GetStaticClass();
	}

public:
	DijkstraMatrOperator(AbstrOperGroup* og, DijkstraFlag df)
		: VariadicOperator(og, flags(df & DijkstraFlag::OD) ? typesafe_cast<const Class*>(GetResultUnitClass(df)) : typesafe_cast<const Class*>(ResultImpType::GetStaticClass())
		,	CalcNrArgs(df))
		,	m_OperFlags(df)
	{
		ClassCPtr* argClsIter = m_ArgClasses.get();

		dms_assert(og->AllowExtraArgs());
		*argClsIter++ = DataArray<SharedStr>::GetStaticClass();
		// weighted network args
		*argClsIter++ = ArgImpType::GetStaticClass();
		*argClsIter++ = ArgNodeType::GetStaticClass();
		*argClsIter++ = ArgNodeType::GetStaticClass();

		dms_assert(m_ArgClassesEnd == argClsIter);
	}

	// Override Operator
	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		DijkstraFlag df = m_OperFlags;

		arg_index argCounter = 0;
		const AbstrDataItem* paramStrA = AsDataItem(args[argCounter++]);
		MG_CHECK(paramStrA->HasVoidDomainGuarantee());

		df = DijkstraFlag(df | ParseDijkstraString(const_array_cast<SharedStr>(paramStrA)->GetIndexedValue(0).c_str()));
		CheckFlags(DijkstraFlag(df));

		MG_CHECK(args.size() == CalcNrArgs(df)); // did user specify correctly?

		const AbstrDataItem* adiLinkImp             = AsDataItem(args[argCounter++]); 
		const AbstrDataItem* adiLinkF1              = AsDataItem(args[argCounter++]);
		const AbstrDataItem* adiLinkF2              = AsDataItem(args[argCounter++]);
		const AbstrDataItem* adiLinkBidirFlag       = flags(df & DijkstraFlag::BidirFlag) ? AsCheckedDataItem(args[argCounter++]) : nullptr;
		const AbstrDataItem* adiStartPointNode      = flags(df & DijkstraFlag::OrgNode) ? AsCheckedDataItem(args[argCounter++]) : nullptr;
		const AbstrDataItem* adiStartPointImpedance = flags(df & DijkstraFlag::OrgImp) ? AsCheckedDataItem(args[argCounter++]) : nullptr;
		const AbstrDataItem* adiStartPoinOrgZone    = flags(df & DijkstraFlag::OrgZone) ? AsCheckedDataItem(args[argCounter++]) : nullptr;
		const AbstrDataItem* adiOrgZoneLocation     = flags(df & DijkstraFlag::OrgZoneLoc) ? AsCheckedDataItem(args[argCounter++]) : nullptr;
		const AbstrDataItem* adiEndPointNode        = flags(df & DijkstraFlag::DstNode) ? AsCheckedDataItem(args[argCounter++]) : nullptr;
		const AbstrDataItem* adiEndPointImpedance   = flags(df & DijkstraFlag::DstImp) ? AsCheckedDataItem(args[argCounter++]) : nullptr;
		const AbstrDataItem* adiEndPointDstZone     = flags(df & DijkstraFlag::DstZone) ? AsCheckedDataItem(args[argCounter++]) : nullptr;
		const AbstrDataItem* adiDstZoneLocation     = flags(df & DijkstraFlag::DstZoneLoc) ? AsCheckedDataItem(args[argCounter++]) : nullptr;

		const AbstrDataItem* adiOrgMaxImp           = flags(df & DijkstraFlag::ImpCut) ? AsCheckedDataItem(args[argCounter++]) : nullptr;
		const AbstrDataItem* adiOrgMassLimit        = flags(df & DijkstraFlag::DstLimit) ? AsCheckedDataItem(args[argCounter++]) : nullptr;
		const AbstrDataItem* adiDstMassLimit        = flags(df & DijkstraFlag::DstLimit) ? AsCheckedDataItem(args[argCounter++]) : nullptr;

		const AbstrDataItem* adiEuclidicSqrDist     = flags(df & DijkstraFlag::UseEuclidicFilter) ? AsCheckedDataItem(args[argCounter++]) : nullptr;

		const AbstrDataItem* adiLinkAltImp      = flags(df & DijkstraFlag::UseAltLinkImp) ? AsCheckedDataItem(args[argCounter++]) : nullptr;

		const AbstrDataItem* adiOrgMinImp  = flags(df & DijkstraFlag::OrgMinImp) ? AsCheckedDataItem(args[argCounter++]) : nullptr;
		const AbstrDataItem* adiDstMinImp  = flags(df & DijkstraFlag::DstMinImp) ? AsCheckedDataItem(args[argCounter++]) : nullptr;
		const AbstrDataItem* adiOrgMass    = flags(df & DijkstraFlag::Interaction) ? AsCheckedDataItem(args[argCounter++]) : nullptr; // orgMass
		const AbstrDataItem* adiDstMass    = flags(df & DijkstraFlag::Interaction) ? AsCheckedDataItem(args[argCounter++]) : nullptr; // dstMassLimit
		const AbstrDataItem* adiDistDecayBetaParam  = flags(df & DijkstraFlag::DistDecay) ? AsCheckedDataItem(args[argCounter++]) : nullptr; // DecayBeta
		const AbstrDataItem* adiDistLogitAlphaParam = flags(df & DijkstraFlag::DistLogit) ? AsCheckedDataItem(args[argCounter++]) : nullptr; // LogitAlpha
		const AbstrDataItem* adiDistLogitBetaParam  = flags(df & DijkstraFlag::DistLogit) ? AsCheckedDataItem(args[argCounter++]) : nullptr; // LogitBeta
		const AbstrDataItem* adiDistLogitGammaParam = flags(df & DijkstraFlag::DistLogit) ? AsCheckedDataItem(args[argCounter++]) : nullptr; // LogistGamma
		const AbstrDataItem* adiOrgAlpha            = flags(df & DijkstraFlag::InteractionAlpha) ? AsCheckedDataItem(args[argCounter++]) : nullptr; // demand alpha

		dms_assert(argCounter == args.size()); // all arguments have been processed.

		dms_assert(adiLinkImp && adiLinkF1 && adiLinkF2);
		dms_assert((adiOrgMassLimit != nullptr) == (adiDstMassLimit != nullptr));
		const Unit<LinkType>* e        = checked_domain<LinkType>(adiLinkImp, "Link Impedance");
		const Unit<NodeType>* v        = const_unit_cast<NodeType>(adiLinkF1->GetAbstrValuesUnit());
		const Unit<ImpType>* impUnit = const_unit_cast<ImpType>(adiLinkImp->GetAbstrValuesUnit());
		const Unit<NodeType>* x = adiStartPointNode ? checked_domain<NodeType>(adiStartPointNode, "startPoint Node_rel") : v;
		const Unit<NodeType>* y = adiEndPointNode   ? checked_domain<NodeType>(adiEndPointNode, "endPoint Node_rel") : v;
		const Unit<NodeType>* orgZones = flags(df & DijkstraFlag::OD) ? (adiStartPoinOrgZone ? const_unit_cast<NodeType>(adiStartPoinOrgZone->GetAbstrValuesUnit()) : x) : nullptr;
		const Unit<NodeType>* dstZones = adiEndPointDstZone ? const_unit_cast<NodeType>(adiEndPointDstZone->GetAbstrValuesUnit()) : y;
		const AbstrUnit* orgZonesOrVoid = orgZones ? orgZones : Unit<Void>::GetStaticClass()->CreateDefault();

		dms_assert(e && v && impUnit && x && y && (orgZones || !flags(df & DijkstraFlag::OD))&& dstZones);
		e->UnifyDomain(adiLinkF1->GetAbstrDomainUnit(), "Links", "Domain of FromNode_rel attribute", UM_Throw);
		e->UnifyDomain(adiLinkF2->GetAbstrDomainUnit(), "Links", "Domain of ToNode_rel attribute", UM_Throw);
		v->UnifyDomain(adiLinkF1->GetAbstrValuesUnit(), "Nodes", "Values of FromNode_rel attribute", UM_Throw);
		v->UnifyDomain(adiLinkF2->GetAbstrValuesUnit(), "Nodes", "Values of ToNode_rel attribute", UM_Throw);
		if (adiLinkBidirFlag) e->UnifyDomain(adiLinkBidirFlag->GetAbstrDomainUnit(), "Links", "Domain of Bidirectional flag attribute", UM_Throw);
		if (adiLinkBidirFlag) MG_USERCHECK(adiLinkBidirFlag->GetAbstrValuesUnit()->IsKindOf(Unit<Bool>::GetStaticClass()));

		if (adiStartPointNode) v->UnifyDomain(adiStartPointNode->GetAbstrValuesUnit(), "NodeSet", "Domain of StartLink Node_rel", UM_Throw);
		if (adiStartPointImpedance) x->UnifyDomain(adiStartPointImpedance->GetAbstrDomainUnit(), "StartLinks", "Domain of StartLink Impedance", UM_Throw);
		if (adiStartPoinOrgZone) x->UnifyDomain(adiStartPoinOrgZone->GetAbstrDomainUnit(), "StartLinks", "Domain of StartLink OrgZone_rel", UM_Throw);
		if (adiStartPointImpedance) impUnit->UnifyValues(adiStartPointImpedance->GetAbstrValuesUnit(), "LinkImpedances", "StartLink Impedances", UM_Throw);
		if (adiOrgZoneLocation) orgZones->UnifyDomain(adiOrgZoneLocation->GetAbstrDomainUnit(), "OrgZones", "OrgZoneLoc", UM_Throw);

		if (adiEndPointNode) v->UnifyDomain(adiEndPointNode->GetAbstrValuesUnit(), "NodeSet", "Domain of EndLink Node_rel ", UM_Throw);
		if (adiEndPointImpedance) y->UnifyDomain(adiEndPointImpedance->GetAbstrDomainUnit(), "EndLinks", "Domain of EndLink Impedance", UM_Throw);
		if (adiEndPointDstZone) y->UnifyDomain(adiEndPointDstZone->GetAbstrDomainUnit(), "EndLinks", "Domain of EndLink OrgZone_rel", UM_Throw);
		if (adiEndPointImpedance) impUnit->UnifyValues(adiEndPointImpedance->GetAbstrValuesUnit(), "LinkImpedances", "EndLink Impedances", UM_Throw);
		if (adiDstZoneLocation) dstZones->UnifyDomain(adiDstZoneLocation->GetAbstrDomainUnit(), "DstZones", "DstZoneLoc", UM_Throw);

		if (adiOrgMaxImp) // maxDist
		{
			orgZonesOrVoid->UnifyDomain(adiOrgMaxImp->GetAbstrDomainUnit(), "OrgZones", "Domain of OrgMaxImp", UnifyMode(UM_Throw | UM_AllowVoidRight));
			impUnit->UnifyValues(adiOrgMaxImp->GetAbstrValuesUnit(), "ImpedanceUnit", "Values of OrgMaxImp", UnifyMode(UM_Throw | UM_AllowDefault));
		}
		if (adiOrgMassLimit) // limitMass
		{
			dms_assert(adiDstMassLimit);
			adiOrgMassLimit->GetAbstrValuesUnit()->UnifyValues(adiDstMassLimit->GetAbstrValuesUnit(), "Values of OrgMassLimit", "Values of DstmassLimit", UnifyMode(UM_Throw | UM_AllowDefault));
			orgZonesOrVoid->UnifyDomain(adiOrgMassLimit->GetAbstrDomainUnit(), "OrgZones", "Domain of OrgMassLimit", UnifyMode(UM_Throw | UM_AllowVoidRight));
			dstZones->UnifyDomain(adiDstMassLimit->GetAbstrDomainUnit(), "DstZones", "Domain of DstMassLimit", UnifyMode(UM_Throw | UM_AllowVoidRight));
		}

		if (adiEuclidicSqrDist)
		{
			assert(adiOrgZoneLocation);
			assert(adiDstZoneLocation);
			MG_USERCHECK(!adiEuclidicSqrDist || adiEuclidicSqrDist->HasVoidDomainGuarantee());
		}

		const Unit<ImpType>* imp2Unit= impUnit;
		if (adiLinkAltImp) // AlternativeLinkWeight
		{
			imp2Unit= const_unit_cast<ImpType>(adiLinkAltImp->GetAbstrValuesUnit());
			e->UnifyDomain(adiLinkAltImp->GetAbstrDomainUnit(), "Edges", "Domain of Alternative Impedances", UnifyMode(UM_Throw | UM_AllowVoidRight));
		}

		// interaction parameters
		if (adiOrgMinImp)
		{
			orgZonesOrVoid->UnifyDomain(adiOrgMinImp->GetAbstrDomainUnit(), "OrgZones", "Domain of OrgMinImp", UnifyMode(UM_Throw | UM_AllowVoidRight));
			imp2Unit->UnifyValues(adiOrgMinImp->GetAbstrValuesUnit(), "Imp2Unit", "Values of OrgMinImp", UnifyMode(UM_Throw | UM_AllowDefault));
		}
		if (adiDstMinImp)
		{
			dstZones->UnifyDomain(adiDstMinImp->GetAbstrDomainUnit(), "DstZones", "Domain of DstMinImp", UnifyMode(UM_Throw | UM_AllowVoidRight));
			imp2Unit->UnifyValues(adiDstMinImp->GetAbstrValuesUnit(), "Imp2Unit", "Values of DstMinImp", UnifyMode(UM_Throw | UM_AllowDefault));
		}
		if (adiOrgMass) // SrcMass
		{
			dms_assert(adiDistDecayBetaParam);
			dms_assert(adiDstMass);
			orgZonesOrVoid->UnifyDomain(adiOrgMass->GetAbstrDomainUnit(), "OrgZones", "Domain of OrgMass attribute", UnifyMode(UM_Throw | UM_AllowVoidRight));
		}
		MG_USERCHECK(!adiDistDecayBetaParam || adiDistDecayBetaParam->HasVoidDomainGuarantee());
		MG_USERCHECK(!adiDistLogitAlphaParam || adiDistLogitAlphaParam->HasVoidDomainGuarantee());
		MG_USERCHECK(!adiDistLogitBetaParam || adiDistLogitBetaParam->HasVoidDomainGuarantee());
		MG_USERCHECK(!adiDistLogitGammaParam || adiDistLogitGammaParam->HasVoidDomainGuarantee());
		assert(adiDistDecayBetaParam || (!adiOrgMass && !adiDstMass));
		if (adiDstMass) // DstMass
		{
			dms_assert(adiOrgMass);
			dstZones->UnifyDomain(adiDstMass->GetAbstrDomainUnit(), "DstZones", "Domain of DstMass attribute", UnifyMode(UM_Throw | UM_AllowVoidRight));
		}
		if (adiOrgAlpha)
		{
			dms_assert(adiOrgMass);
			orgZonesOrVoid->UnifyDomain(adiOrgAlpha->GetAbstrDomainUnit(), "OrgZones", "Domain of OrgAlpha attribute", UnifyMode(UM_Throw | UM_AllowVoidRight));
		}
		const AbstrUnit* resultUnit;
		AbstrUnit* mutableResultUnit;
		AbstrDataItem* resDist;
		TreeItem* resultContext;
		if (flags(df & DijkstraFlag::OD))
		{ 
			mutableResultUnit = GetResultUnitClass(df)->CreateResultUnit(resultHolder);
			resultHolder = mutableResultUnit;
			resultUnit = mutableResultUnit;
			resultContext = resultHolder;
			resDist = flags(df & DijkstraFlag::ProdOdImpedance)
				? CreateDataItem(resultContext, GetTokenID_mt("impedance"), resultUnit, impUnit)
				: nullptr;
		}
		else
		{
			mutableResultUnit = nullptr;
			resultUnit = dstZones;
			if (!resultHolder)
				resultHolder = CreateCacheDataItem(resultUnit, impUnit);
			resultContext = resultHolder.GetNew();
			resDist = AsDataItem(resultContext);
		}
			
		AbstrDataItem* resTB= nullptr;
		if (flags(df & DijkstraFlag::ProdTraceBack))
		{
			dms_assert(!flags(df & DijkstraFlag::OD));
			resTB = CreateDataItem(resultContext, GetTokenID_mt("TraceBack"), v, e);
			resTB->SetTSF(DSF_Categorical);
		}

		AbstrDataItem* resLS = flags(df & DijkstraFlag::ProdOdLinkSet)
			?	CreateDataItem(resultContext, GetTokenID_mt("LinkSet"), resultUnit, e, ValueComposition::Sequence)
			:	nullptr;
		if (resLS) resLS->SetTSF(DSF_Categorical);

		AbstrDataItem* resAltLinkImp = flags(df & DijkstraFlag::ProdOdAltImpedance)
			? CreateDataItem(resultContext, GetTokenID_mt("alt_imp"), resultUnit, imp2Unit)
			:	nullptr;

		AbstrDataItem* resOrgFactor = flags(df & DijkstraFlag::ProdOrgFactor)
			? CreateDataItem(resultContext, GetTokenID_mt("D_i"), orgZonesOrVoid, adiOrgMass->GetAbstrValuesUnit())
			: nullptr;
		AbstrDataItem* resOrgDemand = flags(df & DijkstraFlag::ProdOrgDemand)
			? CreateDataItem(resultContext, GetTokenID_mt("M_ix"), orgZonesOrVoid, adiOrgMass->GetAbstrValuesUnit())
			: nullptr;
		AbstrDataItem* resDstFactor = flags(df & DijkstraFlag::ProdDstFactor)
			? CreateDataItem(resultContext, GetTokenID_mt("C_j"), dstZones, adiOrgMass->GetAbstrValuesUnit())
			: nullptr;
		AbstrDataItem* resDstSupply = flags(df & DijkstraFlag::ProdDstSupply)
			? CreateDataItem(resultContext, GetTokenID_mt("M_xj"), dstZones, adiOrgMass->GetAbstrValuesUnit())
			: nullptr;

		AbstrDataItem* resOrgMaxImp = flags(df & DijkstraFlag::ProdOrgMaxImp)
			? CreateDataItem(resultContext, GetTokenID_mt("OrgZone_MaxImp"), orgZonesOrVoid, impUnit)
			: nullptr;

		AbstrDataItem* resLinkFlow = flags(df & DijkstraFlag::ProdLinkFlow)
			? CreateDataItem(resultContext, GetTokenID_mt("Link_flow"), e, adiOrgMass->GetAbstrValuesUnit())
			: nullptr;
		AbstrDataItem* resSrcZone = flags(df & DijkstraFlag::ProdOdOrgZone_rel)
			? CreateDataItem(resultContext, GetTokenID_mt("OrgZone_rel"), resultUnit, orgZonesOrVoid)
			: nullptr;
		if (resSrcZone) resSrcZone->SetTSF(DSF_Categorical);

		AbstrDataItem* resDstZone = flags(df & DijkstraFlag::ProdOdDstZone_rel)
			? CreateDataItem(resultContext, GetTokenID_mt("DstZone_rel"), resultUnit, dstZones)
			: nullptr;
		if (resDstZone) resDstZone->SetTSF(DSF_Categorical);

		AbstrDataItem* resStartPoint = flags(df & DijkstraFlag::ProdOdStartPoint_rel)
			? CreateDataItem(resultContext, GetTokenID_mt("StartPoint_rel"), resultUnit, x)
			: nullptr;
		if (resStartPoint) resStartPoint->SetTSF(DSF_Categorical);
		AbstrDataItem* resEndPoint = flags(df & DijkstraFlag::ProdOdEndPoint_rel)
			? CreateDataItem(resultContext, GetTokenID_mt("EndPoint_rel"), resultUnit, y)
			: nullptr;
		if (resEndPoint) resEndPoint->SetTSF(DSF_Categorical);

		if (mustCalc)
		{
			const ArgImpType* argLinkImp = const_array_cast<ImpType>(adiLinkImp);
			const ArgNodeType* argLinkF1 = const_array_cast<NodeType>(adiLinkF1);
			const ArgNodeType* argLinkF2 = const_array_cast<NodeType>(adiLinkF2);
			const ArgFlagType* argLinkBidirFlag = const_opt_array_checkedcast<Bool     >(adiLinkBidirFlag);
			const SrcNodeType* argStartPointNode = const_opt_array_checkedcast<NodeType >(adiStartPointNode);
			const SrcDistType* argStartPointImpedance = const_opt_array_checkedcast<ImpType  >(adiStartPointImpedance);
			const SrcZoneType* argStartPoinOrgZone = const_opt_array_checkedcast<ZoneType >(adiStartPoinOrgZone);
			const DstNodeType* argEndPointNode = const_opt_array_checkedcast<NodeType >(adiEndPointNode);
			const DstDistType* argEndPointImpedance = const_opt_array_checkedcast<ImpType  >(adiEndPointImpedance);
			const DstZoneType* argEndPointDstZone = const_opt_array_checkedcast<ZoneType >(adiEndPointDstZone);
			const ArgImpType* argOrgMinImp = const_opt_array_checkedcast<ImpType  >(adiOrgMinImp);
			const ArgImpType* argDstMinImp = const_opt_array_checkedcast<ImpType  >(adiDstMinImp);
			const ArgImpType* argOrgMaxImp = const_opt_array_checkedcast<ImpType  >(adiOrgMaxImp);
			const ArgMassType* argOrgMassLimit = const_opt_array_checkedcast<MassType >(adiOrgMassLimit);
			const ArgMassType* argDstMassLimit = const_opt_array_checkedcast<MassType >(adiDstMassLimit);
			const ArgImpType* argLinkAltImp = const_opt_array_checkedcast<MassType >(adiLinkAltImp);
			const ArgSMType* argOrgMass = const_opt_array_checkedcast<MassType >(adiOrgMass);
			const ArgDMType* argDstMass = const_opt_array_checkedcast<MassType >(adiDstMass);
			const ArgParamType* argDistDecayBetaParam = const_opt_array_checkedcast<ParamType>(adiDistDecayBetaParam);
			const ArgParamType* argDistLogitAlphaParam = const_opt_array_checkedcast<ParamType>(adiDistLogitAlphaParam);
			const ArgParamType* argDistLogitBetaParam = const_opt_array_checkedcast<ParamType>(adiDistLogitBetaParam);
			const ArgParamType* argDistLogitGammaParam = const_opt_array_checkedcast<ParamType>(adiDistLogitGammaParam);
			const ArgParamType* argOrgAlpha = const_opt_array_checkedcast<ParamType>(adiOrgAlpha);

			dms_assert(argLinkImp && argLinkF1 && argLinkF2);
			dms_assert((argOrgMassLimit != nullptr) == (argDstMassLimit != nullptr));

#if MG_DEBUG_DIJKSTRA
			SharedStr groupName = SharedStr(GetGroup()->GetName());
			DBG_START("DijkstraMatrOperator::Calc", groupName.c_str(), MG_DEBUG_DIJKSTRA);
#endif MG_DEBUG_DIJKSTRA

			bool isBidirectional = flags(df & (DijkstraFlag::Bidirectional|DijkstraFlag::BidirFlag));
			
			DataReadLock arg1Lock(adiLinkImp);
			DataReadLock argLinkF1Lock(adiLinkF1);
			DataReadLock argLinkF2Lock(adiLinkF2);
			DataReadLock argLinkFlagLock(adiLinkBidirFlag);
			DataReadLock arg4Lock(adiStartPointNode);
			DataReadLock arg5Lock(adiStartPointImpedance);
			DataReadLock arg6Lock(adiStartPoinOrgZone);
			DataReadLock arg7Lock(adiEndPointNode);
			DataReadLock arg8Lock(adiEndPointImpedance);
			DataReadLock arg9Lock(adiEndPointDstZone);

			DataReadLock argALock(adiOrgMaxImp);
			DataReadLock argBLock(adiOrgMassLimit);
			DataReadLock argCLock(adiDstMassLimit);
			DataReadLock argWLock(adiLinkAltImp);
			DataReadLock argOrgMassLock(adiOrgMass);
			DataReadLock argDstMassLock(adiDstMass);
			DataReadLock argDistDecayB(adiDistDecayBetaParam);
			DataReadLock argDistLogitA(adiDistLogitAlphaParam);
			DataReadLock argDistLogitB(adiDistLogitBetaParam);
			DataReadLock argDistLogitC(adiDistLogitGammaParam);
			DataReadLock argOrgAlphaLock(adiOrgAlpha);

			CheckDefineMode(adiLinkImp, "Link_impedance");
			CheckNoneMode  (adiLinkF1, "Link_Node1_rel");
			CheckNoneMode  (adiLinkF2, "Link_Node2_rel");
			CheckNoneMode  (adiStartPointNode, "startPoint_Node_rel");
			CheckDefineMode(adiStartPointImpedance, "startPoint_impedance");
			CheckNoneMode  (adiStartPoinOrgZone, "startPoint_OrgZone_rel");
			CheckNoneMode  (adiEndPointNode, "endPoint_Node_rel");
			CheckDefineMode(adiEndPointImpedance, "endPoint_impedance");
			CheckNoneMode(adiEndPointDstZone, "endPoint_DstZone_rel");

			CheckDefineMode(adiOrgMaxImp, "OrgZone_MaxImpedance");
			CheckDefineMode(adiOrgMassLimit, "OrgZone_MaxMass");
			CheckDefineMode(adiDstMassLimit, "DstZone_MassLimit");
			CheckDefineMode(adiLinkAltImp, "Link_AltImpedance");
			CheckDefineMode(adiOrgMass, "OrgZone_Mass");
			CheckDefineMode(adiDstMass, "DstZone_Mass");
			CheckDefineMode(adiDistDecayBetaParam, "DistDecayBeta");
			CheckDefineMode(adiDistLogitAlphaParam, "DistLogitAlpha");
			CheckDefineMode(adiDistLogitBetaParam, "DistLogitBeta");
			CheckDefineMode(adiDistLogitGammaParam, "DistLogitGamma");
			CheckDefineMode(adiOrgAlpha, "OrgZone_Alpha");

			typename ArgImpType  ::locked_cseq_t linkImpData           = argLinkImp->GetLockedDataRead();
			         ArgNodeType ::locked_cseq_t linkF1Data            = argLinkF1->GetLockedDataRead();
			         ArgNodeType ::locked_cseq_t linkF2Data            = argLinkF2->GetLockedDataRead();
			         ArgFlagType ::locked_cseq_t linkBidirFlagData     = argLinkBidirFlag       ? argLinkBidirFlag      ->GetLockedDataRead() :          ArgFlagType ::locked_cseq_t();
			         SrcNodeType ::locked_cseq_t startpointNodeData    = argStartPointNode      ? argStartPointNode     ->GetLockedDataRead() :          SrcNodeType ::locked_cseq_t();
			typename SrcDistType ::locked_cseq_t startpointImpData     = argStartPointImpedance ? argStartPointImpedance->GetLockedDataRead() : typename SrcDistType ::locked_cseq_t();
			         SrcZoneType ::locked_cseq_t startpointOrgZoneData = argStartPoinOrgZone    ? argStartPoinOrgZone   ->GetLockedDataRead() :          SrcZoneType ::locked_cseq_t();
			         DstNodeType ::locked_cseq_t endPoint_Node_rel_Data= argEndPointNode        ? argEndPointNode       ->GetLockedDataRead() :          DstNodeType ::locked_cseq_t();
			typename DstDistType ::locked_cseq_t endpointImpData       = argEndPointImpedance   ? argEndPointImpedance  ->GetLockedDataRead() : typename DstDistType ::locked_cseq_t();
			         DstZoneType ::locked_cseq_t endpointDstZoneData   = argEndPointDstZone     ? argEndPointDstZone    ->GetLockedDataRead() :          DstZoneType ::locked_cseq_t();
			typename ArgImpType  ::locked_cseq_t orgMinImpData         = argOrgMinImp           ? argOrgMinImp          ->GetLockedDataRead() : typename ArgImpType  ::locked_cseq_t();
			typename ArgImpType  ::locked_cseq_t dstMinImpData         = argDstMinImp           ? argDstMinImp          ->GetLockedDataRead() : typename ArgImpType  ::locked_cseq_t();
			typename ArgImpType  ::locked_cseq_t orgMaxImpedances      = argOrgMaxImp           ? argOrgMaxImp          ->GetLockedDataRead() : typename ArgImpType  ::locked_cseq_t();
			typename ArgMassType ::locked_cseq_t orgMassLimit          = argOrgMassLimit        ? argOrgMassLimit       ->GetLockedDataRead() : typename ArgMassType ::locked_cseq_t();
			typename ArgMassType ::locked_cseq_t dstMassLimit          = argDstMassLimit        ? argDstMassLimit       ->GetLockedDataRead() : typename ArgMassType ::locked_cseq_t();
			typename ArgImpType  ::locked_cseq_t altWeight             = argLinkAltImp          ? argLinkAltImp         ->GetLockedDataRead() : typename ArgImpType  ::locked_cseq_t();
			typename ArgSMType   ::locked_cseq_t tgOrgMass             = argOrgMass             ? argOrgMass            ->GetLockedDataRead() : typename ArgSMType   ::locked_cseq_t();
			typename ArgDMType   ::locked_cseq_t tgDstMass             = argDstMass             ? argDstMass            ->GetLockedDataRead() : typename ArgDMType   ::locked_cseq_t();
			typename ArgParamType::locked_cseq_t tgDistDecayBetaParam  = argDistDecayBetaParam  ? argDistDecayBetaParam ->GetLockedDataRead() : typename ArgParamType::locked_cseq_t();
			typename ArgParamType::locked_cseq_t tgDistLogitA          = argDistLogitAlphaParam ? argDistLogitAlphaParam->GetLockedDataRead() : typename ArgParamType::locked_cseq_t();
			typename ArgParamType::locked_cseq_t tgDistLogitB          = argDistLogitBetaParam  ? argDistLogitBetaParam ->GetLockedDataRead() : typename ArgParamType::locked_cseq_t();
			typename ArgParamType::locked_cseq_t tgDistLogitC          = argDistLogitGammaParam ? argDistLogitGammaParam->GetLockedDataRead() : typename ArgParamType::locked_cseq_t();
			typename ArgParamType::locked_cseq_t tgOrgAlpha            = argOrgAlpha            ? argOrgAlpha           ->GetLockedDataRead() : typename ArgParamType::locked_cseq_t();

			if (IsDefined(vector_find_if(linkImpData, [](ImpType v) { return v < 0;  })))
				throwErrorF("Dijkstra", "Illegal negative value in Impedance data");
// TODO: 
//			MG_CHECK(v->IsOrdinalAndZeroBased());
//			MG_CHECK(e->IsOrdinalAndZeroBased());
//			MG_CHECK(x->IsOrdinalAndZeroBased());
//			MG_CHECK(y->IsOrdinalAndZeroBased());
//			MG_CHECK(orgZones->IsOrdinalAndZeroBased());
//			MG_CHECK(dstZones->IsOrdinalAndZeroBased());

			NetworkInfo<NodeType, ZoneType, ImpType> networkInfo(
				v->GetCount(), e->GetCount()
			,	x->GetCount(), y->GetCount()
			,	orgZonesOrVoid->GetCount(), dstZones->GetCount()
			,	ZoneInfo<NodeType, ZoneType, ImpType>{startpointNodeData.begin(), startpointImpData.begin(), startpointOrgZoneData.begin()}
			,	ZoneInfo<NodeType, ZoneType, ImpType>{endPoint_Node_rel_Data.begin(), endpointImpData.begin(), endpointDstZoneData.begin()}
			);

			dms_assert(linkImpData.size() == networkInfo.nrE); typename sequence_traits<ImpType>::cseq_t::const_iterator linkImpDataPtr = linkImpData.begin();
			dms_assert(linkF1Data.size() == networkInfo.nrE);
			dms_assert(linkF2Data.size() == networkInfo.nrE);

			// constant internal data structures
			GraphInfo<NodeType, LinkType, ImpType> graph{ linkF1Data.begin(), linkF2Data.begin(), linkImpDataPtr };
			graph.node_link1_inv.Init<NodeType>(linkF1Data.begin(), networkInfo.nrE, networkInfo.nrV);
			if (isBidirectional)
			{
				if (flags(df & DijkstraFlag::Bidirectional))
					graph.node_link2_inv.Init(linkF2Data.begin(), networkInfo.nrE, networkInfo.nrV);
				else
				{
					dms_assert(flags(df & DijkstraFlag::BidirFlag));
					graph.node_link2_inv.Init(linkF2Data.begin(), networkInfo.nrE, networkInfo.nrV, begin_ptr(linkBidirFlagData));
				}
			}
			dms_assert(!argStartPointNode      || networkInfo.nrX == startpointNodeData.size());
			dms_assert(!argStartPointImpedance || networkInfo.nrX == startpointImpData.size());
			dms_assert(!argStartPoinOrgZone    || networkInfo.nrX == startpointOrgZoneData.size());
			dms_assert(!argEndPointNode        || networkInfo.nrY == endPoint_Node_rel_Data.size());
			dms_assert(!argEndPointImpedance   || networkInfo.nrY == endpointImpData.size());
			dms_assert(!argEndPointDstZone     || networkInfo.nrY == endpointDstZoneData.size());

			Inverted_rel<ZoneType> node_endPoint_inv;
			if (argEndPointNode)
				node_endPoint_inv.Init(endPoint_Node_rel_Data.begin(), networkInfo.nrY, networkInfo.nrV);

			SizeT nrRes = -1;
			OwningPtrSizedArray<SizeT> resCount;
			if (mutableResultUnit && flags(df & DijkstraFlag::OD_Data))
			{
				if (flags(df & DijkstraFlag::SparseResult))
				{
					resCount = OwningPtrSizedArray<SizeT>( networkInfo.nrOrgZones MG_DEBUG_ALLOCATOR_SRC("dijkstra: resCount"));
					nrRes = ProcessDijkstra<NodeType, LinkType, ZoneType, ImpType, MassType, ParamType>(resultHolder, networkInfo
						, orgMaxImpedances.begin(), HasVoidDomainGuarantee(adiOrgMaxImp)
						, orgMassLimit.begin(), HasVoidDomainGuarantee(adiOrgMassLimit)
						, dstMassLimit.begin(), HasVoidDomainGuarantee(adiDstMassLimit)
						, graph
						, node_endPoint_inv
						, (df & ~DijkstraFlag::InteractionOrMaxImp) | DijkstraFlag::Counting
						, nullptr, HasVoidDomainGuarantee(adiLinkAltImp)
						, nullptr, HasVoidDomainGuarantee(adiOrgMinImp)
						, nullptr, HasVoidDomainGuarantee(adiDstMinImp)
						, nullptr, HasVoidDomainGuarantee(adiOrgMass)
						, nullptr, HasVoidDomainGuarantee(adiDstMass)
						, ParamType()
						, ParamType(), ParamType(), ParamType()
						, nullptr, HasVoidDomainGuarantee(adiOrgAlpha)
						// mutable src specific working area
						, nullptr
						, resCount.begin()
						// result area, not for counting
						, ResultInfo<ZoneType, ImpType, MassType>{nullptr, nullptr, nullptr, nullptr, nullptr, nullptr
							, sequence_traits<EdgeSeq>::seq_t::iterator()
							, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr
							}
						, no_tile
						, "Counting"
					);
					if (!IsDefined(nrRes))
						return false;
					make_cumulative_base(resCount.begin(), resCount.begin() + networkInfo.nrOrgZones);
				}
				else
				{
					nrRes =  networkInfo.nrOrgZones;
					nrRes *= networkInfo.nrDstZones;
				}
				DBG_TRACE(("OD-pair cardinality = %I64u", (UInt64)nrRes))
				mutableResultUnit->SetCount(nrRes);
			}

			// init result arrays

			DataWriteLock resDistLock   (resDist); // per OD
			DataWriteLock resSrcZoneLock(resSrcZone); // per OD
			DataWriteLock resDstZoneLock(resDstZone); // per OD
			DataWriteLock resStartPointLock(resStartPoint); // per OD
			DataWriteLock resEndPointLock(resEndPoint); // per OD
			DataWriteLock resTraceBackLock(resTB); // per node
			DataWriteLock resLinkSetLock(resLS,      dms_rw_mode::write_only_mustzero); // per OD

			DataWriteLock resALWLock(resAltLinkImp); // per OD
			DataWriteLock resOFLock(resOrgFactor); // per OrgZone
			DataWriteLock resODLock(resOrgDemand); // per OrgZone
			DataWriteLock resOMILock(resOrgMaxImp); // per OrgZone
			DataWriteLock resDFLock(resDstFactor,      dms_rw_mode::write_only_mustzero); // per DstZone
			DataWriteLock resDSLock(resDstSupply,      dms_rw_mode::write_only_mustzero); // per DstZone
			DataWriteLock resLinkFlowLock(resLinkFlow, dms_rw_mode::write_only_mustzero);

			SizeT nrRes2 = ProcessDijkstra<NodeType, LinkType, ZoneType, ImpType, MassType, ParamType>(resultHolder, networkInfo
			,	orgMaxImpedances.begin(), HasVoidDomainGuarantee(adiOrgMaxImp)
			,	orgMassLimit.begin(), HasVoidDomainGuarantee(adiOrgMassLimit)
			,	dstMassLimit.begin(), HasVoidDomainGuarantee(adiDstMassLimit)
			,	graph, node_endPoint_inv
			,	df
			,	altWeight.begin(), HasVoidDomainGuarantee(adiLinkAltImp)
			,	orgMinImpData.begin(), HasVoidDomainGuarantee(adiOrgMinImp)
			,	dstMinImpData.begin(), HasVoidDomainGuarantee(adiDstMinImp)
			,	tgOrgMass.begin(), HasVoidDomainGuarantee(adiOrgMass)
			,	tgDstMass.begin(), HasVoidDomainGuarantee(adiDstMass)
			,	argDistDecayBetaParam ? tgDistDecayBetaParam[0] : ParamType()
			,	argDistLogitAlphaParam? tgDistLogitA[0] : ParamType()
			,	argDistLogitBetaParam? tgDistLogitB[0] : ParamType()
			,	argDistLogitGammaParam? tgDistLogitC[0] : ParamType()
			,   tgOrgAlpha.begin(), HasVoidDomainGuarantee(adiOrgAlpha)
			,	resCount.begin()

			// mutable src specific working area
			,	nullptr
			// result area
			,	ResultInfo<ZoneType, ImpType, MassType>{
					resDist       ? mutable_array_cast<ImpType >(resDistLock)->GetDataWrite(no_tile, dms_rw_mode::write_only_all).begin() : nullptr
				, resAltLinkImp ? mutable_array_cast<ImpType >(resALWLock)->GetDataWrite(no_tile, dms_rw_mode::write_only_all).begin() : nullptr
				, resSrcZone    ? mutable_array_cast<ZoneType>(resSrcZoneLock)->GetDataWrite(no_tile, dms_rw_mode::write_only_all).begin() : nullptr
				, resDstZone    ? mutable_array_cast<ZoneType>(resDstZoneLock)->GetDataWrite(no_tile, dms_rw_mode::write_only_all).begin() : nullptr
				, resStartPoint ? mutable_array_cast<ZoneType>(resStartPointLock)->GetDataWrite(no_tile, dms_rw_mode::write_only_all).begin() : nullptr
				, resEndPoint   ? mutable_array_cast<ZoneType>(resEndPointLock)->GetDataWrite(no_tile, dms_rw_mode::write_only_all).begin() : nullptr
				,	resLS         ? mutable_array_cast<EdgeSeq >(resLinkSetLock)->GetDataWrite(no_tile, dms_rw_mode::write_only_mustzero).begin() : DataArray<EdgeSeq>::iterator()
				,	resTB ? mutable_array_cast<LinkType>(resTraceBackLock)->GetDataWrite(no_tile, dms_rw_mode::write_only_all).begin() : nullptr
				,	resOrgFactor  ? mutable_array_cast<MassType>(resOFLock)->GetDataWrite(no_tile, dms_rw_mode::write_only_mustzero).begin() : nullptr
				,	resOrgDemand  ? mutable_array_cast<MassType>(resODLock)->GetDataWrite(no_tile, dms_rw_mode::write_only_mustzero).begin() : nullptr
				,	resOrgMaxImp  ? mutable_array_cast<ImpType >(resOMILock)->GetDataWrite(no_tile, dms_rw_mode::write_only_mustzero).begin() : nullptr
				,	resDstFactor  ? mutable_array_cast<MassType>(resDFLock)->GetDataWrite(no_tile, dms_rw_mode::write_only_mustzero).begin() : nullptr
				,	resDstSupply  ? mutable_array_cast<MassType>(resDSLock)->GetDataWrite(no_tile, dms_rw_mode::write_only_mustzero).begin() : nullptr
				,	resLinkFlow   ? mutable_array_cast<MassType>(resLinkFlowLock)->GetDataWrite(no_tile, dms_rw_mode::write_only_mustzero).begin() : nullptr
				}
			,	(flags(df & DijkstraFlag::OD_Data) || !mutableResultUnit) ? resultUnit->GetNrTiles() : no_tile
			,	"Filling"
			);
			if (!IsDefined(nrRes2))
				return false;

			if (mutableResultUnit)
			{
				dms_assert((!flags(df & DijkstraFlag::OD_Data)) || (nrRes2 == nrRes));
				if (!flags(df & DijkstraFlag::OD_Data))
				{ 
					dms_assert(nrRes == -1);
					dms_assert(nrRes2 != -1);
					DBG_TRACE(("OD-pair cardinality = %I64u", (UInt64)nrRes2))
					mutableResultUnit->SetCount(nrRes2);
				}
			}
			if (resDist) resDistLock.Commit();
			if (resSrcZone) resSrcZoneLock.Commit();
			if (resDstZone) resDstZoneLock.Commit();
			if (resStartPoint) resStartPointLock.Commit();
			if (resEndPoint) resEndPointLock.Commit();
			if (resTB) resTraceBackLock.Commit();
			if (resLS) resLinkSetLock.Commit();
			if (resAltLinkImp) resALWLock.Commit();
			if (resOrgFactor) resOFLock.Commit();
			if (resOrgDemand) resODLock.Commit();
			if (resOrgMaxImp) resOMILock.Commit();
			if (resDstFactor) resDFLock.Commit();
			if (resDstSupply) resDSLock.Commit();
			if (resLinkFlow) resLinkFlowLock.Commit();
		}
		return true;
	}

private:
	DijkstraFlag m_OperFlags;
};


// *****************************************************************************
//											INSTANTIATION
// *****************************************************************************

#include "RtcTypeLists.h"
#include "utl/TypeListOper.h"

namespace 
{
	typedef boost::mpl::vector<Float64, Float32, UInt32, UInt64> DistTypeList; // Int32 no longer supported as Impedance
	typedef tl_oper::inst_tuple< DistTypeList, DijkstraMatrOperator<_>, AbstrOperGroup*, DijkstraFlag> DijkstraOperListType;

	CommonOperGroup dsGroup("dijkstra_s", oper_policy::allow_extra_args);
	CommonOperGroup dm32Group("dijkstra_m", oper_policy::allow_extra_args);
	CommonOperGroup dm64Group("dijkstra_m64", oper_policy::allow_extra_args);

	DijkstraOperListType dsOpers  (&dsGroup  , DijkstraFlag());
	DijkstraOperListType dm32Opers(&dm32Group, DijkstraFlag(DijkstraFlag::OD));
	DijkstraOperListType dm64Opers(&dm64Group, DijkstraFlag(DijkstraFlag::OD | DijkstraFlag::UInt64_Od));
}
