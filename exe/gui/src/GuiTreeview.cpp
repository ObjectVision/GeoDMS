#include <imgui.h>
#include <imgui_internal.h>
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include "ShvDllInterface.h"
#include "TicInterface.h"
#include "ClcInterface.h"
#include "GeoInterface.h"
#include "StxInterface.h"
#include "RtcInterface.h"

#include "dbg/Debug.h"
#include "dbg/DebugLog.h"
#include "dbg/DmsCatch.h"
#include "ptr/AutoDeletePtr.h"
#include "utl/Environment.h"
#include "utl/splitPath.h"

#include "AbstrDataItem.h"
#include "AbstrDataObject.h"
#include "AbstrUnit.h"
#include "DataLocks.h"
#include "TreeItem.h"
#include "StateChangeNotification.h"
#include "PropFuncs.h"
#include "TreeItemProps.h"
#include "dataview.h"

#include <boost/algorithm/string/classification.hpp> // Include boost::for is_any_of
#include <boost/algorithm/string/split.hpp> // Include for boost::split
#include <type_traits>

#include "GuiTreeview.h"
#include "GuiStyles.h"

GuiTreeViewComponent::~GuiTreeViewComponent() 
{}

void GuiTreeViewComponent::Update(bool* p_open)
{
    if (m_State.TreeViewEvents.HasEvents())
    {
        m_State.TreeViewEvents.Pop();
        m_TemporaryJumpItem = m_State.GetCurrentItem();
    }

    if (!ImGui::Begin("Treeview", p_open, ImGuiWindowFlags_None | ImGuiWindowFlags_NoTitleBar))
    {
        ImGui::End();
        return;
    }

    if (m_State.GetRoot())
        CreateTree();

    if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        SetKeyboardFocusToThisHwnd();

    ImGui::End();
}

UInt32 GetColorFromTreeItemNotificationCode(UInt32 status, bool isFailed)
{
    if (isFailed)
        return IM_COL32(255, 0, 0, 255);

    switch (status)
    {
    case NotificationCode::NC2_FailedFlag || NotificationCode::NC2_Invalidated: return IM_COL32(255, 0, 0, 255);
    case NotificationCode::NC2_DataReady: return IM_COL32(0, 153, 51, 255);
    case NotificationCode::NC2_Committed: return IM_COL32(82, 136, 219, 255);
    default: return IM_COL32(255, 0, 102, 255);
    }
}

void GuiTreeViewComponent::UpdateStateAfterItemClick(TreeItem* nextSubItem)
{
    m_State.SetCurrentItem(nextSubItem);
    m_State.CurrentItemBarEvents.Add(GuiEvents::UpdateCurrentItem);
    m_State.DetailPagesEvents.Add(GuiEvents::UpdateCurrentItem);
    m_State.MainEvents.Add(GuiEvents::UpdateCurrentItem);
}

bool GuiTreeViewComponent::IsAlphabeticalKeyJump(TreeItem* nextItem, bool looped = false)
{
    static bool loop;
    static bool passedCurrentItem;

    if (looped)
    {
        loop = true;
        passedCurrentItem = false;
        return false;
    }

    if (nextItem == m_State.GetCurrentItem())
    {
        passedCurrentItem = true;
        return false;
    }

    if (loop && !passedCurrentItem && (std::string(nextItem->GetName().c_str()).substr(0, 1).c_str() == m_State.m_JumpLetter.first || std::string(nextItem->GetName().c_str()).substr(0, 1).c_str() == m_State.m_JumpLetter.second))
    {
        m_State.m_JumpLetter.first.clear();
        m_State.m_JumpLetter.second.clear();
        loop = false;
        return true;
    }
    
    if (!loop && passedCurrentItem && (std::string(nextItem->GetName().c_str()).substr(0,1).c_str() == m_State.m_JumpLetter.first || std::string(nextItem->GetName().c_str()).substr(0, 1).c_str() == m_State.m_JumpLetter.second))
    {
        m_State.m_JumpLetter.first.clear();
        m_State.m_JumpLetter.second.clear();
        passedCurrentItem = false;
        return true;
    }

    return false;
}

bool IsAncestor(TreeItem* ancestorTarget, TreeItem* descendant)
{
    if (!descendant)
        return false;
    auto ancestorCandidate = descendant->GetParent();
    while (ancestorCandidate)
    {
        if (ancestorCandidate == ancestorTarget)
            return true;
        ancestorCandidate = ancestorCandidate->GetParent();
    }
    return false;
}

void SetTreeViewIcon(GuiTextureID id)
{
    ImGui::Image((void*)(intptr_t)GetIcon(id).GetImage(), ImVec2(GetIcon(id).GetWidth(), GetIcon(id).GetHeight()));
}

bool IsContainerWithTables(TreeItem* container)
{
    auto nextItem = container->_GetFirstSubItem();
    while (nextItem && nextItem->GetParent() == container)
    {
        if (IsDataItem(nextItem))
            return true;
        nextItem = container->GetNextItem();
    }
    return false;
}

bool IsKeyboardFocused()
{
    auto minItemRect = ImGui::GetItemRectMin();
    auto maxItemRect = ImGui::GetItemRectMax();
    if (ImGui::IsItemFocused() && !ImGui::IsMouseHoveringRect(minItemRect, maxItemRect) && (ImGui::IsKeyDown(ImGuiKey_UpArrow) || ImGui::IsKeyDown(ImGuiKey_DownArrow)))
        return true;

    return false;
}

bool GuiTreeViewComponent::CreateBranch(TreeItem* branch)
{
    //TODO: ImGui::IsItemVisible() use for clipping check
    TreeItem* nextSubItem = branch->_GetFirstSubItem();
    if (!nextSubItem)
        return true;
    //nextSubItem->UpdateMetaInfo();

    //nextSubItem->GetConfigFileLineNr
    //nextSubItem->GetConfigFileName
    //nextSubItem->GetCOnfigColNr

    while (nextSubItem)
    {
        if (nextSubItem->m_State.GetProgress() < PS_MetaInfo && !nextSubItem->m_State.IsUpdatingMetaInfo())
        {
            //DMS_TreeItem_RegisterStateChangeNotification(&OnTreeItemChanged, nextSubItem, nullptr);
            nextSubItem->UpdateMetaInfo();
        }     

        ImGuiTreeNodeFlags useFlags = nextSubItem == m_State.GetCurrentItem() ? m_BaseFlags | ImGuiTreeNodeFlags_Selected : m_BaseFlags; //nextSubItem == m_State.GetCurrentItem() ? m_BaseFlags | ImGuiTreeNodeFlags_Selected : m_BaseFlags;

        // status color
        auto status = DMS_TreeItem_GetProgressState(nextSubItem);
        auto failed = nextSubItem->IsFailed();

        if (status == NotificationCode::NC2_Updating) // ti not ready
        {
            nextSubItem = nextSubItem->GetNextItem(); // TODO: do display treeitem, but do not try to process subitems.
            continue;
        }

        ImGui::PushStyleColor(ImGuiCol_Text, GetColorFromTreeItemNotificationCode(status, failed));//IM_COL32(255, 0, 102, 255));//;
        // set TreeView icon
        auto vsflags = SHV_GetViewStyleFlags(nextSubItem); // calls UpdateMetaInfo
        if      (vsflags & ViewStyleFlags::vsfMapView)        { SetTreeViewIcon(GuiTextureID::TV_globe); }
        else if (vsflags & ViewStyleFlags::vsfTableContainer) { SetTreeViewIcon(GuiTextureID::TV_container_table); }
        else if (vsflags & ViewStyleFlags::vsfTableView)      { SetTreeViewIcon(GuiTextureID::TV_table); }
        else if (vsflags & ViewStyleFlags::vsfPaletteEdit)    { SetTreeViewIcon(GuiTextureID::TV_palette); }
        else if (vsflags& ViewStyleFlags::vsfContainer)       { SetTreeViewIcon(GuiTextureID::TV_container); }
        else                                                  { SetTreeViewIcon(GuiTextureID::TV_unit_transparant); }
        ImGui::SameLine(); // icon same line as TreeNode

        if (IsAncestor(nextSubItem, m_State.GetCurrentItem()))
            ImGui::SetNextItemOpen(true);
        auto treeItemIsOpen = ImGui::TreeNodeEx(nextSubItem->GetName().c_str(), nextSubItem->_GetFirstSubItem() ? useFlags : useFlags | ImGuiTreeNodeFlags_Leaf);
        
        // keyboard focus event
        if (IsKeyboardFocused())
            UpdateStateAfterItemClick(nextSubItem);

        // click event
        if (ImGui::IsItemClicked(ImGuiMouseButton_Left) || ImGui::IsItemClicked(ImGuiMouseButton_Right)) // item is clicked
        {
            SetKeyboardFocusToThisHwnd();
            UpdateStateAfterItemClick(nextSubItem);
        }

        // double click event
        if (ImGui::IsItemClicked() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
        {
            m_State.MainEvents.Add(OpenNewDefaultViewWindow);
        }

        // right-mouse popup menu
        if (ImGui::BeginPopupContextItem())
        {
            auto base_style_color = ImGui::GetStyleColorVec4(0);
            auto current_style_color = ImGui::GetStyleColorVec4(1);
            ImGui::PopStyleColor(2);
            if (ImGui::Button("Edit Config Source       Ctrl-E"))
            {
                m_State.MainEvents.Add(OpenConfigSource);
            }

            if (ImGui::Button("Default View"))
            {
                m_State.MainEvents.Add(OpenNewDefaultViewWindow);
            }

            if (ImGui::Button("Map View                 CTRL-M"))
            {
                m_State.MainEvents.Add(OpenNewMapViewWindow);
            }

            if (ImGui::Button("Table View               CTRL-D"))
            {
                m_State.MainEvents.Add(OpenNewTableViewWindow);
            }

            //if (ImGui::Button("Close"))
            //    ImGui::CloseCurrentPopup();
            //ImGui::PushStyleColor(ImGuiCol_Text, GetColorU32(style_color));
            ImGui::EndPopup();
            ImGui::PushStyleColor(ImGuiCol_Text, base_style_color);
            ImGui::PushStyleColor(ImGuiCol_Text, current_style_color);
        }

        // alphabetical letter jump
        if ((!m_State.m_JumpLetter.first.empty() && !m_State.m_JumpLetter.second.empty()) && IsAlphabeticalKeyJump(nextSubItem))
        {
            UpdateStateAfterItemClick(nextSubItem);
            ImGui::SetScrollHereY();
            m_TemporaryJumpItem = nullptr;
        }

        if (treeItemIsOpen)
        {
            // drop event
            if (ImGui::BeginDragDropSource())
            {
                ImGui::SetDragDropPayload("TreeItemPtr", nextSubItem->GetFullName().c_str(), strlen(nextSubItem->GetFullName().c_str()));  // type is a user defined string of maximum 32 characters. Strings starting with '_' are reserved for dear imgui internal types. Data is copied and held by imgui. Return true when payload has been accepted.
                ImGui::TextUnformatted(nextSubItem->GetName().c_str());
                ImGui::EndDragDropSource();
            }

            // jump event
            if (m_TemporaryJumpItem && m_TemporaryJumpItem == nextSubItem)
            {
                UpdateStateAfterItemClick(nextSubItem);
                ImGui::SetScrollHereY();
                m_TemporaryJumpItem = nullptr;
            }

            if (nextSubItem->m_State.GetProgress() >= PS_MetaInfo)
                CreateBranch(nextSubItem);
            ImGui::TreePop();
        }
        ImGui::PopStyleColor();

        nextSubItem = nextSubItem->GetNextItem();
    }
    return true;
}

bool GuiTreeViewComponent::CreateTree()
{
    ImGuiTreeNodeFlags useFlags = m_State.GetRoot() == m_State.GetCurrentItem() ? m_BaseFlags | ImGuiTreeNodeFlags_Selected : m_BaseFlags;
    auto status = DMS_TreeItem_GetProgressState(m_State.GetRoot());
    ImGui::PushStyleColor(ImGuiCol_Text, GetColorFromTreeItemNotificationCode(status, false));
    if (ImGui::TreeNodeEx(m_State.GetRoot()->GetName().c_str(), useFlags | ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (IsKeyboardFocused())
            UpdateStateAfterItemClick(m_State.GetRoot());

        // click events
        if (ImGui::IsItemClicked() && ImGui::IsMouseDown(ImGuiMouseButton_Left))
            UpdateStateAfterItemClick(m_State.GetRoot());
            
        if (m_State.GetRoot()->HasSubItems())
            CreateBranch(m_State.GetRoot());
        ImGui::TreePop();
    }
    ImGui::PopStyleColor();

    if (!m_State.m_JumpLetter.first.empty() && !m_State.m_JumpLetter.second.empty()) // alphabetical jumpletter present, but not acted upon yet
        IsAlphabeticalKeyJump(nullptr, true);

    return true;
}

