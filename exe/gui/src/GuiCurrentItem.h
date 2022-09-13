#pragma once
#include "GuiBase.h"

class GuiCurrentItemComponent : GuiBaseComponent
{
public:
	GuiCurrentItemComponent();
	void Update();

private:
	GuiState			      m_State;
	std::vector<char>		  m_Buf;
};