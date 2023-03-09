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

//StatusMessageViewport GuiEventLog::m_smvp = {};
//EventlogDirectUpdateInformation GuiEventLog::m_direct_update_information = {};

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
    //auto event_queues = GuiEventQueues::getInstance();

    //event_queues->MainEvents.Add(GuiEvents::OpenErrorDialog);
    //ImGui::OpenPopup("Error");
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
    //ImGui::SetNextWindowSize(ImVec2(520, 600), ImGuiCond_FirstUseEver);// TODO: ???
    if (!ImGui::Begin("EventLog", p_open, ImGuiWindowFlags_None | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoScrollbar))
    {
        ImGui::End();
        return;
    }

    m_direct_update_information.viewport = ImGui::GetMainViewport();
    m_direct_update_information.time_since_last_update = std::chrono::system_clock::now();

    AutoHideWindowDocknodeTabBar(is_docking_initialized);

    //ImGuiStyle &style = ImGui::GetStyle();
    //style.ItemSpacing.y = 2.0;
    //auto test = style.ItemSpacing.y;// = -1.0;
        // style.ItemSpacing.y

    // events
    auto event_queues = GuiEventQueues::getInstance();

    // focus
    if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        SetKeyboardFocusToThisHwnd();

    /*// window specific options button
    auto old_cpos = SetCursorPosToOptionsIconInWindowHeader();
    SetClipRectToIncludeOptionsIconInWindowHeader();
    ImGui::Text(ICON_RI_SETTINGS);
    if (MouseHooversOptionsIconInWindowHeader())
    {
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
            state.ShowEventLogOptionsWindow = true;
    }
    ImGui::SetCursorPos(old_cpos);
    ImGui::PopClipRect();*/

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
    m_direct_update_information.log.size = eventlog_window->ContentSize;

    auto cur_window = ImGui::GetCurrentWindow();
    
    //m_direct_update_information.log.pos = ImGui::GetCurrentWindow();
    //m_direct_update_information.log.pos =

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

    m_direct_update_information.log.index = clipper.DisplayStart==0 ? 0 : clipper.DisplayStart--;

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
    
    

    /*m_smvp.vp = ImGui::GetWindowViewport();
    m_smvp.cursor_pos   = ImGui::GetCurrentWindow()->DC.CursorPos;
    m_smvp.display_pos  = m_smvp.vp->Pos;
    m_smvp.display_size = m_smvp.vp->Size; //ImVec2(ImGui::GetContentRegionAvail().x, ImGui::GetTextLineHeightWithSpacing());*/

    ImGui::TextUnformatted(state.contextMessage.Get().c_str());
    
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

/*#include "GuiMain.h"
#include <imgui_impl_opengl3.h>
#include <imgui_impl_glfw.h>
#include <imgui_internal.h>

void DirectUpdateEventLog(GuiMainComponent* main)
{
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame(); // TODO: set  to true for UpdateInputEvents?

    main->m_EventLog.Update(&main->m_State.ShowEventLogWindow, main->m_State);

    ImGui::Render();
    int display_w, display_h;
    glfwGetFramebufferSize(main->m_MainWindow, &display_w, &display_h);
    glViewport(0, 0, display_w, display_h);

    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
    glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w);
    glClear(GL_COLOR_BUFFER_BIT);

    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    // Update and Render additional Platform Windows
    auto& io = ImGui::GetIO();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        GLFWwindow* backup_current_context = glfwGetCurrentContext();
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
        glfwMakeContextCurrent(backup_current_context);
    }
    glfwSwapBuffers(main->m_MainWindow);
}*/

void GuiEventLog::DirectUpdate(GuiState& state)
{
    ImGuiContext& g = *GImGui;
    //main->m_State.m_last_update_time = context_message_time; // TODO: generalize this to whole eventlog update

    m_direct_update_information.time_since_last_update = std::chrono::system_clock::now();

    ImGui_ImplGlfw_ViewportData* viewport_data = static_cast<ImGui_ImplGlfw_ViewportData*>(m_direct_update_information.viewport->PlatformUserData);//m_smvp.vp->PlatformUserData);
    DirectUpdateFrame direct_update_frame(viewport_data->Window, m_direct_update_information.status.pos, m_direct_update_information.status.size); // m_smvp.display_pos, m_smvp.display_size

    auto draw_list_ptr = direct_update_frame.AddDrawList(m_direct_update_information.status.pos, ImVec2(m_direct_update_information.status.pos.x + m_direct_update_information.status.size.x, m_direct_update_information.status.pos.y + m_direct_update_information.status.size.y));
    SetTextBackgroundColor(ImGui::CalcTextSize(state.contextMessage.Get().c_str()), ImGui::GetColorU32(ImGuiCol_WindowBg), draw_list_ptr, &m_direct_update_information.status.cursor);
    draw_list_ptr->AddText(g.Font, g.FontSize, m_direct_update_information.status.cursor, ImColor(255, 0, 0, 255), state.contextMessage.Get().c_str(), NULL, 0.0f); //  ImGui::FindRenderedTextEnd(main->m_State.contextMessage.Get().c_str()
}

void GuiEventLog::GeoDMSContextMessage(ClientHandle clientHandle, CharPtr msg)
{
    auto main = reinterpret_cast<GuiMainComponent*>(clientHandle);
    main->m_State.contextMessage.Set(msg);
    
    // TODO: make sure m_smvp is populated, else do not draw directly

    //static void             AddDrawListToDrawData(ImVector<ImDrawList*>* out_list, ImDrawList* draw_list);
    
    
    ImGuiIO& io = ImGui::GetIO();

    auto context_message_time = std::chrono::system_clock::now();
    std::chrono::duration<double, std::milli> time_since_last_update = context_message_time - main->m_State.m_last_update_time;

    //if (time_since_last_update.count() < 5000) // do not draw directly when the application is still responsive
    //    return;
    
    if (!GImGui)
        return;

    main->m_EventLog.DirectUpdate(main->m_State);

    /*ImGuiContext& g = *GImGui;
    GLFWwindow* current_context_backup = glfwGetCurrentContext(); // Get current active viewport and store as backup
    ImDrawData direct_eventlog_draw_data;
    
    ImDrawListSharedData shared_drawlist_data;
    shared_drawlist_data.TexUvWhitePixel = g.DrawListSharedData.TexUvWhitePixel;

    ImDrawList draw_list(&shared_drawlist_data);
    
    draw_list._ResetForNewFrame();
    draw_list.PushTextureID(g.Font->ContainerAtlas->TexID);
    draw_list.PushClipRect(m_smvp.display_pos, ImVec2(m_smvp.display_pos.x + m_smvp.display_size.x, m_smvp.display_pos.y + m_smvp.display_size.y), false);

    // add draw lists to vector
    ImVector<ImDrawList*> out_list;
    out_list.push_back(&draw_list);

    // init draw data
    direct_eventlog_draw_data.CmdLists = out_list.Data;
    direct_eventlog_draw_data.DisplayPos = m_smvp.display_pos; // Set viewport to status message area: DisplayPos DisplaySize
    direct_eventlog_draw_data.DisplaySize = m_smvp.display_size;
    direct_eventlog_draw_data.FramebufferScale = m_smvp.frame_buffer_scale;
    direct_eventlog_draw_data.CmdListsCount = out_list.Size;

    // make eventlog window the current context
    ImGui_ImplGlfw_ViewportData* viewport_data = static_cast<ImGui_ImplGlfw_ViewportData*>(m_smvp.vp->PlatformUserData);
    glfwMakeContextCurrent(viewport_data->Window); // Make EventLog viewport active
    SetTextBackgroundColor(ImGui::CalcTextSize(main->m_State.contextMessage.Get().c_str()), ImGui::GetColorU32(ImGuiCol_WindowBg), &draw_list, &m_smvp.cursor_pos);
    draw_list.AddText(g.Font, g.FontSize, m_smvp.cursor_pos, ImColor(255, 0, 0, 255), main->m_State.contextMessage.Get().c_str(), NULL, 0.0f); //  ImGui::FindRenderedTextEnd(main->m_State.contextMessage.Get().c_str()

    ImGui_ImplOpenGL3_RenderDrawData(&direct_eventlog_draw_data);
    glfwSwapBuffers(viewport_data->Window);
    glfwMakeContextCurrent(current_context_backup); // restore backup_viewport
    */
    return;
}

void GuiEventLog::GeoDMSMessage(ClientHandle clientHandle, SeverityTypeID st, CharPtr msg)
{
    auto main = reinterpret_cast<GuiMainComponent*>(clientHandle);
    assert(main);
    main->m_EventLog.AddLog(st, msg);
    PostEmptyEventToGLFWContext();
    return;
    //auto g = ImGui::GetCurrentContext();
//    if (g->FrameCountEnded == g->FrameCount)
//        DirectUpdateEventLog(main);
}

