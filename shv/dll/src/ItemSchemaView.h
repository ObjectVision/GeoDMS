// Copyright (C) 2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#pragma once

#if !defined(__SHV_ITEMSCHEMAVIEW_H)
#define __SHV_ITEMSCHEMAVIEW_H

//----------------------------------------------------------------------
// used modules and forward class references
//----------------------------------------------------------------------

#include "GraphDataView.h"
class ItemSchemaController;

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

	SharedTreeItemInterestPtr m_InterestHolder;
	bool                m_ZoomAll;

	DECL_RTTI(SHV_CALL, Class);

public:
	SharedMutableUnitInterestPtr     m_SchemaNodes, m_SchemaLinks;
	SharedMutableDataItemInterestPtr m_SchemaLocation, m_SchemaLabelText, m_F1, m_F2;
};

#endif // !defined(__SHV_ITEMSCHEMAVIEW_H)
