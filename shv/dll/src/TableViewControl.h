// Copyright (C) 2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#pragma once

#if !defined(__SHV_TABLEVIEWCONTROL_H)
#define __SHV_TABLEVIEWCONTROL_H

//----------------------------------------------------------------------
// used modules and forward class references
//----------------------------------------------------------------------

#include "ViewControl.h"
#include "TableControl.h"
class Theme;
class TableDataView;

//----------------------------------------------------------------------

class TableViewControl : public ViewControl
{
	typedef ViewControl base_type;
public:
	TableViewControl(DataView* dv);
	~TableViewControl(); // hide destruction of SharedPtr
	void Init(DataView* dv, CharPtr caption);

	void SetTableControl(TableControl* tableControl);

protected: // override GraphicObject virtuals
	void ProcessSize(TPoint newSize) override;

public:
	void Sync(TreeItem* context, ShvSyncMode sm) override;

public:
	TPoint CalcMaxSize() const override;
	bool OnCommand(ToolButtonID id) override;
	CommandStatus OnCommandEnable(ToolButtonID id) const override;

	      TableControl* GetTableControl()       { return m_TableControl.get(); }
	const TableControl* GetTableControl() const { return m_TableControl.get(); }
	ScrollPort* GetTableScrollPort() { return m_TableScrollPort.get(); }
	bool CanContain(const AbstrDataItem* adi) const;
	SharedStr GetCaption() const override { return GetTableControl()->GetCaption(); }

private:
	void OnTableScrolled();

private:
	std::shared_ptr<TableControl>       m_TableControl;
	std::shared_ptr<ScrollPort>         m_TableScrollPort;
	std::shared_ptr<TableHeaderControl> m_TableHeader;
	std::shared_ptr<ScrollPort>         m_TableHeaderPort;

	ScopedConnection  m_connOnScrolled, m_connOnCaptionChange;

	DECL_RTTI(SHV_CALL, Class);
};

#include "ScrollPort.h"

struct DefaultTableViewControl : TableViewControl
{
	DefaultTableViewControl(DataView* tdv)
		: TableViewControl(tdv)
	{
	}
	void Init(DataView* tdv)
	{
		TableViewControl::Init(tdv, "TableView");
		SetBorder(true);
		SetTableControl(make_shared_gr<TableControl>(GetTableScrollPort())().get());
	}
};


#endif // !defined(__SHV_TABLEVIEWCONTROL_H)
