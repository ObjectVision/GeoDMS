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

void SetTreeViewIcon(GuiTextureID id)
{
    ImGui::Image((void*)(intptr_t)GetIcon(id).GetImage(), ImVec2(GetIcon(id).GetWidth(), GetIcon(id).GetHeight())); //TODO: +5 magic number
}

GuiTreeNode::GuiTreeNode(TreeItem* item)
{
    this->Init(item);
}

GuiTreeNode::GuiTreeNode(TreeItem* item, bool is_open)
{
    this->Init(item);
    m_is_open = is_open;
}

GuiTreeNode::GuiTreeNode(TreeItem* item, GuiTreeNode* parent, bool is_open)
{
    this->Init(item);
    m_parent = parent;
    m_is_open = is_open;
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
    DMS_TreeItem_RegisterStateChangeNotification(&GuiTreeNode::OnTreeItemChanged, m_item, this);
    item->UpdateMetaInfo();
}

GuiTreeNode::~GuiTreeNode()
{
    if (m_item)
        DMS_TreeItem_ReleaseStateChangeNotification(&GuiTreeNode::OnTreeItemChanged, m_item, nullptr);
}

auto GuiTreeNode::GetDepthFromTreeItem() -> UInt8
{
    assert(m_item);
    return DivideTreeItemFullNameIntoTreeItemNames(m_item->GetFullName().c_str()).size();
}

auto GuiTreeNode::DrawItemDropDown() -> bool
{
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImVec2 padding = style.FramePadding;

    const float text_offset_x = g.FontSize + padding.x * 2;           // Collapser arrow width + Spacing
    const float text_offset_y = ImMax(padding.y, window->DC.CurrLineTextBaseOffset);                    // Latch before ItemSize changes it
    const ImVec2 label_size = ImGui::CalcTextSize(m_item->GetName().c_str(), m_item->GetName().c_str() + std::string(m_item->GetName().c_str()).size(), false);
    const float text_width = g.FontSize + label_size.x + padding.x * 2;  // Include collapser
    ImVec2 text_pos(window->DC.CursorPos.x + text_offset_x, window->DC.CursorPos.y + text_offset_y);
    const ImVec2 arrow_button_size(g.FontSize - 2.0f, g.FontSize + g.Style.FramePadding.y * 2.0f);
    /*const ImU32 text_col = ImGui::GetColorU32(ImGuiCol_Text);
    ImGui::RenderArrow(window->DrawList, ImVec2(text_pos.x - text_offset_x + padding.x, text_pos.y), text_col, m_is_open ? ImGuiDir_Down : ImGuiDir_Right, 1.0f);
    */

    /*if (ImGui::ArrowButton(("##>" + m_item->GetFullName()).c_str(), m_is_open ? ImGuiDir_Down : ImGuiDir_Right))
    {
        m_is_open = !m_is_open; // toggle
    }*/

    float offset = 1;
    auto cur_pos = ImGui::GetCursorPos();
    ImGui::SetCursorPos(ImVec2(cur_pos.x, cur_pos.y+offset));
    
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.f, 0.f, 0.f, 0.f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.f, 0.f, 0.f, 0.f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.f, 0.f, 0.f, 0.f));

    if (ImGui::Button(m_is_open ? ICON_RI_MIN : ICON_RI_PLUS))
    {
        

        m_is_open = !m_is_open;

        if (m_is_open && !m_has_been_openend && m_state>= NotificationCode::NC2_MetaReady) // children known at this point
        {
            AddChildren();
            m_has_been_openend = true; // add children once and only once
        }
    }

    auto spacing_w = g.Style.ItemSpacing.x;
    window->DC.CursorPos.x = window->DC.CursorPosPrevLine.x + spacing_w;
    window->DC.CursorPos.y = window->DC.CursorPosPrevLine.y - offset;
    ImGui::PopStyleColor(3);

    return 0;
}

auto GuiTreeNode::DrawItemIcon() -> bool
{
    assert(m_item);
    auto vsflags = SHV_GetViewStyleFlags(m_item); // calls UpdateMetaInfo
    if (vsflags & ViewStyleFlags::vsfMapView) { SetTreeViewIcon(GuiTextureID::TV_globe); }
    else if (vsflags & ViewStyleFlags::vsfTableContainer) { SetTreeViewIcon(GuiTextureID::TV_container_table); }
    else if (vsflags & ViewStyleFlags::vsfTableView) { SetTreeViewIcon(GuiTextureID::TV_table); }
    else if (vsflags & ViewStyleFlags::vsfPaletteEdit) { SetTreeViewIcon(GuiTextureID::TV_palette); }
    else if (vsflags & ViewStyleFlags::vsfContainer) { SetTreeViewIcon(GuiTextureID::TV_container); }
    else { SetTreeViewIcon(GuiTextureID::TV_unit_transparant); }
    return 0;
}

auto GuiTreeNode::DrawItemText() -> bool
{
    assert(m_item);

    // status color
    auto status = DMS_TreeItem_GetProgressState(m_item);
    auto failed = m_item->IsFailed();

    ImGui::PushStyleColor(ImGuiCol_Text, GetColorFromTreeItemNotificationCode(status, failed));
    ImGui::Text(m_item->GetName().c_str());
    ImGui::PopStyleColor();
    return 0;
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

auto GuiTreeNode::GetSiblingIterator() -> std::list<GuiTreeNode>::iterator
{
    return m_children.begin();
}

auto GuiTreeNode::GetSiblingEnd() -> std::list<GuiTreeNode>::iterator
{
    return m_children.end();
}

auto GuiTreeNode::Draw() -> bool
{
    DrawItemDropDown();
    //auto test1 = ImGui::GetCursorPos();
    //ImGui::SameLine(); // SameLine moves cursor back to last draw position..
    //auto test2 = ImGui::GetCursorPos();
    DrawItemIcon();
    ImGui::SameLine();
    DrawItemText();

    return false;
}

GuiTree* GuiTree::instance = 0;
auto GuiTree::getInstance(TreeItem* root) -> GuiTree*
{
    if (!instance) 
    {
        instance = new GuiTree(root);
        return instance;
    }
    else
        return instance;
}
auto GuiTree::getInstance() -> GuiTree*
{
    if (instance)
        return instance;
    return nullptr;
}

auto GuiTree::SpaceIsAvailableForTreeNode() -> bool
{
    return ImGui::GetContentRegionAvail().y > 0;
}

auto GuiTree::DrawBranch(GuiTreeNode& node, GuiState& state) -> bool
{
    if (!SpaceIsAvailableForTreeNode())
        return false;

    if (node.GetState() < NotificationCode::NC2_MetaReady)
        return true;

    auto next_node = node.GetSiblingIterator();
    while (next_node != node.GetSiblingEnd())
    {

        //if (IsAncestor(next_node->GetItem(), state.GetCurrentItem()))
        //    ImGui::SetNextItemOpen(true);

        next_node->Draw();

        // keyboard focus event
        // click event
        // double click event
        // right-mouse popup menu
        // alphabetical letter jump
        // drop event
        // jump event

        if (next_node->GetOpenStatus())
        {
            if (next_node->GetState() >= PS_MetaInfo)
            {
                if (!DrawBranch(*next_node, state))
                {
                    return false;
                }
            }
        }
        
        std::advance(next_node, 1);
    }
    return true;
}

void GuiTree::Draw(GuiState& state)
{
    if (!m_startnode)
        return;

    auto m_currnode = m_startnode;
    m_Root.Draw();
    DrawBranch(*m_currnode, state);
}

GuiTreeViewComponent::~GuiTreeViewComponent() 
{}

void GuiTreeViewComponent::Update(bool* p_open, GuiState& state)
{
    GuiTree* tree = nullptr;
    bool use_default_tree = false;

    auto event_queues = GuiEventQueues::getInstance();
    if (event_queues->TreeViewEvents.HasEvents())
    {
        event_queues->TreeViewEvents.Pop();
        m_TemporaryJumpItem = state.GetCurrentItem();
    }

    //const ImVec2 size(100,10000);
    //ImGui::SetNextWindowContentSize(size);
    if (!ImGui::Begin("Treeview", p_open, ImGuiWindowFlags_None | ImGuiWindowFlags_NoTitleBar))
    {
        ImGui::End();
        return;
    }

    if (!use_default_tree)
    {
        if (state.GetRoot())
            tree = GuiTree::getInstance(state.GetRoot());

        if (tree)
        {
            // visualize tree

            tree->Draw(state);
        }
    }
    else 
    {
        if (state.GetRoot())
            CreateTree(state);

        if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
            SetKeyboardFocusToThisHwnd();
    }

    ImGui::End();
}

void GuiTreeViewComponent::UpdateStateAfterItemClick(GuiState& state, TreeItem* nextSubItem)
{
    auto event_queues = GuiEventQueues::getInstance();
    state.SetCurrentItem(nextSubItem);
    event_queues->CurrentItemBarEvents.Add(GuiEvents::UpdateCurrentItem);
    event_queues->DetailPagesEvents.Add(GuiEvents::UpdateCurrentItem);
    event_queues->MainEvents.Add(GuiEvents::UpdateCurrentItem);
}

bool GuiTreeViewComponent::IsAlphabeticalKeyJump(GuiState& state, TreeItem* nextItem, bool looped = false)
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

bool GuiTreeViewComponent::CreateBranch(GuiState& state, TreeItem* branch)
{
    auto event_queues = GuiEventQueues::getInstance();
    //TODO: ImGui::IsItemVisible() use for clipping check
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
        if (ImGui::BeginPopupContextItem())
        {
            auto base_style_color = ImGui::GetStyleColorVec4(0);
            auto current_style_color = ImGui::GetStyleColorVec4(1);
            ImGui::PopStyleColor(2);
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
            ImGui::PushStyleColor(ImGuiCol_Text, base_style_color);
            ImGui::PushStyleColor(ImGuiCol_Text, current_style_color);
        }

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

bool GuiTreeViewComponent::CreateTree(GuiState& state)
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

