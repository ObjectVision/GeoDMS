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

#ifndef __SHV_WRAPPER_H
#define __SHV_WRAPPER_H

//----------------------------------------------------------------------
// used modules and forward class references
//----------------------------------------------------------------------

#include "GraphicContainer.h"
#include "MovableObject.h"

const int LINE_SCROLL_STEP =  20;
const int PAGE_SCROLL_STEP = 100;

//----------------------------------------------------------------------
// class  : Wrapper
//----------------------------------------------------------------------

class Wrapper: public MovableObject
{
	typedef MovableObject base_type;

protected:
	Wrapper(MovableObject* owner, DataView* dv, CharPtr caption);
	
	GraphicClassFlags GetGraphicClassFlags() const override { dms_assert(!base_type::GetGraphicClassFlags()); return GraphicClassFlags(GCF_PushVisibility|GCF_ClipExtents); }

public:

// props
	void SetContents(sharedPtrGO contents);
	void ClearContents();


	GraphicObject*       GetContents()       { return m_Contents.get(); }
	const GraphicObject* GetContents() const { return m_Contents.get(); }

	SharedStr GetCaption() const override;

//	overrid GraphicObject interface for composition of GraphicObjects (composition pattern)
	virtual SizeT  NrEntries() const override;
	GraphicObject* GetEntry(SizeT i) override;
	std::weak_ptr<DataView> GetDataView() const override { return m_DataView; }

//	override virtual methods of GraphicObject
	void Sync(TreeItem* context, ShvSyncMode sm) override;

	bool OnCommand(ToolButtonID id) override;

	virtual void Export() = 0;

protected:

#if defined(MG_DEBUG)
	bool DataViewOK() const;
#endif
private:
	std::weak_ptr<DataView> m_DataView;
	sharedPtrGO m_Contents;
	SharedStr   m_Caption;

	DECL_ABSTR(SHV_CALL, Class);
};



#endif // __SHV_WRAPPER_H

