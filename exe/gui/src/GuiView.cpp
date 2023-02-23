#include <imgui.h>
#include <imgui_internal.h>


#include "GuiMain.h"

#include <format>
#include <sstream>
#include <string>

#include "GuiView.h"
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

#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#include <winuser.h>

View::~View()
{
    Reset();
}

View::View(View&& other) noexcept
{
    *this = std::move(other);
}

auto View::operator=(View && other) noexcept -> void
{
    Reset();
    m_Name       = std::move(other.m_Name);
    m_ViewStyle  = std::move(other.m_ViewStyle);
    m_DataView   = std::move(other.m_DataView);
    m_HWND       = std::move(other.m_HWND);
    m_DoView     = std::move(other.m_DoView);
    m_ShowWindow = std::move(other.m_ShowWindow);
    has_been_docking_initialized = std::move(other.has_been_docking_initialized);

    other.m_HWND = NULL;
    other.m_DataView = NULL;
}

auto View::Reset() -> void
{
    if (m_HWND)
    {
        DestroyWindow(m_HWND);
        m_HWND = nullptr;
        m_DataView = nullptr;
    }
}

auto GuiView::ProcessEvent(GuiEvents event, TreeItem* currentItem) -> void
{
    switch (event)
    {
    case GuiEvents::UpdateCurrentItem: break;
    case GuiEvents::UpdateCurrentAndCompatibleSubItems: break;
    case GuiEvents::OpenInMemoryDataView: break;
    default: break;
    }
}

auto GuiView::UpdateParentWindow(View& view) -> WindowState
{
    auto glfw_window = glfwGetCurrentContext(); //TODO: known bug, parent window likely not updated correctly in case of outside main window view docked into another view
    auto mainWindow  = glfwGetWin32Window(glfw_window);
    auto window      = ImGui::GetCurrentWindow(); //ImGui::FindWindowByName(m_ViewName.c_str());
    if (!window || !(HWND)window->Viewport->PlatformHandleRaw)
        return WindowState::UNINITIALIZED;

    if (view.m_HWNDParent == (HWND)window->Viewport->PlatformHandleRaw)
        return WindowState::UNCHANGED;
        
    view.m_HWNDParent = (HWND)window->Viewport->PlatformHandleRaw;
    return WindowState::CHANGED;
}

auto GuiView::IsDocked() -> bool // test if the MapViewWindow is docked inside the ImGui main window.
{
    auto glfw_window = glfwGetCurrentContext();
    auto mainWindow = glfwGetWin32Window(glfw_window);
    auto window = ImGui::GetCurrentWindow();//ImGui::FindWindowByName(m_ViewName.c_str()); // GetCurrentWindow
    if (window)
        return mainWindow == (HWND)window->Viewport->PlatformHandleRaw;
    return false; // TODO: this does not make sense.
}

auto GuiView::GetRootParentCurrentWindowOffset() -> ImVec2
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

auto GuiView::ShowOrHideWindow(View& view, bool show) -> void
{
    if (view.m_ShowWindow != show) // change in show state
    {
        view.m_ShowWindow = show;
        ShowWindow(view.m_HWND, view.m_ShowWindow ? SW_NORMAL : SW_HIDE);
    }
}

auto GuiView::UpdateWindowPosition(View& view) -> void
{
    if (!view.m_HWND)
        return;

    ImVec2 crMin = ImGui::GetWindowContentRegionMin();
    ImVec2 crMax = ImGui::GetWindowContentRegionMax();
    ImVec2 cursor_screen_pos = ImGui::GetCursorScreenPos();
    auto wPos = ImGui::GetWindowPos();
    auto mouse_pos = ImGui::GetMousePos();
    auto content_region_avail = ImGui::GetContentRegionAvail();

    ImGuiIO& io = ImGui::GetIO();
    float fb_height = io.DisplaySize.y * io.DisplayFramebufferScale.y;
    float fb_width = io.DisplaySize.x * io.DisplayFramebufferScale.x;

    int xpos, ypos;
    glfwGetWindowPos(glfwGetCurrentContext(), &xpos, &ypos); // content area pos

    auto is_appearing = ImGui::IsWindowAppearing();
    auto is_item_visible = ImGui::IsItemVisible();
    if (IsDocked())
    {
        SetWindowPos(
            view.m_HWND,
            NULL,
            crMin.x + wPos.x - xpos,    // The new position of the left side of the window, in client coordinates.
            crMin.y + wPos.y - ypos,    // The new position of the top of the window, in client coordinates.
            crMax.x - crMin.x,          // The new width of the window, in pixels.
            crMax.y - crMin.y,          // The new height of the window, in pixels.
            view.m_ShowWindow ? SWP_SHOWWINDOW : SWP_HIDEWINDOW
        );
    }
    else
    {
        auto offset = GetRootParentCurrentWindowOffset();
        SetWindowPos(// TODO: When docked, use offset of mainwindow + this window to get correct position.
            view.m_HWND,
            NULL,
            crMin.x + offset.x, // The new position of the left side of the window, in client coordinates.
            crMin.y + offset.y, // The new position of the top of the window, in client coordinates.
            crMax.x - crMin.x,  // The new width of the window, in pixels.
            crMax.y - crMin.y,  // The new height of the window, in pixels.
            view.m_ShowWindow ? SWP_SHOWWINDOW : SWP_HIDEWINDOW
        );
    }
}

auto GuiView::RegisterViewAreaWindowClass(HINSTANCE instance) -> void
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
    wndClassData.lpszClassName = m_ViewIt->m_Name.c_str(); //m_Views.at(m_ViewIndex).m_Name.c_str();
    wndClassData.hIconSm = NULL;

    RegisterClassEx(&wndClassData);
}

auto GuiView::GetHWND() -> HWND
{
    return m_ViewIt->m_HWND; //m_Views.at(m_ViewIndex).m_HWND;
}

auto GuiView::AddView(GuiState& state, TreeItem* currentItem, ViewStyle vs, std::string name) -> void
{
    if (!currentItem)
        return;

    static int s_ViewCounter = 0;
    auto rootItem = (TreeItem*)currentItem->GetRoot();
    auto desktopItem = rootItem->CreateItemFromPath("DesktopInfo");
    auto viewContextItem = desktopItem->CreateItemFromPath(mySSPrintF("View%d", s_ViewCounter++).c_str());

    m_Views.emplace_back(name, vs, SHV_DataView_Create(viewContextItem, vs, ShvSyncMode::SM_Load));
    m_ViewIt = --m_Views.end();


    UpdateParentWindow(m_Views.back()); // Needed before InitWindow
    InitWindow(state.GetCurrentItem());
    SHV_DataView_AddItem(m_ViewIt->m_DataView, state.GetCurrentItem(), false);
    m_AddCurrentItem = true;
}

auto GuiView::InitWindowParameterized(std::string caption, DataView* dv, ViewStyle vs, HWND parent_hwnd, UInt32 min, UInt32 max) -> HWND
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
}

auto GuiView::InitWindow(TreeItem* currentItem) -> WindowState
{
    //TODO: simplify, make function more pure; remove side effects
    ImVec2 crMin = ImGui::GetWindowContentRegionMin();
    ImVec2 crMax = ImGui::GetWindowContentRegionMax();
    HINSTANCE instance = GetInstance(m_ViewIt->m_HWNDParent);//m_Views.at(m_ViewIndex).m_HWNDParent);
    RegisterViewAreaWindowClass(instance);
    auto vs = m_ViewIt->m_ViewStyle == tvsMapView ? WS_DLGFRAME | WS_CHILD : WS_CHILD;
    m_ViewIt->m_HWND = CreateWindowEx(
        0L,                            // no extended styles
        m_ViewIt->m_Name.c_str(),      // MapView control class 
        nullptr,                       // text for window title bar 
        vs,                            // styles
        CW_USEDEFAULT,                 // horizontal position 
        CW_USEDEFAULT,                 // vertical position
        crMax.x - crMin.x,             // width
        crMax.y - crMin.y,             // height 
        m_ViewIt->m_HWNDParent,        // handle to parent
        nullptr,                       // no menu
        instance,                      // instance owning this window 
        m_ViewIt->m_DataView           //m_DataView                                       
    );

    if (!m_ViewIt->m_HWND)
    {
        return WindowState::UNINITIALIZED;
    }

    SHV_DataView_Update(m_ViewIt->m_DataView);
    UpdateWindowPosition(*m_ViewIt);
    ShowOrHideWindow(*m_ViewIt, true);
    return WindowState::CHANGED;
}

auto GuiView::CloseAll() -> void
{
    m_Views.clear();
}

auto GuiView::CloseWindowOnMimimumSize(View &view) -> bool
{
    auto crm = ImGui::GetContentRegionMax();
    static int min_szx = 50, min_szy = 50;
    if (crm.x <= min_szx || crm.y <= min_szy)
    {
        ShowOrHideWindow(view, false);
        return true;
    }

    return false;
}

GuiView::~GuiView(){}

auto GuiView::UpdateAll(GuiState& state) -> void
{
    if (m_EditPaletteWindow)
        Update(state, *m_EditPaletteWindow);

    auto it = m_Views.begin();
    while (it != m_Views.end()) 
    {
        if (it->m_DoView && m_ViewIt->m_DataView && IsWindow(it->m_HWND))
        {
            if (Update(state, *it) && m_ViewIt._Ptr && m_ViewIt != it)
                m_ViewIt = it;

            ++it;
        }
        else // view to be destroyed
        {
            it = m_Views.erase(it);
            if (it == m_Views.end())
                m_ViewIt = m_Views.begin();
            else
                m_ViewIt = it; // TODO: m_ViewIt should be restored to the previously set m_ViewIt
        }
    }
}

#include <imgui_impl_opengl3.h>
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
};

void GuiView::ResetEditPaletteWindow(ClientHandle clientHandle, const TreeItem* self)
{
    auto gui_main_component_ptr = reinterpret_cast<GuiMainComponent*>(clientHandle);
    m_EditPaletteWindow.release();
    
    auto mdi_create_struct_ptr = reinterpret_cast<MdiCreateStruct*>(const_cast<TreeItem*>(self));
    m_EditPaletteWindow = std::make_unique<View>(mdi_create_struct_ptr->caption, mdi_create_struct_ptr->ct, mdi_create_struct_ptr->dataView);

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
    }
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

auto GuiView::Update(GuiState& state, View& view) -> bool
{
    auto event_queues = GuiEventQueues::getInstance();

    // Open window
    ImGui::SetNextWindowSize(ImVec2(500, 500), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin( (view.m_DataView->GetCaption().c_str() + view.m_Name).c_str(), &view.m_DoView, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoTitleBar) || CloseWindowOnMimimumSize(view) || m_Views.empty())
    {
        ShowOrHideWindow(view, false);

        ImGui::End();
        return false;
    }

    auto view_window = ImGui::GetCurrentWindow(); //TODO: temporary
    if (view.has_been_docking_initialized && !IsDocked() && view_window->DockNodeAsHost == NULL && view_window->DockNode == NULL) // floating window
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

        /*auto ctx = ImGui::GetCurrentContext();
        ImGuiDockContext* dc = &ctx->DockContext;
        //auto dockspace_id = ImGui::GetID("GeoDMSDockSpace");

        auto dockspace_docknode = (ImGuiDockNode*)dc->Nodes.GetVoidPtr(state.dockspace_id);
        if (dockspace_docknode && dockspace_docknode->HostWindow)
        {
            ImGui::DockContextQueueDock(ctx, dockspace_docknode->HostWindow, dockspace_docknode, window, ImGuiDir_None, 0.0f, false);
            view.has_been_docking_initialized = true;
        }*/
    }

    return result;
}

auto GuiView::OnOpenEditPaletteWindow(ClientHandle clientHandle, const TreeItem* self, NotificationCode notificationCode) -> void
{
    auto gui_main_component_ptr = reinterpret_cast<GuiMainComponent*>(clientHandle);
    if (notificationCode == NotificationCode::CC_CreateMdiChild) // TODO: this should have its own separate callback
    {
        
        gui_main_component_ptr->m_View.ResetEditPaletteWindow(clientHandle, self);

        //mdi_create_struct_ptr->dataView->CreateViewWindow(mdi_create_struct_ptr->dataView, mdi_create_struct_ptr->caption);
        //event_queues->MapViewEvents.Add();
    }
}