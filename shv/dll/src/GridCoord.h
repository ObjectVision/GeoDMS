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

	const GRect& GetClippedRelDeviceRect() const { dms_assert(!IsDirty()); return m_ClippedRelDeviceRect; } 
	const IRect& GetGridRect      () const { return m_Key.second; }

	IRect GetClippedGridRect       (const GRect& viewRelRect) const;

	GRect GetClippedRelDeviceRect (const IRect& selRect ) const;
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

	GPoint                     m_DeviceSize;
	CrdTransformation          m_World2DeviceTr; // m_w2vTr;
	bool                       m_IsDirty;

	// ========== calculated results

	GRect                      m_ClippedRelDeviceRect; // m_ViewExtents;
	CrdPoint                   m_GridOrigin;
	CrdPoint                   m_GridCellSize;
	OrientationType            m_Orientation;
	CrdPoint                   m_SubPixelFactors;
	grid_coord_array           m_GridRows,  m_GridCols;
	grid_coord_array           m_LinedRows, m_LinedCols;

	friend class ViewPort;
};



#endif // __SHV_GRIDCOORD_H

