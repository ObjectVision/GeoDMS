#include <algorithm>
#include <cstring>
#include <iostream>
#include <vector>

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

// A SEND element is an index when it is all digits, and a menu-item caption otherwise. Naming an
// item keeps a script working when menu entries are added or removed above it, which numbering
// does not: it silently fires whatever moved into that position. Quoting is what makes a caption
// with spaces one element; the tokenizer has already stripped the quotes by the time we see it.
bool IsElemIndex(CharPtr str)
{
	if (!str || !*str)
		return false;
	for (CharPtr i = str; *i; ++i)
		if (*i < '0' || *i > '9')
			return false;
	return true;
}

// The payload of the named pop-up-menu command: the elements as NUL terminated strings, padded
// with NULs to a whole number of UInt32s because the receiving end counts 4-byte words.
std::vector<char> MakeNamedMenuPayload(char* argv[], int first, int last)
{
	std::vector<char> result;
	for (int i = first; i != last; ++i)
	{
		CharPtr elem = argv[i];
		result.insert(result.end(), elem, elem + std::strlen(elem));
		result.emplace_back(char(0));
	}
	while (result.size() % 4)
		result.emplace_back(char(0));
	return result;
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
				throw stx_error(mgFormat2string("numeric overflow at {0}", str).c_str());
			value = newValue;
		}
		if (!str[i] || isspace(str[i]))
			return value;
	}
	throw stx_error(mgFormat2string("numeric value expected at {0}", str).c_str());
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
		std::vector<char>   charBuffer;

		if (std::strcmp(argv[i], "SEND") == 0)
		{
			if (argc <= ++i)
				throw stx_error("command-code expected after SEND");

			int code = str2int(argv[i]);
			myCDS.dwData = code;
			if (argc <= ++i)
				throw stx_error("element count expected after SEND command-code ");
			int size = str2int(argv[i]), allocSize = size ? size : 1;
			int firstElem = i + 1;
			if (argc < firstElem + size)
				throw stx_error(mgFormat2string("{0} elements expected after SEND command", size).c_str());
			i += size;

			// A pop-up-menu path may name its items instead of numbering them; that needs the
			// strings themselves, so it goes out as WmCopyActiveDmsControl command 2 with a
			// NUL separated payload rather than as an array of indices.
			bool hasNamedElem = false;
			for (int j = firstElem; j != firstElem + size; ++j)
				if (!IsElemIndex(argv[j]))
					hasNamedElem = true;

			if (hasNamedElem)
			{
				if (code != int(CommandCode::WmCopyActiveDmsControl) || size < 1 || !IsElemIndex(argv[firstElem]) || str2int(argv[firstElem]) != 1)
					throw stx_error("a named element is only supported in a pop-up menu path, i.e. SEND 4 <n> 1 ...");

				charBuffer = MakeNamedMenuPayload(argv, firstElem + 1, firstElem + size);
				buffer.emplace_back(2); // WmCopyActiveDmsControl sub-command: menu path by name
				buffer.insert(buffer.end()
				,	reinterpret_cast<const UInt32*>(charBuffer.data())
				,	reinterpret_cast<const UInt32*>(charBuffer.data() + charBuffer.size()));
			}
			else if (size > 0)
			{
				buffer.reserve(allocSize);
				for (int j = firstElem; j != firstElem + size; ++j)
					buffer.emplace_back(str2int(argv[j]));
			}
			else
				buffer.emplace_back(0);

			myCDS.cbData = buffer.size() * 4;
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
		else if (std::strcmp(argv[i], "BringToFront") == 0)
		{
			// Raise our own window in the Z-order; no WM_COPYDATA needed.
			// Use continue so the SendMessage at the end of the iteration is skipped.
			auto mw = MainWindow::TheOne();
			if (mw) { mw->raise(); mw->activateWindow(); }
			continue;
		}
		else if (std::strcmp(argv[i], "EditConfigSource") == 0 || std::strcmp(argv[i], "EditConfigRootSource") == 0)
		{
			// Fire the real "edit config source" menu action for the current item (or the
			// root): read the stored DmsEditor command, substitute & expand it, and launch the
			// editor detached. The executed command line is written to the trace log
			// (MsgCategory::commands), so a headless test can assert on it. Handled directly
			// like BringToFront; continue skips the trailing WM_COPYDATA SendMessage.
			auto mw = MainWindow::TheOne();
			if (mw)
			{
				if (std::strcmp(argv[i], "EditConfigRootSource") == 0)
					mw->openConfigRootSource();
				else
					mw->openConfigSource();
			}
			continue;
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
			reportErr(mgFormat2string("Unrecognized keyword: {0}", argv[i]).c_str());

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
		else if (std::strcmp(argv[i], "BringToFront") == 0)
		{
			mw->raise();
			mw->activateWindow();
		}
		else if (std::strcmp(argv[i], "EditConfigSource") == 0)
		{
			mw->openConfigSource();
		}
		else if (std::strcmp(argv[i], "EditConfigRootSource") == 0)
		{
			mw->openConfigRootSource();
		}
		else if (std::strcmp(argv[i], "SEND") == 0)
		{
			if (argc <= ++i)
				throw stx_error("command-code expected after SEND");
			int code = str2int(argv[i]);
			if (argc <= ++i)
				throw stx_error("element count expected after SEND command-code");
			int size = str2int(argv[i]);
			int firstElem = i + 1;
			if (argc < firstElem + size)
				throw stx_error("not enough elements after SEND");
			i += size;

			// see the Win32 branch: a named pop-up-menu path travels as sub-command 2
			bool hasNamedElem = false;
			for (int j = firstElem; j != firstElem + size; ++j)
				if (!IsElemIndex(argv[j]))
					hasNamedElem = true;

			std::vector<UInt32> buf;
			std::vector<char> charBuffer;
			if (hasNamedElem)
			{
				if (code != int(CommandCode::WmCopyActiveDmsControl) || size < 1 || !IsElemIndex(argv[firstElem]) || str2int(argv[firstElem]) != 1)
					throw stx_error("a named element is only supported in a pop-up menu path, i.e. SEND 4 <n> 1 ...");

				charBuffer = MakeNamedMenuPayload(argv, firstElem + 1, firstElem + size);
				buf.emplace_back(2);
				buf.insert(buf.end()
				,	reinterpret_cast<const UInt32*>(charBuffer.data())
				,	reinterpret_cast<const UInt32*>(charBuffer.data() + charBuffer.size()));
			}
			else
			{
				buf.reserve(size ? size : 1);
				for (int j = firstElem; j != firstElem + size; ++j)
					buf.emplace_back(str2int(argv[j]));
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
				reportErr(mgFormat2string("SEND code {0} not implemented on Linux", code).c_str());
				break;
			}
		}
		else
			reportErr(mgFormat2string("Unrecognized keyword (Linux): {0}", argv[i]).c_str());
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
	while (currPtr != line.end() && *currPtr && isspace(*currPtr))
		++currPtr;
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

