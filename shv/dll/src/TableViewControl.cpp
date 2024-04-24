// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "ShvDllPch.h"

#include "TableViewControl.h"
#include "TextControl.h"

#include "dbg/SeverityType.h"
#include "mci/Class.h"
#include "utl/mySPrintF.h"

#include "AbstrUnit.h"

#include "AbstrCmd.h"
#include "DataView.h"
#include "MenuData.h"
#include "MouseEventDispatcher.h"
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
		m_TableControl->SetSepSize(1);
		m_TableHeader = std::make_shared<TableHeaderControl>(m_TableHeaderPort.get(), m_TableControl.get());
		m_TableHeaderPort->SetContents(m_TableHeader);

		m_connOnCaptionChange = ScopedConnection(m_TableControl->m_cmdOnCaptionChange.connect([this]() { this->m_TableHeader->InvalidateView(); }));
	}
}

TableViewControl::~TableViewControl()
{
	SetTableControl(nullptr);
}

void TableViewControl::ProcessSize(CrdPoint newSize) 
{
	bool isColOriented = (m_TableControl) ? m_TableControl->IsColOriented(): true;
	TType  headerHeight = (m_TableHeaderPort) ? m_TableHeaderPort->GetCurrClientSize().FlippableY(isColOriented) : 0; 
		MakeMax(headerHeight, isColOriented ? TType(DEF_TEXT_PIX_HEIGHT + 2*BORDERSIZE): DEF_TEXT_PIX_HEIGHT);
		MakeMin(headerHeight, newSize.FlippableY(isColOriented));

	m_TableHeaderPort->SetClientRect( CrdRect( Point<CrdType>(0, 0), prj2dms_order<CrdType>(newSize.FlippableX(isColOriented), headerHeight, isColOriented)) );
	m_TableScrollPort->SetClientRect(CrdRect( prj2dms_order<CrdType>(0, headerHeight, isColOriented), newSize) );
	assert(IsIncluding(GetCurrFullAbsLogicalRect(),  m_TableHeaderPort->GetCurrFullAbsLogicalRect()));
	assert(IsIncluding(GetCurrFullAbsLogicalRect(),  m_TableScrollPort->GetCurrFullAbsLogicalRect()));

	if (m_TableControl)
		m_TableControl->ShowActiveCell();
}

void TableViewControl::FillMenu(MouseEventDispatcher& med)
{
	med.m_MenuData.push_back(
		MenuItem(
			SharedStr("Toggle Rows and Cols of TableView")
			, make_MembFuncCmd(&TableViewControl::ToggleTableOrientation)
			, this
		)
	);

	base_type::FillMenu(med);
}

void TableViewControl::Sync(TreeItem* context, ShvSyncMode sm)
{
	// Don't call GraphicContainer::Sync since that would want to create the ScrollPort, which is already done and default creation is not supported

	dms_assert(m_TableScrollPort);
	m_TableScrollPort->Sync(context, sm);
}

bool TableViewControl::OnCommand(ToolButtonID id)
{
	if (id == ToolButtonID::TB_ToggleTableOrientation)
	{
		ToggleTableOrientation();
		return true;
	}
	return base_type::OnCommand(id) || m_TableControl->OnCommand(id);
}

void TableViewControl::ToggleTableOrientation()
{
	m_TableControl->ToggleOrientation();
	m_TableHeader->ToggleOrientation();
	ProcessSize(GetCurrClientSize());
}

CommandStatus TableViewControl::OnCommandEnable(ToolButtonID id) const
{
	return GetTableControl()->OnCommandEnable(id);
}

bool TableViewControl::CanContain(const AbstrDataItem* adi) const
{
	return GetTableControl()->CanContain(adi);
}

CrdPoint TableViewControl::CalcMaxSize() const
{
	return ConcatVertical(
		m_TableHeaderPort->CalcMaxSize()
	,	m_TableScrollPort->CalcMaxSize()
	)	+	Size(GetBorderLogicalExtents())
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
