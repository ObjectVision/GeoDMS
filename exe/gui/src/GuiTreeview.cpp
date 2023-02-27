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
#include "geo/color.h"

#include <boost/algorithm/string/classification.hpp> // Include boost::for is_any_of
#include <boost/algorithm/string/split.hpp> // Include for boost::split
#include <boost/algorithm/string.hpp>
#include <type_traits>

#include "GuiTreeview.h"
#include "GuiStyles.h"
#include "GuiMain.h"
#include <functional>
#include<ranges>

auto GetColorFromTreeItemNotificationCode(UInt32 status, bool isFailed) -> UInt32
{
    if (isFailed)
        return DmsRed;// IM_COL32(0, 0, 0, 255);

    // TODO: store these colors somewhere persistently
    auto salmon = IM_COL32(255, 0, 102, 255);
    auto cool_blue   = IM_COL32(82, 136, 219, 255);
    auto cool_green  = IM_COL32(0, 153, 51, 255);

    switch (status)
    {
    case NotificationCode::NC2_FailedFlag: return IM_COL32(0, 0, 0, 255);
    case NotificationCode::NC2_DataReady: return cool_blue; // was green
    case NotificationCode::NC2_Validated: return cool_green; // TODO: new color?
    case NotificationCode::NC2_Committed: return cool_green; // was blue
    case NotificationCode::NC2_Invalidated:
    case NotificationCode::NC2_MetaReady: return salmon;//IM_COL32(255, 0, 102, 255); //return IM_COL32(250, 0, 250, 0xFF);
    default: return salmon; //IM_COL32(255, 0, 102, 255); // IM_COL32(255, 0, 102, 255);
    }
}

void DrawRightMouseClickButtonElement(std::string button_text, bool show, GuiEvents event)
{
    //bool show = vs & vs_capability || vs_capability == ViewStyleFlags::vsfNone;
    ImGui::PushStyleColor(ImGuiCol_Text, show ? IM_COL32(0, 0, 0, 255) : IM_COL32(0, 0, 0, 200));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, show ? IM_COL32(66, 150, 250, 79) : IM_COL32(0, 0, 0, 0));
    if (ImGui::Button(button_text.c_str()))
    {
        if (show)
            GuiEventQueues::getInstance()->MainEvents.Add(event);
    }
    ImGui::PopStyleColor(2);
}

void ShowRightMouseClickPopupWindowIfNeeded(GuiState& state)
{
    // right-mouse popup menu
    //ImGui::OpenPopup("treeview_popup_window");
    if (ImGui::BeginPopupContextItem("treeview_popup_window"))
    {
        auto event_queues = GuiEventQueues::getInstance();

        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(66, 150, 250, 79));
        //ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(0, 0, 0, 200));

        //DrawRightMouseClickButtonElement

        auto vsflags = SHV_GetViewStyleFlags(state.GetCurrentItem());
        auto default_viewstyle = SHV_GetDefaultViewStyle(state.GetCurrentItem());
        if (vsflags & ViewStyleFlags::vsfMapView) {  }
        else if (vsflags & ViewStyleFlags::vsfTableContainer) {  }
        else if (vsflags & ViewStyleFlags::vsfTableView) {  }
        else if (vsflags & ViewStyleFlags::vsfPaletteEdit) {  }
        else if (vsflags & ViewStyleFlags::vsfContainer) {  }
        else {  }

        //auto default_viewstyle = SHV_GetDefaultViewStyle(state.GetCurrentItem());

        DrawRightMouseClickButtonElement("Edit Config Source       Ctrl-E", true, GuiEvents::OpenConfigSource);
        DrawRightMouseClickButtonElement("Default View", (default_viewstyle== ViewStyle::tvsTableContainer) || (default_viewstyle== ViewStyle::tvsTableView) || (default_viewstyle==ViewStyle::tvsMapView), GuiEvents::OpenNewDefaultViewWindow);
        DrawRightMouseClickButtonElement("Table View               CTRL-D", vsflags & ViewStyleFlags::vsfTableView || vsflags & ViewStyleFlags::vsfTableContainer, GuiEvents::OpenNewTableViewWindow);
        DrawRightMouseClickButtonElement("Map View                 CTRL-M", vsflags & ViewStyleFlags::vsfMapView, GuiEvents::OpenNewMapViewWindow);


        //url 
        auto treeitem_metadata_url = TreeItemPropertyValue(state.GetCurrentItem(), urlPropDefPtr);
        if (!treeitem_metadata_url.empty())
        {
            ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(0, 0, 0, 255));
            if (ImGui::Button("Open Metainfo url"))
                OpenUrlInWindowsDefaultBrowser(treeitem_metadata_url.c_str());
            ImGui::PopStyleColor();
        }
        ImGui::PopStyleColor();

        ImGui::EndPopup();
    }
}

void UpdateStateAfterItemClick(GuiState& state, TreeItem* nextSubItem)
{
    auto event_queues = GuiEventQueues::getInstance();
    state.SetCurrentItem(nextSubItem);
    event_queues->CurrentItemBarEvents.Add(GuiEvents::UpdateCurrentItem);
    event_queues->DetailPagesEvents.Add(GuiEvents::UpdateCurrentItem);
    event_queues->MainEvents.Add(GuiEvents::UpdateCurrentItem);
}

bool IsKeyboardFocused()
{
    auto minItemRect = ImGui::GetItemRectMin();
    auto maxItemRect = ImGui::GetItemRectMax();
    if (ImGui::IsItemFocused() && !ImGui::IsMouseHoveringRect(minItemRect, maxItemRect) && (ImGui::IsKeyDown(ImGuiKey_UpArrow) || ImGui::IsKeyDown(ImGuiKey_DownArrow)))
        return true;

    return false;
}

void SetTreeViewIcon(GuiTextureID id)
{
    ImGui::Image((void*)(intptr_t)GetIcon(id).GetImage(), ImVec2(GetIcon(id).GetWidth(), GetIcon(id).GetHeight()));
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

void GuiTreeNode::OnTreeItemChanged(ClientHandle clientHandle, const TreeItem* ti, NotificationCode new_state)
{
    auto tree_node = static_cast<GuiTreeNode*>(clientHandle);
    tree_node->SetState(new_state);
    PostEmptyEventToGLFWContext();
    return;
}

GuiTreeNode::GuiTreeNode(TreeItem* item)
{
    this->Init(item);
}

GuiTreeNode::GuiTreeNode(TreeItem* item, bool is_open)
{
    this->Init(item);
    SetOpenStatus(is_open);
}

GuiTreeNode::GuiTreeNode(TreeItem* item, GuiTreeNode* parent, bool is_open)
{
    this->Init(item);
    m_parent = parent;
    SetOpenStatus(is_open);
}

GuiTreeNode::GuiTreeNode(GuiTreeNode&& other) noexcept
{
    m_item     = other.m_item;
    m_parent   = other.m_parent;
    m_children = std::move(other.m_children);
    m_state = other.m_state;
    m_depth = other.m_depth;
    m_has_been_openend = other.m_has_been_openend;
    m_is_open = other.m_is_open;
    
    DMS_TreeItem_RegisterStateChangeNotification(&GuiTreeNode::OnTreeItemChanged, m_item, this);
}

void GuiTreeNode::Init(TreeItem* item)
{
    m_item = item;
    m_depth = GetDepthFromTreeItem();
    DMS_TreeItem_RegisterStateChangeNotification(&GuiTreeNode::OnTreeItemChanged, m_item, this);
    
    auto default_cursor = SetCursor(LoadCursor(0, IDC_WAIT));
    m_state = (NotificationCode)DMS_TreeItem_GetProgressState(m_item); // calling UpdateMetaInfo for item A can UpdateMetaInfo of item B
    SetCursor(default_cursor);

    //if (m_state < NotificationCode::NC2_MetaReady)
    //    item->UpdateMetaInfo();
    //ImGui::SetMouseCursor(ImGuiMouseCursor_Arrow);
    
}

GuiTreeNode::~GuiTreeNode()
{
    if (m_item)
        DMS_TreeItem_ReleaseStateChangeNotification(&GuiTreeNode::OnTreeItemChanged, m_item, this);
}

auto GuiTreeNode::GetDepthFromTreeItem() -> UInt8
{
    assert(m_item);
    return DivideTreeItemFullNameIntoTreeItemNames(m_item->GetFullName().c_str()).size();
}

bool GuiTreeNode::DrawItemDropDown(GuiState &state)
{
    // TODO:
    // draw without calling update metainfo, HasCalculator true -> +, false -> _GetCurrFirstSubItem -> true + false -
    // DMS_TreeItem_HasStorage: always UpdateMetaInfo
    
    
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    ImGuiContext& g = *GImGui;

    auto cur_pos = ImGui::GetCursorPos();
    //ImGui::SetCursorPos(ImVec2(cur_pos.x+10*m_depth, cur_pos.y+offset));
    ImGui::SetCursorPos(ImVec2(cur_pos.x * m_depth, cur_pos.y));

    auto icon = IsLeaf() ? "    " : m_is_open ? ICON_RI_SUB_BOX : ICON_RI_ADD_BOX;

    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(0, 0, 0, 150));
    ImGui::PushID(m_item); //TODO: do not create button in case if IsLeaf()
    if (ImGui::Button(icon, ImVec2(20, 15)))
    {
        if (IsOpen() && IsAncestor(m_item, state.GetCurrentItem()))
        {
            UpdateStateAfterItemClick(state, m_item); // set dropped down item as current item
        }

        SetOpenStatus(!IsOpen());
    }
    ImGui::PopStyleColor();
    ImGui::PopID();

    auto spacing_w = g.Style.ItemSpacing.x;
    window->DC.CursorPos.x = window->DC.CursorPosPrevLine.x + spacing_w;
    window->DC.CursorPos.y = window->DC.CursorPosPrevLine.y;
    return 0;
}

bool GuiTreeNode::DrawItemIcon(GuiState& state)
{
    assert(m_item);
    auto vsflags = SHV_GetViewStyleFlags(m_item);
    if (vsflags & ViewStyleFlags::vsfMapView) { SetTreeViewIcon(GuiTextureID::TV_globe); }
    else if (vsflags & ViewStyleFlags::vsfTableContainer) { SetTreeViewIcon(GuiTextureID::TV_container_table); }
    else if (vsflags & ViewStyleFlags::vsfTableView) { SetTreeViewIcon(GuiTextureID::TV_table); }
    else if (vsflags & ViewStyleFlags::vsfPaletteEdit) { SetTreeViewIcon(GuiTextureID::TV_palette); }
    else if (vsflags & ViewStyleFlags::vsfContainer) { SetTreeViewIcon(GuiTextureID::TV_container); }
    else { SetTreeViewIcon(GuiTextureID::TV_unit_transparant); }

    if (ImGui::IsItemClicked(ImGuiMouseButton_Left) || ImGui::IsItemClicked(ImGuiMouseButton_Right))
    {
        ImGui::CloseCurrentPopup();
        UpdateStateAfterItemClick(state, m_item);
    }

    // right-mouse popup menu
    /*if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
    {
        ImGui::OpenPopup("treeview_popup_window");
        //ShowRightMouseClickPopupWindowIfNeeded(state);
    }*/

    return 0;
}

bool GuiTreeNode::DrawItemText(GuiState& state, TreeItem*& jump_item)
{
    assert(m_item);
    auto event_queues = GuiEventQueues::getInstance();

    // status color
    //auto status = DMS_TreeItem_GetProgressState(m_item);
    auto failed = m_item->IsFailed();

    bool node_is_selected = (m_item == state.GetCurrentItem());

    // red background for failed item
    if (failed) 
        SetTextBackgroundColor(ImGui::CalcTextSize(m_item->GetName().c_str()));
    else if (node_is_selected)
        SetTextBackgroundColor(ImGui::CalcTextSize(m_item->GetName().c_str()), IM_COL32(66, 150, 250, 79));

    ImGui::PushStyleColor(ImGuiCol_Text, GetColorFromTreeItemNotificationCode(m_state, failed));
    ImGui::PushID(m_item);

    //if (ImGui::Selectable(m_item->GetName().c_str(), node_is_selected))
    //{
    //    UpdateStateAfterItemClick(state, m_item);
    //}

    ImGui::TextUnformatted(m_item->GetName().c_str()); // render treeitem text without extra string allocation
    if (ImGui::IsItemClicked(ImGuiMouseButton_Left) || ImGui::IsItemClicked(ImGuiMouseButton_Right))
    {
        ImGui::CloseCurrentPopup();
        UpdateStateAfterItemClick(state, m_item);
    }



    /*// click event
    else if (ImGui::IsItemClicked(ImGuiMouseButton_Left) || ImGui::IsItemClicked(ImGuiMouseButton_Right))
    {
        SetKeyboardFocusToThisHwnd();
        UpdateStateAfterItemClick(state, m_item); // TODO: necessary?
    }*/
    //ImGui::PopStyleColor(3); // TODO: rework where push style color is set and popped

    // double click event
    if (ImGui::IsItemClicked() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
        event_queues->MainEvents.Add(GuiEvents::OpenNewDefaultViewWindow);

    // right-mouse popup menu
    ShowRightMouseClickPopupWindowIfNeeded(state);

    // drag-drop event
    if (!(m_item == state.GetRoot()) && ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
    {
        ImGui::SetDragDropPayload("TreeItemPtr", m_item->GetFullName().c_str(), strlen(m_item->GetFullName().c_str()));  // type is a user defined string of maximum 32 characters. Strings starting with '_' are reserved for dear imgui internal types. Data is copied and held by imgui. Return true when payload has been accepted.
        ImGui::TextUnformatted(m_item->GetName().c_str());
        ImGui::EndDragDropSource();
    }

    // jump event
    if (jump_item && jump_item == m_item)
    {
        UpdateStateAfterItemClick(state, m_item);
        ImGui::SetScrollHereY();
        jump_item = nullptr;
    }

    // alphabetical letter jump


    ImGui::PopID();

    //ImGui::Text(m_item->GetName().c_str());
    ImGui::PopStyleColor(); // treeitem color
    return 0;
}

void GuiTreeNode::DrawItemWriteStorageIcon()
{
    auto has_storage_manager = m_item->HasStorageManager();
    auto parent = m_item->GetTreeParent();

    if (!has_storage_manager && !parent) // root has no parent
        return;

    auto parent_has_storage_manager = parent->HasStorageManager();
    
    if (!(has_storage_manager || parent_has_storage_manager))
        return;

    bool is_read_only = has_storage_manager ? m_item->GetStorageManager()->IsReadOnly() : parent->GetStorageManager()->IsReadOnly();

    //if (is_read_only && m_item->Clac())

    //auto test = m_item->GetExpr();

    

    if (is_read_only && m_item->HasCalculator())
        return;

    if (m_item->IsDisabledStorage())
        return;

    float offset = 3.0;
    ImGuiContext& g = *GImGui;
    auto window = ImGui::GetCurrentWindow();
    auto spacing_w = g.Style.ItemSpacing.x;
    window->DC.CursorPos.x = window->DC.CursorPosPrevLine.x + spacing_w;
    window->DC.CursorPos.y = window->DC.CursorPosPrevLine.y+offset;

    ImGui::PushStyleColor(ImGuiCol_Text, m_state<NC2_Committed ? IM_COL32(0,0,0,100) : IM_COL32(0,0,0,200));
    ImGui::TextUnformatted(is_read_only ? ICON_RI_DATABASE_SOLID : ICON_RI_FLOPPY_SOLID);
    ImGui::PopStyleColor();
    window->DC.CursorPos.y = window->DC.CursorPos.y - offset;

    return;
}

void GuiTreeNode::clear()
{
    if (m_item)
    {
        DMS_TreeItem_ReleaseStateChangeNotification(&GuiTreeNode::OnTreeItemChanged, m_item, this);
        m_item = nullptr;
        m_children.clear();
        m_has_been_openend = false;
    }
}

void GuiTreeNode::SetOpenStatus(bool do_open)
{ 
    m_is_open = do_open;

    if (m_is_open && !m_has_been_openend && m_state >= NotificationCode::NC2_MetaReady) // children unknown at this point
    {
        AddChildren();
        m_has_been_openend = true; // add children once and only once
    }
}

void GuiTreeNode::SetState(NotificationCode new_state)
{
    m_state = new_state;
}

auto GuiTreeNode::AddChildren() -> void
{
    assert(m_item);
    TreeItem* next_subitem = m_item->_GetFirstSubItem();
    while (next_subitem)
    {
        m_children.emplace_back(next_subitem, this, false);
        next_subitem = next_subitem->GetNextItem();
    }
}

auto GuiTreeNode::GetState() -> NotificationCode
{
    return m_state;
}

auto GuiTreeNode::GetFirstSibling() -> GuiTreeNode*
{
    return &m_children.front();
}

auto GuiTreeNode::GetSiblingIterator() -> std::vector<GuiTreeNode>::iterator
{
    return m_children.begin();
}

auto GuiTreeNode::GetSiblingEnd() -> std::vector<GuiTreeNode>::iterator
{
    return m_children.end();
}

bool GuiTreeNode::IsLeaf()
{
    if (!m_item)
        return false;

    if (m_item->HasSubItems())
        return false;

    return true;
}

bool GuiTreeNode::Draw(GuiState& state, TreeItem*& jump_item)
{
    DrawItemDropDown(state);
    DrawItemIcon(state);
    ImGui::SameLine();
    DrawItemText(state, jump_item);
    DrawItemWriteStorageIcon();

    // Modify y-spacing between TreeView items
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    const UInt8 offset = 4;
    window->DC.CursorPos.y = window->DC.CursorPos.y - offset;

    return false;
}

bool GuiTree::IsInitialized()
{
    return m_is_initialized;
}

void GuiTree::Init(GuiState& state)
{
    if (state.GetRoot())
    {
        m_Root.Init(state.GetRoot());
        m_Root.SetOpenStatus(true);
        m_start_node = &m_Root;
        m_is_initialized = true;
    }
}

GuiTree::~GuiTree()
{
    //if (m_Root)
    //    m_Root.release();
}

bool GuiTree::SpaceIsAvailableForTreeNode()
{
    return ImGui::GetContentRegionAvail().y > 0;
}

auto GetFinalSibblingNode(GuiTreeNode& node) -> GuiTreeNode*
{
    auto& last_child_node = node.m_children.back();

    if (last_child_node.IsLeaf())
        return &last_child_node;

    if (last_child_node.IsOpen())
        return GetFinalSibblingNode(last_child_node);

    return &last_child_node;
}

auto GetFirstSibblingNode(GuiTreeNode& node) -> GuiTreeNode*
{
    auto parent_node = node.m_parent;

    if (!parent_node)
        return &node;

    bool matched = false;
    for (auto& child_node : parent_node->m_children)
    {
        if (child_node.GetItem() == node.GetItem())
        {
            matched = true;
            continue;
        }

        if (!matched)
            continue;

        return &child_node;
    }

    return GetFirstSibblingNode(*parent_node);
}

auto GuiTree::AscendVisibleTree(GuiTreeNode& node) -> GuiTreeNode*
{
    if (node.m_parent==nullptr) // node is root
        return &node;

    auto parent_node = node.m_parent;
    if (parent_node->m_children.begin()->GetItem() == node.GetItem()) // node is first child of parent
        return parent_node;

    bool matched = false;
    for (auto& child_node : std::ranges::views::reverse(parent_node->m_children)) 
    {
        if (child_node.GetItem() == node.GetItem())
        {
            matched = true;
            continue;
        }

        if (!matched)
            continue;

        if (child_node.IsLeaf() || !child_node.IsOpen()) // previous child is leaf or closed
            return &child_node;

        return GetFinalSibblingNode(child_node);
    }
    return &node;
}

auto GuiTree::DescendVisibleTree(GuiTreeNode& node) -> GuiTreeNode*
{
    if (node.IsOpen() && !node.m_children.empty())
        return &*node.m_children.begin();

    return GetFirstSibblingNode(node);
}

auto GuiTree::JumpToLetter(GuiState &state, std::string_view letter) -> TreeItem*
{
    auto next_visible_node = m_curr_node;
    while (true)
    {
        next_visible_node = DescendVisibleTree(*next_visible_node);
        if (boost::iequals(letter.data(), std::string(next_visible_node->GetItem()->GetName().c_str()).substr(0,1)))
        {
            UpdateStateAfterItemClick(state, next_visible_node->GetItem());
            m_curr_node = next_visible_node;
            return next_visible_node->GetItem();
        }
        if (next_visible_node == m_curr_node)
            return nullptr;
    }
}

void GuiTree::ActOnLeftRightArrowKeys(GuiState& state, GuiTreeNode* node)
{
    if (ImGui::IsWindowFocused() && node->GetItem() == state.GetCurrentItem())
    {
        if (ImGui::IsKeyPressed(ImGuiKey_RightArrow))
        {
            if (node->IsOpen())
            {
                auto descended_node = DescendVisibleTree(*node);
                if (descended_node)
                    UpdateStateAfterItemClick(state, descended_node->GetItem());
            }
            else
                node->SetOpenStatus(true);
        }
        if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow))
        {
            if (!node->IsOpen())
            {
                if (node->m_parent)
                    UpdateStateAfterItemClick(state, node->m_parent->GetItem());
            }
            else
                node->SetOpenStatus(false);
        }
    }
}

bool GuiTree::DrawBranch(GuiTreeNode& node, GuiState& state, TreeItem*& jump_item)
{
    //if (!SpaceIsAvailableForTreeNode()) //TODO: implement
    //    return false;

    if (node.GetState() < NotificationCode::NC2_MetaReady)
        return true;

    auto next_node = node.GetSiblingIterator();

    // TODO: use clearer syntax: for (auto& next_node : node.m_children)
    while (next_node != node.GetSiblingEnd())
    {
        if (IsAncestor(next_node->GetItem(), state.GetCurrentItem()))
            next_node->SetOpenStatus(true); // TODO: is optional, can be reconsidered in the future

        ActOnLeftRightArrowKeys(state, &*next_node);

        /*if (ImGui::IsWindowFocused() && next_node->GetItem() == state.GetCurrentItem())
        {
            if (ImGui::IsKeyPressed(ImGuiKey_RightArrow))
            {
                if (next_node->IsOpen())
                {
                    auto descended_node = DescendVisibleTree(*next_node);
                    if (descended_node)
                        UpdateStateAfterItemClick(state, descended_node->GetItem());
                }
                else
                    next_node->SetOpenStatus(true);
            }
            if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow))
            {
                if (!next_node->IsOpen())
                {
                    if (next_node->m_parent)
                        UpdateStateAfterItemClick(state, next_node->m_parent->GetItem());
                }
                else
                    next_node->SetOpenStatus(false);
            }
        }*/

        if (next_node->GetItem() == state.GetCurrentItem())
            m_curr_node = &*next_node;        

        next_node->Draw(state, jump_item);
        if (next_node->IsOpen())
        {
            if (next_node->GetState() >= PS_MetaInfo)
            {
                if (!DrawBranch(*next_node, state, jump_item))
                {
                    return false;
                }
            }
        }
        
        std::advance(next_node, 1);
    }
    return true;
}

void GuiTree::Draw(GuiState& state, TreeItem*& jump_item)
{
    if (!m_start_node)
        return;

    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.f, 0.f, 0.f, 0.f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.f, 0.f, 0.f, 0.f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.f, 0.f, 0.f, 0.f));

    auto m_currnode = m_start_node;
    ActOnLeftRightArrowKeys(state, m_currnode);
    m_Root.Draw(state, jump_item);
    if (m_Root.IsOpen())
        DrawBranch(*m_currnode, state, jump_item);
    ImGui::PopStyleColor(3);
}

void GuiTree::clear()
{
    m_Root.clear();
    m_is_initialized = false;
}

GuiTreeView::~GuiTreeView() 
{}

void GuiTreeView::clear()
{
    m_tree.clear();
}

void GuiTreeView::ProcessTreeviewEvent(GuiEvents& event, GuiState& state)
{
    switch (event)
    {
    case GuiEvents::UpdateCurrentAndCompatibleSubItems:
    case GuiEvents::UpdateCurrentItem:
    {
        m_TemporaryJumpItem = state.GetCurrentItem();
        break;
    }
    case GuiEvents::AscendVisibleTree: // TODO: known issue repeat key up not working
    {
        if (!ImGui::IsWindowFocused())
            break;

        auto ascended_node = m_tree.AscendVisibleTree(*m_tree.m_curr_node);
        if (ascended_node)
        {
            UpdateStateAfterItemClick(state, ascended_node->GetItem());
            m_tree.m_curr_node = ascended_node;
            m_TemporaryJumpItem = ascended_node->GetItem();
        }
        break;
    }
    case GuiEvents::DescendVisibleTree: // TODO: known issue repeat key down not working
    {   
        if (!ImGui::IsWindowFocused())
            break;

        auto descended_node = m_tree.DescendVisibleTree(*m_tree.m_curr_node);
        if (descended_node)
        {
            UpdateStateAfterItemClick(state, descended_node->GetItem());
            m_tree.m_curr_node = descended_node;
            m_TemporaryJumpItem = descended_node->GetItem();
        }
        break;
    }
    case GuiEvents::TreeViewJumpKeyPress:
    {
        m_TemporaryJumpItem = m_tree.JumpToLetter(state, GuiState::m_JumpLetter);
        GuiState::m_JumpLetter.clear();
        break;
    }
    default: break;
    }    
}

void GuiTreeView::Update(bool* p_open, GuiState& state)
{
    if (!m_tree.IsInitialized() && state.GetRoot())
        m_tree.Init(state);

    //const ImVec2 size(100,10000);
    //ImGui::SetNextWindowContentSize(size);
    if (!ImGui::Begin("TreeView", p_open, ImGuiWindowFlags_None | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoNavInputs))
    {
        ImGui::End();
        return;
    }

    AutoHideWindowDocknodeTabBar(is_docking_initialized);

    auto event_queues = GuiEventQueues::getInstance();
    if (event_queues->TreeViewEvents.HasEvents())
    {
        auto event = event_queues->TreeViewEvents.Pop();
        ProcessTreeviewEvent(event, state);
    }

    if (!m_tree.IsInitialized())
    {
        ImGui::End();
        return;
    }

    m_tree.Draw(state, m_TemporaryJumpItem);

    ImGui::End();
}

/*auto GuiTreeView::CreateBranch(GuiState& state, TreeItem* branch) -> bool
{
    auto event_queues = GuiEventQueues::getInstance();
    TreeItem* nextSubItem = branch->_GetFirstSubItem();
    if (!nextSubItem)
        return true;

    while (nextSubItem)
    {
        if (nextSubItem->m_State.GetProgress() < PS_MetaInfo && !nextSubItem->m_State.IsUpdatingMetaInfo())
        {
            //DMS_TreeItem_RegisterStateChangeNotification(&OnTreeItemChanged, nextSubItem, nullptr);
            //CWaitCursor wait;
            nextSubItem->UpdateMetaInfo();
            //wait.Restore();
        }     

        ImGuiTreeNodeFlags useFlags = nextSubItem == state.GetCurrentItem() ? m_BaseFlags | ImGuiTreeNodeFlags_Selected : m_BaseFlags;

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

        if (IsAncestor(nextSubItem, state.GetCurrentItem()))
            ImGui::SetNextItemOpen(true);

        auto treeItemIsOpen = ImGui::TreeNodeEx(nextSubItem->GetName().c_str(), nextSubItem->_GetFirstSubItem() ? useFlags : useFlags | ImGuiTreeNodeFlags_Leaf);
        
        // keyboard focus event
        if (IsKeyboardFocused())
            UpdateStateAfterItemClick(state, nextSubItem);

        // click event
        if (ImGui::IsItemClicked(ImGuiMouseButton_Left) || ImGui::IsItemClicked(ImGuiMouseButton_Right)) // item is clicked
        {
            SetKeyboardFocusToThisHwnd();
            UpdateStateAfterItemClick(state, nextSubItem);
        }

        // double click event
        if (ImGui::IsItemClicked() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
            event_queues->MainEvents.Add(GuiEvents::OpenNewDefaultViewWindow);

        // right-mouse popup menu
        ShowRightMouseClickPopupWindowIfNeeded(state);

        // alphabetical letter jump
        if ((!state.m_JumpLetter.first.empty() && !state.m_JumpLetter.second.empty()) && IsAlphabeticalKeyJump(state, nextSubItem))
        {
            UpdateStateAfterItemClick(state, nextSubItem);
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
                UpdateStateAfterItemClick(state, nextSubItem);
                ImGui::SetScrollHereY();
                m_TemporaryJumpItem = nullptr;
            }

            if (nextSubItem->m_State.GetProgress() >= PS_MetaInfo)
                CreateBranch(state, nextSubItem);
            ImGui::TreePop();
        }
        ImGui::PopStyleColor();

        nextSubItem = nextSubItem->GetNextItem();
    }
    return true;
}*/

/*auto GuiTreeView::CreateTree(GuiState& state) -> bool
{
    ImGuiTreeNodeFlags useFlags = state.GetRoot() == state.GetCurrentItem() ? m_BaseFlags | ImGuiTreeNodeFlags_Selected : m_BaseFlags;
    auto status = DMS_TreeItem_GetProgressState(state.GetRoot());
    ImGui::PushStyleColor(ImGuiCol_Text, GetColorFromTreeItemNotificationCode(status, false));
    if (ImGui::TreeNodeEx(state.GetRoot()->GetName().c_str(), useFlags | ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (IsKeyboardFocused())
            UpdateStateAfterItemClick(state, state.GetRoot());

        // click events
        if (ImGui::IsItemClicked() && ImGui::IsMouseDown(ImGuiMouseButton_Left))
            UpdateStateAfterItemClick(state, state.GetRoot());
            
        if (state.GetRoot()->HasSubItems())
            CreateBranch(state, state.GetRoot());
        ImGui::TreePop();
    }
    ImGui::PopStyleColor();

    if (!state.m_JumpLetter.first.empty() && !state.m_JumpLetter.second.empty()) // alphabetical jumpletter present, but not acted upon yet
        IsAlphabeticalKeyJump(state, nullptr, true);

    return true;
}*/