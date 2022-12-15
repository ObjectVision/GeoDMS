#pragma once
#include "GuiBase.h"

namespace ImGui
{
    // ImGui::InputText() with std::string
    // Because text input needs dynamic resizing, we need to setup a callback to grow the capacity
    IMGUI_API bool InputText(const char* label, std::string* str, ImGuiInputTextFlags flags = 0, ImGuiInputTextCallback callback = NULL, void* user_data = NULL);
}

class GuiCurrentItemComponent : GuiBaseComponent
{
public:
	GuiCurrentItemComponent();
	auto Update(GuiState& state) -> void;

private:
	auto DrawHistoryTreeItemDropdownList(GuiState &state) -> void;
	std::vector<char>		  m_Buf;
	std::string m_string_buf;
};