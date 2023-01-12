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

auto ShowRightMouseClickPopupWindowIfNeeded() -> void
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
    //DMS_TreeItem_ReleaseStateChangeNotification(&GuiTreeNode::OnTreeItemChanged, other.m_item, &other);
    m_item     = other.m_item;
    m_parent   = other.m_parent;
    m_children = std::move(other.m_children);
    m_state = other.m_state; //m_state.store(other.m_state);
    m_depth = other.m_depth;
    m_has_been_openend = other.m_has_been_openend;
    m_is_open = other.m_is_open;
    
    DMS_TreeItem_RegisterStateChangeNotification(&GuiTreeNode::OnTreeItemChanged, m_item, this);

    // clean other
    //other.m_item = nullptr;
    //other.m_parent = nullptr;
    //other.m_children.clear();
}

auto GuiTreeNode::OnTreeItemChanged(ClientHandle clientHandle, const TreeItem* ti, NotificationCode new_state) -> void
{
    auto tree_node = (GuiTreeNode*)clientHandle;
    tree_node->SetState(new_state);
}

auto GuiTreeNode::Init(TreeItem* item) -> void
{
    m_item = item;
    m_depth = GetDepthFromTreeItem();
    m_state = (NotificationCode)DMS_TreeItem_GetProgressState(m_item); // calling UpdateMetaInfo for item A can UpdateMetaInfo of item B

    DMS_TreeItem_RegisterStateChangeNotification(&GuiTreeNode::OnTreeItemChanged, m_item, this);

    if (m_state < NotificationCode::NC2_MetaReady)
        item->UpdateMetaInfo();
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
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    ImGuiContext& g = *GImGui;

    float offset = 0;
    auto cur_pos = ImGui::GetCursorPos();
    ImGui::SetCursorPos(ImVec2(cur_pos.x+10*m_depth, cur_pos.y+offset));
    
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.f, 0.f, 0.f, 0.f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.f, 0.f, 0.f, 0.f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.f, 0.f, 0.f, 0.f));

    auto icon = IsLeaf() ? " " : m_is_open ? ICON_RI_MIN : ICON_RI_PLUS;

    ImGui::PushID(m_item);
    if (ImGui::Button(icon))
    {
        UpdateStateAfterItemClick(state, m_item);
        SetOpenStatus(!GetOpenStatus());
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
    {
        auto draw_list = ImGui::GetWindowDrawList();
        auto cur_pos = ImGui::GetCursorScreenPos();
        auto text_size = ImGui::CalcTextSize(m_item->GetName().c_str());
        draw_list->AddRectFilled(cur_pos, ImVec2(cur_pos.x+text_size.x, cur_pos.y+text_size.y), IM_COL32(225, 6, 0, 255));
    }

    ImGui::PushStyleColor(ImGuiCol_Text, GetColorFromTreeItemNotificationCode(status, failed));
    const bool is_selected = (m_item == state.GetCurrentItem());
    ImGui::PushID(m_item);
    if (ImGui::Selectable(m_item->GetName().c_str(), is_selected)) // m_selectable_name.c_str()
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
    if (IsKeyboardFocused())
        UpdateStateAfterItemClick(state, m_item);

    // double click event
    if (ImGui::IsItemClicked() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
        event_queues->MainEvents.Add(GuiEvents::OpenNewDefaultViewWindow);

    // right-mouse popup menu
    ShowRightMouseClickPopupWindowIfNeeded();

    // drag-drop event
    if (ImGui::BeginDragDropSource())
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

    if (m_is_open && !m_has_been_openend && m_state >= NotificationCode::NC2_MetaReady) // children known at this point
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
        m_children.emplace_back(next_subitem, false);
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
        m_startnode = &m_Root;
        m_is_initialized = true;
    }
}

GuiTree::~GuiTree()
{
    //if (m_Root)
    //    m_Root.release();
}

auto GuiTree::SpaceIsAvailableForTreeNode() -> bool
{
    return ImGui::GetContentRegionAvail().y > 0;
}

auto GuiTree::DrawBranch(GuiTreeNode& node, GuiState& state, TreeItem*& jump_item) -> bool
{
    //if (!SpaceIsAvailableForTreeNode()) //TODO: implement use of this
    //    return false;

    if (node.GetState() < NotificationCode::NC2_MetaReady)
        return true;

    auto next_node = node.GetSiblingIterator();
    while (next_node != node.GetSiblingEnd())
    {
        if (IsAncestor(next_node->GetItem(), state.GetCurrentItem()))
            next_node->SetOpenStatus(true);

        next_node->Draw(state, jump_item);

        if (next_node->GetOpenStatus())
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
    if (!m_startnode)
        return;

    auto m_currnode = m_startnode;
    m_Root.Draw(state, jump_item);
    if (m_Root.GetOpenStatus())
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

auto GuiTreeView::Update(bool* p_open, GuiState& state) -> void
{
    GuiTree* tree = nullptr;
    bool use_default_tree = false;

    auto event_queues = GuiEventQueues::getInstance();
    if (event_queues->TreeViewEvents.HasEvents())
    {
        event_queues->TreeViewEvents.Pop();
        m_TemporaryJumpItem = state.GetCurrentItem();
    }

    if (!m_tree.IsInitialized() && state.GetRoot())
        m_tree.Init(state);

    //const ImVec2 size(100,10000);
    //ImGui::SetNextWindowContentSize(size);
    if (!ImGui::Begin("Treeview", p_open, ImGuiWindowFlags_None | ImGuiWindowFlags_NoTitleBar))
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
    if (use_default_tree)
    {
        if (state.GetRoot())
            CreateTree(state);

        if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
            SetKeyboardFocusToThisHwnd();
    }

    ImGui::End();
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
    
    if (!loop && passedCurrentItem && (std::string(nextItem->GetName().c_str()).substr(0,1).c_str() == state.m_JumpLetter.first || std::string(nextItem->GetName().c_str()).substr(0, 1).c_str() == state.m_JumpLetter.second))
    {
        state.m_JumpLetter.first.clear();
        state.m_JumpLetter.second.clear();
        passedCurrentItem = false;
        return true;
    }

    return false;
}

auto GuiTreeView::CreateBranch(GuiState& state, TreeItem* branch) -> bool
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
            nextSubItem->UpdateMetaInfo();
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
        ShowRightMouseClickPopupWindowIfNeeded();

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
}

auto GuiTreeView::CreateTree(GuiState& state) -> bool
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
}

