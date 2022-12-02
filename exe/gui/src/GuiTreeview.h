#pragma once
#include "GuiBase.h"
#include "StateChangeNotification.h"

class GuiTreeNode
{
public:
	GuiTreeNode() {};
	GuiTreeNode(TreeItem* item);
	~GuiTreeNode();
	void SetItem(TreeItem* item) { m_item = item; };
	auto Draw() -> bool;

private:
	auto GetDepthFromTreeItem() -> UInt8 { return 0; };
	auto DrawItemArrow() -> bool { return 0; };
	auto DrawItemIcon() -> bool { return 0; };
	auto DrawItemText() -> bool { return 0; };

	TreeItem*                     m_item = nullptr;
	NotificationCode m_state = NotificationCode::NC2_DetermineChange;
	std::vector<GuiTreeNode>      m_branch;

	// visualization members
	bool  m_is_open = false;
	UInt8 m_depth = 0;
};

class GuiTree
{
public:
	static GuiTree* getInstance(TreeItem* root);

	~GuiTree()
	{
		if (instance)
			delete instance;
	}

	void Draw();

	static auto OnTreeItemChanged(ClientHandle clientHandle, const TreeItem* ti, NotificationCode state) -> void;
private:
	GuiTree(TreeItem* root)
	{
		m_root = GuiTreeNode(root);
	};
	UInt64       m_max_count = 0;
	GuiTreeNode  m_root;
	GuiTreeNode* m_startnode = &m_root;

	std::map<TreeItem*, GuiTreeNode*> m_treeitem_to_guitreenode;
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