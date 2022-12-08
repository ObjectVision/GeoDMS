#pragma once
#include "GuiBase.h"
#include "StateChangeNotification.h"

class GuiTreeNode
{
public:
	GuiTreeNode() {};
	GuiTreeNode(TreeItem* item);
	GuiTreeNode(TreeItem* item, bool is_open);
	~GuiTreeNode();
	auto SetItem(TreeItem* item) -> void { m_item = item; };
	auto GetItem() -> TreeItem* { return m_item; };
	auto SetState(NotificationCode new_state) -> void;
	auto GetState() -> NotificationCode;
	auto GetFirstSibling() -> GuiTreeNode*;
	auto 

	auto Draw() -> bool;

private:
	auto Init(TreeItem* item) -> void;
	auto GetDepthFromTreeItem() -> UInt8;
	auto DrawItemDropDown() -> bool;
	auto DrawItemIcon() -> bool;
	auto DrawItemText() -> bool;

	TreeItem*                m_item = nullptr;
	GuiTreeNode*             m_parent = nullptr;
	std::vector<GuiTreeNode> m_children;
	NotificationCode         m_state = NotificationCode::NC2_Invalidated;

	// visualization members
	bool  m_is_open = false;
	UInt8 m_depth = 0;
};

class GuiTree
{
public:
	static auto getInstance(TreeItem* root) -> GuiTree*;
	static auto getInstance()->GuiTree*;

	~GuiTree()
	{
		if (instance)
			delete instance;
	}

	auto Draw() -> void;
	auto TryInsert(TreeItem* item) -> void;
	auto GetNode(TreeItem* item) -> GuiTreeNode&;

	static auto OnTreeItemChanged(ClientHandle clientHandle, const TreeItem* ti, NotificationCode state) -> void;
private:
	GuiTree(TreeItem* root)
	{
		m_Root = GuiTreeNode(root, true);
		//m_treeitem_to_guitreenode.insert(std::pair<TreeItem*, GuiTreeNode>(root, {root, true}));
		//m_startnode = &m_treeitem_to_guitreenode.at(root);
	};

	auto SetNextCurrNode(GuiTreeNode& node) -> void;
	auto DrawBranch(GuiTreeNode& node) -> void;
	auto SpaceIsAvailableForTreeNode() -> bool;

	UInt64       m_max_count = 0;
	GuiTreeNode  m_Root;
	GuiTreeNode* m_startnode = nullptr;

	//std::map<TreeItem*, GuiTreeNode> m_treeitem_to_guitreenode;
	static GuiTree* instance;
};

class GuiTreeViewComponent : GuiBaseComponent
{
public:
	~GuiTreeViewComponent();
	void Update(bool* p_open, GuiState& state);
private:
	bool CreateBranch(GuiState& state, TreeItem* branch);
	bool CreateTree(GuiState& state);
	void UpdateStateAfterItemClick(GuiState& state, TreeItem* nextSubItem);
	bool IsAlphabeticalKeyJump(GuiState& state, TreeItem* nextItem, bool looped);

	TreeItem* m_TemporaryJumpItem = nullptr;

	ImGuiTreeNodeFlags m_BaseFlags  = ImGuiWindowFlags_AlwaysAutoResize | ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick;
};