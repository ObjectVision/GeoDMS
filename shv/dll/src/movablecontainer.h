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
enum class MC_Orientation { Cols, Rows };

class AutoSizeContainer : public MovableContainer
{
	using base_type = MovableContainer;

public:
	AutoSizeContainer(MovableObject* owner, MC_Orientation orientation) 
		: base_type(owner) 
		, m_Orientation(orientation)
	{}

	void SetMaxSize(TType  maxSize);
	void SetSepSize(UInt32 sepSize);

	void ToggleOrientation();
	bool IsColOriented() const { return m_Orientation == MC_Orientation::Cols; }

protected:
//	override virtual methods of GraphicObject
	void ProcessCollectionChange() override; // calculates Size

	void DrawBackground(const GraphDrawer& d) const override;

	void GrowHor(CrdType xDelta, CrdType xRelPos, const MovableObject* sourceItem) override;
	void GrowVer(CrdType xDelta, CrdType xRelPos, const MovableObject* sourceItem) override;


	MC_Orientation m_Orientation;

	TType   m_MaxSize = 0;
	UInt32  m_SepSize = 2;
};

//----------------------------------------------------------------------
// class  : GraphicVarRows
//----------------------------------------------------------------------

class GraphicVarRows : public AutoSizeContainer
{
	using base_type = AutoSizeContainer;
public:
	GraphicVarRows(MovableObject* owner) : AutoSizeContainer(owner, MC_Orientation::Rows) {}

//	override virtual methods of GraphicObject

	void SetMaxColWidth(TType  maxColWidth)   { SetMaxSize(maxColWidth); }
	void SetRowSepHeight(UInt32 rowSepHeight) { SetSepSize(rowSepHeight); }

	TType  MaxElemWidth() const { return m_MaxSize;  }
	UInt32 RowSepHeight() const { return m_SepSize; }
};

//----------------------------------------------------------------------
// class  : GraphicVarCols
//----------------------------------------------------------------------

class GraphicVarCols : public AutoSizeContainer
{
	using base_type = AutoSizeContainer;
public:
	GraphicVarCols(MovableObject* owner) 
		: AutoSizeContainer(owner, MC_Orientation::Cols) {}

	void SetMaxRowHeight(TType  maxRowHeight);
	void SetColSepWidth (UInt32 colSepWidth);

	TType   MaxElemHeight() const { return m_MaxSize; } // in logical coordinates
	UInt32  ColSepWidth  () const { return m_SepSize;  } // in logical coordinates
};

#endif // __SHV_MOVABLECONTAINER_H
