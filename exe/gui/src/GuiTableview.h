#pragma once
#include "GuiBase.h"
#include <vector>
#include <string>

class GuiTableView
{
public:
	void Update(bool* p_open, TreeItem*& currentItem);
	void ClearReferences();
private:
	void ProcessEvent(GuiEvents event, TreeItem* currentItem);
	void PopulateDataStringBuffer(SizeT count);
	SizeT GetNumberOfTableColumns();
	void SetupTableHeadersRow();
	SizeT GetCountFromAnyCalculatedActiveItem();

	std::vector<std::vector<std::string>> m_DataStringBuffer;
	GuiState           m_State;
	//GuiTreeItemsHolder m_ActiveItems;
	bool			   m_UpdateData = false;
};