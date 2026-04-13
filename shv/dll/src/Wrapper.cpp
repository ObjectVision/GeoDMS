// Copyright (C) 1998-2025 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "ShvDllPch.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

#include "Wrapper.h"

#include "dbg/DebugContext.h"
#include "mci/Class.h"

#include "TreeItem.h"

#include "ShvUtils.h"
#include "DataView.h"

//----------------------------------------------------------------------
// class  : Wrapper
//----------------------------------------------------------------------

Wrapper::Wrapper(MovableObject* owner, DataView* dv, CharPtr caption)
	:	base_type(owner)
	,	m_DataView(dv ? dv->shared_from_this() : nullptr)
	,	m_Caption(caption)
{
}

void Wrapper::SetContents(sharedPtrGO contents)
{
	m_Contents = contents;
}

void Wrapper::ClearContents()
{
	m_Contents = nullptr;
}

//	overrid GraphicObject interface for composition of GraphicObjects (composition pattern)
gr_elem_index Wrapper::NrEntries() const
{
	return 1; 
}

GraphicObject* Wrapper::GetEntry(SizeT i)       
{
	dms_assert(m_Contents);
	return m_Contents.get(); 
}

SharedStr Wrapper::GetCaption() const
{
	return m_Caption; 
}

bool Wrapper::OnCommand(ToolButtonID id)
{
	switch (id)
	{
		case TB_Export: 
			Export(); 
			return true;
	}
	return base_type::OnCommand(id);
}

static TokenID s_ContentsTokenID = GetTokenID_st("Contents");

void Wrapper::Sync(TreeItem* context, ShvSyncMode sm) 
{
	ObjectContextHandle contextHandle(context, "Wrapper::Sync");


	base_type::Sync(context, sm);
	const TreeItem* contents = FindTreeItemByID(context, s_ContentsTokenID);
	if (!contents)
		contents= context->CreateItem(s_ContentsTokenID).release();
	m_Contents->Sync(const_cast<TreeItem*>(contents), sm);
}

#if defined(MG_DEBUG)

bool Wrapper::DataViewOK() const 
{
	auto dv = GetDataView().lock();
	return dv && dv->GetViewHost();
}

#endif

IMPL_ABSTR_CLASS(Wrapper)

