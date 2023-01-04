#pragma once
#include "GuiBase.h"
#include "StateChangeNotification.h"

class GuiTreeNode
{
public:
	GuiTreeNode() {};
	GuiTreeNode(TreeItem* item);
	GuiTreeNode(TreeItem* item, bool is_open);
	GuiTreeNode(TreeItem* item, GuiTreeNode* parent, bool is_open);
	GuiTreeNode(GuiTreeNode&& other) noexcept;

	~GuiTreeNode();

	//auto Reset() -> void;
	auto SetItem(TreeItem* item) -> void { m_item = item; };
	auto GetItem() -> TreeItem* { return m_item; };
	auto SetOpenStatus(bool do_open) -> void;
	auto GetOpenStatus() -> bool { return m_is_open; };
	auto SetState(NotificationCode new_state) -> void;
	auto AddChildren() -> void;
	auto GetState() -> NotificationCode;
	auto GetFirstSibling() -> GuiTreeNode*;
	auto GetSiblingIterator() -> std::vector<GuiTreeNode>::iterator;
	auto GetSiblingEnd() -> std::vector<GuiTreeNode>::iterator;
	auto IsLeaf() -> bool;
	auto Init(TreeItem* item) -> void;

	auto Draw(GuiState& state, TreeItem*& jump_item) -> bool;
	static auto OnTreeItemChanged(ClientHandle clientHandle, const TreeItem* ti, NotificationCode new_state) -> void;

private:
	auto GetDepthFromTreeItem() -> UInt8;
	auto DrawItemDropDown() -> bool;
	auto DrawItemIcon() -> bool;
	auto DrawItemText(GuiState& state, TreeItem*& jump_item) -> bool;

	TreeItem*                m_item = nullptr;
	GuiTreeNode*             m_parent = nullptr;
	std::vector<GuiTreeNode> m_children;
	NotificationCode m_state = NotificationCode::NC2_Invalidated;

	// visualization members
	bool m_has_been_openend = false;
	bool m_is_open = false;
	UInt8 m_depth = 0;
};

class GuiTree
{
public:
	GuiTree() {}
	/*GuiTree(TreeItem* root)
	{
		m_Root = GuiTreeNode(root, true);//std::make_unique<GuiTreeNode>(root, true);
		m_startnode = m_Root.get();
	};*/
	//static auto getInstance(TreeItem* root) -> GuiTree*;
	//static auto getInstance()->GuiTree*;

	~GuiTree();
	auto Init(GuiState& state) -> void;
	auto IsInitialized() -> bool;
	auto Draw(GuiState& state, TreeItem*& jump_item) -> void;

private:
	auto DrawBranch(GuiTreeNode& node, GuiState& state, TreeItem*& jump_item) -> bool;
	auto SpaceIsAvailableForTreeNode() -> bool;

	UInt64       m_max_count = 0;
	bool		 m_is_initialized = false;
	GuiTreeNode  m_Root = {};
	GuiTreeNode* m_startnode = nullptr;
};

class GuiTreeView : GuiBaseComponent
{
public:
	~GuiTreeView();
	auto Update(bool* p_open, GuiState& state) -> void;
	auto clear() -> void;
private:
	auto CreateBranch(GuiState& state, TreeItem* branch) -> bool;
	auto CreateTree(GuiState& state) -> bool;
	auto IsAlphabeticalKeyJump(GuiState& state, TreeItem* nextItem, bool looped) -> bool;

	TreeItem* m_TemporaryJumpItem = nullptr;
	ImGuiTreeNodeFlags m_BaseFlags  = ImGuiWindowFlags_AlwaysAutoResize | ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick;

	GuiTree m_tree;
};