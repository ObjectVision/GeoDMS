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
    m_Name      = std::move(other.m_Name);
    m_ViewStyle = std::move(other.m_ViewStyle);
    m_DataView  = std::move(other.m_DataView);
    m_HWND      = std::move(other.m_HWND);

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
    case UpdateCurrentItem: // update current item
    {
        m_IsPopulated = true;
        break;
    }
    case UpdateCurrentAndCompatibleSubItems:
    {
        m_IsPopulated = true;
        break;
    }
    case OpenInMemoryDataView:
        break;
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

void GuiView::UpdateWindowPosition(View& view, bool hide)
{
    if (!view.m_HWND)
        return;

    if (hide)
        int i = 0;

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
        SetWindowPos(// TODO: When docked, use offset of mainwindow + this window to get correct position.
            view.m_HWND,
            NULL,
            crMin.x + wPos.x - xpos, // left
            crMin.y + wPos.y - ypos, // top
            crMax.x - crMin.x,
            crMax.y - crMin.y,
            hide ? SWP_HIDEWINDOW : SWP_SHOWWINDOW 
        );
    }
    else
    {
        auto offset = GetRootParentCurrentWindowOffset();
        SetWindowPos(// TODO: When docked, use offset of mainwindow + this window to get correct position.
            view.m_HWND,
            NULL,
            crMin.x + offset.x, // left
            crMin.y + offset.y, // top
            crMax.x - crMin.x,
            crMax.y - crMin.y,
            hide ? SWP_HIDEWINDOW : SWP_SHOWWINDOW
        );
    }
}

void GuiView::RegisterViewAreaWindowClass(HINSTANCE instance)
{
    WNDCLASSEX wndClassData;
    wndClassData.cbSize = sizeof(WNDCLASSEX);
    wndClassData.style = CS_DBLCLKS;
    wndClassData.lpfnWndProc = &DataViewWndProc;
    wndClassData.cbClsExtra = 0;
    wndClassData.cbWndExtra = sizeof(DataView*);
    wndClassData.hInstance = instance;
    wndClassData.hIcon = NULL;
    wndClassData.hCursor = NULL;
    wndClassData.hbrBackground = HBRUSH(COLOR_WINDOW + 1);
    wndClassData.lpszMenuName = NULL;
    wndClassData.lpszClassName = (LPCWSTR)m_Views.at(m_ViewIndex).m_Name.c_str();
    wndClassData.hIconSm = NULL;

    RegisterClassEx(&wndClassData);
}

void GuiView::SetViewIndex(int index)
{
    if (m_ViewIndex == -1)
        m_ViewIndex = index;

    if (index != m_ViewIndex)
    {
        UpdateWindowPosition(m_Views.at(m_ViewIndex), true);
        m_ViewIndex = index;
        UpdateParentWindow(m_Views.at(m_ViewIndex));
    }
}

HWND GuiView::GetHWND()
{
    return m_Views.at(m_ViewIndex).m_HWND;
}

void GuiView::AddView(TreeItem* currentItem, ViewStyle vs, std::string name)
{
    if (!currentItem)
        return;
    //if (m_ViewIndex!=-1)
    //    GuiView::UpdateWindowPosition(true); // hide previous window

    static int s_ViewCounter = 0;
    auto rootItem = (TreeItem*)currentItem->GetRoot();
    auto desktopItem = rootItem->CreateItemFromPath("DesktopInfo");
    auto viewContextItem = desktopItem->CreateItemFromPath(mySSPrintF("View%d", s_ViewCounter++).c_str());

    m_ViewIndex = m_Views.size(); // currentItem->GetName().c_str()
    m_Views.emplace_back(name, vs, SHV_DataView_Create(viewContextItem, vs, ShvSyncMode::SM_Load));

    UpdateParentWindow(m_Views.back());
    InitWindow(m_State.GetCurrentItem());
    SHV_DataView_AddItem(m_Views.at(m_ViewIndex).m_DataView, m_State.GetCurrentItem(), false);
    m_AddCurrentItem = true;
    m_IsPopulated = true;
}

WindowState GuiView::InitWindow(TreeItem* currentItem)
{
    dms_assert(m_ViewIndex<m_Views.size());
    ImVec2 crMin = ImGui::GetWindowContentRegionMin();
    ImVec2 crMax = ImGui::GetWindowContentRegionMax();
    HINSTANCE instance = GetInstance(m_Views.at(m_ViewIndex).m_HWNDParent);
    RegisterViewAreaWindowClass(instance);
    auto vs = m_Views.at(m_ViewIndex).m_ViewStyle == tvsMapView ? WS_DLGFRAME | WS_CHILD : WS_CHILD;
    m_Views.at(m_ViewIndex).m_HWND = CreateWindowEx(
        0L,                                                             // no extended styles
        (LPCWSTR)m_Views.at(m_ViewIndex).m_Name.c_str(),                                    // MapView control class 
        (LPCWSTR)NULL,                                                  // text for window title bar 
        vs,                                                             // styles
        CW_USEDEFAULT,                                                  // horizontal position 
        CW_USEDEFAULT,                                                  // vertical position
        crMax.x - crMin.x,                                              // width
        crMax.y - crMin.y,                                              // height 
        m_Views.at(m_ViewIndex).m_HWNDParent,                           // handle to parent
        (HMENU)NULL,                                                    // no menu
        instance,                                                       // instance owning this window 
        m_Views.at(m_ViewIndex).m_DataView //m_DataView                                       
    );

    if (!m_Views.at(m_ViewIndex).m_HWND)
    {
        return WindowState::UNINITIALIZED;
    }

    SHV_DataView_Update(m_Views.at(m_ViewIndex).m_DataView);
    UpdateWindowPosition(m_Views.at(m_ViewIndex), true);
    return WindowState::CHANGED;
}

void GuiView::Close(bool keepDataView=true)
{
    if (m_ViewIndex!=-1 && !m_Views.empty() && m_Views.at(m_ViewIndex).m_HWND)
    {
        m_Views.erase(m_Views.begin() + m_ViewIndex);
        m_IsPopulated = false;
        m_ViewIndex = -1;
    }
}

void GuiView::CloseAll()
{
    m_Views.clear();
}

void GuiView::SetDoView(bool doView)
{
    m_DoView = doView;
    m_IsPopulated = doView; //TODO: can these two flags be merged?
}

bool GuiView::DoView()
{
    return m_DoView;
}

bool GuiView::IsPopulated()
{
    return m_IsPopulated;
}

bool GuiView::CloseWindowOnMimimumSize()
{
    auto crm = ImGui::GetContentRegionMax();
    static int min_szx = 50, min_szy = 50;
    if (crm.x <= min_szx || crm.y <= min_szy)
    {
        UpdateWindowPosition(m_Views.at(m_ViewIndex), true);
        //Close(true);
        return true;
    }

    return false;
}

GuiView::~GuiView()
{
    Close(false);
}

void GuiView::UpdateAll()
{
    auto it = m_Views.begin();
    while (it != m_Views.end()) 
    {

        if (it->m_DoView)
        {
            Update(*it);
            ++it;
        }
        else
            it = m_Views.erase(it);
    }
}

void GuiView::Update(View& view)
{
    // Open window
    ImGui::SetNextWindowSize(ImVec2(500, 500), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin( (view.m_DataView->GetCaption().c_str() + view.m_Name).c_str(), &view.m_DoView, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoTitleBar) || m_Views.empty())
    {
        //Close(true);
        ImGui::End();
        return;
    }

    auto test = ImGui::IsWindowCollapsed();
    if (test)
        int i = 0;

    // handle events
    EventQueue* eventQueuePtr = nullptr;
    switch (view.m_ViewStyle)
    {
    case ViewStyle::tvsMapView:
        eventQueuePtr = &m_State.MapViewEvents;
        break;
    case ViewStyle::tvsTableContainer:
    case ViewStyle::tvsTableView:
        eventQueuePtr = &m_State.TableViewEvents;
        break;
    default:
        break;
    }
    while (eventQueuePtr->HasEvents())
    {
        auto view_event = eventQueuePtr->Pop();
        if (m_State.GetCurrentItem())
            ProcessEvent(view_event, m_State.GetCurrentItem());
    }

    // currentItem not requested for viewing, do nothing
    if (!m_IsPopulated)
    {
        UpdateWindowPosition(view, true);
        ImGui::End();
        return;
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
                if (SHV_DataView_CanContain(view.m_DataView, m_State.GetCurrentItem()))
                    SHV_DataView_AddItem(view.m_DataView, m_State.GetCurrentItem(), false);
            }
        }
        ImGui::EndDragDropTarget();
    }
    
    // update parent window if changed
    auto mainWindow = glfwGetWin32Window(glfwGetCurrentContext());
    auto parentWindowState = UpdateParentWindow(view);
    if (parentWindowState == WindowState::UNINITIALIZED)// || (m_ViewStyle == ViewStyle::tvsMapView && !IsDataItem(currentItem)))
    {
        ImGui::End();
        return;
    }
    
    assert(view.m_HWNDParent);
    assert(view.m_HWND);

    if (parentWindowState == WindowState::CHANGED)
    {
        SetParent(view.m_HWND, view.m_HWNDParent); // set new parent on dock/undock
        UpdateWindowPosition(view, true);
    }

    // If view window is focused, focus imgui window as well
    if (GetFocus() == view.m_HWND)
        ImGui::FocusWindow(ImGui::GetCurrentWindow());

    // update window
    auto result = SHV_DataView_Update(view.m_DataView);
    view.m_DataView->UpdateView();
    auto hide = ImGui::IsMouseDragging(ImGuiMouseButton_Left) && ImGui::IsMouseHoveringRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
    UpdateWindowPosition(view, hide);

    SuspendTrigger::Resume();

    ImGui::End();
}