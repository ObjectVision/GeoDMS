// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

#if !defined(__STX_DIJKSTRAFLAGS_H)
#define __STX_DIJKSTRAFLAGS_H

#include "StxBase.h"
#include "set/enum_flags.h"

#if defined(MG_DEBUG)
#define MG_DEBUG_DIJKSTRA true
#endif //defined(MG_DEBUG)

enum class DijkstraFlag : UInt64
{
	None = 0,
	VerboseLogging = 0x01,
	Bidirectional = 0x02,
	BidirFlag = 0x04,

	OD = 0x08, // implies: multiple sources

	OrgNode = 0x10,  // defines a selection of Nodes as Src, else all nodes will be a Src, required when not DMOF_OD
	OrgZone = 0x20,  // partition of (selected src)nodes into SrcZones, which implies DMOF_OD
	OrgImp = 0x40,
	OrgData = OrgNode | OrgZone | OrgImp,

	DstNode = 0x0080,
	DstZone = 0x0100,
	DstImp = 0x0200,
	DstData = DstNode | DstZone | DstImp,

	ImpCut = 0x0400,
	DstLimit = 0x0800,

	SparseResult = ImpCut | DstLimit,

	UseAltLinkImp = 0x1000, // requires extra Mass per Edge, produces alternative dist per OD pair.
	UseLinkAttr = 0x2000, // requires extra Mass per Edge, produces alternative dist per OD pair.

	UInt64_Od = 0x4000,

	OrgMinImp = 0x8000,
	DstMinImp = 0x1'0000,
	InteractionVi = 0x4'0000'0000'0000,
	InteractionWj = 0x8'0000'0000'0000,
	InteractionAlpha = 0x2'0000,
	DistDecay = 0x4'0000,
	DistLogit = 0x8'0000,

	// production flags
	ProdTraceBack = 0x10'0000, // only for non DMOF_OD
	ProdOdLinkSet = 0x20'0000,

	ProdOdOrgZone_rel = 0x100'0000,
	ProdOdDstZone_rel = 0x200'0000,
	ProdOdImpedance = 0x400'0000,
	ProdOdAltImpedance = 0x800'0000,
	ProdOdLinkAttr = 0x1000'0000, // used only for aggregation of link related data 

	ProdOrgFactor = 0x1'0000'0000,
	ProdOrgDemand = 0x2'0000'0000,
	ProdOrgMaxImp = 0x4'0000'0000,
	ProdDstFactor = 0x8'0000'0000,
	ProdDstSupply = 0x10'0000'0000,
	ProdLinkFlow  = 0x20'0000'0000,

	ProdOrgNrDstZones  = 0x40'0000'0000,
	ProdOrgSumImp      = 0x80'0000'0000,
	ProdOrgSumLinkAttr = 0x100'0000'0000,
	ProdOrgSqrtDist = 0x200'0000'0000, // TODO, low prio
	ProdOrgDist = 0x400'0000'0000, // TODO, low prio

	Counting = 0x800'0000'0000,

	ProdOdStartPoint_rel = 0x1000'0000'0000,
	ProdOdEndPoint_rel = 0x2000'0000'0000,

	OrgZoneLoc = 0x4000'0000'0000,
	DstZoneLoc = 0x8000'0000'0000,
	UseEuclidicFilter = 0x1'0000'0000'0000,

	PrecalculatedNrDstZones = 0x2'0000'0000'0000,

	EuclidFlags = OrgZoneLoc | DstZoneLoc | UseEuclidicFilter,

	TripDistr = ProdOrgFactor | ProdOrgDemand | ProdDstFactor | ProdDstSupply,
	Interaction = TripDistr | ProdLinkFlow,
	InteractionOrMaxImp = Interaction | ProdOrgMaxImp,
	Calc_pot_ij = ProdDstFactor | ProdDstSupply | ProdLinkFlow | ProdOrgSumImp | ProdOrgSumLinkAttr,

	OD_Data = ProdOdOrgZone_rel | ProdOdDstZone_rel | ProdOdStartPoint_rel | ProdOdEndPoint_rel | ProdOdImpedance | ProdOdAltImpedance | ProdOdLinkAttr | ProdOdLinkSet,
//	TmbTB = DijkstraFlag::UseAltLinkImp | DijkstraFlag::ProdLinkFlow | DijkstraFlag::ProdOdLinkSet,
};

ENUM_FLAGS_EX(DijkstraFlag, UInt64)

// single with 1 or several srcspec, indicating 1 zone
// OD with all nodes as separate zones
// OD|SrcSpec: indicating separate zones
// OD|SrcZones: partitioning the zones
// OD|SrcSpec|SrcZones
// not possible: SrcZones without OD, as without OD there is only one SrcZone

SYNTAX_CALL DijkstraFlag ParseDijkstraString(CharPtr str);

#endif // __STX_DIJKSTRAFLAGS_H