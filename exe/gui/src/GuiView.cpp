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

void GuiView::ProcessEvent(GuiEvents event, TreeItem* currentItem)
{
    switch (event)
    {
    case UpdateCurrentItem: // update current item
    {
        /*if (m_DataView && !m_ActiveItems.contains(currentItem))
        {
            m_ActiveItems.clear();
            Close(false);
        }*/

        m_IsPopulated = true;
        break;
    }
    case UpdateCurrentAndCompatibleSubItems:
    {
        /*if (m_DataView && !m_DataView->DoesContain(currentItem))
        {
            m_ActiveItems.clear();
            Close(false);
        }*/

        m_IsPopulated = true;
        break;
    }
    case OpenInMemoryDataView:
    {
        Close(true);
        break;
    }
    }
}

WindowState GuiView::UpdateParentWindow()
{
    auto glfw_window = glfwGetCurrentContext();
    auto mainWindow  = glfwGetWin32Window(glfw_window);
    auto window      = ImGui::GetCurrentWindow(); //ImGui::FindWindowByName(m_ViewName.c_str());
    //auto window = ImGui::GetCurrentWindow();
    if (!window || !(HWND)window->Viewport->PlatformHandleRaw)
        return WindowState::UNINITIALIZED;

    if (m_HWNDParent == (HWND)window->Viewport->PlatformHandleRaw)
        return WindowState::UNCHANGED;
        
    m_HWNDParent = (HWND)window->Viewport->PlatformHandleRaw;
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

void GuiView::UpdateWindowPosition(bool hide)
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
    float fb_height = io.DisplaySize.y * io.DisplayFramebufferScale.y;
    float fb_width = io.DisplaySize.x * io.DisplayFramebufferScale.x;

    int xpos, ypos;
    glfwGetWindowPos(glfwGetCurrentContext(), &xpos, &ypos); // content area pos

    auto is_appearing = ImGui::IsWindowAppearing();
    auto is_item_visible = ImGui::IsItemVisible();
    if (IsDocked())
    {
        SetWindowPos(// TODO: When docked, use offset of mainwindow + this window to get correct position.
            m_HWND,
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
            m_HWND,
            NULL,
            crMin.x + offset.x, // left
            crMin.y + offset.y, // top
            crMax.x - crMin.x,
            crMax.y - crMin.y,
            hide ? SWP_HIDEWINDOW : SWP_SHOWWINDOW
        );
    }
    
    //auto window = ImGui::GetCurrentWindow();
    //auto parentPos = GetRootParentCurrentWindowOffset();

    /*ImGui::Text(("irmin x, y: " + std::to_string(ImGui::GetItemRectMin().x) + ", " + std::to_string(ImGui::GetItemRectMin().y)).c_str());
    ImGui::Text(("irmax x, y: " + std::to_string(ImGui::GetItemRectMax().x) + ", " + std::to_string(ImGui::GetItemRectMax().y)).c_str());
    ImGui::Text(("wpos x, y: " + std::to_string(wPos.x) + ", " + std::to_string(wPos.y)).c_str());
    ImGui::Text(("gwpos x, y: " + std::to_string(xpos) + ", " + std::to_string(ypos)).c_str());
    ImGui::Text(("mpos x, y: " + std::to_string(mouse_pos.x) + ", " + std::to_string(mouse_pos.y)).c_str());
    ImGui::Text(("crMin x, y: " + std::to_string(crMin.x) + ", " + std::to_string(crMin.y)).c_str());
    ImGui::Text(("crMax x, y: " + std::to_string(crMax.x) + ", " + std::to_string(crMax.y)).c_str());
    ImGui::Text(("pPos x, y: " + std::to_string(parentPos.x) + ", " + std::to_string(parentPos.y)).c_str());*/
}

void GuiView::RegisterMapViewAreaWindowClass(HINSTANCE instance)
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
    wndClassData.lpszClassName = (LPCWSTR)m_ViewName.c_str();
    wndClassData.hIconSm = NULL;

    RegisterClassEx(&wndClassData);
}

void GuiView::SetViewIndex(int index)
{
    m_ViewIndex = index;
}

HWND GuiView::GetHWND()
{
    return m_HWND;
}

void GuiView::InitDataView(TreeItem* currentItem)
{
    if (!currentItem)
        return;

    static int s_ViewCounter = 0;
    auto rootItem = (TreeItem*)currentItem->GetRoot();
    auto desktopItem = rootItem->CreateItemFromPath("DesktopInfo");
    auto viewContextItem = desktopItem->CreateItemFromPath(mySSPrintF("View%d", s_ViewCounter++).c_str());

    m_ViewIndex = m_Views.size();
    m_Views.emplace_back(currentItem->GetName().c_str(), m_ViewStyle, SHV_DataView_Create(viewContextItem, m_ViewStyle, ShvSyncMode::SM_Load));
    Close(true);
    m_AddCurrentItem = true;


    //m_DataView = SHV_DataView_Create(viewContextItem, m_ViewStyle, ShvSyncMode::SM_Load);
}

WindowState GuiView::InitWindow(TreeItem* currentItem)
{
    dms_assert(m_ViewIndex<m_Views.size());
    ImVec2 crMin = ImGui::GetWindowContentRegionMin();
    ImVec2 crMax = ImGui::GetWindowContentRegionMax();
    HINSTANCE instance = GetInstance(m_HWNDParent);
    RegisterMapViewAreaWindowClass(instance);
    auto vs = m_ViewStyle == tvsMapView ? WS_DLGFRAME | WS_CHILD : WS_CHILD;
    m_HWND = CreateWindowEx(
        0L,                                                             // no extended styles
        (LPCWSTR)m_ViewName.c_str(),                                    // MapView control class 
        (LPCWSTR)NULL,                                                  // text for window title bar 
        vs,                                                             // styles
        CW_USEDEFAULT,                                                  // horizontal position 
        CW_USEDEFAULT,                                                  // vertical position
        crMax.x - crMin.x,                                              // width
        crMax.y - crMin.y,                                              // height 
        m_HWNDParent,                                                   // handle to parent
        (HMENU)NULL,                                                    // no menu
        instance,                                                       // instance owning this window 
        m_Views.at(m_ViewIndex).m_DataView //m_DataView                                       
    );

    if (!m_HWND)
    {
        return WindowState::UNINITIALIZED;
    }

    //m_Views.at(m_ViewIndex).m_DataView; //m_DataView->ResetHWnd(m_HWND);

    /*if (SHV_DataView_CanContain(m_Views.at(m_ViewIndex).m_DataView, currentItem) && !m_Views.at(m_ViewIndex).m_ActiveItems.contains(currentItem)) //!m_ActiveItems.contains(currentItem))
    {
        //m_ActiveItems.add(currentItem);
        SHV_DataView_AddItem(m_Views.at(m_ViewIndex).m_DataView, currentItem, false);
    }*/

    SHV_DataView_Update(m_Views.at(m_ViewIndex).m_DataView);
    UpdateWindow(m_HWND);
    UpdateWindowPosition(true);
    return WindowState::CHANGED;
}

void GuiView::Close(bool keepDataView=true)
{
    if (!keepDataView && !m_Views.empty() && m_Views.at(m_ViewIndex).m_DataView)
    {
        SHV_DataView_Destroy(m_Views.at(m_ViewIndex).m_DataView);
        m_Views.erase(m_Views.begin()+m_ViewIndex);
        //m_ActiveItems.clear();
        //m_DataView = nullptr;
        m_IsPopulated = false;
        m_ViewIndex = -1;
    }
    if (m_HWND)
    {
        DestroyWindow(m_HWND);
        m_HWND = nullptr;
    }
}

void GuiView::SetViewStyle(ViewStyle vs)
{
    m_ViewStyle = vs;
}

std::string GuiView::GetViewName()
{
    return m_ViewName;
}

void GuiView::SetViewName(std::string vn)
{
    m_ViewName = vn;
}

void GuiView::SetDoView(bool doView)
{
    m_DoView = doView;
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
        Close(true);
        return true;
    }

    return false;
}

/*GuiView::GuiView(TreeItem*& currentItem, ViewStyle style, std::string name)
{
    SetViewStyle(style);
    SetViewName(name);
    InitDataView(currentItem);
    InitWindow(currentItem);
    m_DoView = true;
}*/

GuiView::~GuiView()
{
    Close(false);
}

void GuiView::ResetView(ViewStyle vs, std::string vn)
{
    SetViewStyle(vs);
    SetViewName(vn);
    m_IsPopulated = true;
}

void GuiView::Update()
{
    // Open window
    if (!ImGui::Begin("DMSView", &m_DoView, ImGuiWindowFlags_NoScrollbar) || CloseWindowOnMimimumSize() || m_Views.empty())
    {
        Close(true);
        ImGui::End();
        return;
    }

    // handle events
    EventQueue* eventQueuePtr = nullptr; // TODO: memory leak.
    switch (m_Views.at(m_ViewIndex).m_ViewStyle)
    {
    case ViewStyle::tvsMapView:
        eventQueuePtr = &m_State.MapViewEvents;
        break;
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
        UpdateWindowPosition(true);
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
                if (SHV_DataView_CanContain(m_Views.at(m_ViewIndex).m_DataView, m_State.GetCurrentItem()) && !m_Views.at(m_ViewIndex).m_ActiveItems.contains(m_State.GetCurrentItem()))//!m_ActiveItems.contains(m_State.GetCurrentItem()))
                {
                    m_Views.at(m_ViewIndex).m_ActiveItems.add(m_State.GetCurrentItem()); //m_ActiveItems.add(m_State.GetCurrentItem());
                    SHV_DataView_AddItem(m_Views.at(m_ViewIndex).m_DataView, m_State.GetCurrentItem(), false);
                }
            }
        }
        ImGui::EndDragDropTarget();
    }
    
    // update parent window if changed
    auto mainWindow = glfwGetWin32Window(glfwGetCurrentContext());
    auto parentWindowState = UpdateParentWindow();
    if (parentWindowState == WindowState::UNINITIALIZED)// || (m_ViewStyle == ViewStyle::tvsMapView && !IsDataItem(currentItem)))
    {
        ImGui::End();
        return;
    }

    // init View window
    //if (!m_Views.at(m_ViewIndex).m_DataView)
    //    InitDataView(m_State.GetCurrentItem());

    if (!m_HWND)
        InitWindow(m_State.GetCurrentItem());

    if (!m_Views.at(m_ViewIndex).m_DataView || !m_HWND || !m_HWNDParent || !IsWindow(m_HWNDParent) || !IsWindow(m_HWND)) // final check
    {
        Close(true);
        ImGui::End();
        return;
    }

    // update parent window
    if (parentWindowState == WindowState::CHANGED)
    {
        SetParent(m_HWND, m_HWNDParent); // set new parent on dock/undock
        UpdateWindowPosition(true);
    }

    // add current item
    if (m_AddCurrentItem)
    {
        SHV_DataView_AddItem(m_Views.at(m_ViewIndex).m_DataView, m_State.GetCurrentItem(), false);
        m_AddCurrentItem = false;
    }

    // If m_HWND is focused, focus imgui window as well
    if (GetFocus() == m_HWND)
    {
        ImGui::FocusWindow(ImGui::GetCurrentWindow());
    }

    // update window
    auto result = SHV_DataView_Update(m_Views.at(m_ViewIndex).m_DataView);
    m_Views.at(m_ViewIndex).m_DataView->UpdateView();
    auto hide = ImGui::IsMouseDragging(ImGuiMouseButton_Left) && ImGui::IsMouseHoveringRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
    UpdateWindowPosition(hide);

    SuspendTrigger::Resume();

    ImGui::End();
}