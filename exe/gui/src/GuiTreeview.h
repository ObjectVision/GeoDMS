#pragma once
#include "GuiBase.h"

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