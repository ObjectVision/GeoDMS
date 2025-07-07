// Copyright (C) 1998-2025 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "ShvDllPch.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

#include "EditPalette.h"
#include "TableDataView.h"

#include "mci/Class.h"
#include "ScrollPort.h"
#include "TableViewControl.h"

//----------------------------------------------------------------------
// TableDataView Implementation
//----------------------------------------------------------------------

TableDataView::TableDataView(TreeItem* viewContext, ShvSyncMode sm)
:	DataView(viewContext)
{
}


TableViewControl* TableDataView::GetTableViewControl()       
{
	return debug_cast<TableViewControl*>(GetContents().get());
}

const TableViewControl* TableDataView::GetTableViewControl() const
{
	return debug_cast<const TableViewControl*>(GetContents().get());
}

ExportInfo TableDataView::GetExportInfo()
{
	return GetTableControl()->GetExportInfo();
}

TableControl*  TableDataView::GetTableControl()
{
	return GetTableViewControl()->GetTableControl();
}

const TableControl*  TableDataView::GetTableControl() const
{
	return GetTableViewControl()->GetTableControl();
}

bool TableDataView::CanContain(const TreeItem* viewCandidate) const
{
	return GetTableControl()->CanContain(viewCandidate);
}

void TableDataView::AddLayer(const TreeItem* viewItem, bool isDragging)
{
	dms_assert(viewItem);
	if (!CanContain(viewItem))
		viewItem->throwItemError("Cannot be represented in this view");
	if (GetTableControl()->NrEntries() == 0)
	{
		auto domain = SHV_DataContainer_GetDomain(viewItem, 1, HasAdminMode());
		auto firstItem = SHV_DataContainer_GetItem(viewItem, domain, 0, 1, HasAdminMode());
		GetTableControl()->AddIdColumn(domain, firstItem);
	}
	GetTableControl()->AddLayer(viewItem, isDragging);
}

IMPL_RTTI_CLASS(TableDataView);
