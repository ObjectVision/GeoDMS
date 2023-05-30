#pragma once
#include "GuiBase.h"

class GuiCurrentItem
{
public:
	void Update(GuiState& state);
	void SetCurrentItemFullNameDirectly(const std::string fullname);

private:
	auto DrawHistoryTreeItemDropdownList(GuiState &state) -> void;
	std::string m_current_item_fullname;
};