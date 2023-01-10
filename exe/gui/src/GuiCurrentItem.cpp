#include "GuiCurrentItem.h"
#include <imgui.h>
#include <vector>
#include <numeric>
#include <iterator>

#include "TicInterface.h"
#include "ser/AsString.h"

auto GuiCurrentItem::DrawHistoryTreeItemDropdownList(GuiState& state) -> void
{
    auto event_queues = GuiEventQueues::getInstance();
    if (ImGui::BeginCombo("##treeitem_history_list", NULL, ImGuiComboFlags_NoPreview))
    {
        auto iterator = state.TreeItemHistoryList.GetEndIterator();
        while (iterator != state.TreeItemHistoryList.GetBeginIterator())
        {
            std::advance(iterator, -1);

            if (!*iterator)
                break;

            const bool is_selected = (iterator == state.TreeItemHistoryList.GetCurrentIterator());
            if (ImGui::Selectable((*iterator)->GetFullName().c_str(), is_selected))
            {
                state.SetCurrentItem(*iterator);
                event_queues->TreeViewEvents.Add(GuiEvents::JumpToCurrentItem);
                event_queues->MainEvents.Add(GuiEvents::UpdateCurrentItem);
                event_queues->CurrentItemBarEvents.Add(GuiEvents::UpdateCurrentItem);
                event_queues->DetailPagesEvents.Add(GuiEvents::UpdateCurrentItem);
            }
        }
        ImGui::EndCombo();
    }
}

void GuiCurrentItem::Update(GuiState &state)
{
    auto event_queues = GuiEventQueues::getInstance();
    if (ImGui::BeginMenuBar())
    {
        if (event_queues->CurrentItemBarEvents.HasEvents()) // new current item
        {
            m_string_buf.resize(state.GetCurrentItem() ? std::strlen(state.GetCurrentItem()->GetFullName().c_str()) : 0);
            m_string_buf = state.GetCurrentItem() ? state.GetCurrentItem()->GetFullName().c_str() : "";
        }

        DrawHistoryTreeItemDropdownList(state);
        
        if (ImGui::ArrowButton("previous", ImGuiDir_Left))
        {

            auto item = state.TreeItemHistoryList.GetPrevious();
            if (item)
                state.SetCurrentItem(item);
        }

        if (ImGui::ArrowButton("next", ImGuiDir_Right))
        {
            auto item = state.TreeItemHistoryList.GetNext();
            if (item)
                state.SetCurrentItem(item);
        }

        ImGui::SetNextItemWidth(ImGui::GetWindowWidth());

        if (ImGui::InputText("##CurrentItem", &m_string_buf, ImGuiInputTextFlags_EnterReturnsTrue, InputTextCallback, nullptr))
        {
            if (state.GetRoot())
            {
                auto best_item_ref = TreeItem_GetBestItemAndUnfoundPart(state.GetRoot(), m_string_buf.c_str());
                auto jump_item = best_item_ref.first;
                if (jump_item)
                {
                    state.SetCurrentItem(const_cast<TreeItem*>(jump_item));
                    event_queues->TreeViewEvents.Add(GuiEvents::JumpToCurrentItem);
                    event_queues->MainEvents.Add(GuiEvents::UpdateCurrentItem);
                    event_queues->DetailPagesEvents.Add(GuiEvents::UpdateCurrentItem);
                }
            }
        }

        if (ImGui::IsItemClicked())
            SetKeyboardFocusToThisHwnd();

        ImGui::EndMenuBar();
    }
}