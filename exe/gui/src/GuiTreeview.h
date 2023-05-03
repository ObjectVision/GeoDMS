#pragma once
#include "GuiBase.h"
#include "StateChangeNotification.h"
#include <imgui.h>
#include <imgui_internal.h>

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
	void DeleteChildren();
	void AddChildren();
	auto GetState() -> NotificationCode;
//REMOVE	auto GetFirstSibling() -> GuiTreeNode*;
//REMOVE	auto GetSiblingIterator() -> std::vector<GuiTreeNode>::iterator;
//REMOVE	auto GetSiblingEnd() -> std::vector<GuiTreeNode>::iterator;
	bool IsLeaf();
	void Init(TreeItem* item);
	auto GetDepthFromTreeItem() -> UInt8;
	auto Draw(GuiState& state, TreeItem*& jump_item) -> ImRect;
	static void OnTreeItemChanged(ClientHandle clientHandle, const TreeItem* ti, NotificationCode new_state);
	
	// TODO: reconsider the public state part: 
	GuiTreeNode* m_parent = nullptr;
	std::vector<GuiTreeNode> m_children;
	UInt8 m_depth = 0;
	bool m_status_changed = false;

private:
	bool DrawItemDropDown(GuiState& state);
	auto DrawItemIcon(GuiState& state) -> ImRect;
	bool DrawItemText(GuiState& state, TreeItem*& jump_item);
	void DrawItemWriteStorageIcon();
	TreeItem*                m_item = nullptr;
	
	NotificationCode m_state = NotificationCode::NC2_Invalidated;

	// visualization members
	bool m_has_been_opened = false;
	bool m_is_open = false;
	
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
	auto AscendVisibleTree(GuiTreeNode& node) -> GuiTreeNode*;
	auto DescendVisibleTree(GuiTreeNode& node) -> GuiTreeNode*;
	auto JumpToLetter(GuiState& state, std::string_view letter) -> GuiTreeNode*;
	void OpenThisAndAllChildNodesRecursively(GuiTreeNode* node);
	void ActOnLeftRightArrowKeys(GuiState& state, GuiTreeNode* node);
	
	GuiTreeNode* m_curr_node = nullptr; //TODO: move to private variables
	

private:
	
	bool DrawBranch(GuiTreeNode& node, GuiState& state, TreeItem*& jump_item, const ImRect& parent_node_rect);
	bool SpaceIsAvailableForTreeNode();
	UInt64       m_max_count = 0;
	bool		 m_is_initialized = false;
	GuiTreeNode  m_Root = {};
	GuiTreeNode* m_start_node = nullptr;
};

class GuiTreeView
{
public:
	~GuiTreeView();
	auto Update(bool* p_open, GuiState& state) -> void;
	auto clear() -> void;

private:
	void GotoNode(GuiState& state, GuiTreeNode* newNode);
	void ProcessTreeviewEvent(GuiEvents &event, GuiState& state);
	bool CreateBranch(GuiState& state, TreeItem* branch);
	bool CreateTree(GuiState& state);
	bool IsAlphabeticalKeyJump(GuiState& state, TreeItem* nextItem, bool looped);

	TreeItem* m_TemporaryJumpItem = nullptr;
	ImGuiTreeNodeFlags m_BaseFlags  = ImGuiWindowFlags_AlwaysAutoResize | ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick;
	bool m_is_docking_initialized = false;

	GuiTree m_tree;
};