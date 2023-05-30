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
	Button(ToolButtonID button_id1, GuiTextureID texture_id, bool is_unique, int group_index, ButtonType type, std::string tooltip, UInt4 state);
	Button(ToolButtonID button_id1, ToolButtonID button_id2, GuiTextureID texture_id, bool is_unique, int group_index, ButtonType type, std::string tooltip, UInt4 state);
	Button(ToolButtonID button_id1, ToolButtonID button_id2, ToolButtonID button_id3, GuiTextureID texture_id, bool is_unique, int group_index, ButtonType type, std::string tooltip, UInt4 state);
	friend bool operator==(const Button& l, const Button& r)
	{
		return l.m_TextureId == r.m_TextureId;
	}
	
	bool Update(GuiViews& view);
	void SetState(UInt8 new_state)
	{
		m_State = new_state;
	}
	bool GetUniqueness()
	{
		return m_IsUnique;
	}

	
	
	auto GetGroupIndex() -> int;

private:
	void UpdateSingle(GuiViews& view);
	void UpdateToggle(GuiViews& view);
	void UpdateTristate(GuiViews& view);
	void UpdateModal(GuiViews& view);

	ToolButtonID m_ToolButtonId1;
	ToolButtonID m_ToolButtonId2;
	ToolButtonID m_ToolButtonId3;
	GuiTextureID m_TextureId;
	bool         m_IsUnique = false;
	int          m_GroupIndex;
	ButtonType   m_Type = ButtonType::SINGLE;
	std::string  m_ToolTip = "";
	UInt8		 m_State = 0;
};

class GuiToolbar
{
public:
	GuiToolbar();
	void ShowMapViewButtons(GuiViews& view);
	void ShowTableViewButtons(GuiViews& view);
	void Update(bool* p_open, GuiState& state, GuiViews& view);

private:
	std::vector<Button>		  m_TableViewButtons;
	std::vector<Button>		  m_MapViewButtons;
	bool is_docking_initialized = false;
};