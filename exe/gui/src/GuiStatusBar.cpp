#include <imgui.h>
#include "GuiStatusBar.h"

GuiStatusBar::GuiStatusBar()
{
    
}

void GuiStatusBar::GeoDMSContextMessage(ClientHandle clientHandle, CharPtr msg)
{
    static GuiState state; // TODO: remove static
    state.contextMessage.Set(msg);
    PostEmptyEventToGLFWContext();
    return;
}

void GuiStatusBar::ParseNewContextMessage(std::string msg)
{
    // TODO: implement
}

void GuiStatusBar::Update(bool* p_open, GuiState& state) // TODO: add int return to button which is its group. Untoggle all buttons in the same group.
{
    if (!ImGui::Begin("StatusBar", p_open, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoTitleBar))
    {
        ImGui::End();
        return;
    }
   
    // focus window when clicked
    if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        SetKeyboardFocusToThisHwnd();

    //if (m_State.contextMessage.HasNew())
    //    ParseNewContextMessage(m_State.contextMessage.Get());

    ImGui::Text(state.contextMessage.Get().c_str());

    ImGui::End();
}