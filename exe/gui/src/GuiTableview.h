#pragma once
#include "GuiBase.h"
#include "GuiView.h"

struct GuiTableCell
{
	std::string content;
	size_t index;
	bool invalidated = true;
};

using gui_table_row  = std::vector<GuiTableCell>;
using gui_table_data = std::vector<gui_table_row>;

class GuiTableView : public AbstractView
{
public:
	GuiTableView(GuiState& state, std::string name);
	~GuiTableView();
	// TODO: implement move constructor
	bool Update(GuiState& state) override;

private:
	SharedTreeItemInterestPtr m_ti_interest_holder = nullptr;
	SharedDataItemInterestPtr m_di_interest_holder = nullptr;
	SharedUnitInterestPtr     m_vu_interest_holder = nullptr;
	/*void ProcessEvent(GuiEvents event, TreeItem* currentItem);
	void PopulateDataStringBuffer(SizeT count);
	SizeT GetNumberOfTableColumns();
	void SetupTableHeadersRow();
	SizeT GetCountFromAnyCalculatedActiveItem();

	std::vector<std::vector<std::string>> m_DataStringBuffer;
	//GuiTreeItemsHolder m_ActiveItems;
	bool			   m_UpdateData = false;*/
};