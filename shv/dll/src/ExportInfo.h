// Copyright (C) 2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#pragma once

#if !defined(__SHV_EXPORTINFO_H)
#define __SHV_EXPORTINFO_H

//----------------------------------------------------------------------
// used modules and forward class references
//----------------------------------------------------------------------

#include "geo/Geometry.h"
#include "ptr/SharedStr.h"

class ViewPort;
const TreeItem* GetExportSettingsContext(const TreeItem* context);
SharedStr GetFullFileNameBase(const TreeItem* context);

//===================================== struct ExportInfo interface

struct ExportInfo
{
	ExportInfo(ViewPort* vp);
	ExportInfo()	:	m_SubPixelFactor(0) {}

	IPoint GetNrSubDotsPerTile() const { return m_SubDotsPerDot * m_DotsPerTile; }
	IPoint GetNrSubDotsPerPage() const { return m_SubDotsPerDot * m_DotsPerTile * m_NrTiles; }

	bool empty() { return m_SubPixelFactor == 0.0;  }

	CrdRect   m_ROI;
	IPoint    m_SubDotsPerDot;
	IPoint    m_DotsPerTile;
	IPoint    m_NrTiles;
	CrdType   m_SubPixelFactor;
	SharedStr m_FullFileNameBase;
};


#endif // !defined(__SHV_EXPORTINFO_H)
