#pragma once
#include "GuiBase.h"
#include "GuiView.h"

class GuiToolbar : GuiBaseComponent
{
public:
	GuiToolbar();
	void Update(bool* p_open, GuiView& view);

private:
	GuiState			      m_State;
	std::vector<char>		  m_Buf;
};