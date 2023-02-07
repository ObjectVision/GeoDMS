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

	auto clear() -> void;

	//auto Reset() -> void;
	void SetItem(TreeItem* item) { m_item = item; };
	auto GetItem() -> TreeItem* { return m_item; };
	void SetOpenStatus(bool do_open);
	bool IsOpen() { return m_is_open; };
	void SetState(NotificationCode new_state);
	void AddChildren();
	auto GetState() -> NotificationCode;
	auto GetFirstSibling() -> GuiTreeNode*;
	auto GetSiblingIterator() -> std::vector<GuiTreeNode>::iterator;
	auto GetSiblingEnd() -> std::vector<GuiTreeNode>::iterator;
	bool IsLeaf();
	void Init(TreeItem* item);

	bool Draw(GuiState& state, TreeItem*& jump_item);
	static void OnTreeItemChanged(ClientHandle clientHandle, const TreeItem* ti, NotificationCode new_state);
	GuiTreeNode* m_parent = nullptr;
	std::vector<GuiTreeNode> m_children;

private:
	auto GetDepthFromTreeItem() -> UInt8;
	bool DrawItemDropDown(GuiState& state);
	bool DrawItemIcon();
	bool DrawItemText(GuiState& state, TreeItem*& jump_item);
	TreeItem*                m_item = nullptr;
	
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
	void Init(GuiState& state);
	bool IsInitialized();
	void Draw(GuiState& state, TreeItem*& jump_item);
	void clear();
	auto AscendVisibleTree(GuiState& state, GuiTreeNode& node) -> GuiTreeNode*;
	auto DescendVisibleTree(GuiState& state, GuiTreeNode& node) -> GuiTreeNode*;
	GuiTreeNode* m_curr_node = nullptr;


private:
	bool DrawBranch(GuiTreeNode& node, GuiState& state, TreeItem*& jump_item);
	bool SpaceIsAvailableForTreeNode();
	UInt64       m_max_count = 0;
	bool		 m_is_initialized = false;
	GuiTreeNode  m_Root = {};
	GuiTreeNode* m_start_node = nullptr;
};

class GuiTreeView : GuiBaseComponent
{
public:
	~GuiTreeView();
	auto Update(bool* p_open, GuiState& state) -> void;
	auto clear() -> void;
	static auto OnStateChange(ClientHandle clientHandle, const TreeItem* self, NotificationCode notificationCode) -> void;

private:
	auto ProcessTreeviewEvent(GuiEvents &event, GuiState& state) -> void;
	auto CreateBranch(GuiState& state, TreeItem* branch) -> bool;
	auto CreateTree(GuiState& state) -> bool;
	auto IsAlphabeticalKeyJump(GuiState& state, TreeItem* nextItem, bool looped) -> bool;

	TreeItem* m_TemporaryJumpItem = nullptr;
	ImGuiTreeNodeFlags m_BaseFlags  = ImGuiWindowFlags_AlwaysAutoResize | ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick;

	GuiTree m_tree;
};