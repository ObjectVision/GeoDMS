//#include "ShvDllInterface.h"
#include "ClcInterface.h"
#include "GeoInterface.h"
#include "TicInterface.h"
#include "ClcInterface.h"
#include "GeoInterface.h"
#include "StxInterface.h"
#include "RtcInterface.h"

#include "dbg/debug.h"
#include "dbg/DebugLog.h"
#include "dbg/DmsCatch.h"
#include "ptr/AutoDeletePtr.h"
#include "utl/Encodes.h"
#include "utl/Environment.h"
#include "utl/scoped_exit.h"
#include "utl/splitPath.h"
#include "xct/DmsException.h"

#include "AbstrDataItem.h"
#include "AbstrDataObject.h"
#include "AbstrUnit.h"
#include "DataLocks.h"

#include <iostream>
#include <algorithm>


// ============== Main

enum class itemCmd { commit, statistics, histogram, list, file };

using itemCmdPair = std::pair<itemCmd, SharedTreeItemInterestPtr>;

int main2(int argc, char** argv)
{
	ParseRegStatusFlags(argc, argv);

	std::cout << std::endl << "LocalDataDir:" << GetLocalDataDir() << std::endl;

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
			std::cout << std::endl << "Item " << *argv;
			CheckTreeItemPath(*argv);
			const TreeItem* item = DMS_TreeItem_GetItem(cfg, *argv);
			if (!item)
			{
				std::cout << " Not found";
				std::cerr << std::endl << "Item " << *argv << " not found";
				result = 1;
			}
			for (const TreeItem* walker = item; walker; walker = item->WalkConstSubTree(walker))
				items.push_back(itemCmdPair(currCmd, SharedPtr<const TreeItem>(walker)));
		}
	}
	std::ostream* dataOut = &std::cout;
	std::ofstream outstream;
	if (!fileName.empty())
	{
		outstream = std::ofstream(fileName);
		dataOut = &outstream;
	}

	// execute all specified items
	for (const auto& itemPair: items)
	{
		const TreeItem* item = itemPair.second;
		dms_assert(item);
		SharedStr itemSourceName = item->GetSourceName();
		CDebugContextHandle ch("Updating", itemSourceName.c_str(), true);
		std::cout  << std::endl << "Update " << itemSourceName.c_str();
		
		DMS_TreeItem_Update(item);
		if (item->IsFailed())
		{
			auto fr = item->GetFailReason();
			if (fr)
				std::cerr << std::endl << "Failure: " << fr->GetAsText();
			result = 1;
		}

		switch (itemPair.first)
		{
		case itemCmd::statistics:
			(*dataOut) << DMS_NumericDataItem_GetStatistics(item, nullptr) << std::endl;
			break;

		case itemCmd::histogram:
			(*dataOut) << "@histogram is Under Construction" << std::endl;
			break;

		case itemCmd::list:
			(*dataOut) << "@list is Under Construction" << std::endl;
			break;
		}

//		itemPair.second = nullptr; // release InterestCount
	}
	return result;
}


void DMS_CONV logMsg(ClientHandle clientHandle, const MsgData* msgData, bool moreToCome)
{
	assert(msgData);
	assert(clientHandle == nullptr);
	if (msgData->m_SeverityType < SeverityTypeID::ST_MajorTrace)
		return;

	auto msgCat = msgData->m_MsgCategory;
	if (msgCat != MsgCategory::other)
		std::cout << AsString(msgCat);
	std::cout << msgData->m_Txt << std::endl;
}

void DMS_CONV reportMsg(CharPtr msg)
{
	std::cerr << std::endl << "\nCaught at Main:" << msg << std::endl;
}

int main_without_se(int argc, char** argv)
{
	DMS_CALL_BEGIN

		DMS_RegisterMsgCallback(logMsg, nullptr);
		auto exitGuard = make_scoped_exit([]
			{ 
				ReportFixedAllocFinalSummary();
				DMS_ReleaseMsgCallback(logMsg, nullptr);
			}
		);

		if (argc > 0)
			DMS_Appl_SetExeDir(splitFullPath(ConvertDosFileName(SharedStr(argv[0])).c_str()).c_str());

		DBG_START("Main", "", true);
		SuspendTrigger::FencedBlocker lockSuspend("@DmsRun main");
		--argc; ++argv;
		CharPtr firstParam = argv[0];
		if ((argc > 0) && firstParam[0] == '/' && firstParam[1] == 'L')
		{
			SharedStr dmsLogFileName = ConvertDosFileName(SharedStr(firstParam + 2));

			CDebugLog log(MakeAbsolutePath(dmsLogFileName.c_str()));
			SetCachedStatusFlag(RSF_TraceLogFile);
			return main2(argc - 1, argv + 1);
		}
		return main2(argc, argv);

	DMS_CALL_END
	return 2;
}

int main(int argc, char** argv)
{
	DMS_Geo_Load();
	DMS_Clc_Load();

	DMS_SetGlobalCppExceptionTranslator(reportMsg);

	DMS_SE_CALL_BEGIN

		return main_without_se(argc, argv);

	DMS_SE_CALL_END

	return GetLastExceptionCode();
}
