#include "GuiEmail.h"

GuiEmail::GuiEmail()
{
	//hModMAPI = LoadLibrary(L"%windir%\system32\mapi32.dll");
	hModMAPI = LoadLibrary(L"mapi32.dll");

	if (hModMAPI)
	{
		pfnMAPIInitialize = GetProcAddress(hModMAPI, "MAPIInitialize");
		pfnMAPIUninitialize = GetProcAddress(hModMAPI, "MAPIUninitialize");
		pfnMAPISendMail = (LPMAPISENDMAIL)GetProcAddress(hModMAPI, "MAPISendMail");
	}
	if (pfnMAPIInitialize)
		auto hr = (*pfnMAPIInitialize)();
}

GuiEmail::~GuiEmail()
{
	if (pfnMAPIUninitialize)
		(*pfnMAPIUninitialize)();
}

MapiRecipDesc GuiEmail::GetOVRecipientDescriptor()
{
	MapiRecipDesc recipient_description;
	recipient_description.ulRecipClass = MAPI_TO;
	recipient_description.lpszName     = (LPSTR)"support@objectvision.nl";
	recipient_description.lpszAddress  = (LPSTR)"support@objectvision.nl";

	return recipient_description;
}

bool GuiEmail::SendMailUsingDefaultWindowsEmailApplication(std::string message)
{
	if (!hModMAPI)
		return false;

	if (!pfnMAPISendMail)
		return false;

	auto recipient_description = GetOVRecipientDescriptor();

	MapiMessage email_description;
	email_description.nRecipCount	= 1;
	email_description.lpRecips		= &recipient_description;
	email_description.lpszSubject	= (LPSTR)"Internal GeoDMS Error report";
	email_description.lpszNoteText  = (LPSTR)"test_message";

	auto result = pfnMAPISendMail(0, 0, &email_description, MAPI_LOGON_UI | MAPI_DIALOG, 0);

	if (!result == SUCCESS_SUCCESS)
		return false;

	MAPI_E_FAILURE;

	return true;
}