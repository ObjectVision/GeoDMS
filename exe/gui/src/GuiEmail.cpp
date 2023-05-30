#include "GuiEmail.h"
#include "RtcInterface.h"
#include "windows.h"

#include <locale>
#include <codecvt>

bool GuiEmail::SendMailUsingDefaultWindowsEmailApplication(std::string message)
{
	/*std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;

	std::wstring wstr_version = converter.from_bytes(DMS_GetVersion());
	std::wstring wstr_message = converter.from_bytes(message);
	std::wstring body = std::wstring(L"{You can insert an optional message here}%0D%0A") +
									 L"==================================================%0D%0A" +
									 L"DMS version: " + wstr_version + L"%0D%0A" +
									 L"===================================================%0D%0A" +
									 wstr_message;

	std::wstring mailCommand =   std::wstring() + L"mailto:support@objectvision.nl"+ L"?subject=GeoDMS Bugreport&body=" + body;

	auto result = ShellExecuteW(0, NULL, mailCommand.c_str(), NULL, NULL, SW_SHOWNORMAL);
	*/

	std::string body1 = std::string("{You can insert an optional message here}%0D%0A") +
		"==================================================%0D%0A" +
		"DMS version: " + DMS_GetVersion() + "%0D%0A" +
		"===================================================%0D%0A" +
		message;

	std::string mailCommand1 = std::string() + "mailto:support@objectvision.nl" + "?subject=GeoDMS Bugreport&body=" + body1;
	auto result = ShellExecuteA(0, NULL, mailCommand1.c_str(), NULL, NULL, SW_SHOWNORMAL);

	return true;
}