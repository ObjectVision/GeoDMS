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

#ifndef __SHV_GRAPHICCONTAINER_H
#define __SHV_GRAPHICCONTAINER_H

//----------------------------------------------------------------------
// used modules and forward class references
//----------------------------------------------------------------------
#include "ShvSignal.h"

#include "TreeItem.h"

#include "geo/SeqVector.h"
#include "ptr/SharedPtr.h"
#include "ptr/OwningPtr.h"
#include "set/VectorFunc.h"

#include "GraphicObject.h"

enum class ElemShuffleCmd { ToFront, ToBack, Forward, Backward, Remove, };

//----------------------------------------------------------------------
// class  : GraphicContainer
//----------------------------------------------------------------------

template <typename T> struct owner_type                 { typedef T             type; };
template <>           struct owner_type<ScalableObject> { typedef GraphicObject type; };

template <typename ElemType> // ElemType = MovableObject or ScalableObject
class GraphicContainer : public ElemType
{
	typedef ElemType                            base_type;
	typedef GraphicContainer<ElemType>          this_type;
	typedef typename owner_type<ElemType>::type owner_t;

protected:
	typedef std::vector< std::shared_ptr<ElemType> > array_type;

	GraphicContainer(owner_t* owner);

public:
//	override GraphicObject interface for composition of GraphicObjects (composition pattern)
	gr_elem_index NrEntries() const override  { return m_Array.size(); }
	ElemType* GetEntry(gr_elem_index i) { dms_assert(i < NrEntries()); return m_Array[i].get(); }
//	const ElemType* GetConstEntry(gr_elem_index i) const { dms_assert(i < NrEntries()); return m_Array[i]; }
	void SetDisconnected() override;

	virtual void ProcessCollectionChange();

//	override virtual methods of GraphicObject
	void Sync(TreeItem* viewContext, ShvSyncMode sm) override;
	void OnChildSizeChanged() override;
	void DoUpdateView() override;

	ElemType* GetActiveEntry();

	void SetActiveEntry(ElemType* go);

	gr_elem_index GetEntryPos(ElemType* entry) const;

	void InsertEntry   (ElemType* newEntry);
	void InsertEntryAt (ElemType* newEntry, gr_elem_index pos);
	void ReplaceEntryAt(ElemType* newEntry, gr_elem_index pos);
	void RemoveEntry  (gr_elem_index  i);
	void RemoveAllEntries();
	virtual void RemoveEntry(ElemType* g);
	void MoveEntry(ElemType* g, this_type* dst, gr_elem_index pos);

protected:
	void SaveOrder();

public:
	CmdSignal  m_cmdElemSetChanged;
protected:
	array_type m_Array;
};


#endif // __SHV_GRAPHICCONTAINER_H
