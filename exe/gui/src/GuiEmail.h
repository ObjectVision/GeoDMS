#pragma once
#include "GuiBase.h"

struct GuiEmailImpl;

class GuiEmail
{
public:
	GuiEmail();
	~GuiEmail();

	bool SendMailUsingDefaultWindowsEmailApplication(std::string message);

	std::unique_ptr<GuiEmailImpl> pImpl;
};