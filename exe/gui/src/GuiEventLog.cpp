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
    //AddLog(SeverityTypeID::ST_Error, msg);

    GuiState state;
    ImGui::OpenPopup("Error");
    state.errorDialogMessage.Set(msg);


    /*static bool open_error_window = true;
    //if (ImGui::Begin("ERRORBOT", &open_error_window, ImGuiWindowFlags_None))
    //{
        ImGui::OpenPopup("Errordialog");
        if (ImGui::BeginPopupModal("Errordialog", NULL, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::InputTextMultiline("##ErrorDialog", const_cast<char*>(msg), sizeof(msg), ImVec2(-FLT_MIN, ImGui::GetTextLineHeight() * 16));
            if (ImGui::Button("Cancel", ImVec2(120, 0))) { ImGui::CloseCurrentPopup(); }
            ImGui::EndPopup();
        }
        // knop OK mag weg
        // knop terminate Vervangen door Abort
        // alleen mail knop bij "contact object vision" in tekst DMSException.cpp 367


        // Early out if the window is collapsed, as an optimization.
        //ImGui::End();*/
        return;
    //}



}

void GuiEventLog::Update(bool* p_open)
{
    ImGui::SetNextWindowSize(ImVec2(520, 600), ImGuiCond_FirstUseEver);// TODO: ???
    if (!ImGui::Begin("EventLog", p_open, ImGuiWindowFlags_None))
    {
        ImGui::End();
        return;
    }

    if (ImGui::IsWindowHovered() && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
        SetKeyboardFocusToThisHwnd();

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
    /*if (ImGui::SmallButton("Test SeverityTypeID messages"))
    {
        AddLog(SeverityTypeID::ST_MinorTrace, "ST_MinorTrace: this is a minor trace message.");
        AddLog(SeverityTypeID::ST_MajorTrace, "ST_MajorTrace: this is a major trace message.");
        AddLog(SeverityTypeID::ST_Warning, "ST_Warning: this is a warning message.");
        AddLog(SeverityTypeID::ST_Error, "ST_Error: this is an error message.");
        AddLog(SeverityTypeID::ST_FatalError, "ST_FatalError: this is a fatal error message.");
        AddLog(SeverityTypeID::ST_DispError, "ST_DispError: this is a disp error message.");
        AddLog(SeverityTypeID::ST_Nothing, "ST_Nothing: this is a nothing message.");
    }*/


    /*if (ImGui::SmallButton("Clear")) { ClearLog(); }
    ImGui::SameLine();
    bool copy_to_clipboard = ImGui::SmallButton("Copy");

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
    ImGui::SameLine();*/
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

    //ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 1)); // Tighten spacing
    //if (copy_to_clipboard)
    //    ImGui::LogToClipboard();

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

    //if (copy_to_clipboard)
    //    ImGui::LogFinish();

    if (ScrollToBottom || (AutoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()))
        ImGui::SetScrollHereY(1.0f);
    ScrollToBottom = false;

    //ImGui::PopStyleVar();
    ImGui::EndChild();
    ImGui::Separator();

    ImGui::End();
}; 

ImColor GuiEventLog::ConvertSeverityTypeIDToColor(SeverityTypeID st)
{
    ImColor color = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);
    switch (st)
    {
    case SeverityTypeID::ST_MinorTrace:
    {
        color = ImVec4(46.0f/255.0f, 139.0f/255.0f, 87.0f/255.0f, 1.0f);
        break;
    }
    case SeverityTypeID::ST_MajorTrace:
    {
        color = ImVec4(60.0f/255.0f, 179.0f/255.0f, 113.0f/255.0f, 1.0f);
        break;
    }
    case SeverityTypeID::ST_Warning:
    {
        color = ImVec4(255.0f/255.0f, 140.0f/255.0f, 0.0f/255.0f, 1.0f);
        break;
    }
    case SeverityTypeID::ST_Error:
    {
        color = ImVec4(1.0f, 0.0f, 0.0f, 1.0f);
        break;
    }
    case SeverityTypeID::ST_FatalError:
    {
        color = ImVec4(1.0f, 0.0f, 0.0f, 1.0f);
        break;
    }
    case SeverityTypeID::ST_DispError:
    {
        color = ImVec4(1.0f, 0.0f, 0.0f, 1.0f);
        break;
    }
    case SeverityTypeID::ST_Nothing:
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
    case SeverityTypeID::ST_MinorTrace:
        return m_State.m_OptionsEventLog.ShowMessageTypeMinorTrace;
    case SeverityTypeID::ST_MajorTrace:
        return m_State.m_OptionsEventLog.ShowMessageTypeMajorTrace;
    case SeverityTypeID::ST_Warning:
        return m_State.m_OptionsEventLog.ShowMessageTypeWarning;
    case SeverityTypeID::ST_Error:
    case SeverityTypeID::ST_DispError:
    case SeverityTypeID::ST_FatalError:
        return m_State.m_OptionsEventLog.ShowMessageTypeError;
    case SeverityTypeID::ST_Nothing:
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