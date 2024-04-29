// Copyright (C) 1998-2024 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

#if !defined(__SHV_TABLECONTROL_H)
#define __SHV_TABLECONTROL_H

#include "MovableContainer.h"

//----------------------------------------------------------------------
// class  : TableHeaderControl
//----------------------------------------------------------------------

class TableHeaderControl: public AutoSizeContainer
{
	using base_type = AutoSizeContainer;

public:
	TableHeaderControl(MovableObject* owner, TableControl* tableControl);

	GraphicClassFlags GetGraphicClassFlags() const override { return GraphicClassFlags::ChildCovered; };

//	implement GraphicObject methods
	void DoUpdateView() override;
	bool MouseEvent(MouseEventDispatcher& med) override;
	bool OnKeyDown(UInt32 virtKey) override;

private:
	TableControl*    m_TableControl;

	ScopedConnection m_connElemSetChanged;
};

#endif // __SHV_TABLECONTROL_H
