// Copyright (C) 1998-2024 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "ShvDllPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

#include "TableViewControl.h"
#include "TextControl.h"

#include "dbg/SeverityType.h"
#include "mci/Class.h"
#include "utl/StrFormat.h"

#include "AbstrUnit.h"

#include "AbstrCmd.h"
#include "DataItemColumn.h"
#include "DataView.h"
#include "MenuData.h"
#include "MouseEventDispatcher.h"
#include "ScrollPort.h"
#include "ShvUtils.h"
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
	// the header strip size follows m_TableHeader's max size, which the user can
	// adjust by dragging the strip's edge in row-oriented mode (issue #1150)
	TType  headerHeight = m_TableHeader
		? m_TableHeader->GetMaxSize()
		: TType(isColOriented ? GetDefaultRowHeightDIP(GetFontSizeCategory()) : DEF_TEXT_PIX_WIDTH) + DOUBLE_BORDERSIZE;
	MakeMin(headerHeight, newSize.FlippableY(isColOriented));

	m_TableScrollPort->SetClientRect(CrdRect( prj2dms_order<CrdType>(0, headerHeight, isColOriented), newSize) );
	assert(IsIncluding(GetCurrFullAbsLogicalRect(),  m_TableHeaderPort->GetCurrFullAbsLogicalRect()));
	assert(IsIncluding(GetCurrFullAbsLogicalRect(),  m_TableScrollPort->GetCurrFullAbsLogicalRect()));

	if (m_TableControl)
		m_TableControl->ShowActiveCell();

	OnTableScrolled();
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
	bool aggrValuesMustBeDefined = m_TableControl->m_State.Get(TCF_MustBeDefined);
	med.m_MenuData.push_back(
		MenuItem(
			SharedStr(aggrValuesMustBeDefined ? "Also group by null values" : "Stop group by null values")
			, make_MembFuncCmd(&TableViewControl::ToggleGroupByNullValues)
			, this
			, aggrValuesMustBeDefined ? MF_UNCHECKED : MF_CHECKED
		)
	);

	base_type::FillMenu(med);
}

void TableViewControl::Sync(TreeItem* context, ShvSyncMode sm)
{
	// Don't call GraphicContainer::Sync since that would want to create the ScrollPort, which is already done and default creation is not supported

	assert(m_TableScrollPort);
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
	m_TableHeader->SetMaxSize((m_TableHeader->IsColOriented() ? GetDefaultRowHeightDIP(GetFontSizeCategory()) : DEF_TEXT_PIX_WIDTH) + DOUBLE_BORDERSIZE);

	// the element axis shared by all attributes must be uniform, or the records
	// misalign: the row height when col-oriented, the record-column width when
	// row-oriented; per-attribute resizes of the other axis (issue #1150) may
	// have diversified it, so unify to the maximum
	bool isColOriented = m_TableControl->IsColOriented();
	UInt16 sharedSize = 0;
	gr_elem_index n = m_TableControl->NrEntries();
	for (gr_elem_index i = 0; i != n; ++i)
		if (auto* col = m_TableControl->GetColumn(i))
			MakeMax(sharedSize, isColOriented ? col->ElemSize().Y() : col->ElemSize().X());
	if (sharedSize)
		for (gr_elem_index i = 0; i != n; ++i)
			if (auto* col = m_TableControl->GetColumn(i))
				col->SetElemSize(isColOriented
					? WPoint(col->ElemSize().X(), sharedSize)
					: WPoint(sharedSize, col->ElemSize().Y()));

	ProcessSize(GetCurrClientSize());
}

void TableViewControl::SetHeaderStripSize(TType stripSize)
{
	if (!m_TableHeader)
		return;
	MakeMax(stripSize, TType(MIN_COL_ELEM_WIDTH) + DOUBLE_BORDERSIZE);
	if (stripSize == m_TableHeader->GetMaxSize())
		return;

	m_TableHeader->SetMaxSize(stripSize);
	m_TableHeader->InvalidateView(); // entry sizes follow GetMaxSize in TableHeaderControl::DoUpdateView
	ProcessSize(GetCurrClientSize());
	InvalidateDraw();
}

void TableViewControl::ToggleGroupByNullValues()
{
	bool aggrGroupDefinedValues = m_TableControl->m_State.Get(TCF_MustBeDefined);
	m_TableControl->m_State.Set(TCF_MustBeDefined, !aggrGroupDefinedValues);
	if (m_TableControl->m_GroupByEntity)
		m_TableControl->CreateTableGroupBy(true);
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
	m_TableScrollPort->CalcNettSize();

	bool isColOriented = m_TableControl->IsColOriented();

	TType  headerHeight = m_TableScrollPort->GetCurrClientRelPos().FlippableY(isColOriented);
	TType headerWidth = m_TableScrollPort->GetCurrNettLogicalSize().FlippableX(isColOriented);
	auto headerSize = prj2dms_order<TType>(headerWidth, headerHeight, isColOriented);

	m_TableHeaderPort->SetClientRect(CrdRect(CrdPoint(0,0), headerSize));

	auto scrollPos = prj2dms_order<TType>(m_TableControl->GetCurrClientRelPos().FlippableX(isColOriented), 0, isColOriented);
	m_TableHeaderPort->ScrollLogicalTo(scrollPos);
}

IMPL_RTTI_CLASS(TableViewControl)
