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
