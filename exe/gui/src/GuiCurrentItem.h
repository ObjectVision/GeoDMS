#pragma once
#include "GuiBase.h"

class GuiCurrentItemComponent : GuiBaseComponent
{
public:
	GuiCurrentItemComponent();
	void Update(GuiState& state);

private:
	std::vector<char>		  m_Buf;
};