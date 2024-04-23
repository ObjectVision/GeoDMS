// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#pragma once

#ifndef __SHV_SELCARET_H
#define __SHV_SELCARET_H

//----------------------------------------------------------------------
// used modules and forward class references
//----------------------------------------------------------------------

#include "ShvBase.h"

#include "LockedIndexCollectorPtr.h"

//----------------------------------------------------------------------
// class  : SelCaret
//----------------------------------------------------------------------

#include "Region.h"

struct SelCaret : std::enable_shared_from_this<SelCaret>
{
	SelCaret(ViewPort* owner, const sel_caret_key& key, GridCoordPtr gridCoords);
	~SelCaret();

	void SetSelCaretRgn(      Region&& selCaret);
	void XOrSelCaretRgn(const Region& selCaret);
	const Region& GetSelCaretRgn() const { return m_SelCaretRgn; }

	void OnZoom();
	void OnDeviceScroll(const GPoint& delta);

	std::weak_ptr<ViewPort> GetOwner() const;

	void UpdateRgn(const Region& updateRgn);

private:
	Region UpdateRectImpl(const GRect& updateRect);
	void ForwardDiff(const Region& diff);

	std::weak_ptr<ViewPort>        m_Owner;	
	WeakPtr<const IndexCollector>  m_EntityIndexCollectorPtr;
	OptionalIndexCollectorAray     m_EntityIndexCollectorArray;
	SharedPtr<const AbstrDataItem> m_SelAttr;
	GridCoordPtr                   m_GridCoords;
	Region                         m_SelCaretRgn;
	bool                           m_Ready;   // UpdateRgn was called after OnZoom of ctor, thus OnScroll should update incrementally

	friend class ViewPort;
};

#endif // __SHV_SELCARET_H

