#include "DmsMainWindow.h"

#include "ser/FormattedStream.h"
#include "ser/FileStreamBuff.h"
#include "TestScript.h"


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

int PassMsg(int argc, char* argv[])
{
	assert(argc > 0);

	int i = 0;
	if (std::strcmp(argv[i], "WAIT") == 0)
	{
		if (argc <= ++i)
			throw stx_error("#seconds expected after WAIT");

		int waitMilliSec = str2int(argv[i++]);
		return waitMilliSec;
	}

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
		else if (std::strcmp(argv[i], "DefaultView") == 0)
		{
			myCDS.dwData = ULONG_PTR(CommandCode::DefaultView);
			myCDS.cbData = 0;
			myCDS.lpData = nullptr;
//			assert(((char*)myCDS.lpData)[myCDS.cbData - 1] == 0);
		}
		else if (std::strcmp(argv[i], "GOTO") == 0 || std::strcmp(argv[i], "ActivateItem") == 0)
		{
			if (argc <= ++i)
				throw stx_error("path expected after ActivateItem");
			myCDS.dwData = ULONG_PTR(CommandCode::ActivateItem);
			myCDS.cbData = std::strlen(argv[i]) + 1;
			myCDS.lpData = argv[i];
			assert(((char*)myCDS.lpData)[myCDS.cbData - 1] == 0);
		}
		else if (std::strcmp(argv[i], "EXPAND") == 0 || std::strcmp(argv[i], "Expand") == 0)
		{
			myCDS.dwData = ULONG_PTR(CommandCode::Expand);
			myCDS.cbData = 4;
			buffer.emplace_back(1); // code for expand
			myCDS.lpData = &(buffer[0]);
		}
		else if (std::strcmp(argv[i], "Collapse") == 0)
		{
			myCDS.dwData = ULONG_PTR(CommandCode::Expand);
			myCDS.cbData = 4;
			buffer.emplace_back(0); // code for collapes
			myCDS.lpData = &(buffer[0]);
		}
		else if (std::strcmp(argv[i], "ExpandAll") == 0)
		{
			myCDS.dwData = ULONG_PTR(CommandCode::ExpandAll);
			myCDS.cbData = 0;
//			buffer.emplace_back(1); // code for expand all
			myCDS.lpData = nullptr;
		}
		else if (std::strcmp(argv[i], "ExpandRecursive") == 0)
		{
			myCDS.dwData = ULONG_PTR(CommandCode::ExpandRecursive);
			myCDS.cbData = 0;
			//			buffer.emplace_back(1); // code for expand all
			myCDS.lpData = nullptr;
		}
		else if ((std::strcmp(argv[i], "DP") == 0) || (std::strcmp(argv[i], "ShowDetailPage") == 0))
		{
			myCDS.dwData = ULONG_PTR(CommandCode::ShowDetailPage);
			myCDS.cbData = 4;
			if (argc <= ++i)
				throw stx_error("number expected after ShowDetailPage");
			buffer.emplace_back(str2int(argv[i]));
			myCDS.lpData = &(buffer[0]);
		}
		else if ((std::strcmp(argv[i], "SAVE_DP") == 0) || (std::strcmp(argv[i], "SaveDetailPage") == 0))
		{
			if (argc <= ++i)
				throw stx_error("path expected after SaveDetailPage");
			myCDS.dwData = ULONG_PTR(CommandCode::SaveDetailPage);
			myCDS.cbData = std::strlen(argv[i]) + 1;
			myCDS.lpData = argv[i];
			assert(((char*)myCDS.lpData)[myCDS.cbData - 1] == 0);
		}
		else if (std::strcmp(argv[i], "CascadeSubWindows") == 0)
		{
			myCDS.dwData = ULONG_PTR(CommandCode::CascadeSubWindows);
			myCDS.cbData = 0;
			myCDS.lpData = nullptr;

		}
		else if (std::strcmp(argv[i], "TileSubWindows") == 0)
		{
			myCDS.dwData = ULONG_PTR(CommandCode::TileSubWindows);
			myCDS.cbData = 0;
			myCDS.lpData = nullptr;
		}
		else if (std::strcmp(argv[i], "SaveValueInfo") == 0)
		{
			if (argc <= ++i)
				throw stx_error("path expected after SaveValueInfo");
			myCDS.dwData = ULONG_PTR(CommandCode::SaveValueInfo);
			myCDS.cbData = std::strlen(argv[i]) + 1;
			myCDS.lpData = argv[i];
			assert(((char*)myCDS.lpData)[myCDS.cbData - 1] == 0);

		}
		else
			reportErr(mgFormat2string("Unrecognized keyword: %1%", argv[i]).c_str());

			auto mainWindow = MainWindow::TheOne(); assert(mainWindow);
			auto hwDispatch = (HWND)(mainWindow->winId());
			SendMessage(hwDispatch, WM_COPYDATA, WPARAM(NULL), LPARAM(&myCDS));
	}
	return 0;
}

int RunTestLine(SharedStr line)
{
	const int MAX_ARG_COUNT = 20;
	char* argv[MAX_ARG_COUNT];
	int argc = 0;
	char* currPtr = line.begin();
	while (argc < MAX_ARG_COUNT && currPtr != line.end() && *currPtr)
	{
		if (*currPtr == '"')
		{
			argv[argc++] = ++currPtr; // skip quote
			// adopt first char
			while (currPtr != line.end() && *currPtr && *currPtr != '"')
				++currPtr;
		}
		else
		{
			argv[argc++] = currPtr++;
			while (currPtr != line.end() && *currPtr && !isspace(*currPtr))
				++currPtr;
		}

		if (currPtr == line.end() || !*currPtr)
			break;
		*currPtr++ = char(0);

		while (currPtr != line.end() && *currPtr && isspace(*currPtr))
			++currPtr;
	}
	if (!argc)
		return 0;

	if (argc < MAX_ARG_COUNT)
		argv[argc] = 0;

	return PassMsg(argc, argv);
}

SharedStr ReadLine(FormattedInpStream& fis)
{
	SharedStr result; // TODO: us reusable allocate buffer.
	while (!fis.AtEnd() && fis.NextChar() != '\n' && fis.NextChar() != EOF)
	{
		auto ch = fis.NextChar(); fis.ReadChar();
		if (ch == '/')
			if (fis.NextChar() == '/') // rest of line is commented out
				break;
		result += ch;
	}

	// read up till end or past EOL
	while (!fis.AtEnd() && fis.NextChar() != EOF)
	{
		auto ch = fis.NextChar(); fis.ReadChar();
		if (ch == '\n')
			break;
	}

	return result;
}

#include <future>

int RunTestScript(SharedStr testScriptName)
{
	auto fileBuff = FileInpStreamBuff(testScriptName, nullptr, true);
    auto fis = FormattedInpStream(&fileBuff);
	while (!fis.AtEnd() && fis.NextChar() != EOF)
	{
		auto line = ReadLine(fis);

		std::promise<int> p;
		std::future<int> mainThreadResult = p.get_future();
		PostMainThreadOper([line, &p]
			{
				auto waitMilliSec = RunTestLine(line);
				p.set_value(waitMilliSec);
			}
		);

		auto waitMilliSec = mainThreadResult.get();
		if (waitMilliSec)
			Sleep(waitMilliSec);
	}
    return 0;
}

