// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#pragma once

#ifndef __SHV_MOVABLECONTAINER_H
#define __SHV_MOVABLECONTAINER_H

//----------------------------------------------------------------------
// used modules and forward class references
//----------------------------------------------------------------------

#include "MovableObject.h"
#include "GraphicContainer.h"

//----------------------------------------------------------------------
// class  : MovableContainer
//----------------------------------------------------------------------

class MovableContainer : public GraphicContainer<MovableObject>
{
	typedef GraphicContainer<MovableObject> base_type;
protected:
	MovableContainer(MovableObject* owner) : base_type(owner) {}

//	override virtual methods of GraphicObject
	GraphVisitState InviteGraphVistor(AbstrVisitor& v) override;
};

//----------------------------------------------------------------------
// class  : AutoSizeContainer
//----------------------------------------------------------------------

class AutoSizeContainer : public MovableContainer
{
protected:
	using base_type = MovableContainer;

	AutoSizeContainer(MovableObject* owner) : base_type(owner) {}

//	override virtual methods of GraphicObject
	void ProcessCollectionChange() override; // calculates Size
};

//----------------------------------------------------------------------
// class  : GraphicVarRows
//----------------------------------------------------------------------

class GraphicVarRows : public AutoSizeContainer
{
	using base_type = AutoSizeContainer;
public:
	GraphicVarRows(MovableObject* owner);

//	override virtual methods of GraphicObject
  	GraphVisitState InviteGraphVistor(class AbstrVisitor& gv) override;
	void ProcessCollectionChange() override;
	void DrawBackground(const GraphDrawer& d) const override;

	void GrowHor(CrdType xDelta, CrdType xRelPos, const MovableObject* sourceItem) override;
	void GrowVer(CrdType xDelta, CrdType xRelPos, const MovableObject* sourceItem) override;

	void SetMaxColWidth (TType  maxColWidth);
	void SetRowSepHeight(UInt32 rowSepHeight);

	TType  MaxElemWidth() const { return m_MaxColWidth;  }
	UInt32 RowSepHeight() const { return m_RowSepHeight; }

private:
	TType   m_MaxColWidth  = 0;
	UInt32  m_RowSepHeight = 2;
};

//----------------------------------------------------------------------
// class  : GraphicVarCols
//----------------------------------------------------------------------

class GraphicVarCols : public AutoSizeContainer
{
	using base_type = AutoSizeContainer;
public:
	GraphicVarCols(MovableObject* owner);

//	override virtual methods of GraphicObject
  	GraphVisitState InviteGraphVistor(class AbstrVisitor& gv) override;
	void ProcessCollectionChange() override;
	void DrawBackground(const GraphDrawer& d) const override;

	void GrowHor(CrdType xDelta, CrdType xRelPos, const MovableObject* sourceItem) override;
	void GrowVer(CrdType xDelta, CrdType xRelPos, const MovableObject* sourceItem) override;

	void SetMaxRowHeight(TType  maxRowHeight);
	void SetColSepWidth (UInt32 colSepWidth);

	TType   MaxElemHeight() const { return m_MaxRowHeight; } // in logical coordinates
	UInt32  ColSepWidth  () const { return m_ColSepWidth;  } // in logical coordinates

private:
	TType  m_MaxRowHeight = 0;
	UInt32 m_ColSepWidth  = 1;
};

#endif // __SHV_MOVABLECONTAINER_H
