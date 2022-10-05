#include <filesystem>
#include <imgui.h>
#include <imgui_internal.h>
#include "GuiBase.h"
#include <boost/algorithm/string/classification.hpp> // Include boost::for is_any_of
#include <boost/algorithm/string/split.hpp> // Include for boost::split
#include <Windows.h>
#include "TicInterface.h"

#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

bool GuiState::ShowDemoWindow			    = false; 
bool GuiState::ShowOptionsWindow		    = false;
bool GuiState::ShowDetailPagesOptionsWindow = false;
bool GuiState::ShowEventLogOptionsWindow    = false;
bool GuiState::ShowOpenFileWindow		    = false;
bool GuiState::ShowConfigSource			    = false;
bool GuiState::ShowTreeviewWindow = true;
bool GuiState::ShowMapviewWindow		    = false;
bool GuiState::ShowTableviewWindow = false;//true;
bool GuiState::ShowDetailPagesWindow = true;
bool GuiState::ShowEventLogWindow = true;
bool GuiState::ShowToolbar                  = false;
bool GuiState::ShowCurrentItemBar = true;
bool GuiState::MapViewIsActive = false;// true;
bool GuiState::TableViewIsActive		    = false;
TreeItem* GuiState::m_Root = nullptr;
TreeItem* GuiState::m_CurrentItem = nullptr;

StringStateManager GuiState::configFilenameManager;

std::pair<std::string, std::string> GuiState::m_JumpLetter;

GuiSparseTree GuiState::m_SparseTree;
OptionsEventLog GuiState::m_OptionsEventLog;

EventQueue GuiState::MainEvents;
EventQueue GuiState::CurrentItemBarEvents;
EventQueue GuiState::TreeViewEvents;
EventQueue GuiState::TableViewEvents;
EventQueue GuiState::MapViewEvents;
EventQueue GuiState::DetailPagesEvents;
EventQueue GuiState::GuiMenuFileComponentEvents;

void GuiState::clear()
{
    if (m_Root && !(m_CurrentItem == m_Root))
        m_Root->EnableAutoDelete();
    m_CurrentItem = nullptr;
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

std::vector<std::string> DivideTreeItemFullNameIntoTreeItemNames(std::string fullname)
{
    std::vector<std::string> SeparatedTreeItemFullName;
    boost::split(SeparatedTreeItemFullName, fullname, boost::is_any_of("/"), boost::token_compress_on);
    return SeparatedTreeItemFullName;
}

TreeItem* SetJumpItemFullNameToOpenInTreeView(TreeItem* root, std::vector<std::string> SeparatedTreeItemFullName)
{
    TreeItem* tmpItem = root;
    for (auto& name : SeparatedTreeItemFullName)
    {
        if (name.empty()) // root
            continue;

        tmpItem = tmpItem->_GetFirstSubItem();
        while (tmpItem)
        {
            if (name == tmpItem->GetName().c_str())
            {
                break;
            }
            tmpItem = tmpItem->GetNextItem();
            if (!tmpItem)
                return NULL;
        }
    }

    return tmpItem;
}

std::string GetExeFilePath()
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
    ImGui::SetCursorPos(ImVec2(window_size.x - 38, 7)); // TODO: hardcoded offsets, do these scale with different screen resolutions?
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
    //auto window = ImGui::GetCurrentWindow();
    //SetFocus((HWND)window->Viewport->PlatformHandleRaw);
}