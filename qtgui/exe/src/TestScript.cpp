#include <QApplication>
#include <QAbstractNativeEventFilter>
#include <QResource>
#include <QByteArray>
#include <memory>

#include "ShvDllInterface.h"
#include "act/Any.h"
#include "utl/Environment.h"
#include "utl/splitPath.h"
#include "dbg/DebugLog.h"
#include "ShvUtils.h"

#include "DmsMainWindow.h"

#include "ser/FormattedStream.h"
#include "ser/FileStreamBuff.h"

using stx_error = std::exception;

void reportErr(CharPtr errMsg)
{
	std::cerr << std::endl << errMsg;
}

UINT32 str2int(CharPtr str)
{
	int i = 0;
	unsigned char nextNum = (str[i] - '0');
	if (nextNum <= 9)
	{
		UINT32 value = nextNum;

		while ((nextNum = (str[++i] - '0')) <= 9)
		{
			UINT32 newValue = value * 10 + nextNum;
			if (newValue < value)
				throw stx_error(mgFormat2string("numeric overflow at %1%", str).c_str());
			value = newValue;
		}
		if (!str[i] || isspace(str[i]))
			return value;
	}
	throw stx_error(mgFormat2string("numeric value expected at %1%", str).c_str());
}

enum class CommandCode {
	// MSG, WPARAM, LPARAM, 
	SendApp = 0,
	SendMain = 1,
	SendFocus = 2,
	SendActiveDmsControl = 3,
	CopyActiveDmsControl = 4, // MSG=WM_COPYDATA, WPARAM=WindowHandle, LPARAM=cds(nrRemainingNumbers, {numbers})
	DefaultView = 5,
	GOTO = 6,
	EXPAND = 8,
	DP = 9, // followed by integer page number.
	SAVE_DP = 10
};

int PassMsg(int argc, char* argv[])
{
	assert(argc > 1);

	int i = 1;
	if (std::strcmp(argv[i], "WAIT") == 0)
	{
		if (argc <= ++i)
			throw stx_error("#seconds expected after WAIT");

		int waitSec = 0, steps = 0;
		int waitMilliSec = str2int(argv[i++]);
		Sleep(waitMilliSec);
		return 0;
	}

	HWND hwDispatch = (HWND)(MainWindow::TheOne()->winId());

	for (; i < argc; ++i)
	{
		COPYDATASTRUCT myCDS;
		std::vector<UINT32> buffer;

		if (std::strcmp(argv[i], "SEND") == 0)
		{
			if (argc <= ++i)
				throw stx_error("command-code expected after SEND");

			int code = str2int(argv[i]);
			myCDS.dwData = code;
			if (argc <= ++i)
				throw stx_error("DWORD count expected after SEND command-code ");
			int size = str2int(argv[i]), allocSize = size ? size : 1;
			buffer.reserve(allocSize);
			myCDS.cbData = allocSize * 4; // j 32-bit integers follow
			if (size > 0)
			{
				for (int j = 0; j != size; ++j)
				{
					if (argc <= ++i)
						throw stx_error(mgFormat2string("%1% DWORDs expected after SEND command", size).c_str());
					buffer.emplace_back(str2int(argv[i]));
				}
			}
			else
				buffer.emplace_back(0);
			myCDS.lpData = &(buffer[0]);
		}
		else if (std::strcmp(argv[i], "GOTO") == 0)
		{
			if (argc <= ++i)
				throw stx_error("path expected after GOTO");
			myCDS.dwData = ULONG_PTR(CommandCode::GOTO);
			myCDS.cbData = std::strlen(argv[i]) + 1;
			myCDS.lpData = argv[i];
			assert(((char*)myCDS.lpData)[myCDS.cbData - 1] == 0);
		}
		else if (std::strcmp(argv[i], "EXPAND") == 0)
		{
			myCDS.dwData = ULONG_PTR(CommandCode::EXPAND);
			myCDS.cbData = 4;
			buffer.emplace_back(1); // code for expand all
			myCDS.lpData = &(buffer[0]);
		}
		else if (std::strcmp(argv[i], "DP") == 0)
		{
			myCDS.dwData = ULONG_PTR(CommandCode::DP);
			myCDS.cbData = 4;
			if (argc <= ++i)
				throw stx_error("number expected after DP");
			buffer.emplace_back(str2int(argv[i]));
			myCDS.lpData = &(buffer[0]);
		}
		else if (std::strcmp(argv[i], "SAVE_DP") == 0)
		{
			if (argc <= ++i)
				throw stx_error("path expected after SAVE_DP");
			myCDS.dwData = ULONG_PTR(CommandCode::SAVE_DP);
			myCDS.cbData = std::strlen(argv[i]) + 1;
			myCDS.lpData = argv[i];
			assert(((char*)myCDS.lpData)[myCDS.cbData - 1] == 0);
		}
		else
			reportErr(mgFormat2string("Unrecognized keyword: %1%", argv[i]).c_str());

		PostMessage(hwDispatch, UINT(WM_COPYDATA), WPARAM(NULL), LPARAM(&myCDS));
	}
	return 0;
}

int RunTestLine(SharedStr line)
{
	const int MAX_ARG_COUNT = 20;
	char* argv[MAX_ARG_COUNT];
	int argc = 0;
	char* currPtr = line.begin();
	while (argc < MAX_ARG_COUNT && currPtr != line.end())
	{
		argv[argc++] = currPtr++;
		while (currPtr != line.end() && !isspace(*currPtr))
			++currPtr;
		while (currPtr != line.end() && isspace(*currPtr))
			++currPtr;
	}

	return PassMsg(argc, argv);
}

int RunTestScript(SharedStr testScriptName)
{
    auto fileBuff = FileInpStreamBuff(testScriptName, nullptr, true);
    auto fis = FormattedInpStream(&fileBuff);
	while (!fis.AtEnd())
	{
		SharedStr line;
		fis >> line;
		auto result = RunTestLine(line);
		if (result)
			return result;
	}
    return 0;
}

