#pragma once
#include "GuiBase.h"
#include "GuiView.h"
#include "GuiStyles.h"

enum class ButtonType
{
	SINGLE,
	TOGGLE,
	MODAL
};

class Button
{
public:
	Button(ToolButtonID button_id1, GuiTextureID texture_id, int group_index, ButtonType type, std::string tooltip, bool state);
	Button(ToolButtonID button_id1, ToolButtonID toggle_id2, GuiTextureID texture_id, int group_index, ButtonType type, std::string tooltip, bool state);
	void Update(GuiView& view);

private:
	void UpdateSingle(GuiView& view);
	void UpdateToggle(GuiView& view);
	void UpdateModal(GuiView& view);

	ToolButtonID m_ToolButtonId1;
	ToolButtonID m_ToolButtonId2;
	GuiTextureID m_TextureId;
	int          m_GroupIndex;
	ButtonType   m_Type = ButtonType::SINGLE;
	std::string  m_ToolTip = "";
	bool		 m_State = false;
};

class GuiToolbar : GuiBaseComponent
{
public:
	GuiToolbar();
	void ShowMapViewButtons(GuiView& view);
	void ShowTableViewButtons(GuiView& view);
	void Update(bool* p_open, GuiView& view);

private:
	GuiState			      m_State;
	std::vector<char>		  m_Buf;
	std::vector<Button>		  m_TableViewButtons;
	std::vector<Button>		  m_MapViewButtons;
};