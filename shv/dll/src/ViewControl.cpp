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
