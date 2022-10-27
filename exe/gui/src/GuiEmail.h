#pragma once
#include "GuiBase.h"
#include "windows.h"
#include "MAPI.h"

class GuiEmail
{
public:
	GuiEmail();
	~GuiEmail();
	bool SendMailUsingDefaultWindowsEmailApplication(std::string message);

private:
	MapiRecipDesc GetOVRecipientDescriptor();


	HMODULE hModMAPI			   = nullptr;
	FARPROC pfnMAPIInitialize      = nullptr;
	FARPROC pfnMAPIUninitialize    = nullptr;
	LPMAPISENDMAIL pfnMAPISendMail = nullptr;
};