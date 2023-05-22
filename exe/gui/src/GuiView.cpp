#include <imgui.h>
#include <imgui_internal.h>


#include "GuiMain.h"

#include <format>
#include <sstream>
#include <string>

#include "GuiView.h"
#include "GuiTableView.h"

#include "TicInterface.h"
#include "ShvDllInterface.h"
#include "StateChangeNotification.h"
#include "AbstrUnit.h"
#include "AbstrDataItem.h"
#include "UnitProcessor.h"
#include "DataArray.h"
#include "DataLocks.h"
#include "DataView.h"
#include "dbg/DmsCatch.h"
#include "TreeItem.h"
#include "utl/mySPrintF.h"
#include "ClcInterface.h"
#include "Explain.h"

#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#include <winuser.h>

DMSView::~DMSView()
{
    Reset();
}

DMSView::DMSView(DMSView&& other) noexcept
{
    *this = std::move(other);
}

auto DMSView::operator=(DMSView&& other) noexcept -> void
{
    Reset();
    m_Name       = std::move(other.m_Name);
    m_Icon       = std::move(other.m_Icon);
    m_ViewStyle  = std::move(other.m_ViewStyle);
    m_DataView   = std::move(other.m_DataView);
    m_HWND       = std::move(other.m_HWND);
    m_DoView     = std::move(other.m_DoView);
    m_ShowWindow = std::move(other.m_ShowWindow);
    has_been_docking_initialized = std::move(other.has_been_docking_initialized);

    other.m_HWND = NULL;
    other.m_DataView = NULL;
}

static void DockNodeAddWindow(ImGuiDockNode* node, ImGuiWindow* window, bool add_to_tab_bar)
{
    auto ctx = ImGui::GetCurrentContext();
    IM_ASSERT(window->DockNode == NULL || window->DockNodeAsHost == NULL);

    node->Windows.push_back(window);
    node->WantHiddenTabBarUpdate = true;
    window->DockNode = node;
    window->DockId = node->ID;
    window->DockIsActive = (node->Windows.Size > 1);
    window->DockTabWantClose = false;

    // When reactivating a node with one or two loose window, the window pos/size/viewport are authoritative over the node storage.
    // In particular it is important we init the viewport from the first window so we don't create two viewports and drop one.
    if (node->HostWindow == NULL && node->IsFloatingNode())
    {
        if (node->AuthorityForPos == ImGuiDataAuthority_Auto)
            node->AuthorityForPos = ImGuiDataAuthority_Window;
        if (node->AuthorityForSize == ImGuiDataAuthority_Auto)
            node->AuthorityForSize = ImGuiDataAuthority_Window;
        if (node->AuthorityForViewport == ImGuiDataAuthority_Auto)
            node->AuthorityForViewport = ImGuiDataAuthority_Window;
    }

    // Add to tab bar if requested
    if (add_to_tab_bar)
    {
        if (node->TabBar == NULL)
        {
            IM_ASSERT(node->TabBar == NULL);
            node->TabBar = IM_NEW(ImGuiTabBar);
            node->TabBar->SelectedTabId = node->TabBar->NextSelectedTabId = node->SelectedTabId;

            // Add existing windows
            for (int n = 0; n < node->Windows.Size - 1; n++)
                ImGui::TabBarAddTab(node->TabBar, ImGuiTabItemFlags_None, node->Windows[n]);
        }
        ImGui::TabBarAddTab(node->TabBar, ImGuiTabItemFlags_Unsorted, window);
    }

    //ImGui::DockNodeUpdateVisibleFlag(node);

    // Update this without waiting for the next time we Begin() in the window, so our host window will have the proper title bar color on its first frame.
    if (node->HostWindow)
        ImGui::UpdateWindowParentAndRootLinks(window, window->Flags | ImGuiWindowFlags_ChildWindow, node->HostWindow);
}

bool IsDocked() // test if the MapViewWindow is docked inside the ImGui main window.
{
    auto glfw_window = glfwGetCurrentContext();
    auto mainWindow = glfwGetWin32Window(glfw_window);
    auto window = ImGui::GetCurrentWindow();
    if (window)
        return mainWindow == (HWND)window->Viewport->PlatformHandleRaw;
    return false; // TODO: throw error
}

void CreateDockNodeForFloatingWindowIfNecessary(const bool has_been_docking_initialized, ImGuiWindow* window)
{
    if (has_been_docking_initialized && !IsDocked() && window->DockNodeAsHost == NULL && window->DockNode == NULL) // floating window
    {
        auto ctx = ImGui::GetCurrentContext();
        auto id = ImGui::DockContextGenNodeID(ctx);
        ImGuiDockNode* node = IM_NEW(ImGuiDockNode)(id);
        ctx->DockContext.Nodes.SetVoidPtr(node->ID, node);
        node->Pos = window->Pos;
        node->Size = window->Size;
        DockNodeAddWindow(node, window, true);
        node->TabBar->Tabs[0].Flags &= ~ImGuiTabItemFlags_Unsorted;
        window->DockIsActive = true;
    }
}

/*void UglyStackOverFlowUtility()
{

    while (true)
    {
        UglyStackOverFlowUtility();
    }
}*/

bool DMSView::Update(GuiState& state)
{
    //UglyStackOverFlowUtility();
    
    auto event_queues = GuiEventQueues::getInstance();

    // Open window
    ImGui::SetNextWindowSize(ImVec2(500, 500), ImGuiCond_FirstUseEver); // TODO: use OnCaptionChanged to dynamically update caption
    ImGui::SetNextWindowDockID(GetGeoDMSDataViewAreaNodeID(state), ImGuiCond_Once);
    if (!ImGui::Begin((m_Icon + m_DataView->GetCaption().c_str() + m_Name).c_str(), &m_DoView, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoTitleBar) || CloseWindowOnMimimumSize() || !m_HWND)//|| m_Views.empty())
    {
        if (m_HWND)
            ShowOrHideWindow(false);

        ImGui::End();
        return false;
    }

    auto view_window = ImGui::GetCurrentWindow();

    static bool test = false;

    auto view_window_hwnd_handle = (HWND)view_window->Viewport->PlatformHandleRaw;
    auto view_window_parent = GetParent(view_window_hwnd_handle);
    if (!test )//!view_window_parent) // parent not set, should always be GuiState::m_MainWindow;
    {
        //SetParent(view_window_hwnd_handle, glfwGetWin32Window(state.m_MainWindow));
        
        //ImGui::UpdateWindowParentAndRootLinks(view_window, ImGuiWindowFlags_::ImGuiWindowFlags_None, ImGui::FindWindowByName("GeoDMSGui"));
        test = true;
    }
    //auto error_code_1 = GetLastError();

    //view_window_parent = GetParent(view_window_hwnd_handle);
    //auto error_code_2 = GetLastError();
    

    CreateDockNodeForFloatingWindowIfNecessary(has_been_docking_initialized, view_window);

    // handle events
    EventQueue* eventQueuePtr = nullptr;
    switch (m_ViewStyle)
    {
    case ViewStyle::tvsMapView:
        eventQueuePtr = &event_queues->MapViewEvents;
        break;
    case ViewStyle::tvsTableContainer:
    case ViewStyle::tvsTableView:
        eventQueuePtr = &event_queues->TableViewEvents;
        break;
    default:
        break;
    }
    while (eventQueuePtr->HasEvents())
    {
        auto view_event = eventQueuePtr->Pop();
        //if (state.GetCurrentItem()) // TODO: candidate for removal
        //    ProcessEvent(view_event, state.GetCurrentItem());
    }

    // drag-drop target
    ImGui::Dummy(ImGui::GetWindowSize());
    if (ImGui::BeginDragDropTarget())
    {
        auto payload = ImGui::AcceptDragDropPayload("TreeItemPtr");
        if (payload)
        {
            auto droppedTreeItem = reinterpret_cast<const char*>(payload->Data);
            if (droppedTreeItem)
            {
                if (SHV_DataView_CanContain(m_DataView, state.GetCurrentItem()))
                    SHV_DataView_AddItem(m_DataView, state.GetCurrentItem(), false);
            }
        }
        ImGui::EndDragDropTarget();
    }

    // update parent window if changed
    auto parentWindowState = UpdateParentWindow();
    if (parentWindowState == WindowState::UNINITIALIZED)// || (m_ViewStyle == ViewStyle::tvsMapView && !IsDataItem(currentItem)))
    {
        ImGui::End();
        return false;
    }

    assert(m_HWNDParent);
    assert(m_HWND);



    if (parentWindowState == WindowState::CHANGED)
    {
        SetParent(m_HWND, m_HWNDParent); // set new parent on dock/undock
        UpdateWindowPosition();
        ShowOrHideWindow(false);
    }

    bool result = false;

    // update window
    SHV_DataView_Update(m_DataView);

    auto show = !(ImGui::IsMouseDragging(ImGuiMouseButton_Left) && ImGui::IsMouseHoveringRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax()));
    ShowOrHideWindow(show);
    UpdateWindowPosition();
    ImGui::End();

    SuspendTrigger::Resume();

    if (!has_been_docking_initialized)
    {
        if (view_window->DockId) // window has a dockid, so it is docked
        {
            has_been_docking_initialized = true;
            return result;
        }

        //if (TryDockViewInGeoDMSDataViewAreaNode(state, view_window)) // TODO: check if this is the correct window.
            has_been_docking_initialized = true;
    }

    return result;
}

auto DMSView::Reset() -> void
{
    if (m_HWND)
    {
        DestroyWindow(m_HWND);
        m_HWND = nullptr;
        m_DataView = nullptr;
    }
}

auto DMSView::CloseWindowOnMimimumSize() -> bool
{
    auto crm = ImGui::GetContentRegionMax();
    static int min_szx = 50, min_szy = 50;
    if (crm.x <= min_szx || crm.y <= min_szy)
    {
        ShowOrHideWindow(false);
        return true;
    }

    return false;
}

auto DMSView::ShowOrHideWindow(bool show) -> void
{
    if (m_ShowWindow != show) // change in show state
    {
        m_ShowWindow = show;
        ShowWindow(m_HWND, m_ShowWindow ? SW_NORMAL : SW_HIDE);
    }
}

auto DMSView::UpdateParentWindow() -> WindowState
{
    auto glfw_window = glfwGetCurrentContext();
    auto mainWindow = glfwGetWin32Window(glfw_window);
    auto window = ImGui::GetCurrentWindow(); //ImGui::FindWindowByName(m_ViewName.c_str());
    if (!window || !(HWND)window->Viewport->PlatformHandleRaw)
        return WindowState::UNINITIALIZED;

    if (m_HWNDParent == (HWND)window->Viewport->PlatformHandleRaw)
        return WindowState::UNCHANGED;

    m_HWNDParent = (HWND)window->Viewport->PlatformHandleRaw;
    return WindowState::CHANGED;
}

auto DMSView::UpdateWindowPosition() -> void
{
    if (!m_HWND)
        return;

    ImVec2 crMin = ImGui::GetWindowContentRegionMin();
    ImVec2 crMax = ImGui::GetWindowContentRegionMax();
    ImVec2 cursor_screen_pos = ImGui::GetCursorScreenPos();
    auto wPos = ImGui::GetWindowPos();
    auto mouse_pos = ImGui::GetMousePos();
    auto content_region_avail = ImGui::GetContentRegionAvail();

    ImGuiIO& io = ImGui::GetIO();
    //float fb_height = io.DisplaySize.y * io.DisplayFramebufferScale.y;
    //float fb_width = io.DisplaySize.x * io.DisplayFramebufferScale.x;

    //auto test = ImGui::GetWindowContentRegionMax();
    

    int xpos, ypos;
    glfwGetWindowPos(glfwGetCurrentContext(), &xpos, &ypos); // content area pos

    //auto test1 = crMin.x + wPos.x - xpos;
    //auto test2 = crMin.y + wPos.y - ypos;

    auto is_appearing = ImGui::IsWindowAppearing();
    auto is_item_visible = ImGui::IsItemVisible();
    if (IsDocked())
    {
        SetWindowPos(
            m_HWND,
            NULL,
            crMin.x + wPos.x - xpos,    // The new position of the left side of the window, in client coordinates.
            crMin.y + wPos.y - ypos,    // The new position of the top of the window, in client coordinates.
            crMax.x - crMin.x,          // The new width of the window, in pixels.
            crMax.y - crMin.y,          // The new height of the window, in pixels.
            m_ShowWindow ? SWP_SHOWWINDOW : SWP_HIDEWINDOW
        );
    }
    else
    {
        auto offset = GetRootParentCurrentWindowOffset();
        SetWindowPos(// TODO: When docked, use offset of mainwindow + this window to get correct position.
            m_HWND,
            NULL,
            crMin.x + offset.x, // The new position of the left side of the window, in client coordinates.
            crMin.y + offset.y, // The new position of the top of the window, in client coordinates.
            crMax.x - crMin.x,  // The new width of the window, in pixels.
            crMax.y - crMin.y,  // The new height of the window, in pixels.
            m_ShowWindow ? SWP_SHOWWINDOW : SWP_HIDEWINDOW
        );
    }
}

auto DMSView::GetRootParentCurrentWindowOffset() -> ImVec2
{
    ImVec2 parentPos(0, 0);
    ImVec2 offset(0, 0);
    ImGuiWindow* parentWindow = nullptr;
    auto currWindow = ImGui::GetCurrentWindow(); // assume there is a current window active
    dms_assert(currWindow);

    parentWindow = currWindow->ParentWindow;
    if (!parentWindow)
        return offset;

    while (parentWindow)
    {
        parentPos = parentWindow->Pos;
        parentWindow = parentWindow->ParentWindow;
    }
    auto wPos = ImGui::GetWindowPos();
    offset.x = wPos.x - parentPos.x;
    offset.y = wPos.y - parentPos.y;
    return offset;
}

auto DMSView::InitWindow(TreeItem* currentItem) -> WindowState
{
    //TODO: simplify, make function more pure; remove side effects
    ImVec2 crMin = ImGui::GetWindowContentRegionMin();
    ImVec2 crMax = ImGui::GetWindowContentRegionMax();
    HINSTANCE instance = GetInstance(m_HWNDParent);//m_Views.at(m_ViewIndex).m_HWNDParent);
    RegisterViewAreaWindowClass(instance);
    auto vs = m_ViewStyle == tvsMapView ? WS_DLGFRAME | WS_CHILD : WS_CHILD;
    m_HWND = CreateWindowEx(
        0L,                            // no extended styles
        m_Name.c_str(),      // MapView control class 
        nullptr,                       // text for window title bar 
        vs,                            // styles
        CW_USEDEFAULT,                 // horizontal position 
        CW_USEDEFAULT,                 // vertical position
        crMax.x - crMin.x,             // width
        crMax.y - crMin.y,             // height 
        m_HWNDParent,        // handle to parent
        nullptr,                       // no menu
        instance,                      // instance owning this window 
        m_DataView           //m_DataView                                       
    );

    if (!m_HWND)
    {
        return WindowState::UNINITIALIZED;
    }

    SHV_DataView_Update(m_DataView);
    UpdateWindowPosition();
    ShowOrHideWindow(true);
    return WindowState::CHANGED;
}

auto DMSView::RegisterViewAreaWindowClass(HINSTANCE instance) -> void
{
    WNDCLASSEX wndClassData;
    wndClassData.cbSize = sizeof(WNDCLASSEX);
    wndClassData.style = CS_DBLCLKS; // CS_OWNDC
    wndClassData.lpfnWndProc = &DataViewWndProc;
    wndClassData.cbClsExtra = 0;
    wndClassData.cbWndExtra = sizeof(DataView*);
    wndClassData.hInstance = instance;
    wndClassData.hIcon = NULL;
    wndClassData.hCursor = NULL;
    wndClassData.hbrBackground = HBRUSH(COLOR_WINDOW + 1);
    wndClassData.lpszMenuName = NULL;
    wndClassData.lpszClassName = m_Name.c_str(); //m_Views.at(m_ViewIndex).m_Name.c_str();
    wndClassData.hIconSm = NULL;

    RegisterClassEx(&wndClassData);
}

StatisticsView::StatisticsView(GuiState& state, std::string name) // TODO: move statistics view to separate .h and .cpp file
{
    m_item = state.GetCurrentItem();
    m_Name = "\xE2\x88\x91 " + std::string("Statistics for ") + std::string(m_item->GetName().c_str()) + name; //  "\xE2\x88\x91" 
}

StatisticsView::~StatisticsView()
{
    m_item.release();
}

void StatisticsView::UpdateData()
{
    if (!m_item) // TODO: make sure m_item gets cleared when opening a new configuration
        return;

//    DMS_ExplainValue_Clear(); // TODO: is this necessary? See fMain.pas line 629

    m_done = false;
    m_data = DMS_NumericDataItem_GetStatistics(m_item, nullptr);//&m_done);
    //std::string statistics_string = DMS_NumericDataItem_GetStatistics(m_item, nullptr);//&m_done);
    //m_data = StringToTable(std::move(statistics_string), ":"); // CODE REVIEW: why a table ?

    m_is_ready = m_done;

    auto ps = DMS_TreeItem_GetProgressState(m_item);
    if (ps > NotificationCode::NC2_MetaReady && ps <= NotificationCode::NC2_Committed) //TODO: fix bug with DMS_NumericDataItem_GetStatistics bool ptr
    {
        m_is_ready = true;
        m_item = nullptr;
    }
}

bool StatisticsView::Update(GuiState& state)
{
    if (!m_is_ready)
        UpdateData(); // TODO: performance wise, check every idle time frame?

    ImGui::SetNextWindowSize(ImVec2(500, 500), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowDockID(GetGeoDMSDataViewAreaNodeID(state), ImGuiCond_Once);
    if (!ImGui::Begin(m_Name.c_str(), &m_DoView, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoTitleBar))
    {
        ImGui::End();
        return false;
    }
    auto view_window = ImGui::GetCurrentWindow();
    CreateDockNodeForFloatingWindowIfNecessary(has_been_docking_initialized, view_window); //TODO: candidate for removal, likely unnecessary here

    if (!has_been_docking_initialized)
    {
        if (view_window->DockId) // window has a dockid, so it is docked
            has_been_docking_initialized = true;
        else if (TryDockViewInGeoDMSDataViewAreaNode(state, view_window)) // TODO: check if this is the correct window.
            has_been_docking_initialized = true;
    }

    ImGui::TextWrapped(m_data.c_str());
    ImGui::End();

    return true;
}

auto GuiViews::ProcessEvent(GuiEvents event, TreeItem* currentItem) -> void
{
    switch (event)
    {
    case GuiEvents::UpdateCurrentItem: break;
    case GuiEvents::UpdateCurrentAndCompatibleSubItems: break;
    case GuiEvents::OpenInMemoryDataView: break;
    default: break;
    }
}

auto GuiViews::GetHWND() -> HWND
{
    return m_dms_view_it->m_HWND; //m_Views.at(m_ViewIndex).m_HWND;
}

auto GuiViews::AddDMSView(GuiState& state, ViewStyle vs, std::string name) -> void
{
    auto current_item = state.GetCurrentItem();
    if (!current_item)
        return;

    static int s_ViewCounter = 0;
    auto rootItem = dynamic_cast<const TreeItem*>(current_item->GetRoot());
    auto desktopItem = GetDefaultDesktopContainer(rootItem); // rootItem->CreateItemFromPath("DesktopInfo");
    auto viewContextItem = desktopItem->CreateItemFromPath(mySSPrintF("View%d", s_ViewCounter++).c_str());

    m_dms_views.emplace_back(name, vs, SHV_DataView_Create(viewContextItem, vs, ShvSyncMode::SM_Load));
    m_dms_view_it = --m_dms_views.end();
    m_dms_view_it->UpdateParentWindow(); // m_Views.back().UpdateParentWindow(); // Needed before InitWindow
    m_dms_view_it->InitWindow(current_item); // m_Views.back().InitWindow(state.GetCurrentItem());
    SHV_DataView_AddItem(m_dms_view_it->m_DataView, current_item, false);
    m_AddCurrentItem = true;
}

void GuiViews::AddTableView(GuiState& state)
{
    auto current_item = state.GetCurrentItem();
    if (!current_item)
        return;

    m_table_views.emplace_back(state, ICON_RI_TABLE + std::string(" ") + current_item->GetName().c_str() + std::string(" ###ImGuiTableView") + std::to_string(m_table_views.size()));
}

void GuiViews::AddStatisticsView(GuiState& state, std::string name)
{
    auto current_item = state.GetCurrentItem();
    if (!current_item)
        return;

    m_statistic_views.emplace_back(state, name);
}

/*auto GuiViews::InitWindowParameterized(std::string caption, DataView* dv, ViewStyle vs, HWND parent_hwnd, UInt32 min, UInt32 max) -> HWND
{
    HINSTANCE instance = GetInstance(parent_hwnd);
    RegisterViewAreaWindowClass(instance);
    //auto vs = m_ViewIt->m_ViewStyle == tvsMapView ? WS_DLGFRAME | WS_CHILD : WS_CHILD;
    return CreateWindowEx(
        0L,                            // no extended styles
        caption.c_str(),      // MapView control class 
        nullptr,                       // text for window title bar 
        vs,                            // styles
        CW_USEDEFAULT,                 // horizontal position 
        CW_USEDEFAULT,                 // vertical position
        min,             // width
        max,             // height 
        parent_hwnd,        // handle to parent
        nullptr,                       // no menu
        instance,                      // instance owning this window 
        dv           //m_DataView                                       
    );
}*/

auto GuiViews::CloseAll() -> void
{
    m_dms_views.clear();
    m_statistic_views.clear();
}

GuiViews::~GuiViews(){}

auto GuiViews::UpdateAll(GuiState& state) -> void
{
    // determine clientarea docknode, if available
    /*ImGuiID current_clientarea_docknode = GetGeoDMSDataViewAreaNodeID(state);
    if (m_dms_view_it != m_dms_views.end())
    {
        // TODO: get imgui window ptr associated with m_dms_view_it
        current_clientarea_docknode = m_dms_view_it->m_Name
    }*/


    //auto edit_palette_it = m_EditPaletteWindows.begin();
    //for (auto& palette_editor : m_edit_palette_windows)
    //    palette_editor.Update(state);




    // update DMS views
    auto it = m_dms_views.begin();
    while (it != m_dms_views.end())
    {
        if (it->m_DoView && m_dms_view_it->m_DataView && IsWindow(it->m_HWND))
        {
            if (it->Update(state) && it._Ptr && m_dms_view_it != it)
                m_dms_view_it = it;

            ++it;
        }
        else // view to be destroyed
        {
            it = m_dms_views.erase(it);
            if (it == m_dms_views.end())
                m_dms_view_it = m_dms_views.begin();
            else
                m_dms_view_it = it; // TODO: m_ViewIt should be restored to the previously set m_ViewIt
        }
    }

    // update table views
    auto table_it = m_table_views.begin();
    while (table_it != m_table_views.end())
    {
        if (table_it->m_DoView)
        {
            table_it->Update(state) && table_it._Ptr;
            ++table_it;
        }
        else // view to be destroyed
            table_it = m_table_views.erase(table_it);
    }

    // update Statistic views
    auto stat_it = m_statistic_views.begin();
    while (stat_it != m_statistic_views.end())
    {
        if (stat_it->m_DoView)
        {
            stat_it->Update(state);
            ++stat_it;
        }
        else // view to be destroyed
            stat_it = m_statistic_views.erase(stat_it);
    }
}

/*#include <imgui_impl_opengl3.h>
#include <imgui_impl_glfw.h>
class ImGuiFrame
{
public:
    ImGuiFrame(GuiMainComponent* main)
    {
        m_main_ptr = main;
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame(); // TODO: set  to true for UpdateInputEvents?
    }

    ~ImGuiFrame()
    {
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(m_main_ptr->m_MainWindow, &display_w, &display_h);
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
        glfwSwapBuffers(m_main_ptr->m_MainWindow);
    }

private:
    GuiMainComponent* m_main_ptr = nullptr;
};*/

void GuiViews::ResetEditPaletteWindow(ClientHandle clientHandle, const TreeItem* self)
{
    auto gui_main_component_ptr = reinterpret_cast<GuiMainComponent*>(clientHandle);
    
    auto mdi_create_struct_ptr = reinterpret_cast<MdiCreateStruct*>(const_cast<TreeItem*>(self));
    
    //m_EditPaletteWindows.emplace_back(mdi_create_struct_ptr->caption, mdi_create_struct_ptr->ct, mdi_create_struct_ptr->dataView->shared_from_this());

    /*m_EditPaletteWindow = std::make_unique<View>(mdi_create_struct_ptr->caption, mdi_create_struct_ptr->ct, mdi_create_struct_ptr->dataView);

    { // create initial empty window, we need a parent
        ImGuiFrame test = { gui_main_component_ptr };
        if (ImGui::Begin(mdi_create_struct_ptr->caption, &m_EditPaletteWindow->m_DoView))
        {
            ImGui::Text("test");
            ImGui::End();
        }
    }

    if (m_EditPaletteWindow)
    {
        auto window = ImGui::FindWindowByName(mdi_create_struct_ptr->caption);
        auto tv_window = ImGui::FindWindowByName("TreeView");
        HWND parent_hwnd = nullptr;
        if (window)
        {
            parent_hwnd = (HWND)window->Viewport->PlatformHandleRaw;
        }

        //ImVec2 crMin = ImGui::GetWindowContentRegionMin();
        //ImVec2 crMax = ImGui::GetWindowContentRegionMax();

        m_EditPaletteWindow->m_HWND = InitWindowParameterized(mdi_create_struct_ptr->caption, m_EditPaletteWindow->m_DataView, m_EditPaletteWindow->m_ViewStyle, parent_hwnd, 100, 100);
        mdi_create_struct_ptr->hWnd = m_EditPaletteWindow->m_HWND;
    }*/
}

/*bool GuiViews::Update(GuiState& state, View& view)
{
    auto event_queues = GuiEventQueues::getInstance();

    // Open window
    ImGui::SetNextWindowSize(ImVec2(500, 500), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin( (view.m_DataView->GetCaption().c_str() + view.m_Name).c_str(), &view.m_DoView, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoTitleBar) || CloseWindowOnMimimumSize(view) || !view.m_HWND)//|| m_Views.empty())
    {
        if (view.m_HWND)
            view.ShowOrHideWindow(false);

        ImGui::End();
        return false;
    }

    auto view_window = ImGui::GetCurrentWindow(); //TODO: temporary
    if (view.has_been_docking_initialized && !view.IsDocked() && view_window->DockNodeAsHost == NULL && view_window->DockNode == NULL) // floating window
    {
        auto ctx = ImGui::GetCurrentContext();
        auto id = ImGui::DockContextGenNodeID(ctx);
        ImGuiDockNode* node = IM_NEW(ImGuiDockNode)(id);
        ctx->DockContext.Nodes.SetVoidPtr(node->ID, node);
        node->Pos = view_window->Pos;
        node->Size = view_window->Size;
        DockNodeAddWindow(node, view_window, true);
        node->TabBar->Tabs[0].Flags &= ~ImGuiTabItemFlags_Unsorted;
        view_window->DockIsActive = true;

    }
    
    if (!ImGui::GetWindowAlwaysWantOwnTabBar(view_window))
        view_window->WindowClass.DockingAlwaysTabBar = true; // Floating view windows always have their own tabbar
        

    // handle events
    EventQueue* eventQueuePtr = nullptr;
    switch (view.m_ViewStyle)
    {
    case ViewStyle::tvsMapView:
        eventQueuePtr = &event_queues->MapViewEvents;
        break;
    case ViewStyle::tvsTableContainer:
    case ViewStyle::tvsTableView:
        eventQueuePtr = &event_queues->TableViewEvents;
        break;
    default:
        break;
    }
    while (eventQueuePtr->HasEvents())
    {
        auto view_event = eventQueuePtr->Pop();
        if (state.GetCurrentItem())
            ProcessEvent(view_event, state.GetCurrentItem());
    }

    // drag-drop target
    ImGui::Dummy(ImGui::GetWindowSize());
    if (ImGui::BeginDragDropTarget())
    {
        auto payload = ImGui::AcceptDragDropPayload("TreeItemPtr");
        if (payload)
        {
            auto droppedTreeItem = reinterpret_cast<const char*>(payload->Data);
            if (droppedTreeItem)
            {
                if (SHV_DataView_CanContain(view.m_DataView, state.GetCurrentItem()))
                    SHV_DataView_AddItem(view.m_DataView, state.GetCurrentItem(), false);
            }
        }
        ImGui::EndDragDropTarget();
    }
    
    // update parent window if changed
    auto parentWindowState = UpdateParentWindow(view);
    if (parentWindowState == WindowState::UNINITIALIZED)// || (m_ViewStyle == ViewStyle::tvsMapView && !IsDataItem(currentItem)))
    {
        ImGui::End();
        return false;
    }
    
    assert(view.m_HWNDParent);
    assert(view.m_HWND);

    if (parentWindowState == WindowState::CHANGED)
    {
        SetParent(view.m_HWND, view.m_HWNDParent); // set new parent on dock/undock
        UpdateWindowPosition(view);
        ShowOrHideWindow(view, false);
    }

    // If view window is focused, focus imgui window as well
    bool result = false;
    if (ImGui::IsWindowFocused() || GetFocus() == view.m_HWND)
    {
        result = true;
    }

    // update window
    SHV_DataView_Update(view.m_DataView);

    auto show = !(ImGui::IsMouseDragging(ImGuiMouseButton_Left) && ImGui::IsMouseHoveringRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax()));
    ShowOrHideWindow(view, show);
    UpdateWindowPosition(view);


    SuspendTrigger::Resume();

    

    ImGui::End();





    if (!view.has_been_docking_initialized)// && m_Views.size()==1)
    {
        //DockNodeCalcDropRectsAndTestMousePos

        if (view_window->DockId)
        {
            view.has_been_docking_initialized = true;
            return result;
        }

        if (TryDockViewInGeoDMSDataViewAreaNode(state, view_window)) // TODO: check if this is the correct window.
            view.has_been_docking_initialized = true;

        //auto ctx = ImGui::GetCurrentContext();
        //ImGuiDockContext* dc = &ctx->DockContext;
        ////auto dockspace_id = ImGui::GetID("GeoDMSDockSpace");

        //auto dockspace_docknode = (ImGuiDockNode*)dc->Nodes.GetVoidPtr(state.dockspace_id);
        //if (dockspace_docknode && dockspace_docknode->HostWindow)
        //{
        //    ImGui::DockContextQueueDock(ctx, dockspace_docknode->HostWindow, dockspace_docknode, window, ImGuiDir_None, 0.0f, false);
        //    view.has_been_docking_initialized = true;
        //}
    }

    return result;
}*/

auto GuiViews::OnOpenEditPaletteWindow(ClientHandle clientHandle, const TreeItem* self, NotificationCode notificationCode) -> void
{
    auto gui_main_component_ptr = reinterpret_cast<GuiMainComponent*>(clientHandle);
    if (notificationCode == NotificationCode::CC_CreateMdiChild) // TODO: this should have its own separate callback
    {
        
        gui_main_component_ptr->m_Views.ResetEditPaletteWindow(clientHandle, self);

        //mdi_create_struct_ptr->dataView->CreateViewWindow(mdi_create_struct_ptr->dataView, mdi_create_struct_ptr->caption);
        //event_queues->MapViewEvents.Add();
    }
    PostEmptyEventToGLFWContext();
    return;
}