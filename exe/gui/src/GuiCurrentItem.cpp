#include <imgui.h>
#include "GuiCurrentItem.h"
#include <vector>
#include <numeric>
#include <iterator>

#include "TicInterface.h"
#include "ser/AsString.h"

GuiCurrentItemComponent::GuiCurrentItemComponent()
{
    m_Buf.resize(1024); // TODO: remove magic number
}

auto GuiCurrentItemComponent::DrawHistoryTreeItemDropdownList(GuiState& state) -> void
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

struct InputTextCallback_UserData
{
    std::string* Str;
    ImGuiInputTextCallback  ChainCallback;
    void* ChainCallbackUserData;
};

static int InputTextCallback(ImGuiInputTextCallbackData* data)
{
    InputTextCallback_UserData* user_data = (InputTextCallback_UserData*)data->UserData;
    if (data->EventFlag == ImGuiInputTextFlags_CallbackResize)
    {
        // Resize string callback
        // If for some reason we refuse the new length (BufTextLen) and/or capacity (BufSize) we need to set them back to what we want.
        std::string* str = user_data->Str;
        IM_ASSERT(data->Buf == str->c_str());
        str->resize(data->BufTextLen);
        data->Buf = (char*)str->c_str();
    }
    else if (user_data->ChainCallback)
    {
        // Forward to user callback, if any
        data->UserData = user_data->ChainCallbackUserData;
        return user_data->ChainCallback(data);
    }
    return 0;
}

bool ImGui::InputText(const char* label, std::string* str, ImGuiInputTextFlags flags, ImGuiInputTextCallback callback, void* user_data)
{
    IM_ASSERT((flags & ImGuiInputTextFlags_CallbackResize) == 0);
    flags |= ImGuiInputTextFlags_CallbackResize;

    InputTextCallback_UserData cb_user_data;
    cb_user_data.Str = str;
    cb_user_data.ChainCallback = callback;
    cb_user_data.ChainCallbackUserData = user_data;
    return InputText(label, (char*)str->c_str(), str->capacity() + 1, flags, InputTextCallback, &cb_user_data);
}

void GuiCurrentItemComponent::Update(GuiState &state)
{
    auto event_queues = GuiEventQueues::getInstance();
    if (ImGui::BeginMenuBar())
    {
        if (event_queues->CurrentItemBarEvents.HasEvents()) // new current item
        {
            //event_queues->CurrentItemBarEvents.Pop();
            //m_Buf.assign(m_Buf.size(), char());
            //auto tmpPath = state.GetCurrentItem() ? state.GetCurrentItem()->GetFullName() : SharedStr("");
            //std::copy(tmpPath.begin(), tmpPath.end(), m_Buf.begin());
            
            m_string_buf.resize(state.GetCurrentItem() ? std::strlen(state.GetCurrentItem()->GetFullName().c_str()) : 0);
            m_string_buf = state.GetCurrentItem()->GetFullName().c_str();
        }

        DrawHistoryTreeItemDropdownList(state);

        ImGui::SetNextItemWidth(ImGui::GetWindowWidth());


        //ImGui::InputText(const char* label, std::string * str, ImGuiInputTextFlags flags, ImGuiInputTextCallback callback, void* user_data)
        //if (ImGui::InputText("##CurrentItem", reinterpret_cast<char*> (&m_Buf[0]), m_Buf.size(), ImGuiInputTextFlags_EnterReturnsTrue))
        if (ImGui::InputText("##CurrentItem", &m_string_buf, ImGuiInputTextFlags_EnterReturnsTrue, InputTextCallback, nullptr))
        {
            if (state.GetRoot())
            {
                //TreeItem* jumpItem = (TreeItem*)DMS_TreeItem_GetBestItemAndUnfoundPart(state.GetRoot(), m_string_buf.c_str(), &unfound_part); // reinterpret_cast<char*> (&m_Buf[0])
                
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