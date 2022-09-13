#include "GuiInput.h"
#include "GuiEventLog.h"
#include "dbg/SeverityType.h"
#include <string>

//ImVector<char*>       GuiEventLog::Items;

std::vector<std::pair<SeverityTypeID, std::string>> GuiEventLog::m_Items;
UInt32 GuiEventLog::m_MaxLogLines = 1000;

GuiEventLog::GuiEventLog()
{
    ClearLog();
    memset(InputBuf, 0, sizeof(InputBuf));
    //HistoryPos = -1;

    // "CLASSIFY" is here to provide the test case where "C"+[tab] completes to "CL" and display multiple matches.
    //Commands.push_back("HELP");
    //Commands.push_back("HISTORY");
    //Commands.push_back("CLEAR");
    //Commands.push_back("CLASSIFY");
    AutoScroll = true;
    ScrollToBottom = false;
};

GuiEventLog::~GuiEventLog()
{
    ClearLog();
    //for (int i = 0; i < History.Size; i++)
    //    free(History[i]);
};

void GuiEventLog::GeoDMSMessage(ClientHandle clientHandle, SeverityTypeID st, CharPtr msg)
{
    //if (st == SeverityTypeID::ST_Error || st == SeverityTypeID::ST_FatalError || st == SeverityTypeID::ST_DispError)
    //    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 0, 0, 255));
    AddLog(st, msg);//msg);
    //if (st == SeverityTypeID::ST_Error || st == SeverityTypeID::ST_FatalError || st == SeverityTypeID::ST_DispError)
    //    ImGui::PopStyleColor();
}

void GuiEventLog::GeoDMSExceptionMessage(CharPtr msg)
{
    // add popup on error or fatal error
    AddLog(SeverityTypeID::ST_Error, msg);
}

void GuiEventLog::Update(bool* p_open)
{
    ImGui::SetNextWindowSize(ImVec2(520, 600), ImGuiCond_FirstUseEver);// TODO: ???
    if (!ImGui::Begin("EventLog", p_open))
    {
        ImGui::End();
        return;
    }

    // window specific options button
    auto old_cpos = SetCursorPosToOptionsIconInWindowHeader();
    SetClipRectToIncludeOptionsIconInWindowHeader();
    ImGui::Text(ICON_RI_SETTINGS);
    if (MouseHooversOptionsIconInWindowHeader())
    {
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
            m_State.ShowEventLogOptionsWindow = true;
    }
    ImGui::SetCursorPos(old_cpos);
    ImGui::PopClipRect();

    // TODO: display items starting from the bottom
    if (ImGui::SmallButton("Test SeverityTypeID messages"))
    {
        AddLog(SeverityTypeID::ST_MinorTrace, "ST_MinorTrace: this is a minor trace message.");
        AddLog(SeverityTypeID::ST_MajorTrace, "ST_MajorTrace: this is a major trace message.");
        AddLog(SeverityTypeID::ST_Warning, "ST_Warning: this is a warning message.");
        AddLog(SeverityTypeID::ST_Error, "ST_Error: this is an error message.");
        AddLog(SeverityTypeID::ST_FatalError, "ST_FatalError: this is a fatal error message.");
        AddLog(SeverityTypeID::ST_DispError, "ST_DispError: this is a disp error message.");
        AddLog(SeverityTypeID::ST_Nothing, "ST_Nothing: this is a nothing message.");
    }


    //if (ImGui::SmallButton("Add Debug Text")) { AddLog("%d some text", Items.Size); AddLog("some more text"); AddLog("display very important message here!"); }
    //ImGui::SameLine();
    //if (ImGui::SmallButton("Add Debug Error")) { AddLog(SeverityTypeID::ST_Error, "[error] something went wrong"); }
    //ImGui::SameLine();
    if (ImGui::SmallButton("Clear")) { ClearLog(); }
    ImGui::SameLine();
    bool copy_to_clipboard = ImGui::SmallButton("Copy");
    //static float t = 0.0f; if (ImGui::GetTime() - t > 0.02f) { t = ImGui::GetTime(); AddLog("Spam %f", t); }

    ImGui::Separator();

    // Options menu
    if (ImGui::BeginPopup("Options"))
    {
        ImGui::Checkbox("Auto-scroll", &AutoScroll);
        ImGui::EndPopup();
    }

    // Options, Filter
    if (ImGui::Button("Options"))
        ImGui::OpenPopup("Options");
    ImGui::SameLine();
    Filter.Draw("Filter (\"incl,-excl\") (\"error\")", 180);
    ImGui::Separator();

    // Reserve enough left-over height for 1 separator + 1 input text
    const float footer_height_to_reserve = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();
    ImGui::BeginChild("ScrollingRegion", ImVec2(0, -footer_height_to_reserve), false, ImGuiWindowFlags_HorizontalScrollbar);
    if (ImGui::BeginPopupContextWindow())
    {
        if (ImGui::Selectable("Clear")) ClearLog();
        ImGui::EndPopup();
    }

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 1)); // Tighten spacing
    if (copy_to_clipboard)
        ImGui::LogToClipboard();

    for (auto& item : m_Items)
    {
        if (!Filter.PassFilter(item.second.c_str()) || !EventFilter(item.first))
            continue;

        ImVec4 color = ConvertSeverityTypeIDToColor(item.first);
        ImGui::PushStyleColor(ImGuiCol_Text, color);
        ImGui::TextUnformatted(item.second.c_str());
        ImGui::PopStyleColor();
    }

    /*ImGuiListClipper clipper;
    clipper.Begin(m_Items.size());        
    while (clipper.Step())
    {
        int current_index = clipper.DisplayStart;
        for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++)
        {
            current_index++;
            if (current_index >= m_Items.size())
                break;
            std::string item = m_Items[current_index].second.c_str();

            if (!Filter.PassFilter(item.c_str()) || !EventFilter(m_Items[i].first))
            {
                //i--; // adjust clipper index for filtered item
                continue;
            }

            ImVec4 color = ConvertSeverityTypeIDToColor(m_Items[i].first);
            ImGui::PushStyleColor(ImGuiCol_Text, color);
            ImGui::TextUnformatted(item.c_str());
            ImGui::PopStyleColor();
        }
    }*/

    if (copy_to_clipboard)
        ImGui::LogFinish();

    if (ScrollToBottom || (AutoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()))
        ImGui::SetScrollHereY(1.0f);
    ScrollToBottom = false;

    ImGui::PopStyleVar();
    ImGui::EndChild();
    ImGui::Separator();

    ImGui::End();
}; 

ImColor GuiEventLog::ConvertSeverityTypeIDToColor(SeverityTypeID st)
{
    ImColor color = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);
    switch (st)
    {
    case ST_MinorTrace:
    {
        color = ImVec4(46.0f/255.0f, 139.0f/255.0f, 87.0f/255.0f, 1.0f);
        break;
    }
    case ST_MajorTrace:
    {
        color = ImVec4(60.0f/255.0f, 179.0f/255.0f, 113.0f/255.0f, 1.0f);
        break;
    }
    case ST_Warning:
    {
        color = ImVec4(255.0f/255.0f, 140.0f/255.0f, 0.0f/255.0f, 1.0f);
        break;
    }
    case ST_Error:
    {
        color = ImVec4(1.0f, 0.0f, 0.0f, 1.0f);
        break;
    }
    case ST_FatalError:
    {
        color = ImVec4(1.0f, 0.0f, 0.0f, 1.0f);
        break;
    }
    case ST_DispError:
    {
        color = ImVec4(1.0f, 0.0f, 0.0f, 1.0f);
        break;
    }
    case ST_Nothing:
    {
        break;
    }
    };
    return color;
}

void GuiEventLog::ClearLog()
{
    //for (int i = 0; i < Items.size(); i++)
    //    free(Items[i]);
    m_Items.clear();
};

bool GuiEventLog::EventFilter(SeverityTypeID st)
{
    switch (st)
    {
    case ST_MinorTrace:
        return m_State.m_OptionsEventLog.ShowMessageTypeMinorTrace;
    case ST_MajorTrace:
        return m_State.m_OptionsEventLog.ShowMessageTypeMajorTrace;
    case ST_Warning:
        return m_State.m_OptionsEventLog.ShowMessageTypeWarning;
    case ST_Error:
    case ST_DispError:
    case ST_FatalError:
        return m_State.m_OptionsEventLog.ShowMessageTypeError;
    case ST_Nothing:
        return m_State.m_OptionsEventLog.ShowMessageTypeNothing;
    }
    return true;
}

void GuiEventLog::AddLog(SeverityTypeID st, std::string msg)
{
    if (m_Items.size() > m_MaxLogLines) // TODO: delete first n m_Items and shift items from n+1 to end n places back
    {
        m_Items.clear();
        m_Items.push_back(std::pair(SeverityTypeID::ST_Nothing, "Log cleanup."));
    }
    m_Items.push_back(std::pair(st, msg));
};

/*void GuiEventLog::ExecCommand(const char* command_line)
{
    AddLog(SeverityTypeID::ST_Nothing, "# %s\n", command_line);

    // Insert into history. First find match and delete it so it can be pushed to the back.
    // This isn't trying to be smart or optimal.
    HistoryPos = -1;
    for (int i = History.Size - 1; i >= 0; i--)
        if (Stricmp(History[i], command_line) == 0)
        {
            free(History[i]);
            History.erase(History.begin() + i);
            break;
        }
    History.push_back(Strdup(command_line));

    // Process command
    if (Stricmp(command_line, "CLEAR") == 0)
    {
        ClearLog();
    }
    else if (Stricmp(command_line, "HELP") == 0)
    {
        AddLog("Commands:");
        for (int i = 0; i < Commands.Size; i++)
            AddLog("- %s", Commands[i]);
    }
    else if (Stricmp(command_line, "HISTORY") == 0)
    {
        int first = History.Size - 10;
        for (int i = first > 0 ? first : 0; i < History.Size; i++)
            AddLog("%3d: %s\n", i, History[i]);
    }
    else
    {
        AddLog("Unknown command: '%s'\n", command_line);
    }

    // On command input, we scroll to bottom even if AutoScroll==false
    ScrollToBottom = true;
};*/

int   GuiEventLog::Stricmp(const char* s1, const char* s2)
{
    int d; 
    while ((d = toupper(*s2) - toupper(*s1)) == 0 && *s1) 
    { 
        s1++; 
        s2++; 
    } 
    return d;
}

int   GuiEventLog::Strnicmp(const char* s1, const char* s2, int n)
{
    int d = 0; 
    while (n > 0 && (d = toupper(*s2) - toupper(*s1)) == 0 && *s1) 
    { 
        s1++; 
        s2++; 
        n--; 
    } 
    return d;
};
char* GuiEventLog::Strdup(const char* s)
{
    IM_ASSERT(s); 
    size_t len = strlen(s) + 1; 
    void* buf = malloc(len); 
    IM_ASSERT(buf); 
    return (char*)memcpy(buf, (const void*)s, len);

};

void  GuiEventLog::Strtrim(char* s)
{
    char* str_end = s + strlen(s); 
    while (str_end > s && str_end[-1] == ' ')
    {
        str_end--; 
        *str_end = 0;
    }
};

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