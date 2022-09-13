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
// SheetVisualTestView.h : interface of the DataView class
//
/////////////////////////////////////////////////////////////////////////////

#if !defined(__SHV_TABLEVIEWCONTROL_H)
#define __SHV_TABLEVIEWCONTROL_H

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

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
