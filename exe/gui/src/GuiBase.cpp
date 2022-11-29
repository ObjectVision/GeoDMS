#include <filesystem>
#include <iterator>
#include <imgui.h>
#include <imgui_internal.h>
#include "GuiBase.h"
#include <boost/algorithm/string/classification.hpp> // Include boost::for is_any_of
#include <boost/algorithm/string/split.hpp> // Include for boost::split
#include <Windows.h>
#include "TicInterface.h"
#include "utl/Environment.h"

#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

GuiEventQueues* GuiEventQueues::instance = 0;
GuiEventQueues* GuiEventQueues::getInstance()
{
    if (!instance) {
        instance = new GuiEventQueues();
        return instance;
    }
    else
        return instance;
}

TreeItemHistory::TreeItemHistory()
{
    m_Iterator = m_History.begin();
}

void TreeItemHistory::Insert(TreeItem* new_item)
{
    if (m_Iterator == m_History.end())
    {
        m_History.push_back(new_item);
        m_Iterator = std::prev(m_History.end());
        return;
    }

    if (new_item == *m_Iterator)
        return;

    m_Iterator = m_History.insert(std::next(m_Iterator), new_item);
}

TreeItem* TreeItemHistory::GetNext()
{
    if (std::distance(m_History.end(), m_Iterator))
    {
        if (std::next(m_Iterator) == m_History.end())
            return NULL;

        std::advance(m_Iterator, 1);
        return *m_Iterator;
    }
    return NULL;
}

TreeItem* TreeItemHistory::GetPrevious()
{
    auto test = std::distance(m_History.begin(), m_Iterator);
    if (std::distance(m_Iterator, m_History.begin()))
    {
        std::advance(m_Iterator, -1);
        return *m_Iterator;
    }
    return NULL;
}

std::list<TreeItem*>::iterator TreeItemHistory::GetCurrentIterator()
{
    return m_Iterator;
}

std::list<TreeItem*>::iterator TreeItemHistory::GetBeginIterator()
{
    return m_History.begin();
}

std::list<TreeItem*>::iterator TreeItemHistory::GetEndIterator()
{
    return m_History.end();
}

auto GuiState::clear() -> void
{
    if (m_Root.has_ptr())
        m_Root->EnableAutoDelete();
    //m_Root.reset();
    //m_CurrentItem.reset();
}

GuiState::~GuiState()
{
    clear();
}

bool GuiTreeItemsHolder::contains(TreeItem* item)
{
    if (std::find(m_TreeItems.begin(), m_TreeItems.end(), item) != m_TreeItems.end())
        return true;
    return false;
}

void GuiTreeItemsHolder::add(TreeItem* item)
{
    m_TreeItems.push_back(item);
}

void GuiTreeItemsHolder::add(const AbstrDataItem* adi)
{
    m_DataHolder.push_back(adi);
}

void GuiTreeItemsHolder::clear()
{
    m_TreeItems.clear();
    m_DataHolder.clear();
}

SizeT GuiTreeItemsHolder::size()
{
    return m_TreeItems.size();
}

std::vector<InterestPtr<TreeItem*>>& GuiTreeItemsHolder::get()
{
    return m_TreeItems;
}


GuiBaseComponent::GuiBaseComponent(){}
GuiBaseComponent::~GuiBaseComponent(){}
void GuiBaseComponent::Update(){}

auto DivideTreeItemFullNameIntoTreeItemNames(std::string fullname, std::string separator) -> std::vector<std::string>
{
    std::vector<std::string> SeparatedTreeItemFullName;
    boost::split(SeparatedTreeItemFullName, fullname, boost::is_any_of(separator), boost::token_compress_on);
    return SeparatedTreeItemFullName;
}

auto GetExeFilePath() -> std::string
{
    wchar_t buffer[MAX_PATH];
    GetModuleFileName(NULL, buffer, MAX_PATH);
    auto exePath = std::wstring(std::filesystem::path(std::move(buffer)).remove_filename());
    return std::string(exePath.begin(), exePath.end());
}

ImVec2 SetCursorPosToOptionsIconInWindowHeader()
{
    auto oldCursosPos = ImGui::GetCursorPos();
    auto window_size = ImGui::GetWindowSize();
    ImGui::SetCursorPos(ImVec2(window_size.x - 38, 6)); // TODO: hardcoded offsets, do these scale with different screen resolutions?
    return oldCursosPos;
}

void SetClipRectToIncludeOptionsIconInWindowHeader()
{
    auto test = ImGui::GetWindowDrawList();
    auto window_clip_rect_min = ImGui::GetWindowDrawList()->GetClipRectMin();
    ImGui::PushClipRect(ImVec2(window_clip_rect_min.x, window_clip_rect_min.y - 20), ImGui::GetWindowDrawList()->GetClipRectMax(), false);
}

bool MouseHooversOptionsIconInWindowHeader()
{
    auto window_clip_rect_min = ImGui::GetWindowDrawList()->GetClipRectMin();
    auto window_clip_rect_max = ImGui::GetWindowDrawList()->GetClipRectMax();
    auto rect_min = ImVec2(window_clip_rect_max.x - 35, window_clip_rect_min.y);
    auto rect_max = ImVec2(window_clip_rect_max.x - 20, window_clip_rect_min.y + 15);
    return ImGui::IsMouseHoveringRect(rect_min, rect_max);
}

void SetKeyboardFocusToThisHwnd()
{
    auto window = ImGui::GetCurrentWindow();
    SetFocus((HWND)window->Viewport->PlatformHandleRaw);
}

void GuiState::SaveWindowOpenStatusFlags()
{
    UInt32 flags = 0;
    flags = ShowTreeviewWindow      ? flags |= GWOF_TreeView        : flags &= ~GWOF_TreeView;
    flags = ShowDetailPagesWindow   ? flags |= GWOF_DetailPages     : flags &= ~GWOF_DetailPages;
    flags = ShowEventLogWindow      ? flags |= GWOF_EventLog        : flags &= ~GWOF_EventLog;
    flags = ShowOptionsWindow       ? flags |= GWOF_Options         : flags &= ~GWOF_Options;
    flags = ShowToolbar             ? flags |= GWOF_ToolBar         : flags &= ~GWOF_ToolBar;
    flags = ShowCurrentItemBar      ? flags |= GWOF_CurrentItemBar  : flags &= ~GWOF_CurrentItemBar;
    flags = ShowStatusBar           ? flags |= GWOF_StatusBar       : flags &= ~GWOF_StatusBar;
    SetGeoDmsRegKeyDWord("WindowOpenStatusFlags", flags);        
}

void GuiState::SetWindowOpenStatusFlagsOnFirstUse() // first use based on availability of registry param
{
    ShowTreeviewWindow      = true;
    ShowDetailPagesWindow   = true;
    ShowEventLogWindow      = true;
    ShowOptionsWindow       = false;
    ShowToolbar             = true;
    ShowCurrentItemBar      = true;
    ShowStatusBar           = true;
    SaveWindowOpenStatusFlags();
}

void GuiState::LoadWindowOpenStatusFlags()
{
    bool exists = false;
    auto flags = GetRegFlags("WindowOpenStatusFlags", exists);

    if (!exists)
    {
        SetWindowOpenStatusFlagsOnFirstUse();
        return;
    }

    // update open state based on flags
    ShowTreeviewWindow      = flags & GWOF_TreeView;
    ShowDetailPagesWindow   = flags & GWOF_DetailPages;
    ShowEventLogWindow      = flags & GWOF_EventLog;
    ShowOptionsWindow       = flags & GWOF_Options;
    ShowToolbar             = flags & GWOF_ToolBar;
    ShowCurrentItemBar      = flags & GWOF_CurrentItemBar;
    ShowStatusBar           = flags & GWOF_StatusBar;
}

std::string GetInitialWindowComposition()
{
    std::string result =
        "[Window][GeoDMSGui]\n"
        "Pos=0,21\n"
        "Size=1280,699\n"
        "Collapsed=0\n"
        "\n"
        "[Window][Toolbar]\n"
        "Pos=8,50\n"
        "Size=1264,31\n"
        "Collapsed=0\n"
        "DockId=0x00000009,0\n"
        "\n"
        "[Window][Detail Pages]\n"
        "Pos=891,83\n"
        "Size=381,254\n"
        "Collapsed=0\n"
        "DockId=0x00000004,0\n"
        "\n"
        "[Window][Treeview]\n"
        "Pos=8,83\n"
        "Size=367,254\n"
        "Collapsed=0\n"
        "DockId=0x00000001,0\n"
        "\n"
        "[Window][EventLog]\n"
        "Pos=8,339\n"
        "Size=1264,339\n"
        "Collapsed=0\n"
        "DockId=0x00000006,0\n"
        "\n"
        "[Window][DMSView]\n"
        "Pos=377,83\n"
        "Size=1792,911\n"
        "Collapsed=0\n"
        "DockId=0x00000002,0\n"
        "\n"
        "[Window][Debug##Default]\n"
        "ViewportPos=94,117\n"
        "ViewportId=0x9F5F46A1\n"
        "Size=400,400\n"
        "Collapsed=0\n"
        "\n"
        "[Window][StatusBar]\n"
        "Pos=8,680\n"
        "Size=1264,32\n"
        "Collapsed=0\n"
        "DockId=0x00000008,0\n"
        "\n"
        "[Docking][Data]\n"
        "DockSpace           ID=0x54D8F03E Window=0x47EE5377 Pos=120,185 Size=1264,662 Split=Y\n"
        "  DockNode          ID=0x00000009 Parent=0x54D8F03E SizeRef=2544,31 HiddenTabBar=1 Selected=0x738351EE\n"
        "  DockNode          ID=0x0000000A Parent=0x54D8F03E SizeRef=2544,1286 Split=Y\n"
        "    DockNode        ID=0x00000007 Parent=0x0000000A SizeRef=1264,1252 Split=Y\n"
        "      DockNode      ID=0x00000005 Parent=0x00000007 SizeRef=1264,911 Split=X\n"
        "        DockNode    ID=0x00000003 Parent=0x00000005 SizeRef=881,662 Split=X\n"
        "          DockNode  ID=0x00000001 Parent=0x00000003 SizeRef=367,662 Selected=0x0C84ACA2\n"
        "          DockNode  ID=0x00000002 Parent=0x00000003 SizeRef=895,662 CentralNode=1 Selected=0x1BA3A327\n"
        "        DockNode    ID=0x00000004 Parent=0x00000005 SizeRef=381,662 Selected=0x89482BF9\n"
        "      DockNode      ID=0x00000006 Parent=0x00000007 SizeRef=1264,339 Selected=0xB76E45CC\n"
        "    DockNode        ID=0x00000008 Parent=0x0000000A SizeRef=1264,32 HiddenTabBar=1 Selected=0x51C70801\n"
        "\n";
    return result;
}

void SetWindowCompositionOnFirstUse()
{
    SetGeoDmsRegKeyString("WindowComposition", GetInitialWindowComposition());
}

void LoadIniFromRegistry()
{
    auto ini_registry_contents = GetGeoDmsRegKey("WindowComposition");
    if (ini_registry_contents.empty())
    {
        SetWindowCompositionOnFirstUse();
        ini_registry_contents = GetGeoDmsRegKey("WindowComposition");
    }

    if (!ini_registry_contents.empty())
    {
        reportF(SeverityTypeID::ST_MajorTrace, "Loading GeoDMS window composition from registry.");
        ImGui::LoadIniSettingsFromMemory(ini_registry_contents.c_str());
    }
}

void   SaveIniToRegistry()
{
    std::string ini_contents = ImGui::SaveIniSettingsToMemory();
    SetGeoDmsRegKeyString("WindowComposition", ini_contents);
    reportF(SeverityTypeID::ST_MajorTrace, "Storing GeoDMS window composition in registry.");
}