//<HEADER> 
/*
Data & Model Server (DMS) is a server written in C++ for DSS applications. 
Version: see srv/dms/rtc/dll/src/RtcVersion.h for version info.

Copyright (C) 1998-2004  YUSE GSO Object Vision BV. 

Documentation on using the Data & Model Server software can be found at:
http://www.ObjectVision.nl/DMS/

See additional guidelines and notes in srv/dms/Readme-srv.txt 

This library is free software; you can use, redistribute, and/or
modify it under the terms of the GNU General Public License version 2 
(the License) as published by the Free Software Foundation,
provided that this entire header notice and readme-srv.txt is preserved.

See LICENSE.TXT for terms of distribution or look at our web site:
http://www.objectvision.nl/DMS/License.txt
or alternatively at: http://www.gnu.org/copyleft/gpl.html

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
General Public License for more details. However, specific warranties might be
granted by an additional written contract for support, assistance and/or development
*/
//</HEADER>
#include "StxPch.h"
#pragma hdrstop

#include "StxInterface.h"
#include "TicInterface.h"

#include "act/UpdateMark.h"
#include "dbg/DebugContext.h"
#include "dbg/DmsCatch.h"
#include "geo/PointOrder.h"
#include "geo/IterRangeFuncs.h"
#include "ser/FileStreamBuff.h"
#include "utl/Encodes.h"
#include "utl/Environment.h"
#include "utl/splitPath.h"
#include "utl/StringFunc.h"
#include "xml/XmlTreeParser.h"

#include "stg/AbstrStorageManager.h"
#include "AbstrCalculator.h"
#include "DataStoreManagerCaller.h"
#include "SessionData.h"
#include "TreeItem.h"
#include "TreeItemProps.h"

#include "ConfigProd.h"
#include "ConfigFileName.h"

namespace {
	TokenID s_DesktopRootID_OBSOLETE = GetTokenID_st("DesktopRoot");
	TokenID s_DesktopRootID = GetTokenID_st("Desktops");
}

SYNTAX_CALL void DMS_CONV DMS_Stx_Load()
{
	DMS_Tic_Load();
}

// *****************************************************************************
// Function/Procedure:DMS_CreateTreeFromConfiguration
// Description:       parse a config file and built sheet tree.
// Parameters:        
//     CharPtr p_pSourcefile: 
// Returns:           TreeItem*: root of the tree
// *****************************************************************************


SYNTAX_CALL TreeItem* DMS_CONV DMS_CreateTreeFromConfiguration(CharPtr sourceFilename)
{
	DMS_CALL_BEGIN

		TreeItem* res = nullptr;
		try {
			CDebugContextHandle debugContext("DMS_CreateTreeFromConfiguration", sourceFilename, false);

			SharedStr sourceFileNameStr( sourceFilename);
			sourceFileNameStr = ConvertDosFileName(sourceFileNameStr);

			SharedStr configLoadDir = splitFullPath(sourceFileNameStr.begin());
			CharPtr fileName = sourceFileNameStr.c_str();
			if (configLoadDir.ssize()) fileName += (configLoadDir.ssize()+1);

			SessionData::Create( configLoadDir.c_str(), getFileNameBase(fileName).c_str() );
			SessionData::Curr()->SetConfigPointColFirst( GetConfigPointColFirst() );

			ConfigurationFilenameContainer filenameContainer(configLoadDir);
			{
				StaticMtIncrementalLock<TreeItem::s_NotifyChangeLockCount> dontNotify;
				StaticStIncrementalLock<TreeItem::s_ConfigReadLockCount  > dontCommit;

				MG_LOCKER_NO_UPDATEMETAINFO

				UpdateMarker::ChangeSourceLock changeAtZeroTime(UpdateMarker::tsBereshit, "CreateTreeFromConfigFile");

				#if defined(MG_DEBUG_INTERESTSOURCE)
					DemandManagement::IncInterestDetector incInterestLock("DMS_CreateTreeFromConfiguration()");
				#endif // MG_DEBUG_INTERESTSOURCE
				res = AppendTreeFromConfiguration(fileName, 0);
			}
			SessionData::Curr()->Open(res);
			auto fts = UpdateMarker::GetFreshTS(MG_DEBUG_TS_SOURCE_CODE("CreateTreeFromConfiguration"));
			dms_assert(fts > UpdateMarker::tsBereshit);
			return res;
		}
		catch (...)
		{
			if (res)
				res->EnableAutoDelete();
			throw;
		}
	DMS_CALL_END
	return nullptr;
}

SYNTAX_CALL TreeItem* DMS_CONV DMS_CreateTreeFromString(CharPtr configString)
{
	DMS_CALL_BEGIN

		TreeItem* res = 0;
		CDebugContextHandle debugContext("DMS_CreateTreeFromString", configString, false);

		SessionData::Create(GetCurrentDir().c_str(), "" );
		ConfigurationFilenameContainer filenameContainer(SharedStr::SharedStr());
		{
			StaticMtIncrementalLock<TreeItem::s_NotifyChangeLockCount> dontNotify;
			StaticStIncrementalLock<TreeItem::s_ConfigReadLockCount  > dontCommit;

			MG_LOCKER_NO_UPDATEMETAINFO

			UpdateMarker::ChangeSourceLock changeAtZeroTime(UpdateMarker::tsBereshit, "CreateTreeFromString");
#if defined(MG_DEBUG_INTERESTSOURCE)
			DemandManagement::IncInterestDetector incInterestLock("DMS_CreateTreeFromString()");
#endif // MG_DEBUG_INTERESTSOURCE

			ConfigProd cp(0);
			res = cp.ParseString(configString);
		}
		SessionData::Curr()->Open(res);

		return res;

	DMS_CALL_END
	return 0;
}


// *****************************************************************************
// Function/Procedure:AppendTreeFromConfiguration
// Description:       parse a config file and build tree.
// Returns:           TreeItem*: root of the tree
// *****************************************************************************

bool TryAppendTreeFromConfiguration(CharPtr dektopRootFolderName, CharPtr desktopRootFile, TreeItem* context)
{
	SharedStr desktopRootFileNameStr = 
		DelimitedConcat(
			dektopRootFolderName
		,	desktopRootFile
		);

	if (!IsFileOrDirAccessible(desktopRootFileNameStr))
		return false;
	AppendTreeFromConfiguration(desktopRootFile, context);
	return true;
}

TreeItem* AppendTreeFromConfiguration(CharPtr sourceFileName, TreeItem* context /*can be NULL*/ )
{
	TreeItem* result = nullptr;

	MG_PRECONDITION(sourceFileName);
	CDebugContextHandle debugContext("AppendTreeFromConfiguration", sourceFileName, false);

	SharedStr sourceFileNameStr(sourceFileName);
	sourceFileNameStr = ConvertDosFileName(sourceFileNameStr);

	SharedStr sourcePathNameStr = AbstrStorageManager::GetFullStorageName(
		ConfigurationFilenameLock::GetCurrentDirNameFromConfigLoadDir(), 
		sourceFileNameStr.c_str()
	);

	CharPtr sourcePathName = sourcePathNameStr.c_str();
	SharedStr relPath = getFileNameBase(sourcePathName);

	ConfigurationFilenameLock reserveThisName(sourcePathNameStr, relPath);
	SharedStr sourcePathNameStrFromCurrent = DelimitedConcat(ConfigurationFilenameContainer::GetConfigLoadDirFromCurrentDir().c_str(), sourcePathName);
	if (!IsFileOrDirAccessible(sourcePathNameStrFromCurrent))
		throwErrorF("File Open", "Cannot open configuration file %s.\n"
			"Note that #include statements are relative from the subdir that accompanies the referent"
			, sourcePathName);

	if (!stricmp(getFileNameExtension(sourcePathName), "xml"))
	{
		FileInpStreamBuff streamBuff(sourcePathNameStrFromCurrent, DSM::GetSafeFileWriterArray(), true);
		XmlTreeParser xmlParse(&streamBuff);
		result =  xmlParse.ReadTree(context);

		// read last changed time in the context of an open file to prevent getting the wrong info.
		reserveThisName.GetFileRef()->m_Fdt = GetFileOrDirDateTime(sourcePathNameStrFromCurrent);
	}
	else
	{
		ConfigProd cp(context);
		result = cp.ParseFile(sourcePathNameStrFromCurrent.c_str());

		// read last changed time in the context of an open file to prevent getting the wrong info.
		reserveThisName.GetFileRef()->m_Fdt = GetFileOrDirDateTime(sourcePathNameStrFromCurrent);
	}
	
	if (!result)
		return nullptr;

	configStorePropDefPtr->SetValue(result, GetTokenID_mt(sourceFileName));
	if (context == nullptr) // did we process the root?
	{
		// Append DesktopRoot
		if (!result->GetSubTreeItemByID(s_DesktopRootID))
		if (!result->GetSubTreeItemByID(s_DesktopRootID_OBSOLETE))
		{
			SharedStr desktopRootFolderNameStr = 
				DelimitedConcat(
					ConfigurationFilenameContainer::GetConfigLoadDirFromCurrentDir().c_str()
				,	ConfigurationFilenameLock::GetCurrentDirNameFromConfigLoadDir()
				);

			if (TryAppendTreeFromConfiguration( desktopRootFolderNameStr.c_str(),  "Desktops.dms", result))
				TryAppendTreeFromConfiguration( desktopRootFolderNameStr.c_str(),  "DesktopTemplates.dms", result);
			else
				TryAppendTreeFromConfiguration( desktopRootFolderNameStr.c_str(),  "DesktopRoot.dms", result);
		}
	}

	return result;
}

// *****************************************************************************
// ProcessADMS en ProcessPostData
// *****************************************************************************

#include "dbg/Debug.h"
#include "mci/AbstrValue.h"

#include "AbstrDataItem.h"
#include "AbstrDataObject.h"
#include "DataLocks.h"
#include "TreeItemContextHandle.h"

#include "act/TriggerOperator.h"

IStringHandle DMS_ProcessADMS(const TreeItem* context, CharPtr url)
{
	DMS_CALL_BEGIN

		SuspendTrigger::FencedBlocker blockSuspendCheck;  // DEBUG, HELPS?
		StaticMtIncrementalLock<TreeItem::s_NotifyChangeLockCount> dontNotify;

		CDebugContextHandle checkPtr("STX", "DMS_ProcessADMS", true);

		SharedStr localUrl = SharedStr(url);
		SharedStr result;
		MappedConstFileMapHandle file(localUrl, DSM::GetSafeFileWriterArray(), true, false );
		CharPtr fileCurr = file.DataBegin(), fileEnd = file.DataEnd();
		while (true) {
			CharPtr markerPos = Search(CharPtrRange(fileCurr, fileEnd), "<%"); // TODO: Parse Expr instead of Search
			result += CharPtrRange(fileCurr, markerPos);
			if (markerPos == fileEnd)
				return IString::Create(result.c_str());
			markerPos += 2;
			fileCurr = Search( CharPtrRange(markerPos, fileEnd), "%>");
			if (fileCurr == fileEnd)
				throwErrorD("DMS_ProcessADMS", "unbalanced script markers");

			SharedStr evalRes = AbstrCalculator::EvaluateExpr(const_cast<TreeItem*>(context), CharPtrRange(markerPos, fileCurr), CalcRole::Other, 1);
			if (!evalRes.IsDefined())
				evalRes = UNDEFINED_VALUE_STRING;
			result += evalRes;

			fileCurr += 2;
		}

	DMS_CALL_END
	return nullptr;
}

void ProcessNameValue(TreeItem* context, WeakStr name, WeakStr value)
{
	DBG_START(name.c_str(), value.c_str(), true);

	SharedStr decodedName = UrlDecode(name);
	TreeItem* ti = context->GetItem(name);
	if (!ti)
	{
		DBG_TRACE(("Item not found"))
		return;
	}
	AbstrDataItem* adi = AsDataItem(ti);
	if (!adi || !adi->HasVoidDomainGuarantee())
		ti->throwItemError("Can only set values to parameters");

	SharedStr decodedValue = UrlDecode(value);
	DataWriteLock dwl(adi, dms_rw_mode::write_only_mustzero);

	OwningPtr<AbstrValue> av = dwl->CreateAbstrValue();

	av->AssignFromCharPtrs(decodedValue.begin(), decodedValue.send());
	dwl->SetAbstrValue(0, *av);
	dwl.Commit();
}

bool DMS_ProcessPostData(TreeItem* context, CharPtr postData, UInt32 dataSize)
{
	DMS_CALL_BEGIN

		DBG_START("DMS_ProcessPostData", postData, true);
		TreeItemContextHandle checkPtr(context, "DMS_ProcessPostData");

		dms_assert( (StrLen(postData) + 1) == dataSize );
		if (_strnicmp(postData, "DMSPOST=",8))
			return false;

		MemoInpStreamBuff inpBuff(postData, postData+dataSize);
		FormattedInpStream inpStream(&inpBuff);
		while (!inpBuff.AtEnd())
		{
			CharPtr firstNameChar = postData + inpBuff.CurrPos()-1;
			while (!inpBuff .AtEnd() && inpStream.NextChar() != '=')
				inpStream.ReadChar();
			SharedStr name(firstNameChar, postData + inpBuff.CurrPos()-1);
			if (inpBuff.AtEnd())
				context->throwItemError("Unexpected token:"+name);

			inpStream >> "=";

			CharPtr firstValueChar = postData + inpBuff.CurrPos()-1;
			while (!inpBuff.AtEnd() && inpStream.NextChar() != '&')
				inpStream.ReadChar();
			SharedStr value(firstValueChar, postData + inpBuff.CurrPos()-1);

			ProcessNameValue(context, name, value);

			if (!inpBuff.AtEnd())
				inpStream >> "&";
		}
		return true;

	DMS_CALL_END
	return false;
}
