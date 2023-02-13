#pragma once
#include "GuiBase.h"
#include "GuiView.h"
#include "GuiStyles.h"

enum class ButtonType
{
	SINGLE,
	TOGGLE,
	TRISTATE,
	MODAL,
};

class Button
{
public:
	Button(ToolButtonID button_id1, GuiTextureID texture_id, int group_index, ButtonType type, std::string tooltip, UInt4 state);
	Button(ToolButtonID button_id1, ToolButtonID button_id2, GuiTextureID texture_id, int group_index, ButtonType type, std::string tooltip, UInt4 state);
	Button(ToolButtonID button_id1, ToolButtonID button_id2, ToolButtonID button_id3, GuiTextureID texture_id, int group_index, ButtonType type, std::string tooltip, UInt4 state);
	void Update(GuiView& view);

private:
	void UpdateSingle(GuiView& view);
	void UpdateToggle(GuiView& view);
	void UpdateTristate(GuiView& view);
	void UpdateModal(GuiView& view);

	ToolButtonID m_ToolButtonId1;
	ToolButtonID m_ToolButtonId2;
	ToolButtonID m_ToolButtonId3;
	GuiTextureID m_TextureId;
	int          m_GroupIndex;
	ButtonType   m_Type = ButtonType::SINGLE;
	std::string  m_ToolTip = "";
	UInt4		 m_State = 0;
};

class GuiToolbar : GuiBaseComponent
{
public:
	GuiToolbar();
	void ShowMapViewButtons(GuiView& view);
	void ShowTableViewButtons(GuiView& view);
	void Update(bool* p_open, GuiState& state, GuiView& view);

private:
	std::vector<Button>		  m_TableViewButtons;
	std::vector<Button>		  m_MapViewButtons;
	bool is_docking_initialized = false;
};