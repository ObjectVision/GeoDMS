#include "GuiEmail.h"
#include "RtcInterface.h"
#include "imgui_internal.h"
#include "windows.h"

bool GuiEmail::SendMailUsingDefaultWindowsEmailApplication(std::string message)
{
	// "support@objectvision.nl"



    // Format subject
	std::string subject = "GeoDMS Bugreport";

	// Format body
	std::string body = std::string() + "{You can insert an optional message here}\n\n" +
		"==================================================\n" +
		"DMS version:" + DMS_GetVersion() + "\n" +
		"===================================================\n" +
		message;

	// Format mailto: string
	std::string mailCommand = "mailto:support@objectvision.nl"; //+ 
												//"?subject=" + subject + 
												//"&body=" + body;

	auto result = ShellExecute(NULL, 0, (LPCWSTR)mailCommand.c_str(), (LPCWSTR)"", (LPCWSTR)"", SW_SHOWNORMAL); //SW_SHOWNOACTIVATE);

	return true;
}