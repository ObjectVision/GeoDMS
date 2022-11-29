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

int TextCallBacka(ImGuiInputTextCallbackData* data)
{
    int i = 0;
    /*switch (data->EventFlag)
    {
    case ImGuiInputTextFlags_CallbackAlways:
    {
        int i = 0;
        break;
    }
    }*/
    return 1;
}

void GuiCurrentItemComponent::Update(GuiState &state)
{
    auto event_queues = GuiEventQueues::getInstance();
    if (ImGui::BeginMenuBar())
    {
        if (event_queues->CurrentItemBarEvents.HasEvents()) // new current item
        {
            event_queues->CurrentItemBarEvents.Pop();
            m_Buf.assign(m_Buf.size(), char());
            auto tmpPath = state.GetCurrentItem() ? state.GetCurrentItem()->GetFullName() : SharedStr("");
            std::copy(tmpPath.begin(), tmpPath.end(), m_Buf.begin());
        }

        static ImGuiComboFlags flags = ImGuiComboFlags_None | ImGuiComboFlags_NoPreview;

        const char* items[] = { "AAAA", "BBBB", "CCCC", "DDDD", "EEEE", "FFFF", "GGGG", "HHHH", "IIII", "JJJJ", "KKKK", "LLLLLLL", "MMMM", "OOOOOOO" };
        static int item_current_idx = 0; // Here we store our selection data as an index.
        const char* combo_preview_value = items[item_current_idx];  // Pass in the preview value visible before opening the combo (it could be anything)
        if (ImGui::BeginCombo("##combo 1", combo_preview_value, flags))
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

        ImGui::SetNextItemWidth(ImGui::GetWindowWidth());
        if (ImGui::InputText("##CurrentItem", reinterpret_cast<char*> (&m_Buf[0]), m_Buf.size(), ImGuiInputTextFlags_EnterReturnsTrue))
        {
            if (state.GetRoot())
            {
                auto unfound_part = IString::Create("");
                TreeItem* jumpItem = (TreeItem*)DMS_TreeItem_GetBestItemAndUnfoundPart(state.GetRoot(), reinterpret_cast<char*> (&m_Buf[0]), &unfound_part);
                
                //auto jumpItem = SetJumpItemFullNameToOpenInTreeView(m_State.GetRoot(), DivideTreeItemFullNameIntoTreeItemNames(reinterpret_cast<char*> (&m_Buf[0])));
                if (jumpItem)
                {
                    state.SetCurrentItem(jumpItem);
                    event_queues->TreeViewEvents.Add(GuiEvents::JumpToCurrentItem);
                    event_queues->MainEvents.Add(GuiEvents::UpdateCurrentItem);
                    event_queues->DetailPagesEvents.Add(GuiEvents::UpdateCurrentItem);
                }
                if (!unfound_part->empty())
                {
                    // TODO: do something with unfound part
                }

                unfound_part->Release(unfound_part);
            }
        }

        if (ImGui::IsItemClicked())
            SetKeyboardFocusToThisHwnd();

        ImGui::EndMenuBar();
    }
}

/*int GuiEventLog::TextEditCallback(ImGuiInputTextCallbackData* data)
{
    //AddLog("cursor: %d, selection: %d-%d", data->CursorPos, data->SelectionStart, data->SelectionEnd);
    switch (data->EventFlag)
    {
    case ImGuiInputTextFlags_CallbackCompletion:
    {
        // Example of TEXT COMPLETION

        // Locate beginning of current word
        const char* word_end = data->Buf + data->CursorPos;
        const char* word_start = word_end;
        while (word_start > data->Buf)
        {
            const char c = word_start[-1];
            if (c == ' ' || c == '\t' || c == ',' || c == ';')
                break;
            word_start--;
        }

        // Build a list of candidates
        ImVector<const char*> candidates;
        for (int i = 0; i < Commands.Size; i++)
            if (Strnicmp(Commands[i], word_start, (int)(word_end - word_start)) == 0)
                candidates.push_back(Commands[i]);

        if (candidates.Size == 0)
        {
            // No match
            AddLog(SeverityTypeID::ST_Error, "No match");// \"%.*s\"!\n", (int)(word_end - word_start), word_start);
        }
        else if (candidates.Size == 1)
        {
            // Single match. Delete the beginning of the word and replace it entirely so we've got nice casing.
            data->DeleteChars((int)(word_start - data->Buf), (int)(word_end - word_start));
            data->InsertChars(data->CursorPos, candidates[0]);
            data->InsertChars(data->CursorPos, " ");
        }
        else
        {
            // Multiple matches. Complete as much as we can..
            // So inputing "C"+Tab will complete to "CL" then display "CLEAR" and "CLASSIFY" as matches.
            int match_len = (int)(word_end - word_start);
            for (;;)
            {
                int c = 0;
                bool all_candidates_matches = true;
                for (int i = 0; i < candidates.Size && all_candidates_matches; i++)
                    if (i == 0)
                        c = toupper(candidates[i][match_len]);
                    else if (c == 0 || c != toupper(candidates[i][match_len]))
                        all_candidates_matches = false;
                if (!all_candidates_matches)
                    break;
                match_len++;
            }

            if (match_len > 0)
            {
                data->DeleteChars((int)(word_start - data->Buf), (int)(word_end - word_start));
                data->InsertChars(data->CursorPos, candidates[0], candidates[0] + match_len);
            }

            // List matches
            AddLog("Possible matches:\n");
            for (int i = 0; i < candidates.Size; i++)
                AddLog("- %s\n", candidates[i]);
        }

        break;
    }
    case ImGuiInputTextFlags_CallbackHistory:
    {
        // Example of HISTORY
        const int prev_history_pos = HistoryPos;
        if (data->EventKey == ImGuiKey_UpArrow)
        {
            if (HistoryPos == -1)
                HistoryPos = History.Size - 1;
            else if (HistoryPos > 0)
                HistoryPos--;
        }
        else if (data->EventKey == ImGuiKey_DownArrow)
        {
            if (HistoryPos != -1)
                if (++HistoryPos >= History.Size)
                    HistoryPos = -1;
        }

        // A better implementation would preserve the data on the current input line along with cursor position.
        if (prev_history_pos != HistoryPos)
        {
            const char* history_str = (HistoryPos >= 0) ? History[HistoryPos] : "";
            data->DeleteChars(0, data->BufTextLen);
            data->InsertChars(0, history_str);
        }
    }
    }
    return 0;
};*/