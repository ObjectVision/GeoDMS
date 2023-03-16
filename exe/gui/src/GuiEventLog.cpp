#include "GuiInput.h"
#include "GuiEventLog.h"
#include "dbg/SeverityType.h"
#include <string>
#include "ser/AsString.h"
#include "TicInterface.h"
#include "GuiMain.h"
#include "imgui_internal.h"

#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "GuiGraphics.h"

// TODO: remove singletons
std::vector<EventLogItem> GuiEventLog::m_Items;
std::vector<UInt64>       GuiEventLog::m_FilteredItemIndices;
std::string               GuiEventLog::m_FilterText = "";
OptionsEventLog           GuiEventLog::m_FilterEvents;

struct href_message {
    std::string user_text;
    std::string link;
};

auto ReplaceLinkInLogMessageIfNecessary(std::string_view original_message) -> href_message
{
    href_message result;
    result.user_text = original_message;
    auto link_opening = original_message.find("[[");
    auto link_closing = original_message.rfind("]]"); // assume at most one link occurence per log entry

    if (link_opening != std::string_view::npos && link_closing != std::string_view::npos)
    {
        result.user_text.replace(link_opening, link_closing + 2 - link_opening, "");
        result.link = original_message.substr(link_opening + 2, link_closing - link_opening - 2);
    }
    return result;
}

GuiEventLog::GuiEventLog()
{
    ClearLog();
    AutoScroll = true;
    ScrollToBottom = false;
};

GuiEventLog::~GuiEventLog()
{
    ClearLog();
};

void GuiEventLog::ShowEventLogOptionsWindow(bool* p_open)
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

void GuiEventLog::GeoDMSExceptionMessage(CharPtr msg) //TODO: add client handle to exception message function
{
    GuiState state;
    state.errorDialogMessage.Set(msg);
    PostEmptyEventToGLFWContext();
    return;
}

void GuiEventLog::OnItemClick(GuiState& state, EventLogItem* item)
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

void GuiEventLog::DrawItem(EventLogItem *item)
{
    ImVec4 color = ConvertSeverityTypeIDToColor(item->m_Severity_type);
    ImGui::PushStyleColor(ImGuiCol_Text, color);
    ImGui::TextUnformatted(item->m_Text.c_str());

    OnItemClickItemTextTextToClipboard(item->m_Text);

    ImGui::PopStyleColor();
}

void GuiEventLog::Update(bool* p_open, GuiState& state)
{
    if (!ImGui::Begin("EventLog", p_open, ImGuiWindowFlags_None | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoScrollbar))
    {
        ImGui::End();
        return;
    }

    ImGuiContext& g = *GImGui;
    auto backup_spacing = g.Style.ItemSpacing.y;
    g.Style.ItemSpacing.y = 0.0f;

    m_direct_update_information.viewport = ImGui::GetMainViewport();
    m_direct_update_information.time_since_last_update = std::chrono::system_clock::now();

    AutoHideWindowDocknodeTabBar(is_docking_initialized);

    // events
    auto event_queues = GuiEventQueues::getInstance();

    // focus
    if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        SetKeyboardFocusToThisHwnd();

    // filter
    ImGui::SetNextItemWidth(ImGui::GetWindowWidth());
    if (ImGui::InputText("##filter", &m_FilterText, ImGuiInputTextFlags_EnterReturnsTrue, InputTextCallback, nullptr))
    {
        Refilter();
    }

    ImGui::Separator();

    // Reserve enough left-over height for 1 separator + 1 input text
    const float footer_height_to_reserve = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();
    ImGui::BeginChild("ScrollingRegion", ImVec2(0, -20.0f), false, ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_AlwaysAutoResize); //-footer_height_to_reserve
    
    // right-click event
    if (ImGui::BeginPopupContextWindow())
    {
        if (ImGui::Selectable("Clear")) ClearLog();
        ImGui::EndPopup();
    }

    // update content region
    auto eventlog_window = ImGui::GetCurrentWindow();
    m_direct_update_information.log.cursor = ImGui::GetCursorScreenPos();
    m_direct_update_information.log.pos = eventlog_window->ClipRect.Min;
    m_direct_update_information.log.size = ImGui::GetWindowContentRegionMax();

    auto cur_window = ImGui::GetCurrentWindow();

    ImGuiListClipper clipper;
    if (m_FilteredItemIndices.size() == 1 && m_FilteredItemIndices.at(0) == 0xFFFFFFFFFFFFFFFF)
    {
        ImGui::EndChild();
        ImGui::Separator();
        ImGui::End();
        return;
    }
    else if (!m_FilteredItemIndices.empty())
        clipper.Begin(m_FilteredItemIndices.size());

    else
        clipper.Begin(m_Items.size());

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

    if (ScrollToBottom || (AutoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()))
        ImGui::SetScrollHereY(1.0f);
    ScrollToBottom = false;

    if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        SetKeyboardFocusToThisHwnd();

    ImGui::EndChild();
    ImGui::Separator();
    
    // StatusBar
    auto vp = ImGui::GetWindowViewport();
    m_direct_update_information.status.cursor = ImGui::GetCursorScreenPos();
    m_direct_update_information.status.pos = vp->Pos;
    m_direct_update_information.status.size = vp->Size;

    ImGui::TextUnformatted(state.contextMessage.Get().c_str());

    g.Style.ItemSpacing.y = backup_spacing;
    ImGui::End();
}; 

auto GuiEventLog::ConvertSeverityTypeIDToColor(SeverityTypeID st) -> ImColor
{
    ImColor color = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);
    switch (st)
    {
    case SeverityTypeID::ST_MinorTrace: { color = ImVec4(46.0f/255.0f, 139.0f/255.0f, 87.0f/255.0f, 1.0f); break; }
    case SeverityTypeID::ST_MajorTrace: { break; }//color = ImVec4(60.0f/255.0f, 179.0f/255.0f, 113.0f/255.0f, 1.0f);  break;
    case SeverityTypeID::ST_Warning:    { color = ImVec4(255.0f/255.0f, 140.0f/255.0f, 0.0f/255.0f, 1.0f); break; }
    case SeverityTypeID::ST_Error:      { color = ImVec4(1.0f, 0.0f, 0.0f, 1.0f); break; }
    case SeverityTypeID::ST_FatalError: { color = ImVec4(1.0f, 0.0f, 0.0f, 1.0f); break; }
    case SeverityTypeID::ST_DispError:  { color = ImVec4(1.0f, 0.0f, 0.0f, 1.0f); break; }
    case SeverityTypeID::ST_Nothing:    { break;}
    };
    return color;
}

bool ItemPassesEventFilter(EventLogItem* item, OptionsEventLog *options)
{
    switch (item->m_Severity_type)
    {
    case SeverityTypeID::ST_MinorTrace: return options->ShowMessageTypeMinorTrace;
    case SeverityTypeID::ST_MajorTrace: return options->ShowMessageTypeMajorTrace;
    case SeverityTypeID::ST_Warning:    return options->ShowMessageTypeWarning;
    case SeverityTypeID::ST_Error:
    case SeverityTypeID::ST_DispError:
    case SeverityTypeID::ST_FatalError: return options->ShowMessageTypeError;
    case SeverityTypeID::ST_Nothing:    return options->ShowMessageTypeNothing;
    }
    return false;
}

bool ItemPassesTextFilter(EventLogItem* item, std::string_view filter_text)
{
    if (item->m_Text.contains(filter_text))
        return true;
    return false;
}

bool ItemPassesFilter(EventLogItem* item, OptionsEventLog* options, std::string_view filter_text)
{
    if (ItemPassesEventFilter(item, options) && ItemPassesTextFilter(item, filter_text))
        return true;
    return false;
}

void GuiEventLog::ClearLog()
{
    m_Items.clear();
    m_FilteredItemIndices.clear();
};

void GuiEventLog::Refilter()
{
    m_FilteredItemIndices.clear();

    UInt64 index = 0;
    for (auto& item : m_Items)
    {
        if (ItemPassesFilter(&item, &m_FilterEvents, m_FilterText))
            m_FilteredItemIndices.push_back(index);

        index++;
    }
    if (m_FilteredItemIndices.empty())
        m_FilteredItemIndices.push_back(0xFFFFFFFFFFFFFFFF); // special empty search result indicator
}

void GuiEventLog::AddLog(SeverityTypeID severity_type, std::string_view original_message)
{
    auto hrefMsg = ReplaceLinkInLogMessageIfNecessary(original_message);
    m_Items.emplace_back(severity_type, hrefMsg.user_text, hrefMsg.link);

    if (ItemPassesFilter(&m_Items.back(), &m_FilterEvents, m_FilterText))
        m_FilteredItemIndices.push_back(m_Items.size()-1);
};

bool EventlogDirectUpdateInformation::RequiresUpdate()
{
    auto log_message_time = std::chrono::system_clock::now();
    std::chrono::duration<double, std::milli> delta_time_update = log_message_time - time_since_last_update;
    return delta_time_update.count() > 5000; // TODO: expose param to user
}

void GuiEventLog::DirectUpdate(GuiState& state)
{
    if (!m_direct_update_information.viewport)
        return;

    ImGuiContext& g = *GImGui;
    m_direct_update_information.time_since_last_update = std::chrono::system_clock::now();

    ImGui_ImplGlfw_ViewportData* viewport_data = static_cast<ImGui_ImplGlfw_ViewportData*>(m_direct_update_information.viewport->PlatformUserData);//m_smvp.vp->PlatformUserData);
    DirectUpdateFrame direct_update_frame(viewport_data->Window, m_direct_update_information.status.pos, m_direct_update_information.status.size); // m_smvp.display_pos, m_smvp.display_size

    // log
    auto log_draw_list_ptr = direct_update_frame.AddDrawList(m_direct_update_information.log.pos, ImVec2(m_direct_update_information.log.pos.x + m_direct_update_information.log.size.x, m_direct_update_information.log.pos.y + m_direct_update_information.log.size.y));
    SetTextBackgroundColor(m_direct_update_information.log.size, ImGui::GetColorU32(ImGuiCol_WindowBg), log_draw_list_ptr, &m_direct_update_information.log.cursor);
    size_t number_of_log_lines = std::ceil(m_direct_update_information.log.size.y / ImGui::GetTextLineHeight());
    if (number_of_log_lines > m_Items.size())
        number_of_log_lines = m_Items.size();
    size_t log_direct_update_index = m_Items.size() <= number_of_log_lines ? 0 : m_Items.size()-(number_of_log_lines+1);
    
    ImVec2 log_cursor = m_direct_update_information.log.cursor;
    for (int i = log_direct_update_index; i < log_direct_update_index + number_of_log_lines; i++)
    {
        auto eventlog_item_ptr = &m_Items[i];
        //ImVec4 color = 
        log_draw_list_ptr->AddText(g.Font, g.FontSize, log_cursor, ConvertSeverityTypeIDToColor(eventlog_item_ptr->m_Severity_type), eventlog_item_ptr->m_Text.c_str(), NULL, 0.0f);
        log_cursor = ImVec2(log_cursor.x, log_cursor.y+ImGui::GetTextLineHeight());
    }

    // status
    auto status_draw_list_ptr = direct_update_frame.AddDrawList(m_direct_update_information.status.pos, ImVec2(m_direct_update_information.status.pos.x + m_direct_update_information.status.size.x, m_direct_update_information.status.pos.y + m_direct_update_information.status.size.y));
    SetTextBackgroundColor(ImGui::CalcTextSize(state.contextMessage.Get().c_str()), ImGui::GetColorU32(ImGuiCol_WindowBg), status_draw_list_ptr, &m_direct_update_information.status.cursor);
    status_draw_list_ptr->AddText(g.Font, g.FontSize, m_direct_update_information.status.cursor, ImColor(255, 0, 0, 255), state.contextMessage.Get().c_str(), NULL, 0.0f); //  ImGui::FindRenderedTextEnd(main->m_State.contextMessage.Get().c_str()
}

void GuiEventLog::GeoDMSContextMessage(ClientHandle clientHandle, CharPtr msg)
{
    auto main = reinterpret_cast<GuiMainComponent*>(clientHandle);
    main->m_State.contextMessage.Set(msg);

    ImGuiIO& io = ImGui::GetIO();

    auto context_message_time = std::chrono::system_clock::now();
    std::chrono::duration<double, std::milli> time_since_last_update = context_message_time - main->m_State.m_last_update_time;

    if (!GImGui)
        return;

    if (main->m_EventLog.m_direct_update_information.RequiresUpdate())
        main->m_EventLog.DirectUpdate(main->m_State);
    return;
}

void GuiEventLog::GeoDMSMessage(ClientHandle clientHandle, SeverityTypeID st, CharPtr msg)
{
    auto main = reinterpret_cast<GuiMainComponent*>(clientHandle);
    assert(main);
    main->m_EventLog.AddLog(st, msg);
    if (main->m_EventLog.m_direct_update_information.RequiresUpdate())
        main->m_EventLog.DirectUpdate(main->m_State);

    return;
}

