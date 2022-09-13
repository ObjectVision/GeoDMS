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
#pragma once

#if !defined(__STX_DIJKSTRAFLAGS_H)
#define __STX_DIJKSTRAFLAGS_H

#include "StxBase.h"
#include "set/enum_flags.h"

#if defined(MG_DEBUG)
#define MG_DEBUG_DIJKSTRA true
#endif //defined(MG_DEBUG)

enum class DijkstraFlag : UInt32
{
	Bidirectional = 0x02,
	Directed      = 0x00, // REMOVE
	BidirFlag     = 0x04,

	OD = 0x08, // implies: multiple sources

	OrgNode = 0x10,  // defines a selection of Nodes as Src, else all nodes will be a Src, required when not DMOF_OD
	OrgZone = 0x20,  // partition of (selected src)nodes into SrcZones, which implies DMOF_OD
	OrgImp  = 0x40,
	OrgData = OrgNode | OrgZone | OrgImp,

	DstNode = 0x0080,
	DstZone = 0x0100,
	DstImp  = 0x0200,
	DstData = DstNode | DstZone | DstImp,

	ImpCut   = 0x0400,
	DstLimit = 0x0800,

	SparseResult = ImpCut | DstLimit,

	UseAltLinkImp = 0x1000, // requires extra Mass per Edge, produces alternative dist per OD pair.

	UInt64_Od = 0x2000,

	OrgMinImp = 0x4000,
	DstMinImp = 0x8000,
	InteractionAlpha = 0x10000,
	DistDecay = 0x40000000,
	DistLogit = 0x80000000,

//	DMOF_OrgMass = DMOF_TripDistrData,
//	DMOF_DstgMass = DMOF_TripDistrData,
//	DMOF_DistDecay = DMOF_TripDistrData,

	// production flags
	ProdTraceBack = 0x20000, // only for non DMOF_OD
	ProdOdLinkSet = 0x40000,

	ProdOdOrgZone_rel = 0x80000,
	ProdOdDstZone_rel = 0x100000,
	ProdOdImpedance = 0x200000, 
	ProdOdAltImpedance = 0x400000,

	ProdOrgFactor = 0x00800000, // TODO ?
	ProdOrgDemand = 0x01000000, // DONE ?
	ProdOrgMaxImp = 0x02000000, // TODO
	ProdDstFactor = 0x04000000, // TODO
	ProdDstSupply = 0x08000000, // DONE ?
	ProdLinkFlow  = 0x10000000, // DONE ?
	Counting      = 0x20000000,

	TripDistr = ProdOrgFactor | ProdOrgDemand | ProdDstFactor | ProdDstSupply,
	Interaction = TripDistr | ProdLinkFlow,
	InteractionOrMaxImp = Interaction | ProdOrgMaxImp,
	Calc_pot_ij = ProdDstFactor | ProdDstSupply | ProdLinkFlow,

	OD_Data = ProdOdOrgZone_rel | ProdOdDstZone_rel | ProdOdImpedance | ProdOdAltImpedance | ProdOdLinkSet,
//	TmbTB = DijkstraFlag::UseAltLinkImp | DijkstraFlag::ProdLinkFlow | DijkstraFlag::ProdOdLinkSet,
};

ENUM_FLAGS_EX(DijkstraFlag, UInt32)

// single with 1 or several srcspec, indicating 1 zone
// OD with all nodes as separate zones
// OD|SrcSpec: indicating separate zones
// OD|SrcZones: partitioning the zones
// OD|SrcSpec|SrcZones
// not possible: SrcZones without OD, as without OD there is only one SrcZone

SYNTAX_CALL DijkstraFlag ParseDijkstraString(CharPtr str);

#endif // __STX_DIJKSTRAFLAGS_H