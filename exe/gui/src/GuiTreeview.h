#pragma once
#include "GuiBase.h"

class GuiTreeViewComponent : GuiBaseComponent
{
public:
	~GuiTreeViewComponent();
	void Update(bool* p_open);
private:
	bool CreateBranch(TreeItem* branch);
	bool CreateTree();
	void UpdateStateAfterItemClick(TreeItem* nextSubItem);
	bool IsAlphabeticalKeyJump(TreeItem* nextItem, bool looped);
	GuiState	m_State;
	
	TreeItem*   m_TemporaryJumpItem = nullptr;

	ImGuiTreeNodeFlags m_BaseFlags  = ImGuiWindowFlags_AlwaysAutoResize | ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick;
};