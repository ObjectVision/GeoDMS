#pragma once
#include "GuiBase.h"
#include "GuiView.h"

struct GuiTableCell
{
	std::string content;
	size_t index;
	bool invalidated = true;
};

using table_row  = std::vector<GuiTableCell>;
using table_data = std::vector<table_row>;

//using table_row

class GuiTableView : AbstractView
{
public:
	bool Update(GuiState& state) override;

private:
	/*void ProcessEvent(GuiEvents event, TreeItem* currentItem);
	void PopulateDataStringBuffer(SizeT count);
	SizeT GetNumberOfTableColumns();
	void SetupTableHeadersRow();
	SizeT GetCountFromAnyCalculatedActiveItem();

	std::vector<std::vector<std::string>> m_DataStringBuffer;
	//GuiTreeItemsHolder m_ActiveItems;
	bool			   m_UpdateData = false;*/
};