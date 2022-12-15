#pragma once
#include "GuiBase.h"

class GuiCurrentItemComponent : GuiBaseComponent
{
public:
	auto Update(GuiState& state) -> void;

private:
	auto DrawHistoryTreeItemDropdownList(GuiState &state) -> void;
	std::string m_string_buf;
};