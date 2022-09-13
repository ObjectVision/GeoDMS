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
// SheetVisualTestView.h : interface of the DataView class
//
/////////////////////////////////////////////////////////////////////////////

#if !defined(__SHV_ITEMSCHEMAVIEW_H)
#define __SHV_ITEMSCHEMAVIEW_H

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

//----------------------------------------------------------------------
// used modules and forward class references
//----------------------------------------------------------------------

#include "GraphDataView.h"
class ItemSchemaController;
using TreeItemInterestPtr = InterestPtr<const TreeItem*>;

//----------------------------------------------------------------------
// class  : ItemSchemaView
//----------------------------------------------------------------------

class ItemSchemaView : public GraphDataView
{
	typedef GraphDataView base_type;

public:
	ItemSchemaView(TreeItem* viewContext, ViewStyle ct, ShvSyncMode sm);
	void SetContents(std::shared_ptr<MovableObject> contents, ShvSyncMode sm) override;
	~ItemSchemaView();
		
private:
	bool CanContain(const TreeItem* viewCandidate) const override;
	void AddLayer  (const TreeItem*, bool isDropped) override;
	GraphVisitState UpdateView() override;

	bool MustUpdate() const;

	ViewStyle m_ViewStyle;

	OwningPtr<ItemSchemaController> m_Controller;

	TreeItemInterestPtr m_InterestHolder;
	bool                m_ZoomAll;

	DECL_RTTI(SHV_CALL, Class);

public:
	InterestPtr< SharedPtr<AbstrUnit    > > m_SchemaNodes, m_SchemaLinks;
	InterestPtr< SharedPtr<AbstrDataItem> > m_SchemaLocation, m_SchemaLabelText, m_F1, m_F2;
};

#endif // !defined(__SHV_ITEMSCHEMAVIEW_H)
