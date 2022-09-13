#include <imgui.h>
#include "GuiCurrentItem.h"
#include <vector>
#include <numeric>

GuiCurrentItemComponent::GuiCurrentItemComponent()
{
    m_Buf.resize(1024);
}

void GuiCurrentItemComponent::Update()
{
    // TODO: add GetClipboardText();
    ImGui::SetNextItemWidth(ImGui::GetWindowWidth());
    if (ImGui::BeginMenuBar())
    {
        if (m_State.CurrentItemBarEvents.HasEvents()) // new current item
        {
            m_State.CurrentItemBarEvents.Pop();
            m_Buf.assign(m_Buf.size(), 0);
            auto tmpPath = m_State.GetCurrentItem() ? m_State.GetCurrentItem()->GetFullName() : SharedStr("");
            std::copy(tmpPath.begin(), tmpPath.end(), m_Buf.begin());
        }

        if (ImGui::InputText("", reinterpret_cast<char*> (&m_Buf[0]), m_Buf.size(), ImGuiInputTextFlags_EnterReturnsTrue))
        {
            auto jumpItem = SetJumpItemFullNameToOpenInTreeView(m_State.GetRoot(), DivideTreeItemFullNameIntoTreeItemNames(reinterpret_cast<char*> (&m_Buf[0])));
            if (jumpItem)
            {
                m_State.SetCurrentItem(jumpItem);
                //currentItem = jumpItem;
                m_State.TreeViewEvents.Add(GuiEvents::JumpToCurrentItem);
            }
        }

        ImGui::EndMenuBar();
    }
}