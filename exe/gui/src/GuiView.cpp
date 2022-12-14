#include <imgui.h>
#include <imgui_internal.h>

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

void View::operator=(View && other) noexcept
{
    Reset();
    m_Name       = std::move(other.m_Name);
    m_ViewStyle  = std::move(other.m_ViewStyle);
    m_DataView   = std::move(other.m_DataView);
    m_HWND       = std::move(other.m_HWND);
    m_DoView     = std::move(other.m_DoView);
    m_ShowWindow = std::move(other.m_ShowWindow);

    other.m_HWND = NULL;
    other.m_DataView = NULL;
}

void View::Reset()
{
    if (m_HWND)
    {
        DestroyWindow(m_HWND);
        m_HWND = nullptr;
        m_DataView = nullptr;
    }
}

void GuiView::ProcessEvent(GuiEvents event, TreeItem* currentItem)
{
    switch (event)
    {
    case GuiEvents::UpdateCurrentItem: break;
    case GuiEvents::UpdateCurrentAndCompatibleSubItems: break;
    case GuiEvents::OpenInMemoryDataView: break;
    }
}

WindowState GuiView::UpdateParentWindow(View& view)
{
    auto glfw_window = glfwGetCurrentContext();
    auto mainWindow  = glfwGetWin32Window(glfw_window);
    auto window      = ImGui::GetCurrentWindow(); //ImGui::FindWindowByName(m_ViewName.c_str());
    if (!window || !(HWND)window->Viewport->PlatformHandleRaw)
        return WindowState::UNINITIALIZED;

    if (view.m_HWNDParent == (HWND)window->Viewport->PlatformHandleRaw)
        return WindowState::UNCHANGED;
        
    view.m_HWNDParent = (HWND)window->Viewport->PlatformHandleRaw;
    return WindowState::CHANGED;
}

bool GuiView::IsDocked() // test if the MapViewWindow is docked inside the ImGui main window.
{
    auto glfw_window = glfwGetCurrentContext();
    auto mainWindow = glfwGetWin32Window(glfw_window);
    auto window = ImGui::GetCurrentWindow();//ImGui::FindWindowByName(m_ViewName.c_str()); // GetCurrentWindow
    if (window)
        return mainWindow == (HWND)window->Viewport->PlatformHandleRaw;
    return false; // TODO: this does not make sense.
}

ImVec2 GuiView::GetRootParentCurrentWindowOffset()
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

void GuiView::ShowOrHideWindow(View& view, bool show)
{
    if (view.m_ShowWindow != show) // change in show state
    {
        view.m_ShowWindow = show;
        ShowWindow(view.m_HWND, view.m_ShowWindow ? SW_NORMAL : SW_HIDE);
    }
}

void GuiView::UpdateWindowPosition(View& view)
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

void GuiView::RegisterViewAreaWindowClass(HINSTANCE instance)
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
    wndClassData.lpszClassName = (LPCWSTR)m_ViewIt->m_Name.c_str(); //m_Views.at(m_ViewIndex).m_Name.c_str();
    wndClassData.hIconSm = NULL;

    RegisterClassEx(&wndClassData);
}

HWND GuiView::GetHWND()
{
    return m_ViewIt->m_HWND; //m_Views.at(m_ViewIndex).m_HWND;
}

void GuiView::AddView(GuiState& state, TreeItem* currentItem, ViewStyle vs, std::string name)
{
    if (!currentItem)
        return;

    static int s_ViewCounter = 0;
    auto rootItem = (TreeItem*)currentItem->GetRoot();
    auto desktopItem = rootItem->CreateItemFromPath("DesktopInfo");
    auto viewContextItem = desktopItem->CreateItemFromPath(mySSPrintF("View%d", s_ViewCounter++).c_str());

    m_Views.emplace_back(name, vs, SHV_DataView_Create(viewContextItem, vs, ShvSyncMode::SM_Load));
    m_ViewIt = --m_Views.end();


    UpdateParentWindow(m_Views.back());
    InitWindow(state.GetCurrentItem());
    SHV_DataView_AddItem(m_ViewIt->m_DataView, state.GetCurrentItem(), false);
    m_AddCurrentItem = true;
}

WindowState GuiView::InitWindow(TreeItem* currentItem)
{
    ImVec2 crMin = ImGui::GetWindowContentRegionMin();
    ImVec2 crMax = ImGui::GetWindowContentRegionMax();
    HINSTANCE instance = GetInstance(m_ViewIt->m_HWNDParent);//m_Views.at(m_ViewIndex).m_HWNDParent);
    RegisterViewAreaWindowClass(instance);
    auto vs = m_ViewIt->m_ViewStyle == tvsMapView ? WS_DLGFRAME | WS_CHILD : WS_CHILD;
    m_ViewIt->m_HWND = CreateWindowEx(
        0L,                                                             // no extended styles
        (LPCWSTR)m_ViewIt->m_Name.c_str(),                                    // MapView control class 
        (LPCWSTR)NULL,                                                  // text for window title bar 
        vs,                                                             // styles
        CW_USEDEFAULT,                                                  // horizontal position 
        CW_USEDEFAULT,                                                  // vertical position
        crMax.x - crMin.x,                                              // width
        crMax.y - crMin.y,                                              // height 
        m_ViewIt->m_HWNDParent,                           // handle to parent
        (HMENU)NULL,                                                    // no menu
        instance,                                                       // instance owning this window 
        m_ViewIt->m_DataView //m_DataView                                       
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

void GuiView::CloseAll()
{
    m_Views.clear();
}

bool GuiView::CloseWindowOnMimimumSize(View &view)
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

void GuiView::UpdateAll(GuiState& state)
{
    auto it = m_Views.begin();
    while (it != m_Views.end()) 
    {
        if (it->m_DoView)
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

bool GuiView::Update(GuiState& state, View& view)
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
    return result;
}