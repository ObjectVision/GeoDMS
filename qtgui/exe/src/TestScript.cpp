#include <iostream>

#include "DmsMainWindow.h"
#include "DmsAddressBar.h"
#include "DmsViewArea.h"

#include "ser/FormattedStream.h"
#include "ser/FileStreamBuff.h"
#include "TestScript.h"

#ifndef Q_OS_WIN
#include "KeyFlags.h"
#include "DataView.h"
#include "DmsDetailPages.h"
#include "MovableObject.h"
#include "ShvUtils.h"
#include "act/TriggerOperator.h"
#include <QMdiArea>
#include <QMdiSubWindow>

void SaveDetailPage(CharPtr fileName); // defined in main_qt.cpp
#endif


void reportErr(CharPtr errMsg)
{
	std::cerr << std::endl << errMsg;
}

UInt32 str2int(CharPtr str)
{
	int i = 0;
	unsigned char nextNum = (str[i] - '0');
	if (nextNum <= 9)
	{
		UInt32 value = nextNum;

		while ((nextNum = (str[++i] - '0')) <= 9)
		{
			UInt32 newValue = value * 10 + nextNum;
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

#ifdef Q_OS_WIN
	for (; i < argc; ++i)
	{
		COPYDATASTRUCT myCDS;
		std::vector<UInt32> buffer;

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
#else
	// Linux: direct method calls instead of Win32 IPC
	for (; i < argc; ++i)
	{
		auto mw = MainWindow::TheOne();
		if (!mw) return 0;

		if (std::strcmp(argv[i], "DefaultView") == 0)
		{
			mw->defaultView();
		}
		else if (std::strcmp(argv[i], "GOTO") == 0 || std::strcmp(argv[i], "ActivateItem") == 0)
		{
			if (argc <= ++i)
				throw stx_error("path expected after ActivateItem");
			mw->m_address_bar->setPath(argv[i]);
		}
		else if (std::strcmp(argv[i], "EXPAND") == 0 || std::strcmp(argv[i], "Expand") == 0)
		{
			mw->expandActiveNode(true);
		}
		else if (std::strcmp(argv[i], "Collapse") == 0)
		{
			mw->expandActiveNode(false);
		}
		else if (std::strcmp(argv[i], "ExpandAll") == 0)
		{
			mw->expandAll();
		}
		else if (std::strcmp(argv[i], "ExpandRecursive") == 0)
		{
			mw->expandRecursiveFromCurrentItem();
		}
		else if ((std::strcmp(argv[i], "DP") == 0) || (std::strcmp(argv[i], "ShowDetailPage") == 0))
		{
			if (argc <= ++i)
				throw stx_error("number expected after ShowDetailPage");
			mw->m_detail_pages->show((ActiveDetailPage)str2int(argv[i]));
		}
		else if ((std::strcmp(argv[i], "SAVE_DP") == 0) || (std::strcmp(argv[i], "SaveDetailPage") == 0))
		{
			if (argc <= ++i)
				throw stx_error("path expected after SaveDetailPage");
			SaveDetailPage(argv[i]);
		}
		else if (std::strcmp(argv[i], "CascadeSubWindows") == 0)
		{
			mw->m_mdi_area->cascadeSubWindows();
		}
		else if (std::strcmp(argv[i], "TileSubWindows") == 0)
		{
			mw->m_mdi_area->tileSubWindows();
		}
		else if (std::strcmp(argv[i], "SEND") == 0)
		{
			if (argc <= ++i)
				throw stx_error("command-code expected after SEND");
			int code = str2int(argv[i]);
			if (argc <= ++i)
				throw stx_error("DWORD count expected after SEND command-code");
			int size = str2int(argv[i]);
			std::vector<UInt32> buf;
			buf.reserve(size ? size : 1);
			for (int j = 0; j < size; ++j)
			{
				if (argc <= ++i)
					throw stx_error("not enough DWORDs after SEND");
				buf.emplace_back(str2int(argv[i]));
			}
			if (buf.empty()) buf.emplace_back(0);

			auto getActiveDV = [&]() -> std::shared_ptr<DataView> {
				auto aw = mw->m_mdi_area->activeSubWindow();
				if (!aw) return nullptr;
				auto va = dynamic_cast<QDmsViewArea*>(aw);
				if (!va) return nullptr;
				return va->getDataView();
			};

			switch (code)
			{
			case 1: // SendMain: {message, wParam, lParam}
			{
				UInt32 message = buf[0];
				if (message == 16) // WM_CLOSE
					mw->close();
				// other messages not implemented
				break;
			}
			case 3: // SendActiveDmsControl: {message, wParam, lParam}
			{
				auto dv = getActiveDV();
				if (!dv) break;
				UInt32 message = buf[0];
				UInt32 wParam  = buf.size() > 1 ? buf[1] : 0;
				if (message == 258) // WM_CHAR
					dv->OnKeyDown(wParam | KeyInfo::Flag::Char);
				else if (message == 256) // WM_KEYDOWN
					dv->OnKeyDown(wParam);
				else if (message == 273) // WM_COMMAND: trigger OnCommand(LOWORD(wParam))
				{
					auto contents = dv->GetContents();
					if (!contents) break;
					SuspendTrigger::FencedBlocker suspendLock("TestScript::WM_COMMAND");
					contents->OnCommand(ToolButtonID(wParam & 0xFFFF));
				}
				break;
			}
			case 4: // WmCopyActiveDmsControl: {cmd, data...}
			{
				auto dv = getActiveDV();
				if (!dv) break;
				UInt32 cmd = buf[0];
				const UInt32* dataBegin = buf.data() + 1;
				const UInt32* dataEnd   = buf.data() + buf.size();
				dv->OnCopyData(cmd, dataBegin, dataEnd);
				break;
			}
			default:
				reportErr(mgFormat2string("SEND code %1% not implemented on Linux", code).c_str());
				break;
			}
		}
		else
			reportErr(mgFormat2string("Unrecognized keyword (Linux): %1%", argv[i]).c_str());
	}
#endif // Q_OS_WIN
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
#include <thread>
#include <chrono>

int RunTestScript(SharedStr testScriptName, bool* mustTerminateToken)
{
	auto fileBuff = FileInpStreamBuff(testScriptName, true);
    auto fis = FormattedInpStream(&fileBuff);
	while (!fis.AtEnd() && fis.NextChar() != EOF)
	{
		auto line = ReadLine(fis);

		std::promise<int> p;
		auto mainThreadResult = p.get_future();
		if (*mustTerminateToken)
			return 0;

		auto mw = MainWindow::TheOne();
		if (!mw)
			return 0;

		mw->PostAppOper([line, &p]
			{
				auto waitMilliSec = RunTestLine(line);
				p.set_value(waitMilliSec);
			}
		);

		auto waitMilliSec = mainThreadResult.get();
		if (waitMilliSec)
			std::this_thread::sleep_for(std::chrono::milliseconds(waitMilliSec));
	}
    return 0;
}

