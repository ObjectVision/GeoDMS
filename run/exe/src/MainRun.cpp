// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////

// GeoDmsRun: the command-line runner — loads a configuration and updates
// the requested items, with logging and test-script support.

//#include "ShvDllInterface.h"
#include "ClcInterface.h"
#include "GeoInterface.h"
#include "TicInterface.h"
#include "StxInterface.h"
#include "RtcInterface.h"

#include "dbg/debug.h"
#include "dbg/DebugLog.h"
#include "dbg/DmsCatch.h"
#include "ptr/AutoDeletePtr.h"
#include "utl/Encodes.h"
#include "utl/Environment.h"
#include "utl/FileSystem.h"
#include "utl/Registry.h"
#include "utl/scoped_exit.h"
#include "utl/splitPath.h"
#include "act/MainThread.h" // SetMainThreadID
#include "xct/DmsException.h"

#include "AbstrDataItem.h"
#include "AbstrDataObject.h"
#include "AbstrUnit.h"
#include "DataLocks.h"
#include "OperationContext.h"

#include <iostream>
#include <algorithm>

#ifdef _WIN32
#include <windows.h>
#endif

#if defined(_MSC_VER) && defined(_DEBUG)
#include <crtdbg.h>
#include <cstdlib>
#include <cstdio>

// Headless-run assertion handling. By default a failed Debug assert (or abort()) pops a modal
// Retry/Ignore/"abort() has been called" dialog, which silently stalls any automated/headless run
// (unit suite, /T script driver) forever. This hook routes the CRT assert/error text to stderr (where
// the run log captures it via 2>&1) and terminates the process, so the driver continues with the next
// test and the failure is visible in the log instead of blocking on a dialog. Installed ONLY when no
// debugger is attached -- when running under cdb/VS, IsDebuggerPresent() is true and we keep the normal
// break-into-debugger behaviour so the assertion's stack can be inspected.
static int DmsHeadlessCrtReportHook(int reportType, char* message, int* returnValue)
{
	if (returnValue)
		*returnValue = 0;
	if (reportType == _CRT_WARN)
		return FALSE; // continue default processing (RtcStreamLock routes _CRT_WARN to stderr when headless); warnings don't dialog
	if (message)
		std::fputs(message, stderr);
	std::fflush(stderr);
	std::fflush(stdout);
	_exit(3); // 3 == abort-like; ends THIS process so an automated driver proceeds to the next test
	return TRUE; // unreached
}

static void InstallHeadlessAssertHandlerIfNoDebugger()
{
	if (IsDebuggerPresent())
		return; // keep modal break-into-debugger when a debugger is attached
	_CrtSetReportHook(DmsHeadlessCrtReportHook);
	_set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT); // no abort() message box / Watson
	SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX); // no critical-error / GPF dialogs
}
#endif

// ============== Main

enum class itemCmd { commit, statistics, histogram, list, file };

using itemCmdPair = std::pair<itemCmd, SharedTreeItemInterestPtr>;

// reporter for the '@checkfunctions' verb: one line per audited function definition
static void DMS_CONV ReportFunctionDefinitionCheck(ClientHandle /*clientHandle*/, const TreeItem* funcItem, bool ok, CharPtr message)
{
	if (ok)
		reportF(SeverityTypeID::ST_MajorTrace, "function definition OK: {}", funcItem->GetFullName().c_str());
	else
		reportF(SeverityTypeID::ST_Error, "function definition FAILED: {}: {}", funcItem->GetFullName().c_str(), message);
}

int main2_without_SE(int argc, char** argv)
{
	ParseRegStatusFlags(argc, argv);

	std::cout << std::endl << "LocalDataDir:" << GetLocalDataDir() << std::endl;

	// Unknown-option guard: on Windows '/x' is the option-prefix convention,
	// on Linux it's '-x'. Anything that survives the known-option parsing
	// above (/L, /S<X>, /C<X>) and still looks like an option must be a typo
	// or unsupported — fail loudly rather than treating it as a cfg name.
	if (argc >= 1)
	{
#ifdef _WIN32
		bool isUnknownOpt = (argv[0][0] == '/');
#else
		bool isUnknownOpt = (argv[0][0] == '-' && argv[0][1] != '\0');
#endif
		if (isUnknownOpt)
		{
			std::cerr << std::endl
				<< "Unknown command-line option " << argv[0]
				<< ". Known options: /L<log>, /S<X>/C<X> status flags." << std::endl;
			return 2;
		}
	}

	if (argc <= 1)
	{
		std::cerr << "To (re)calculate a resulting item use:\n\n"
			<< "   GeoDmsRun.exe [/PProjName] [/LLogFileName] ConfigFileName ItemNames\n\n"
			<< "Multiple item names can be specified and data will be committed to the external storages that are configured for the mentioned items\n\n";
		return 2;
	}

	int result = 0;

	std::cout << std::endl << "Read configuration file " << argv[0] << std::endl;
	AutoDeletePtr<TreeItem> cfg = DMS_CreateTreeFromConfiguration( argv[0] );
	if (!cfg)
	{
		std::cerr << "Failure to read configuration file " << argv[0] << std::endl;
		std::cerr << "Last ErrMsg: " << DMS_GetLastErrorMsg() << std::endl;
		return 2;
	}

	--argc; ++argv;
	std::vector<itemCmdPair> items;


	auto currCmd = itemCmd::commit;
	std::string fileName;
	// find all specified items
	for (; argc; --argc, ++argv) {
		if ((*argv)[0] == '@')
		{
			CharPtr cmd = (*argv) + 1;
			if (!stricmp(cmd, "statistics"))
				currCmd = itemCmd::statistics;
			if (!stricmp(cmd, "commit"))
				currCmd = itemCmd::commit;
			if (!stricmp(cmd, "histogram"))
				currCmd = itemCmd::histogram;
			if (!stricmp(cmd, "list"))
				currCmd = itemCmd::list;
			if (!stricmp(cmd, "checkfunctions"))
			{
				// opt-in typed-HOF audit: type-check EVERY function definition in the
				// config (including never-referenced ones, which the ordinary
				// application-triggered checker never reaches). Reports per function and
				// raises the error level if any definition fails.
				std::cout << std::endl << "Checking all function definitions..." << std::endl;
				UInt32 nrFailed = CheckAllFunctionDefinitions(cfg, &ReportFunctionDefinitionCheck, nullptr);
				ProcessMainThreadOpers();
				std::cout << std::endl << "Function definition check complete: " << nrFailed << " failed." << std::endl;
				if (nrFailed)
				{
					reportF(SeverityTypeID::ST_Error, "ErrorLevel up to 1 because {} function definition(s) failed the type check.", nrFailed);
					result = 1;
				}
			}
			if (!stricmp(cmd, "dumpconfig"))
			{
				// headless config-source dump: write the loaded configuration back out in
				// DMS syntax (the same serialization the GUI 'Configuration' detail page
				// shows). Handy for inspecting how items — functions in particular — are
				// represented, and for round-trip checks.  Usage: @dumpconfig <out.dms>
				if (argc > 1)
				{
					--argc, ++argv;
					std::cout << std::endl << "Dumping configuration to " << *argv << std::endl;
					if (!DMS_TreeItem_Dump(cfg, *argv))
					{
						reportF(SeverityTypeID::ST_Error, "ErrorLevel up to 1 because the configuration dump to '{}' failed.", *argv);
						result = 1;
					}
					ProcessMainThreadOpers();
				}
			}
			if (!stricmp(cmd, "file"))
			{
				if (argc > 1)
				{
					--argc, ++argv;
					fileName = *argv;
				}
			}
		}
		else
		{
			reportF(SeverityTypeID::ST_MajorTrace, "Item {}", *argv);
			CheckTreeItemPath(*argv);
			const TreeItem* item = DMS_TreeItem_GetItem(cfg, *argv);
			if (!item)
			{
				reportF(SeverityTypeID::ST_Error, "ErrorLevel up to 1 because the specified item '{}' was not found.", *argv);
				std::cerr << std::endl << "Item " << *argv << " not found" << std::endl;
				result = 1;
			}
			ProcessMainThreadOpers();

			for (const TreeItem* walker = item; walker; walker = item->WalkConstSubTree(walker))
				items.push_back(itemCmdPair(currCmd, make_shared_tree(walker, existing_obj{})));
		}
	}
	std::ostream* dataOut = &std::cout;
	std::ofstream outstream;
	if (!fileName.empty())
	{
#if defined(_MSC_VER)
		// fileName is UTF-8 (CLI arg). MSVC's std::ofstream(const std::string&)
		// interprets the path as the active code page; for non-ASCII paths
		// this fails or opens the wrong file. Use the wchar_t* overload,
		// transcoding via Utf8_2_wchar (same fix family as #1101).
		auto wideName = Utf8_2_wchar(fileName.c_str());
		outstream = std::ofstream(wideName.get());
#else
		outstream = std::ofstream(fileName);
#endif
		dataOut = &outstream;
	}

	// execute all specified items
	for (const auto& itemPair: items)
	{
		const TreeItem* item = itemPair.second;
		assert(item);
		SharedStr itemSourceName = item->GetSourceName();
		CDebugContextHandle ch("Updating", itemSourceName.c_str(), true);
		std::cout  << std::endl << "Update " << itemSourceName.c_str() << std::endl;
		
		DMS_TreeItem_Update(item);
		if (item->IsFailed())
		{
			auto fr = item->GetFailReason();
			if (fr)
			{
				reportF(SeverityTypeID::ST_Error, "ErrorLevel up to 1 due to failure: {}", fr->GetAsText().c_str()); ProcessMainThreadOpers();
				std::cerr << std::endl << "Failure: " << fr->GetAsText() << std::endl;
			}
			result = 1;
			continue; // skip this item
		}

		switch (itemPair.first)
		{
		case itemCmd::statistics:
			(*dataOut) << DMS_NumericDataItem_GetStatistics(item, nullptr) << std::endl;
			break;

		case itemCmd::histogram:
			(*dataOut) << "@histogram is Not Yet Implemented" << std::endl;
			break;

		case itemCmd::list:
			(*dataOut) << "@list is Under Construction" << std::endl;
			break;
		}

//		itemPair.second = nullptr; // release InterestCount
	}
	return result;
}

int main2(int argc, char** argv)
{
	DMS_SE_CALLBACK_BEGIN

		auto result = main2_without_SE(argc, argv);
		ProcessMainThreadOpers();
		return result;

	DMS_SE_CALLBACK_END // throws
}

int s_argcOrg = 0;
char** s_argvOrg = nullptr;


void logCommandLine(const char* msg)
{
	if (!s_argcOrg)
		return;
	assert(s_argvOrg);
	char** argv = s_argvOrg;


	reportF(SeverityTypeID::ST_MajorTrace, msg);
	for (auto argc = 0; argc != s_argcOrg; ++argc, ++argv)
		reportF(SeverityTypeID::ST_MajorTrace, "{}:{}", argc, *argv);
	 
	ProcessMainThreadOpers(); // flush
}

void DMS_CONV logMsg(ClientHandle clientHandle, const MsgData* msgData, bool moreToCome)
{
	assert(msgData);
	assert(clientHandle == nullptr);


	if (!msgData->m_IsFollowup)
	{
		std::cout 
			<< "[" << SeverityAsChar(msgData->m_SeverityType) 
			<< "][" << AsString(msgData->m_DateTime) 
			<< "][" << msgData->m_ThreadID << "]";
		auto msgCat = msgData->m_MsgCategory;
		std::cout << AsString(msgCat);
	}
	else
		std::cout << "   ";
	std::cout << msgData->m_Txt << std::endl;

}

int main1(int argc, char** argv)
{
	SetMainThreadID(); // identify the main thread (formerly done via DMS_Appl_SetExeDir)

	SuspendTrigger::FencedBlocker lockSuspend("@DmsRun main");
	--argc; ++argv;
	CharPtr firstParam = argv[0];
	if ((argc > 0) && firstParam[0] == '/' && firstParam[1] == 'L')
	{
		SharedStr dmsLogFileName = ConvertDosFileName(SharedStr(firstParam + 2));

		CDebugLog log(MakeAbsolutePath(dmsLogFileName.c_str()), true);
		SetCachedStatusFlag(RSF_TraceLogFile);
		return main2(argc - 1, argv + 1);
	}
	return main2(argc, argv);
}

int main_with_catch(int argc, char** argv)
{
	DMS_CALL_BEGIN

		return main1(argc, argv);

	DMS_CALL_END
	return 2;
}

void printCommandLine()
{
	if (!s_argcOrg)
		return;
	assert(s_argvOrg);

	int argc = s_argcOrg;
	char** argv = s_argvOrg;

	std::cerr << std::endl << "CommandLine> ";
	while (argc--)
		std::cerr << *argv++ << " ";
	std::cerr << std::endl;
}

int main_with_error_report(int argc, char** argv)
{
	logCommandLine("GeoDmsRun.exe STARTED with the folling chopped CommandLine");
	auto result = main_with_catch(argc, argv);
	if (result != 0)
	{
		reportF(SeverityTypeID::ST_FatalError, "GeoDmsRun failed with code {}", result);
		assert(s_argvOrg);
		printCommandLine();
	}
	else
		reportF(SeverityTypeID::ST_MajorTrace, "GeoDmsRun completed successfully.");
	logCommandLine("GeoDmsRun.exe STOPPED with the folling chopped CommandLine");

	return result;
}


void DMS_CONV reportMsg(CharPtr msg)
{
	std::cerr << std::endl << "\nCaught at Main:" << msg << std::endl;
}

int main(int argc, char** argv)
{
#if defined(_MSC_VER) && defined(_DEBUG)
	if (auto breakAllocStr = std::getenv("DMS_CRT_BREAK_ALLOC")) // diag aid: break on CRT allocation #N (leak hunting under cdb)
		_CrtSetBreakAlloc(std::atol(breakAllocStr));
#endif
#ifdef _WIN32
	// Lock the DLL search path before any LoadLibrary call (GDAL drivers,
	// RunDllProc) so a planted DLL in CWD or PATH cannot hijack the process.
	::SetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
	::SetDllDirectoryW(L"");
#endif

#if defined(_MSC_VER) && defined(_DEBUG)
	InstallHeadlessAssertHandlerIfNoDebugger(); // don't stall automated runs on a modal assert/abort() dialog
#endif

	s_argcOrg = argc;
	s_argvOrg = argv;

	DMS_Geo_Load();
	DMS_Clc_Load();

	DMS_SetGlobalCppExceptionTranslator(reportMsg);

	DMS_RegisterMsgCallback(logMsg, nullptr);
	auto exitGuard = make_scoped_exit([]
		{
			ReportFixedAllocFinalSummary();
			DMS_ReleaseMsgCallback(logMsg, nullptr);
		}
	);

	tg_maintainer manageOperationContextTasks;

	auto result = main_with_error_report(argc, argv);

	// 4) when you’re done, detach so the default scheduler resumes
//	concurrency::CurrentScheduler::Detach();

	return result;
}
