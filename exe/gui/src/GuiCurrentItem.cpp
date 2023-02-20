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

            if (!iterator->tiContext)
                break;

            const bool is_selected = (iterator == state.TreeItemHistoryList.GetCurrentIterator());
            if (ImGui::Selectable((iterator->tiContext)->GetFullName().c_str(), is_selected))
            {
                state.SetCurrentItem(iterator->tiContext);
                //if (iterator->sAction.contains("dp.vi.attr"))
                //    event_queues->DetailPagesEvents.Add(GuiEvents::FocusValueInfoTab);
                //else
                    event_queues->DetailPagesEvents.Add(GuiEvents::UpdateCurrentItem);
                event_queues->TreeViewEvents.Add(GuiEvents::JumpToCurrentItem);
                event_queues->MainEvents.Add(GuiEvents::UpdateCurrentItem);
                event_queues->CurrentItemBarEvents.Add(GuiEvents::UpdateCurrentItem);
                
            }
        }
        ImGui::EndCombo();
    }
}

void GuiCurrentItem::SetCurrentItemFullNameDirectly(const std::string fullname)
{
    m_current_item_fullname = fullname;
}

void GuiCurrentItem::Update(GuiState &state)
{
    auto event_queues = GuiEventQueues::getInstance();
    bool set_current_item_directly = false;
    if (ImGui::BeginMenuBar())
    {
        if (event_queues->CurrentItemBarEvents.HasEvents()) // new current item
        {
            auto event = event_queues->CurrentItemBarEvents.Pop();
            switch (event)
            {
            case GuiEvents::UpdateCurrentAndCompatibleSubItems:
            case GuiEvents::UpdateCurrentItem:
            {
                auto current_item = state.GetCurrentItem();
                m_current_item_fullname.resize(current_item ? std::strlen(current_item->GetFullName().c_str()) : 0);
                m_current_item_fullname = current_item ? current_item->GetFullName().c_str() : "";
                break;
            }
            case GuiEvents::UpdateCurrentItemDirectly:
            {
                set_current_item_directly = true;
                break;
            }
            }
        }

        DrawHistoryTreeItemDropdownList(state);
        
        if (ImGui::ArrowButton("previous", ImGuiDir_Left))
        {

            auto item = state.TreeItemHistoryList.GetPrevious().tiContext;
            if (item)
                state.SetCurrentItem(item);
        }

        if (ImGui::ArrowButton("next", ImGuiDir_Right))
        {
            auto item = state.TreeItemHistoryList.GetNext().tiContext;
            if (item)
                state.SetCurrentItem(item);
        }

        ImGui::SetNextItemWidth(ImGui::GetWindowWidth());

        if (ImGui::InputText("##CurrentItem", &m_current_item_fullname, ImGuiInputTextFlags_EnterReturnsTrue, InputTextCallback, nullptr) || set_current_item_directly)
        {
            if (state.GetRoot())
            {
                auto best_item_ref = TreeItem_GetBestItemAndUnfoundPart(state.GetRoot(), m_current_item_fullname.c_str());
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