#include "GuiInput.h"
#include "GuiEventLog.h"
#include "dbg/SeverityType.h"
#include <string>
#include "ser/AsString.h"
#include "TicInterface.h"

std::list<EventLogItem> GuiEventLog::m_Items;
UInt32 GuiEventLog::m_MaxLogLines = 32000; // perform rollover after max loglines have been reached

GuiEventLog::GuiEventLog()
{
    ClearLog();
    m_Iterator.Init(m_Items.begin(), 0);
    AutoScroll = true;
    ScrollToBottom = false;
};

GuiEventLog::~GuiEventLog()
{
    ClearLog();
    //for (int i = 0; i < History.Size; i++)
    //    free(History[i]);
};

auto GuiEventLog::GeoDMSMessage(ClientHandle clientHandle, SeverityTypeID st, CharPtr msg) -> void
{
    //if (st == SeverityTypeID::ST_Error || st == SeverityTypeID::ST_FatalError || st == SeverityTypeID::ST_DispError)
    //    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 0, 0, 255));
    AddLog(st, msg);//msg);
    //if (st == SeverityTypeID::ST_Error || st == SeverityTypeID::ST_FatalError || st == SeverityTypeID::ST_DispError)
    //    ImGui::PopStyleColor();
}

auto GuiEventLog::GeoDMSExceptionMessage(CharPtr msg) -> void
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

auto GuiEventLog::DrawItem(EventLogItem &item) -> void
{
    ImVec4 color = ConvertSeverityTypeIDToColor(item.m_Severity_type);
    ImGui::PushStyleColor(ImGuiCol_Text, color);
    ImGui::TextUnformatted(item.m_Text.c_str());
    ImGui::PopStyleColor();
}

auto GuiEventLog::Update(bool* p_open, GuiState& state) -> void
{
    ImGui::SetNextWindowSize(ImVec2(520, 600), ImGuiCond_FirstUseEver);// TODO: ???
    if (!ImGui::Begin("EventLog", p_open, ImGuiWindowFlags_None | ImGuiWindowFlags_NoTitleBar))
    {
        ImGui::End();
        return;
    }

    if (!m_Items.empty() && m_Iterator.GetIterator() == m_Items.end())
        m_Iterator.Init(m_Items.begin());

    if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        SetKeyboardFocusToThisHwnd();

    // window specific options button
    auto old_cpos = SetCursorPosToOptionsIconInWindowHeader();
    SetClipRectToIncludeOptionsIconInWindowHeader();
    ImGui::Text(ICON_RI_SETTINGS);
    if (MouseHooversOptionsIconInWindowHeader())
    {
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
            state.ShowEventLogOptionsWindow = true;
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
    
    //Filter.Draw("Filter (\"incl,-excl\") (\"error\")", 180);
    //ImGui::Separator();

    // Reserve enough left-over height for 1 separator + 1 input text
    const float footer_height_to_reserve = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();
    ImGui::BeginChild("ScrollingRegion", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar); //-footer_height_to_reserve
    
    // right-click event
    if (ImGui::BeginPopupContextWindow())
    {
        if (ImGui::Selectable("Clear")) ClearLog();
        ImGui::EndPopup();
    }

    //ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 1)); // Tighten spacing
    //if (copy_to_clipboard)
    //    ImGui::LogToClipboard();

    /*for (auto& item : m_Items)
    {
        if (!Filter.PassFilter(item.m_Text.c_str()) || !EventFilter(item.m_Severity_type, state))
            continue;

        ImVec4 color = ConvertSeverityTypeIDToColor(item.m_Severity_type);
        ImGui::PushStyleColor(ImGuiCol_Text, color);
        ImGui::TextUnformatted(item.m_Text.c_str());
        ImGui::PopStyleColor();

        if (ImGui::IsItemClicked())
        {
            SetKeyboardFocusToThisHwnd();
            if (!item.m_Link.empty())
            {

            }
        }  
    }*/

    ImGuiListClipper clipper;
    clipper.Begin(m_Items.size());

    auto iterator = m_Iterator.GetIterator();

    bool first_pass = true;
    while (clipper.Step())
    {
        if (first_pass) 
        {
            if (!m_Items.empty())
                DrawItem(*m_Items.begin());

            first_pass = false;
            continue;
        }

        if (!first_pass && !m_Iterator.CompareStartPositions(clipper.DisplayStart))
        {
            m_Iterator.SetStartPosition(clipper.DisplayStart);
            iterator = m_Iterator.GetIterator();
        }
        
        for (int i = 0; i < clipper.DisplayEnd - clipper.DisplayStart; i++)
        {
            DrawItem(*iterator);
            if (ImGui::IsItemClicked())
            {
                SetKeyboardFocusToThisHwnd();
                if (!iterator->m_Link.empty())
                {
                    // TODO: move this part to separate func, duplicate code also occuring in GuiCurrentItem
                    auto unfound_part = IString::Create("");
                    TreeItem* jumpItem = (TreeItem*)DMS_TreeItem_GetBestItemAndUnfoundPart(state.GetRoot(), iterator->m_Link.c_str(), &unfound_part);

                    //auto jumpItem = SetJumpItemFullNameToOpenInTreeView(m_State.GetRoot(), DivideTreeItemFullNameIntoTreeItemNames(reinterpret_cast<char*> (&m_Buf[0])));
                    if (jumpItem)
                    {
                        auto event_queues = GuiEventQueues::getInstance();
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

            iterator = std::next(iterator);
        }

        /*if (clipper.DisplayStart == 0 && clipper.DisplayEnd == 1)
        {
            DrawItem(*m_Items.begin());
            continue;
        }
        else if (!m_Iterator.CompareStartPositions(clipper.DisplayStart))
        {
            m_Iterator.SetStartPosition(clipper.DisplayStart);
        }

        
        for (int i = 0; i < clipper.DisplayEnd - clipper.DisplayStart; i++)
        {
            DrawItem(*iterator);
            iterator = std::next(iterator);

            if (iterator == m_Items.end())
                break;
        }*/


        /*for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++)
        {


            ImVec4 color = ConvertSeverityTypeIDToColor(m_Items[i].first);
            ImGui::PushStyleColor(ImGuiCol_Text, color);
            ImGui::TextUnformatted(item.c_str());
            ImGui::PopStyleColor();
        }*/
    }

    //if (copy_to_clipboard)
    //    ImGui::LogFinish();

    if (ScrollToBottom || (AutoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()))
        ImGui::SetScrollHereY(1.0f);
    ScrollToBottom = false;

    if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        SetKeyboardFocusToThisHwnd();

    ImGui::EndChild();
    ImGui::Separator();

    ImGui::End();
}; 

auto GuiEventLog::ConvertSeverityTypeIDToColor(SeverityTypeID st) -> ImColor
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

auto GuiEventLog::ClearLog() -> void
{
    m_Items.clear();
};

auto GuiEventLog::EventFilter(SeverityTypeID st, GuiState& state) -> bool
{
    switch (st)
    {
    case SeverityTypeID::ST_MinorTrace:
        return state.m_OptionsEventLog.ShowMessageTypeMinorTrace;
    case SeverityTypeID::ST_MajorTrace:
        return state.m_OptionsEventLog.ShowMessageTypeMajorTrace;
    case SeverityTypeID::ST_Warning:
        return state.m_OptionsEventLog.ShowMessageTypeWarning;
    case SeverityTypeID::ST_Error:
    case SeverityTypeID::ST_DispError:
    case SeverityTypeID::ST_FatalError:
        return state.m_OptionsEventLog.ShowMessageTypeError;
    case SeverityTypeID::ST_Nothing:
        return state.m_OptionsEventLog.ShowMessageTypeNothing;
    }
    return true;
}

auto FilterLogMessage(std::string_view original_message, std::string &filtered_message, std::string &link)
{
    filtered_message = original_message;
    auto link_opening = original_message.find("[[");
    auto link_closing = original_message.rfind("]]"); // assume at most one link occurence per log entry

    if (link_opening != std::string_view::npos && link_closing != std::string_view::npos)
    {
        link = original_message.substr(link_opening+2, link_closing-link_opening-2);
        filtered_message.replace(link_opening, link_closing + 2 - link_opening, "");
    }
}

auto GuiEventLog::AddLog(SeverityTypeID severity_type, std::string original_message) -> void
{
    if (m_Items.size() > m_MaxLogLines)
        m_Items.pop_front();

    std::string filtered_message = "";
    std::string link = "";
    FilterLogMessage(original_message, filtered_message, link);
    m_Items.emplace_back(severity_type, filtered_message, link);
};