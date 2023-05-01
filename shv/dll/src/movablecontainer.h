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
	typedef MovableContainer base_type;
protected:
	AutoSizeContainer(MovableObject* owner) : base_type(owner) {}

//	override virtual methods of GraphicObject
	void ProcessCollectionChange() override; // calculates Size
};

//----------------------------------------------------------------------
// class  : GraphicVarRows
//----------------------------------------------------------------------

class GraphicVarRows : public AutoSizeContainer
{
	typedef AutoSizeContainer base_type;
protected:
	GraphicVarRows(MovableObject* owner);

public:
//	override virtual methods of GraphicObject
  	GraphVisitState InviteGraphVistor(class AbstrVisitor& gv) override;
	void ProcessCollectionChange() override;
	void DrawBackground(const GraphDrawer& d) const override;

	void GrowHor(TType xDelta, TType xRelPos, const MovableObject* sourceItem) override;
	void GrowVer(TType xDelta, TType xRelPos, const MovableObject* sourceItem) override;

	void SetMaxColWidth (TType  maxColWidth);
	void SetRowSepHeight(UInt32 rowSepHeight);

	TType  MaxElemWidth() const { return m_MaxColWidth;  }
	UInt32 RowSepHeight() const { return m_RowSepHeight; }

private:
	TType   m_MaxColWidth;
	UInt32  m_RowSepHeight;
};

//----------------------------------------------------------------------
// class  : GraphicVarCols
//----------------------------------------------------------------------

class GraphicVarCols : public AutoSizeContainer
{
	using base_type = AutoSizeContainer;
protected:
	GraphicVarCols(MovableObject* owner);

public:
//	override virtual methods of GraphicObject
  	GraphVisitState InviteGraphVistor(class AbstrVisitor& gv) override;
	void ProcessCollectionChange() override;
	void DrawBackground(const GraphDrawer& d) const override;

	void GrowHor(TType xDelta, TType xRelPos, const MovableObject* sourceItem) override;
	void GrowVer(TType xDelta, TType xRelPos, const MovableObject* sourceItem) override;

	void SetMaxRowHeight(TType  maxRowHeight);
	void SetColSepWidth (UInt32 colSepWidth);

	TType   MaxElemHeight() const { return m_MaxRowHeight; }
	UInt32  ColSepWidth  () const { return m_ColSepWidth;  }

private:
	TType  m_MaxRowHeight;
	UInt32 m_ColSepWidth;
};

#endif // __SHV_MOVABLECONTAINER_H
