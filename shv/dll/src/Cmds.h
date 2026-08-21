// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////
#pragma once

#ifndef __CMD_H
#define __CMD_H

#include "cpc/Types.h"

//----------------------------------------------------------------------
// used modules and forward class references
//----------------------------------------------------------------------

#include "AbstrCmd.h"

#include "geom/Geometry.h"

//----------------------------------------------------------------------
// class  : CmdSelectDistrict
//----------------------------------------------------------------------

struct CmdSelectDistrict : AbstrCmd
{
	CmdSelectDistrict(const CrdPoint& worldPoint)
		:	m_WorldPoint(worldPoint)
	{}

	// Implement virtuals of AbstrCmd
  	GraphVisitState DoGridLayer(GridLayer*  dgl) override;

protected:
	CrdPoint m_WorldPoint;
};

#endif // __CMD_H


