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
#include <functional>
#include<ranges>

auto GetColorFromTreeItemNotificationCode(UInt32 status, bool isFailed) -> UInt32
{
    if (isFailed)
        return IM_COL32(0, 0, 0, 255);

    switch (status)
    {
    case NotificationCode::NC2_FailedFlag || NotificationCode::NC2_Invalidated: return IM_COL32(0, 0, 0, 255);
    case NotificationCode::NC2_DataReady: return IM_COL32(0, 153, 51, 255);
    case NotificationCode::NC2_Committed: return IM_COL32(82, 136, 219, 255);
    default: return IM_COL32(255, 0, 102, 255);
    }
}


auto GuiTreeView::IsAlphabeticalKeyJump(GuiState& state, TreeItem* nextItem, bool looped = false) -> bool
{
    static bool loop;
    static bool passedCurrentItem;

    if (looped)
    {
        loop = true;
        passedCurrentItem = false;
        return false;
    }

    if (nextItem == state.GetCurrentItem())
    {
        passedCurrentItem = true;
        return false;
    }

    if (loop && !passedCurrentItem && (std::string(nextItem->GetName().c_str()).substr(0, 1).c_str() == state.m_JumpLetter.first || std::string(nextItem->GetName().c_str()).substr(0, 1).c_str() == state.m_JumpLetter.second))
    {
        state.m_JumpLetter.first.clear();
        state.m_JumpLetter.second.clear();
        loop = false;
        return true;
    }

    if (!loop && passedCurrentItem && (std::string(nextItem->GetName().c_str()).substr(0, 1).c_str() == state.m_JumpLetter.first || std::string(nextItem->GetName().c_str()).substr(0, 1).c_str() == state.m_JumpLetter.second))
    {
        state.m_JumpLetter.first.clear();
        state.m_JumpLetter.second.clear();
        passedCurrentItem = false;
        return true;
    }

    return false;
}

auto ShowRightMouseClickPopupWindowIfNeeded(GuiState& state) -> void
{
    // right-mouse popup menu
    if (ImGui::BeginPopupContextItem())
    {
        auto event_queues = GuiEventQueues::getInstance();
        //auto base_style_color = ImGui::GetStyleColorVec4(0);
        //auto current_style_color = ImGui::GetStyleColorVec4(1);
        //ImGui::PopStyleColor(2);
        if (ImGui::Button("Edit Config Source       Ctrl-E"))
            event_queues->MainEvents.Add(GuiEvents::OpenConfigSource);

        if (ImGui::Button("Default View"))
            event_queues->MainEvents.Add(GuiEvents::OpenNewDefaultViewWindow);

        if (ImGui::Button("Map View                 CTRL-M"))
            event_queues->MainEvents.Add(GuiEvents::OpenNewMapViewWindow);

        if (ImGui::Button("Table View               CTRL-D"))
            event_queues->MainEvents.Add(GuiEvents::OpenNewTableViewWindow);

        //url 
        auto treeitem_metadata_url = TreeItemPropertyValue(state.GetCurrentItem(), urlPropDefPtr);
        if (!treeitem_metadata_url.empty())
        {
            if (ImGui::Button("Open Metainfo url"))
            {
                ShellExecuteA(0, NULL, treeitem_metadata_url.c_str(), NULL, NULL, SW_SHOWNORMAL);
            }
        }

        //if (ImGui::Button("Close"))
        //    ImGui::CloseCurrentPopup();
        //ImGui::PushStyleColor(ImGuiCol_Text, GetColorU32(style_color));
        ImGui::EndPopup();
        //ImGui::PushStyleColor(ImGuiCol_Text, base_style_color);
        //ImGui::PushStyleColor(ImGuiCol_Text, current_style_color);
    }
}

auto UpdateStateAfterItemClick(GuiState& state, TreeItem* nextSubItem) -> void
{
    auto event_queues = GuiEventQueues::getInstance();
    state.SetCurrentItem(nextSubItem);
    event_queues->CurrentItemBarEvents.Add(GuiEvents::UpdateCurrentItem);
    event_queues->DetailPagesEvents.Add(GuiEvents::UpdateCurrentItem);
    event_queues->MainEvents.Add(GuiEvents::UpdateCurrentItem);
}

auto IsKeyboardFocused() -> bool
{
    auto minItemRect = ImGui::GetItemRectMin();
    auto maxItemRect = ImGui::GetItemRectMax();
    if (ImGui::IsItemFocused() && !ImGui::IsMouseHoveringRect(minItemRect, maxItemRect) && (ImGui::IsKeyDown(ImGuiKey_UpArrow) || ImGui::IsKeyDown(ImGuiKey_DownArrow)))
        return true;

    return false;
}

auto SetTreeViewIcon(GuiTextureID id) -> void
{
    ImGui::Image((void*)(intptr_t)GetIcon(id).GetImage(), ImVec2(GetIcon(id).GetWidth(), GetIcon(id).GetHeight()));
}

auto IsAncestor(TreeItem* ancestorTarget, TreeItem* descendant) -> bool
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

auto GuiTreeNode::OnTreeItemChanged(ClientHandle clientHandle, const TreeItem* ti, NotificationCode new_state) -> void
{
    auto tree_node = (GuiTreeNode*)clientHandle;
    tree_node->SetState(new_state);
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

auto GuiTreeNode::Init(TreeItem* item) -> void
{
    m_item = item;
    m_depth = GetDepthFromTreeItem();
    auto default_cursor = SetCursor(LoadCursor(0, IDC_WAIT));
    m_state = (NotificationCode)DMS_TreeItem_GetProgressState(m_item); // calling UpdateMetaInfo for item A can UpdateMetaInfo of item B

    DMS_TreeItem_RegisterStateChangeNotification(&GuiTreeNode::OnTreeItemChanged, m_item, this);

    if (m_state < NotificationCode::NC2_MetaReady)
        item->UpdateMetaInfo();

    SetCursor(default_cursor);
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

auto GuiTreeNode::DrawItemDropDown(GuiState &state) -> bool
{
    // TODO:
    // draw without calling update metainfo, HasCalculator true -> +, false -> _GetCurrFirstSubItem -> true + false -
    // DMS_TreeItem_HasStorage: always UpdateMetaInfo
    
    
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    ImGuiContext& g = *GImGui;

    float offset = 0;
    auto cur_pos = ImGui::GetCursorPos();
    ImGui::SetCursorPos(ImVec2(cur_pos.x+10*m_depth, cur_pos.y+offset));
    
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.f, 0.f, 0.f, 0.f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.f, 0.f, 0.f, 0.f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.f, 0.f, 0.f, 0.f));

    auto icon = IsLeaf() ? "    " : m_is_open ? ICON_RI_SUB_BOX : ICON_RI_ADD_BOX;

    ImGui::PushID(m_item);
    if (ImGui::Button(icon))
    {
        UpdateStateAfterItemClick(state, m_item);
        SetOpenStatus(!IsOpen());
    }
    ImGui::PopID();

    auto spacing_w = g.Style.ItemSpacing.x;
    window->DC.CursorPos.x = window->DC.CursorPosPrevLine.x + spacing_w;
    window->DC.CursorPos.y = window->DC.CursorPosPrevLine.y - offset;
    ImGui::PopStyleColor(3);

    return 0;
}

auto GuiTreeNode::DrawItemIcon() -> bool
{
    assert(m_item);
    auto vsflags = SHV_GetViewStyleFlags(m_item); // TODO: check if this calls UpdateMetaInfo
    if (vsflags & ViewStyleFlags::vsfMapView) { SetTreeViewIcon(GuiTextureID::TV_globe); }
    else if (vsflags & ViewStyleFlags::vsfTableContainer) { SetTreeViewIcon(GuiTextureID::TV_container_table); }
    else if (vsflags & ViewStyleFlags::vsfTableView) { SetTreeViewIcon(GuiTextureID::TV_table); }
    else if (vsflags & ViewStyleFlags::vsfPaletteEdit) { SetTreeViewIcon(GuiTextureID::TV_palette); }
    else if (vsflags & ViewStyleFlags::vsfContainer) { SetTreeViewIcon(GuiTextureID::TV_container); }
    else { SetTreeViewIcon(GuiTextureID::TV_unit_transparant); }
    return 0;
}

auto GuiTreeNode::DrawItemText(GuiState& state, TreeItem*& jump_item) -> bool
{
    assert(m_item);
    auto event_queues = GuiEventQueues::getInstance();

    // status color
    auto status = DMS_TreeItem_GetProgressState(m_item);
    auto failed = m_item->IsFailed();

    // red background for failed item
    if (failed) 
        SetTextBackgroundRed(ImGui::CalcTextSize(m_item->GetName().c_str()));
    /* {
        auto draw_list = ImGui::GetWindowDrawList();
        auto cur_pos = ImGui::GetCursorScreenPos();
        auto text_size = ImGui::CalcTextSize(m_item->GetName().c_str());
        draw_list->AddRectFilled(cur_pos, ImVec2(cur_pos.x+text_size.x, cur_pos.y+text_size.y), IM_COL32(225, 6, 0, 255));
    }*/

    ImGui::PushStyleColor(ImGuiCol_Text, GetColorFromTreeItemNotificationCode(status, failed));
    bool is_selected = (m_item == state.GetCurrentItem());
    ImGui::PushID(m_item);
    if (ImGui::Selectable(m_item->GetName().c_str(), is_selected))
    {
        UpdateStateAfterItemClick(state, m_item);
    }
    // click event
    else if (ImGui::IsItemClicked(ImGuiMouseButton_Left) || ImGui::IsItemClicked(ImGuiMouseButton_Right))
    {
        SetKeyboardFocusToThisHwnd();
        UpdateStateAfterItemClick(state, m_item);
    }

    // keyboard selection event
    //if (IsKeyboardFocused())
    //    UpdateStateAfterItemClick(state, m_item);

    // double click event
    if (ImGui::IsItemClicked() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
        event_queues->MainEvents.Add(GuiEvents::OpenNewDefaultViewWindow);

    // right-mouse popup menu
    ShowRightMouseClickPopupWindowIfNeeded(state);

    // drag-drop event
    if (ImGui::BeginDragDropSource() && !(m_item== state.GetRoot()))
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
    ImGui::PopStyleColor();
    return 0;
}

auto GuiTreeNode::clear() -> void
{
    if (m_item)
    {
        DMS_TreeItem_ReleaseStateChangeNotification(&GuiTreeNode::OnTreeItemChanged, m_item, this);
        m_item = nullptr;
        m_children.clear();
        m_has_been_openend = false;
    }
}

auto GuiTreeNode::SetOpenStatus(bool do_open) -> void
{ 
    m_is_open = do_open;

    if (m_is_open && !m_has_been_openend && m_state >= NotificationCode::NC2_MetaReady) // children unknown at this point
    {
        AddChildren();
        m_has_been_openend = true; // add children once and only once
    }
}

auto GuiTreeNode::SetState(NotificationCode new_state) -> void
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

auto GuiTreeNode::IsLeaf() -> bool
{
    if (!m_item)
        return false;

    if (m_item->HasSubItems())
        return false;

    return true;
}

auto GuiTreeNode::Draw(GuiState& state, TreeItem*& jump_item) -> bool
{
    DrawItemDropDown(state);
    //auto test1 = ImGui::GetCursorPos();
    //ImGui::SameLine(); // SameLine moves cursor back to last draw position..
    //auto test2 = ImGui::GetCursorPos();
    DrawItemIcon();
    ImGui::SameLine();
    DrawItemText(state, jump_item);
    
    // Modify y-spacing between TreeView items
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    const UInt8 offset = 4;
    window->DC.CursorPos.y = window->DC.CursorPos.y - offset;

    return false;
}

auto GuiTree::IsInitialized() -> bool
{
    return m_is_initialized;
}

auto GuiTree::Init(GuiState& state) -> void
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

auto GuiTree::AscendVisibleTree(GuiState &state, GuiTreeNode& node) -> GuiTreeNode*
{
    if (node.GetItem()==state.GetRoot()) // node is root
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

auto GuiTree::DescendVisibleTree(GuiState& state, GuiTreeNode& node) -> GuiTreeNode*
{
    if (node.IsOpen() && !node.m_children.empty())
        return &*node.m_children.begin();

    return GetFirstSibblingNode(node);
}

auto GuiTree::DrawBranch(GuiTreeNode& node, GuiState& state, TreeItem*& jump_item) -> bool
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

        if (ImGui::IsWindowFocused() && next_node->GetItem() == state.GetCurrentItem())
        {
            if (ImGui::IsKeyPressed(ImGuiKey_RightArrow))
            {
                if (next_node->IsOpen())
                {
                    auto descended_node = DescendVisibleTree(state, *next_node);
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
                    //auto ascended_node = AscendVisibleTree(state, *next_node);
                    //if (ascended_node)
                    if (next_node->m_parent)
                        UpdateStateAfterItemClick(state, next_node->m_parent->GetItem());
                }
                else
                    next_node->SetOpenStatus(false);
            }
        }

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

auto GuiTree::Draw(GuiState& state, TreeItem*& jump_item) -> void
{
    if (!m_start_node)
        return;

    auto m_currnode = m_start_node;
    m_Root.Draw(state, jump_item);
    if (m_Root.IsOpen())
        DrawBranch(*m_currnode, state, jump_item);
}

auto GuiTree::clear() -> void
{
    m_Root.clear();
    m_is_initialized = false;
}

GuiTreeView::~GuiTreeView() 
{}

auto GuiTreeView::clear() -> void
{
    m_tree.clear();
}


auto GuiTreeView::OnStateChange(ClientHandle clientHandle, const TreeItem* self, NotificationCode notificationCode) -> void
{
    if (notificationCode == NotificationCode::CC_CreateMdiChild)
    { 
        auto event_queues = GuiEventQueues::getInstance();
        //event_queues->MapViewEvents.Add();
    }
}

auto GuiTreeView::ProcessTreeviewEvent(GuiEvents& event, GuiState& state) -> void
{
    switch (event)
    {
    case GuiEvents::UpdateCurrentAndCompatibleSubItems:
    case GuiEvents::UpdateCurrentItem:
    {
        m_TemporaryJumpItem = state.GetCurrentItem();
        break;
    }
    case GuiEvents::AscendVisibleTree:
    {
        auto ascended_node = m_tree.AscendVisibleTree(state, *m_tree.m_curr_node);
        if (ascended_node)
        {
            UpdateStateAfterItemClick(state, ascended_node->GetItem());
            m_tree.m_curr_node = ascended_node;
        }
        break;
    }
    case GuiEvents::DescendVisibleTree:
    {   
        auto descended_node = m_tree.DescendVisibleTree(state, *m_tree.m_curr_node);
        if (descended_node)
        {
            UpdateStateAfterItemClick(state, descended_node->GetItem());
            m_tree.m_curr_node = descended_node;
        }
        break;
    }
    default: break;
    }    
}

auto GuiTreeView::Update(bool* p_open, GuiState& state) -> void
{
    GuiTree* tree = nullptr;
    bool use_default_tree = false;

    auto event_queues = GuiEventQueues::getInstance();
    if (event_queues->TreeViewEvents.HasEvents())
    {
        if (event_queues->TreeViewEvents.Size() > 1)
            int i = 0;

        auto event = event_queues->TreeViewEvents.Pop();
        ProcessTreeviewEvent(event, state);
    }

    if (!m_tree.IsInitialized() && state.GetRoot())
        m_tree.Init(state);

    //const ImVec2 size(100,10000);
    //ImGui::SetNextWindowContentSize(size);
    if (!ImGui::Begin("TreeView", p_open, ImGuiWindowFlags_None | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoNavInputs))
    {
        ImGui::End();
        return;
    }

    if (!m_tree.IsInitialized())
    {
        ImGui::End();
        return;
    }

    m_tree.Draw(state, m_TemporaryJumpItem);


    /*if (!use_default_tree)
    {
        if (!m_tree && state.GetRoot())
            m_tree = std::make_unique<GuiTree>(state.GetRoot());

        //if (state.GetRoot())
        //    tree = GuiTree::getInstance(state.GetRoot());

        if (m_tree)
        {
            // visualize tree

            m_tree->Draw(state, m_TemporaryJumpItem);
        }
    }*/
    /*if (use_default_tree)
    {
        if (state.GetRoot())
            CreateTree(state);

        if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
            SetKeyboardFocusToThisHwnd();
    }*/

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