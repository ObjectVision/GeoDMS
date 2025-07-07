// Copyright (C) 1998-2025 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "ShvDllPch.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

#include "ViewControl.h"
#include "DataView.h"

#include "mci/Class.h"

#include "KeyFlags.h"

//----------------------------------------------------------------------
// class  : ViewControl
//----------------------------------------------------------------------

ViewControl::ViewControl(DataView* dv)
	:	base_type(0)
	,	m_DataView( dv->weak_from_this() )
{
};

void ViewControl::Init(DataView* dv)
{}

std::weak_ptr<DataView> ViewControl::GetDataView() const
{
	return m_DataView;
}

bool ViewControl::OnCommand(ToolButtonID id)
{
	switch (id)
	{
	case TB_Copy: {
		auto dv = GetDataView().lock();
		if (dv)
			CopyToClipboard(dv.get());
		return true;
	}
	}
	return base_type::OnCommand(id);
}

bool ViewControl::OnKeyDown(UInt32 virtKey)
{
	if ( KeyInfo::IsCtrl(virtKey) )
	{
		switch (KeyInfo::CharOf(virtKey)) {
			case 'C': return OnCommand(TB_Copy);
		}
	}
	return base_type::OnKeyDown(virtKey);
}

void ViewControl::SetClientSize(CrdPoint newSize)
{
	ProcessSize(LowerBound( newSize, GetCurrClientSize()));
	base_type::SetClientSize(newSize);
	ProcessSize(newSize);
}

IMPL_ABSTR_CLASS(ViewControl)
