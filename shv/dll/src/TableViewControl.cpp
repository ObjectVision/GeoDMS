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

#include "TableViewControl.h"
#include "TextControl.h"

#include "dbg/SeverityType.h"
#include "mci/Class.h"
#include "utl/mySPrintF.h"

#include "AbstrUnit.h"

#include "DataView.h"
#include "ScrollPort.h"
#include "TableHeaderControl.h"

//----------------------------------------------------------------------

TableViewControl::TableViewControl(DataView* dv)
	: ViewControl(dv)
{}

void TableViewControl::Init(DataView* dv, CharPtr caption)
{
	base_type::Init(dv);
	m_TableScrollPort = make_shared_gr<ScrollPort>(this, dv, caption, false)();
	m_TableHeaderPort = make_shared_gr<ScrollPort>(this, dv, "TableHeader", true)();

	m_connOnScrolled = ScopedConnection(m_TableScrollPort->m_cmdOnScrolled.connect([this]() { this->OnTableScrolled();}));

	InsertEntry(m_TableHeaderPort.get());
	InsertEntry(m_TableScrollPort.get());
}

void TableViewControl::SetTableControl(TableControl* tableControl)
{
	if (m_TableControl)
		m_TableControl->m_TableView = nullptr;

	m_TableControl = (tableControl)
		?	tableControl->shared_from_base<TableControl>()
		:	std::shared_ptr<TableControl>();

	if (m_TableControl)
	{
		m_TableControl->m_TableView = this;
		m_TableScrollPort->SetContents(m_TableControl);
		m_TableControl->SetColSepWidth(1);
		m_TableHeader = std::make_shared<TableHeaderControl>(m_TableHeaderPort.get(), m_TableControl.get());
		m_TableHeaderPort->SetContents(m_TableHeader);

		m_connOnCaptionChange = ScopedConnection(m_TableControl->m_cmdOnCaptionChange.connect([this]() { this->m_TableHeader->InvalidateView(); }));
	}
}

TableViewControl::~TableViewControl()
{
	SetTableControl(nullptr);
}

void TableViewControl::ProcessSize(TPoint newSize) 
{
	TType  headerHeight = (m_TableHeaderPort) ? m_TableHeaderPort->GetCurrClientSize().Y() : 0; 
		MakeMax(headerHeight, TType(DEF_TEXT_PIX_HEIGHT + 2*BORDERSIZE)); 
		MakeMin(headerHeight, newSize.Y());

	m_TableHeaderPort->SetClientRect( TRect( Point<TType>(0, 0), shp2dms_order<TType>(newSize.X(), headerHeight)) );
	m_TableScrollPort->SetClientRect( TRect( shp2dms_order<TType>(0, headerHeight), newSize) );
	assert(IsIncluding(GetCurrFullAbsLogicalRect(),  m_TableHeaderPort->GetCurrFullAbsLogicalRect()));
	assert(IsIncluding(GetCurrFullAbsLogicalRect(),  m_TableScrollPort->GetCurrFullAbsLogicalRect()));

	if (m_TableControl)
		m_TableControl->ShowActiveCell();
}

void TableViewControl::Sync(TreeItem* context, ShvSyncMode sm)
{
	// Don't call GraphicContainer::Sync since that would want to create the ScrollPort, which is already done and default creation is not supported

	dms_assert(m_TableScrollPort);
	m_TableScrollPort->Sync(context, sm);
}

bool TableViewControl::OnCommand(ToolButtonID id)
{
	return base_type::OnCommand(id) || m_TableControl->OnCommand(id);
}

CommandStatus TableViewControl::OnCommandEnable(ToolButtonID id) const
{
	return GetTableControl()->OnCommandEnable(id);
}

bool TableViewControl::CanContain(const AbstrDataItem* adi) const
{
	return GetTableControl()->CanContain(adi);
}

TPoint TableViewControl::CalcMaxSize() const
{
	return ConcatVertical(
		m_TableHeaderPort->CalcMaxSize(),
		m_TableScrollPort->CalcMaxSize()
	)	+	GetBorderLogicalExtents().Size()
	;
}

void TableViewControl::OnTableScrolled()
{
	auto clientSize = shp2dms_order<TType>(m_TableScrollPort->GetCurrNettLogicalSize().X(), m_TableHeaderPort->GetCurrClientSize().Y());
	m_TableHeaderPort->SetClientSize(clientSize);

	auto delta = shp2dms_order<TType>(m_TableControl->GetCurrClientRelPos().X(), 0);
	m_TableHeaderPort->ScrollLogicalTo( delta);
}

IMPL_RTTI_CLASS(TableViewControl)
