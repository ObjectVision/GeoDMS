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

#ifndef __SHV_GRIDCOORD_H
#define __SHV_GRIDCOORD_H

//----------------------------------------------------------------------
// used modules and forward class references
//----------------------------------------------------------------------

#include "ShvUtils.h"

#include "geo/Pair.h"
#include "ptr/SharedBase.h"

using grid_rowcol_id = UInt32 ;
using view_rowcol_id = UInt32;
using grid_coord_array = std::vector<grid_rowcol_id>;

//----------------------------------------------------------------------
// class  : GridCoordInfo
//----------------------------------------------------------------------

struct GridCoord : public std::enable_shared_from_this<GridCoord>
{
	GridCoord(ViewPort* owner, const grid_coord_key&, GPoint clientSize, const CrdTransformation& w2vTr);
	~GridCoord();

	void Init(GPoint clientSize, const CrdTransformation& w2vTr);

	void OnDeviceScroll(const GPoint& delta);
	void UpdateToScale(CrdPoint subPixelFactors);
	void UpdateUnscaled() { UpdateToScale(CrdPoint(1.0, 1.0)); }

	bool Empty  () const;
	bool IsDirty() const { return m_IsDirty; }

	const GRect& GetClippedRelRect() const { dms_assert(!IsDirty()); return m_ClippedRelRect; } 
	const IRect& GetGridRect      () const { return m_Key.second; }

	IRect GetClippedGridRect       (const GRect& viewRelRect) const;

	GRect GetClippedRelRect (const IRect& selRect ) const;
	IPoint GetExtGridCoord       (GPoint clientRelPoint) const;
	IPoint GetExtGridCoordFromAbs(GPoint clientAbsPoint) const;

	const UInt32* GetGridRowPtr(UInt32 currRelViewRow, bool withLines) const;
	const UInt32* GetGridColPtr(UInt32 currRelViewCol, bool withLines) const;
	const UInt32* GetGridRowPtrFromAbs(UInt32 currViewAbsRow, bool withLines) const;
	const UInt32* GetGridColPtrFromAbs(UInt32 currViewAbsCol, bool withLines) const;

	std::weak_ptr<ViewPort> GetOwner() const { return m_Owner; }

private:
	void Recalc();
	IPoint GetGridCoord       (const GPoint& clientRelPoint) const;
	GType FirstViewCol(Int32 col) const;
	GType FirstViewRow(Int32 row) const;

	std::weak_ptr<ViewPort>    m_Owner;
	grid_coord_key             m_Key;

	GPoint                     m_ClientSize;     // in device coordinates
	CrdTransformation          m_World2ClientTr; // m_w2vTr;
	bool                       m_IsDirty;

	// ========== calculated results

	GRect                      m_ClippedRelRect; // m_ViewExtents;
	CrdPoint                   m_GridOrigin;
	CrdPoint                   m_GridCellSize;
	OrientationType            m_Orientation;
	CrdPoint                   m_SubPixelFactors;
	grid_coord_array           m_GridRows,  m_GridCols;
	grid_coord_array           m_LinedRows, m_LinedCols;

	friend class ViewPort;
};



#endif // __SHV_GRIDCOORD_H

