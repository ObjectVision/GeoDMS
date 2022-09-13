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

#include "ShvDllPch.h"

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
		contents= context->CreateItem(s_ContentsTokenID);
	m_Contents->Sync(const_cast<TreeItem*>(contents), sm);
}

#if defined(MG_DEBUG)

bool Wrapper::DataViewOK() const 
{
	auto dv = GetDataView().lock();
	return dv && dv->GetHWnd(); 
}

#endif

IMPL_ABSTR_CLASS(Wrapper)

