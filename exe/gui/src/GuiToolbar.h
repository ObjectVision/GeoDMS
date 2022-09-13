#pragma once
#include "GuiBase.h"

class GuiToolbar : GuiBaseComponent
{
public:
	GuiToolbar();
	void Update(bool* p_open);

private:
	GuiState			      m_State;
	std::vector<char>		  m_Buf;
};