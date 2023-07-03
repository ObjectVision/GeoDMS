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
// SheetVisualTestView.cpp : implementation of the DataView class
//
#include "ShvDllPch.h"

#include "ItemSchemaView.h"

#include "act/ActorVisitor.h"
#include "act/SupplierVisitFlag.h"
#include "mci/Class.h"
#include "set/QuickContainers.h"
#include "set/StackUtil.h"

#include "AbstrUnit.h"
#include "DataArray.h"
#include "ItemUpdate.h"


#include "AbstrCmd.h"
#include "GraphicRect.h"
#include "LayerInfo.h"
#include "LayerClass.h"
#include "LayerSet.h"
#include "MapControl.h"
#include "ScrollPort.h"
#include "Theme.h"
#include "ViewPort.h"
#include "FeatureLayer.h"


/////////////////////////////////////////////////////////////////////////////
// SubItemSchemaControl Implementation

struct EmptyState : Void
{
	void clear() {}
};

struct SubItemQuery
{
	typedef EmptyState GlobalState;
	typedef const TreeItem* pointer;

	SubItemQuery(const TreeItem* self, GlobalState*) : m_Curr(self->GetFirstSubItem())
	{}

	template <typename Action>
	void operator()(Action&& action)
	{
		for(; m_Curr; m_Curr.assign( m_Curr->GetNextItem() ) )
			action(m_Curr);
	}

	WeakPtr<const TreeItem> m_Curr;
};

struct SeenActorSet: std::set<const Actor*>
{
	bool IsNew(const Actor* self)
	{
		auto ip = lower_bound(self);
		if (ip != end() && *ip == self)
			return false;
		insert(ip, self);
		return true;
	}
};


template <typename Action>
struct RecurseTreeItem : ActorVisitor
{
	RecurseTreeItem(const Action& rhs, SeenActorSet* sas)
		:	m_Action(rhs) 
		,	m_SAS(sas)
	{}

	ActorVisitState operator() (const Actor* self) const override
	{
		dms_assert(self);
		if (m_SAS->IsNew(self))
		{

			const TreeItem* item = dynamic_cast<const TreeItem*>(self);
			if (item)
				m_Action(item);
		}
		return AVS_Ready;
	}
	const Action& m_Action;
	SeenActorSet* m_SAS;
};

struct UniqueSupplierQuery
{
	typedef const TreeItem* pointer;
	typedef SeenActorSet GlobalState;

	UniqueSupplierQuery(pointer self, GlobalState* sas)
		:	m_Curr(self)
		,	m_SAS(sas)
		,	m_SVF(SupplierVisitFlag::InspectAll)
	{
		dms_assert(self);
		dms_assert(sas);
	}

	template <typename Action>
	void operator()(const Action& action)
	{
		assert(m_Curr);
		m_Curr->UpdateMetaInfo();
		ActorVisitState res = m_Curr->VisitSuppliers(m_SVF, RecurseTreeItem<Action>(action, m_SAS));
		dms_assert(res == AVS_Ready);
	}

	WeakPtr<const TreeItem> m_Curr;
	SeenActorSet*           m_SAS;
	SupplierVisitFlag       m_SVF;
};

struct TreeItemFilterFunc //: std::unary_function<const Actor*, const TreeItem*>
{
	const TreeItem* operator() (const Actor* act) const 
	{
		return dynamic_cast<const TreeItem*>(act);
	}
};

/////////////////////////////////////////////////////////////////////////////
// ItemSchemaControl Implementation

class ItemSchemaController
{	
public:
	void AddItem(const TreeItem* item)
	{
		m_RootItems.emplace_back(item);
	}
		
	virtual void Update(ItemSchemaView*) = 0;

protected:
	template <typename QueryType> void UpdateItems(ItemSchemaView* isv);

	std::vector<SharedTreeItemInterestPtr> m_RootItems;
	std::vector<SharedActorInterestPtr>    m_AllItems;

	friend class ItemSchemaView;
	friend struct ItemSchemaControllerWriter;
};

/////////////////////////////////////////////////////////////////////////////
// ItemSchemaControllerWriter Implementation

struct ItemSchemaControllerWriterBase : WeakPtr<ItemSchemaView>
{
	ItemSchemaControllerWriterBase(ItemSchemaView* isv, UInt32 nrItems, UInt32 nrRoots)
		:	WeakPtr(isv)
	{
		isv->m_SchemaNodes->SetCount(nrItems);           isv->m_SchemaNodes->SetTSF(USF_HasConfigRange);
		isv->m_SchemaLinks->SetCount(nrItems - nrRoots); isv->m_SchemaLinks->SetTSF(USF_HasConfigRange);
	}
};

struct ItemSchemaControllerWriter: ItemSchemaControllerWriterBase
{
	ItemSchemaControllerWriter(ItemSchemaController* self, ItemSchemaView* isv, UInt32 nrItems)
		:	ItemSchemaControllerWriterBase(isv, nrItems, self->m_RootItems.size())
		,	m_ISC(self)
		,	labelTextLock(isv->m_SchemaLabelText)
		,	locationLock (isv->m_SchemaLocation)
		,	f1Lock       (isv->m_F1)
		,	f2Lock       (isv->m_F2)
	{
		m_ISC->m_AllItems.clear();
		m_ISC->m_AllItems.reserve(nrItems);
	}

	void WriteNode(const Actor* item)
	{
		locationLock->SetValue<SPoint   >(nodeIndex, loc);
		labelTextLock->SetValue<SharedStr>(nodeIndex, SharedStr(item->GetID()));
		dms_assert(m_ISC->m_AllItems.size() < m_ISC->m_AllItems.capacity()); // nrItems at construction is assumed to perfectly predict 
		m_ISC->m_AllItems.push_back( SharedPtr<const Actor>(item) );
		++nodeIndex;
	}
	void WriteLink(UInt32 node1, UInt32 node2)
	{
		dms_assert(node1 < node2);
		dms_assert(node2 > rootIndex);
		UInt32 currLink = node2 - (1 + rootIndex);

		f1Lock->SetValue<UInt32>(currLink, node1);
		f2Lock->SetValue<UInt32>(currLink, node2);
	}
	void NextRow() { loc.Row() -= 05; }
	void NextCol() { loc.Col() += 80; }
	void PrevCol() { loc.Col() -= 80; }

	void Commit()
	{
		labelTextLock.Commit();
		locationLock.Commit();
		f1Lock.Commit();
		f2Lock.Commit();
	}

	WeakPtr<ItemSchemaController> m_ISC;
	SPoint loc;
	UInt32 nodeIndex = 0;
	UInt32 rootIndex = 0;
private:
	DataWriteLock labelTextLock, locationLock, f1Lock, f2Lock;
};

/////////////////////////////////////////////////////////////////////////////
// TreeQuery Implementations

template <typename QueryType>
void PlaceLabels(ItemSchemaControllerWriter& writer, typename QueryType::pointer item, typename QueryType::GlobalState* gs)
{
	writer.WriteNode(item);

	QueryType query(item, gs);

	writer.NextCol();
	UInt32 f1 = writer.nodeIndex-1;
	query(
		[&] (typename QueryType::pointer supplier) -> void
		{
			UInt32 f2 = writer.nodeIndex; // next element to be written
			PlaceLabels<QueryType>(writer, supplier, gs);
			writer.WriteLink(f1, f2);
			writer.NextRow(); // TODO: not for the last element or absorb in caller
		}
	);
	writer.PrevCol();
}

template <typename QueryType>
UInt32 CountSubItems(typename QueryType::pointer item, typename QueryType::GlobalState* gs)
{
	dms_assert(item);

	UInt32 result = 1;
	QueryType query(item, gs);

	query(
		[&](typename QueryType::pointer subItem) -> void
		{
			dms_assert(subItem);
			result += CountSubItems<QueryType>(subItem, gs);
		}
	);
	return result;
}

/////////////////////////////////////////////////////////////////////////////
// ItemSchemaControl Implementation


template <typename QueryType>
void ItemSchemaController::UpdateItems(ItemSchemaView* isv)
{
	InterestRetainContextBase base;
	UInt32 nrItems = 0;
	typename QueryType::GlobalState gs;
	UInt32 n = m_RootItems.size();
	for (UInt32 i=0; i!= n; ++i)
		nrItems += CountSubItems<QueryType>(m_RootItems[i], &gs);

	ItemSchemaControllerWriter writer(this, isv, nrItems);

	gs.clear();
	for (writer.rootIndex = 0; writer.rootIndex != n; ++writer.rootIndex)
	{
		PlaceLabels<QueryType>(writer, m_RootItems[writer.rootIndex], &gs);
		writer.NextRow();
	}
	assert(m_AllItems.size() == nrItems);
	writer.Commit();
};

class SubItemSchemaController : public ItemSchemaController
{
	void Update(ItemSchemaView* isv) override
	{
		UpdateItems<SubItemQuery>(isv);
	};
};

/////////////////////////////////////////////////////////////////////////////
// SupplierSchemaControl Implementation

class SupplierSchemaController : public ItemSchemaController
{
	void Update(ItemSchemaView* isv) override
	{
		UpdateItems<UniqueSupplierQuery>(isv);
	};
};

/////////////////////////////////////////////////////////////////////////////
// ExprSchemaControl Implementation

class ExprSchemaController : public ItemSchemaController
{
	void Update(ItemSchemaView*) override
	{
	};
};

/////////////////////////////////////////////////////////////////////////////
// ItemSchemaView Implementation
#include "DataItemClass.h"
#include "Unit.h"
#include "UnitClass.h"

ItemSchemaView::ItemSchemaView(TreeItem* viewContext, ViewStyle ct, ShvSyncMode sm)
	: GraphDataView(viewContext, sm)
	, m_ViewStyle(ct)
	, m_ZoomAll(false)
{}

ItemSchemaView::~ItemSchemaView()
{}

void ItemSchemaView::SetContents(std::shared_ptr<MovableObject> contents, ShvSyncMode sm)
{
	base_type::SetContents(contents, sm);

	ViewPort* vp = debug_valcast<ViewPort*>(GetContents()->GetEntry(1));
	LayerSet* ls = vp->GetLayerSet();
	dms_assert(ls);
	auto gnl = std::make_shared<GraphicNetworkLayer>(ls);
	ls->InsertEntry(gnl.get());
	{
		TreeItem* gnlContext = gnl->GetContext();
		dms_assert(gnlContext);
		StaticStIncrementalLock<TreeItem::s_MakeEndoLockCount> makeEndoLock;
		m_SchemaNodes    = Unit<UInt32>::GetStaticClass()->CreateUnit(gnlContext, GetTokenID_mt("Nodes"));
		m_SchemaLinks    = Unit<UInt32>::GetStaticClass()->CreateUnit(gnlContext, GetTokenID_mt("Links"));

		m_SchemaLocation = CreateDataItem(m_SchemaNodes.get_ptr(), GetTokenID_mt("Location") , m_SchemaNodes, Unit<SPoint   >::GetStaticClass()->CreateDefault());
		m_SchemaLabelText= CreateDataItem(m_SchemaNodes.get_ptr(), GetTokenID_mt("LabelText"), m_SchemaNodes, Unit<SharedStr>::GetStaticClass()->CreateDefault());

		m_F1 = CreateDataItem(m_SchemaLinks.get_ptr(), GetTokenID_mt("F1"), m_SchemaLinks, m_SchemaNodes);
		m_F2 = CreateDataItem(m_SchemaLinks.get_ptr(), GetTokenID_mt("F2"), m_SchemaLinks, m_SchemaNodes);
	}

	gnl->SetTheme(Theme::Create(AN_Feature,   nullptr, nullptr, m_SchemaLocation ).get(), ls->GetContext());
	gnl->SetTheme(Theme::Create(AN_LabelText, m_SchemaLabelText, nullptr, nullptr).get(), ls->GetContext());
	gnl->SetTheme(Theme::Create(AN_NetworkF1, m_F1, nullptr, nullptr).get(), ls->GetContext());
	gnl->SetTheme(Theme::Create(AN_NetworkF2, m_F2, nullptr, nullptr).get(), ls->GetContext());


	switch (m_ViewStyle)
	{
		case tvsUpdateTree:
		case tvsSubItemSchema:
			m_Controller.assign( new SubItemSchemaController );
			return;
		case tvsUpdateItem:
		case tvsSupplierSchema:
			m_Controller.assign( new SupplierSchemaController );
			return;
		case tvsExprSchema:
			m_Controller.assign( new ExprSchemaController );
			return;
	}
}

bool ItemSchemaView::CanContain(const TreeItem* viewCandidate) const
{
	return true;
}

void ItemSchemaView::AddLayer(const TreeItem* viewItem, bool isDropped)
{
	m_Controller->AddItem(viewItem);
	m_Controller->Update(this);

	Invalidate();

	m_ZoomAll = true;
}

bool ItemSchemaView::MustUpdate() const
{
	return m_ViewStyle == tvsUpdateItem || m_ViewStyle == tvsUpdateTree;
}

GraphVisitState ItemSchemaView::UpdateView()
{
	if (base_type::UpdateView() != GVS_Continue)
		return GVS_Break;
	GetContents()->GetViewPort()->InvalidateView();

	if (MustUpdate())
	{
		ItemSchemaController* isc = m_Controller;
		for(UInt32 i = 0; i < isc->m_RootItems.size(); ++i)
		{
			const TreeItem* theItem = isc->m_RootItems[i];
			switch (m_ViewStyle) {
				case tvsUpdateItem:
					if (ItemUpdateImpl(theItem, "UpdateItem", m_InterestHolder ) == AVS_SuspendedOrFailed)
						return GVS_BreakOnSuspended();
					break;
				case tvsUpdateTree:
					if (TreeUpdateImpl(theItem, "UpdateTree", m_InterestHolder ) == AVS_SuspendedOrFailed)
						return GVS_BreakOnSuspended();
					break;
			}
		}
		if (m_ZoomAll)
		{
			GetContents()->OnCommand(TB_ZoomAllLayers);
			m_ZoomAll = false;
		}
	}
	return GVS_Continue;
}

IMPL_RTTI_CLASS(ItemSchemaView);
