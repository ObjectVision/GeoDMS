// Copyright (C) 1998-2025 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "ShvDllPch.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

#include "Cmds.h"
#include "ViewPort.h"
#include "GridLayer.h"

//----------------------------------------------------------------------
// class  : CmdSelectDistrict
//----------------------------------------------------------------------

GraphVisitState CmdSelectDistrict::DoGridLayer(GridLayer* dgl)
{
	dms_assert(dgl);

	dgl->SelectDistrict(m_WorldPoint, EventID(0) );

	return GVS_Handled;
}

