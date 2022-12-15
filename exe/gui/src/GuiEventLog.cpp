#include "GuiInput.h"
#include "GuiEventLog.h"
#include "dbg/SeverityType.h"
#include <string>
#include "ser/AsString.h"
#include "TicInterface.h"

std::vector<EventLogItem> GuiEventLog::m_Items;
std::vector<UInt64>       GuiEventLog::m_FilteredItemIndices;
std::string               GuiEventLog::m_FilterText = "";
OptionsEventLog           GuiEventLog::m_FilterEvents;

auto FilterLogMessage(std::string_view original_message, std::string& filtered_message, std::string& link)
{
    filtered_message = original_message;
    auto link_opening = original_message.find("[[");
    auto link_closing = original_message.rfind("]]"); // assume at most one link occurence per log entry

    if (link_opening != std::string_view::npos && link_closing != std::string_view::npos)
    {
        link = original_message.substr(link_opening + 2, link_closing - link_opening - 2);
        filtered_message.replace(link_opening, link_closing + 2 - link_opening, "");
    }
}

GuiEventLog::GuiEventLog()
{
    ClearLog();
    //m_Iterator.Init(m_Items.begin(), 0);
    AutoScroll = true;
    ScrollToBottom = false;
};

GuiEventLog::~GuiEventLog()
{
    ClearLog();
    //for (int i = 0; i < History.Size; i++)
    //    free(History[i]);
};


auto GuiEventLog::ShowEventLogOptionsWindow(bool* p_open) -> void
{
    if (ImGui::Begin("Eventlog options", p_open, NULL))
    {
        if (ImGui::Checkbox("show minor-trace messages", &m_FilterEvents.ShowMessageTypeMinorTrace)) {Refilter();}
        if (ImGui::Checkbox("show major-trace messages", &m_FilterEvents.ShowMessageTypeMajorTrace)) { Refilter(); }
        if (ImGui::Checkbox("show warning messages", &m_FilterEvents.ShowMessageTypeWarning)) { Refilter(); }
        if (ImGui::Checkbox("show error messages", &m_FilterEvents.ShowMessageTypeError)) { Refilter(); }
        if (ImGui::Checkbox("show nothing messages", &m_FilterEvents.ShowMessageTypeNothing)) { Refilter(); }

        ImGui::End();
    }
}

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

auto GuiEventLog::OnItemClick(GuiState& state, EventLogItem* item) -> void
{
    if (!item)
        return;

    auto best_item_ref = TreeItem_GetBestItemAndUnfoundPart(state.GetRoot(), item->m_Link.c_str());
    auto jump_item = best_item_ref.first;

    if (jump_item)
    {
        auto event_queues = GuiEventQueues::getInstance();
        state.SetCurrentItem(const_cast<TreeItem*>(jump_item));
        event_queues->TreeViewEvents.Add(GuiEvents::JumpToCurrentItem);
        event_queues->MainEvents.Add(GuiEvents::UpdateCurrentItem);
        event_queues->DetailPagesEvents.Add(GuiEvents::UpdateCurrentItem);
    }
}

auto GuiEventLog::GetItem(size_t index) -> EventLogItem*
{
    if (m_FilteredItemIndices.empty()) // no filter
        return &m_Items.at(index);
    else
        return &m_Items.at(m_FilteredItemIndices.at(index));
}

auto GuiEventLog::DrawItem(EventLogItem *item) -> void
{
    ImVec4 color = ConvertSeverityTypeIDToColor(item->m_Severity_type);
    ImGui::PushStyleColor(ImGuiCol_Text, color);
    ImGui::TextUnformatted(item->m_Text.c_str());
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

    // events
    auto event_queues = GuiEventQueues::getInstance();

    // focus
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

    // filter
    ImGui::SetNextItemWidth(ImGui::GetWindowWidth());
    if (ImGui::InputText("##filter", &m_FilterText, ImGuiInputTextFlags_EnterReturnsTrue, InputTextCallback, nullptr))
    {
        
    }

    // Reserve enough left-over height for 1 separator + 1 input text
    const float footer_height_to_reserve = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();
    ImGui::BeginChild("ScrollingRegion", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar); //-footer_height_to_reserve
    
    // right-click event
    if (ImGui::BeginPopupContextWindow())
    {
        if (ImGui::Selectable("Clear")) ClearLog();
        ImGui::EndPopup();
    }

    ImGuiListClipper clipper;
    bool has_item_filter = !m_FilteredItemIndices.empty();
    if (has_item_filter)
        clipper.Begin(m_FilteredItemIndices.size());
    else
        clipper.Begin(m_Items.size());

    //auto iterator = m_Iterator.GetIterator();

    bool first_pass = true;
    while (clipper.Step())
    {        
        for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; row++)
        {
            auto item_ptr = GetItem(row);
            DrawItem(item_ptr);
            if (ImGui::IsItemClicked())
            {
                SetKeyboardFocusToThisHwnd();
                OnItemClick(state, item_ptr);
            }
        }
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

auto ItemPassesEventFilter(EventLogItem* item, OptionsEventLog *options) -> bool
{
    switch (item->m_Severity_type)
    {
    case SeverityTypeID::ST_MinorTrace:
        return options->ShowMessageTypeMinorTrace;
    case SeverityTypeID::ST_MajorTrace:
        return options->ShowMessageTypeMajorTrace;
    case SeverityTypeID::ST_Warning:
        return options->ShowMessageTypeWarning;
    case SeverityTypeID::ST_Error:
    case SeverityTypeID::ST_DispError:
    case SeverityTypeID::ST_FatalError:
        return options->ShowMessageTypeError;
    case SeverityTypeID::ST_Nothing:
        return options->ShowMessageTypeNothing;
    }
    return false;
}

auto ItemPassesTextFilter(EventLogItem* item, std::string_view filter_text) -> bool
{
    if (item->m_Text.contains(filter_text))
        return true;
    return false;
}

auto ItemPassesFilter(EventLogItem* item, OptionsEventLog* options, std::string_view filter_text) -> bool
{
    if (ItemPassesEventFilter(item, options) && ItemPassesTextFilter(item, filter_text))
        return true;
    return false;
}

auto GuiEventLog::ClearLog() -> void
{
    m_Items.clear();
    m_FilteredItemIndices.clear();
};

auto GuiEventLog::Refilter() -> void
{
    m_FilteredItemIndices.clear();

    UInt64 index = 0;
    for (auto& item : m_Items)
    {
        if (ItemPassesFilter(&item, &m_FilterEvents, m_FilterText))
            m_FilteredItemIndices.push_back(index);

        index++;
    }
}

auto GuiEventLog::AddLog(SeverityTypeID severity_type, std::string original_message) -> void
{
    std::string filtered_message = "";
    std::string link = "";
    FilterLogMessage(original_message, filtered_message, link);
    m_Items.emplace_back(severity_type, filtered_message, link);

    if (ItemPassesFilter(&m_Items.back(), &m_FilterEvents, m_FilterText))
        m_FilteredItemIndices.push_back(m_Items.size()-1);
};
