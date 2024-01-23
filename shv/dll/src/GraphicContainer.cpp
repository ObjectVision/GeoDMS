// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "ShvDllPch.h"

#include "GraphicContainer.h"

#include "dbg/DebugContext.h"
#include "mci/SingleLinkedTree.inc"

//----------------------------------------------------------------------
// class  : GraphicContainer --- template member implementation
//----------------------------------------------------------------------

#include "TreeItemClass.h"

template <typename ElemType>
GraphicContainer<ElemType>::GraphicContainer(owner_t* owner)
	:	base_type(owner)
{}

template <typename ElemType>
ElemType* GraphicContainer<ElemType>::GetActiveEntry()
{
	for (SizeT i=0, n=NrEntries(); i!=n; ++i)
	{
		ElemType* go = GetEntry(i);
		if (go->IsActive())
			return go;
	}
	return nullptr;
}

template <typename ElemType>
void GraphicContainer<ElemType>::ProcessCollectionChange()
{
	this->InvalidateDraw();
	m_cmdElemSetChanged();
}

template <typename ElemType>
void GraphicContainer<ElemType>::Sync(TreeItem* viewContext, ShvSyncMode sm)
{
	dms_assert(viewContext);
	ObjectContextHandle contextHandle(viewContext, "GraphicContainer::Sync");


	base_type::Sync(viewContext, sm);

	if (sm == SM_Save)
	{
		if (viewContext)
		{
			for (UInt32 i=0, n=m_Array.size(); i!=n; ++i)
			{
				GraphicObject* g = m_Array[i].get(); dms_assert(g);
				TreeItem* gvc = g->GetContext();
				if (!gvc)
					g->Sync(viewContext->CreateItem(UniqueName(viewContext, g->GetDynamicClass())), SM_Save);
				else if (!gvc->IsEndogenous())
					g->Sync(gvc, SM_Save);
			}
		}
		return;
	}

	dms_assert(sm == SM_Load);

	viewContext = const_cast<TreeItem*>( viewContext->GetFirstVisibleSubItem() );
	while (viewContext)
	{
		if (!viewContext->GetTSF(TSF_IsHidden))
		{
			typename array_type::value_type newGraphic = std::dynamic_pointer_cast<ElemType>( CreateFromContext(viewContext, this) );
			if (newGraphic)
			{
				m_Array.push_back( newGraphic );
				if (newGraphic->IsVisible())
					ProcessCollectionChange();
			}
		}
		viewContext = const_cast<TreeItem*>( viewContext->GetNextVisibleItem() );
	}
}

template <typename ElemType>
void GraphicContainer<ElemType>::SetActiveEntry(ElemType* go)
{
	dms_assert(go);
	if (go->IsActive())
		return;

	ElemType* activeEntry = GetActiveEntry();
	if (activeEntry)
		activeEntry->SetActive(false);
	go->SetActive(true);

	m_cmdElemSetChanged();
}

inline void ConnectObject(GraphicObject* g, TreeItem* viewContext)
{
	if (viewContext)
		g->Sync(
			viewContext->CreateItem(UniqueName(viewContext, g->GetDynamicClass())),
			SM_Save
		);
}

template <typename ElemType>
void GraphicContainer<ElemType>::InsertEntry(ElemType* g)
{
	dms_assert(GetEntryPos(g) >= m_Array.size());

	g->SetOwner(this);
	m_Array.push_back(g->shared_from_base<ElemType>());

	ConnectObject(g, this->GetContext());
	ProcessCollectionChange();
}

template <typename ElemType>
void GraphicContainer<ElemType>::InsertEntryAt(ElemType* g, gr_elem_index pos)
{
	dms_assert(GetEntryPos(g) >= m_Array.size());
	dms_assert(pos <= m_Array.size());

	g->SetOwner(this);
	m_Array.insert(m_Array.begin() + pos, g->shared_from_base<ElemType>() );

	ConnectObject(g, this->GetContext());
	ProcessCollectionChange();
}

template <typename ElemType>
void GraphicContainer<ElemType>::ReplaceEntryAt(ElemType* g, gr_elem_index pos)
{
	dms_assert(GetEntryPos(g) >= m_Array.size());
	dms_assert(pos <= m_Array.size());

	bool wasVisible = m_Array[pos]->AllVisible(); 
	m_Array[pos]->SetDisconnected();
	g->SetOwner(this);
	m_Array[pos] = g->shared_from_base<ElemType>();

	ConnectObject(g, this->GetContext());
	ProcessCollectionChange();
}

template <typename ElemType>
void GraphicContainer<ElemType>::RemoveEntry(ElemType* g)
{
	bool wasVisible = g->IsVisible();
	g->SetDisconnected();
	vector_erase_first_if(m_Array, [g](auto&& e) { return e.get() == g; });
	if (wasVisible)
		ProcessCollectionChange();
	else
		m_cmdElemSetChanged();
}

template <typename ElemType>
void GraphicContainer<ElemType>::RemoveEntry(gr_elem_index i)
{
	RemoveEntry(m_Array[i].get());
}

template <typename ElemType>
void GraphicContainer<ElemType>::RemoveAllEntries()
{
	gr_elem_index n = NrEntries();
	while (n)
		RemoveEntry(--n);
	ProcessCollectionChange();
}

template <typename ElemType>
void GraphicContainer<ElemType>::SetDisconnected()
{
	base_type::SetDisconnected();
//	RemoveAllEntries();
}

template <typename ElemType>
gr_elem_index GraphicContainer<ElemType>::GetEntryPos(ElemType* entry) const
{
	return Convert<gr_elem_index>(vector_find_if(m_Array, [entry](auto& e) { return e.get() == entry;}));
}

template <typename ElemType>
void GraphicContainer<ElemType>::MoveEntry(ElemType* g, this_type* dst, gr_elem_index newPos)
{
	gr_elem_index curPos = GetEntryPos(g);
	dms_assert(curPos < m_Array.size());

	dms_assert(!g->IsOwnerOf(dst));

	if (dst == this)
	{
		if (newPos == curPos || newPos == curPos+1)
			return;
		if (newPos < curPos)
			std::rotate(m_Array.begin()+newPos, m_Array.begin()+curPos, m_Array.begin()+curPos+1);
		else
			std::rotate(m_Array.begin()+curPos, m_Array.begin()+curPos+1, m_Array.begin()+newPos);
		SaveOrder();
	}
	else
	{
		dms_assert(dst);

		g->ClearContext();
		g->ClearOwner();
		g->SetOwner(dst);
		dst->InsertEntryAt(g, newPos);
		m_Array.erase(m_Array.begin() + curPos);
	}
	ProcessCollectionChange();
}

template <typename ElemType>
void GraphicContainer<ElemType>::SaveOrder()
{
	TreeItem* viewContext = this->GetContext();
	if (viewContext)
	{
		typedef std::vector< TreeItem* > TreeItemPtrArray;
		TreeItemPtrArray orderBuffer;
		orderBuffer.reserve(viewContext->CountNrSubItems() );

		TreeItem* item = viewContext->_GetFirstSubItem();
		while (item)
		{
			orderBuffer.push_back(item);
			item = item->GetNextItem();
		}
		TreeItemPtrArray::iterator
			oi = orderBuffer.begin(),
			oe = orderBuffer.end();

		// reorder based on order of occurence of Context in m_Array
		for (auto ai = m_Array.begin(), ae = m_Array.end(); ai != ae; ++ai, ++oi)
		{
			const TreeItem* subItemContext = (*ai)->GetContext();
			dms_assert(subItemContext);
			dms_assert(oi != oe);
			if (*oi != subItemContext)
			{
				TreeItemPtrArray::iterator oj = std::find(oi+1, oe, subItemContext);
				dms_assert(oj != oe); // must be found
				omni::swap(*oi, *oj);
			}
			dms_assert(*oi == subItemContext);
		}
			
		viewContext->Reorder(begin_ptr( orderBuffer ), end_ptr( orderBuffer ) );
	}
	OnChildSizeChanged();
}

template <typename ElemType>
void GraphicContainer<ElemType>::OnChildSizeChanged()
{
	base_type::OnChildSizeChanged();
	m_cmdElemSetChanged();
}

template <typename ElemType>
void GraphicContainer<ElemType>::DoUpdateView()
{
	dbg_assert(!SuspendTrigger::DidSuspend());
	base_type::DoUpdateView();
		
	gr_elem_index n = NrEntries();
	while (n)
	{
		ElemType* oth = GetEntry(--n);
		dbg_assert(!SuspendTrigger::DidSuspend());
		oth->UpdateView();
	}
}

#include "MovableObject.h"
#include "ScalableObject.h"

template GraphicContainer<MovableObject>;
template GraphicContainer<ScalableObject>;