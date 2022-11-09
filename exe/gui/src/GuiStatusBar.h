#pragma once
#include "GuiBase.h"

class GuiStatusBar : GuiBaseComponent
{
public:
	GuiStatusBar();
	void Update(bool* p_open);
	static void GeoDMSContextMessage(ClientHandle clientHandle, CharPtr msg);

private:
	void ParseNewContextMessage(std::string msg);
	GuiState			      m_State;
	UInt32 items_calculated       = 0;
	UInt32 items_to_be_calculated = 0;
};