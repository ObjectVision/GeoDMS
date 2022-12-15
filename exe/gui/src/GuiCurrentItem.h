#pragma once
#include "GuiBase.h"

class GuiCurrentItemComponent : GuiBaseComponent
{
public:
	GuiCurrentItemComponent();
	auto Update(GuiState& state) -> void;

private:
	auto DrawHistoryTreeItemDropdownList(GuiState &state) -> void;
	std::vector<char>		  m_Buf;
};