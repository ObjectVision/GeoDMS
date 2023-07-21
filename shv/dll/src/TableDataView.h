// Copyright (C) 2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#pragma once

#if !defined(__SHV_TABLEDATAVIEW_H)
#define __SHV_TABLEDATAVIEW_H

//----------------------------------------------------------------------
// used modules and forward class references
//----------------------------------------------------------------------

#include "DataView.h"
class TableViewControl;

//----------------------------------------------------------------------
// class  : TableDataView
//----------------------------------------------------------------------

class TableDataView : public DataView
{
	typedef DataView base_type;
public:
	TableDataView(TreeItem* viewContext, ShvSyncMode sm);

	auto GetViewType() const -> ViewStyle override { return ViewStyle::tvsTableView; }

	      TableViewControl* GetTableViewControl();
	const TableViewControl* GetTableViewControl() const;
	      TableControl*     GetTableControl();
	const TableControl*     GetTableControl() const;

protected:
	bool CanContain(const TreeItem* viewCandidate) const override;
	void AddLayer(const TreeItem*, bool isDropped) override;
	ExportInfo GetExportInfo() override;

	DECL_RTTI(SHV_CALL, Class);
};

#endif // !defined(__SHV_TABLEDATAVIEW_H)
