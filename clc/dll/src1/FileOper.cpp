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

#include "ClcPCH.h"
#pragma hdrstop

#include "RtcInterface.h"
#include "set/VectorFunc.h"
#include "utl/Environment.h"
#include "geo/Conversions.h"
#include "geo/StringArray.h"
#include "mci/CompositeCast.h"
#include "stg/AbstrStorageManager.h"

#include "DataItemClass.h"
#include "Param.h"
#include "SessionData.h"
#include "TreeItemClass.h"
#include "Unit.h"
#include "UnitClass.h"

#include "ConstOper.h"
#include "OperUnit.h"

#include <functional>
#include <iterator>
#include <algorithm>


// *****************************************************************************
//											CLASSES
// *****************************************************************************


class CurrentDirParamOperator : public Operator
{
	typedef DataArray<SharedStr> ResultType;

public:
	CurrentDirParamOperator(AbstrOperGroup* gr)
		: Operator(gr, ResultType::GetStaticClass()) {}

	// Override Operator
	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		dms_assert(args.size() == 0);
		if (!resultHolder)
			resultHolder = CreateCacheParam<SharedStr>();

		if (mustCalc)
		{
			AbstrDataItem* res = AsDataItem(resultHolder.GetNew());
			DataWriteLock resLock(res); 

			Assign(mutable_array_cast<SharedStr>(resLock)->GetDataWrite()[0], GetCurrentDir());

			resLock.Commit();
		}
		return true;
	}
};

class MakeDirOperator : public UnaryOperator
{
	typedef DataArray<SharedStr> ArgType;
	typedef DataArray<SharedStr> ResultType;

public:
	MakeDirOperator(AbstrOperGroup* gr)
		: UnaryOperator(gr, ResultType::GetStaticClass(), ArgType::GetStaticClass()) {}

	// Override Operator
	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		dms_assert(args.size() == 1);

		if (!resultHolder)
			resultHolder = CreateCacheParam<SharedStr>();

		if (mustCalc)
		{
			SharedStr dirName = AbstrStorageManager::GetFullStorageName(
				SessionData::Curr()->GetConfigRoot()
			,	GetTheCurrValue<SharedStr>(args[0])
			);
			
			MakeDirsForFile(dirName);
			MakeDir(dirName);

			AbstrDataItem* res = AsDataItem(resultHolder.GetNew());

			DataWriteLock lock(res);
			ResultType* result = mutable_array_cast<SharedStr>(lock);
			Assign(result->GetDataWrite()[0], dirName);
			lock.Commit();
		}
		return true;
	}
};

class CopyFileOperator : public BinaryOperator
{
	typedef DataArray<SharedStr> Arg1Type;
	typedef DataArray<SharedStr> Arg2Type;
	typedef DataArray<SharedStr> ResultType;

	bool m_Always, m_SrcMustExist, m_GetNameOnly;
public:
	CopyFileOperator(AbstrOperGroup* gr, bool always, bool srcMustExist, bool getNameOnly)
		:	BinaryOperator(gr, 
				ResultType::GetStaticClass(), 
				Arg1Type::GetStaticClass(), Arg2Type::GetStaticClass()
			)
		,	m_Always(always)
		,	m_SrcMustExist(srcMustExist) 
		,	m_GetNameOnly(getNameOnly)
	{}

	// Override Operator
	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		dms_assert(args.size() == 2);
		
		if (!resultHolder)
			resultHolder = CreateCacheParam<SharedStr>();

		if (mustCalc)
		{
			const TreeItem* configRoot = SessionData::Curr()->GetConfigRoot();
			SharedStr fileSrcName  = GetTheCurrValue<SharedStr>(args[0]);
			SharedStr fileDestName = GetTheCurrValue<SharedStr>(args[1]);
			if (!IsDefined(fileSrcName )) throwErrorD(GetGroup()->GetName().c_str(), "Source file name is undefined");
			if (!IsDefined(fileDestName)) throwErrorD(GetGroup()->GetName().c_str(), "Destination file name is undefined");

			fileSrcName  = AbstrStorageManager::GetFullStorageName(configRoot, fileSrcName );
			fileDestName = AbstrStorageManager::GetFullStorageName(configRoot, fileDestName);

			if (m_GetNameOnly)
			{
				if (!IsFileOrDirAccessible(fileDestName))
					fileDestName = fileSrcName;
			}
			else if (m_Always || !IsFileOrDirAccessible(fileDestName))
			{
				if (m_SrcMustExist || IsFileOrDirAccessible(fileSrcName))
				{
					GetWritePermission(fileDestName);
					CopyFileOrDir(fileSrcName.c_str(), fileDestName.c_str(), false);
				}
			}

			AbstrDataItem* res = AsDataItem(resultHolder.GetNew());
			DataWriteLock lock(res);
			ResultType* result = mutable_array_cast<SharedStr>(lock);

			Assign(result->GetDataWrite()[0], fileDestName);
			lock.Commit();
		}
		return true;
	}
} ;

class FullPathNameOperator : public BinaryOperator
{
	typedef TreeItem          Arg1Type;
	typedef DataArray<SharedStr> Arg2Type;
	typedef DataArray<SharedStr> ResultType;

public:
	FullPathNameOperator(AbstrOperGroup* gr)
		:	BinaryOperator(gr, ResultType::GetStaticClass(), 
				Arg1Type::GetStaticClass(), Arg2Type::GetStaticClass()
			) 
	{}

	// Override Operator
	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const
	{
		dms_assert(args.size() == 2);
		
		const AbstrDataItem* fileNameDataA = AsDataItem(args[1]);
		if (!resultHolder)
			resultHolder = CreateCacheDataItem(fileNameDataA->GetAbstrDomainUnit(), fileNameDataA->GetAbstrValuesUnit());

		if (mustCalc)
		{
			AbstrDataItem* res = AsDataItem(resultHolder.GetNew());
			DataWriteLock lock(res);

			ResultType* result = mutable_array_cast<SharedStr>(lock);
			auto resultData = result->GetDataWrite();

			auto fileNameData= const_array_cast<SharedStr>(fileNameDataA)->GetDataRead();

			for (SizeT i = 0, e = fileNameData.size(); i!=e; ++i)
				Assign(resultData[i],
					AbstrStorageManager::GetFullStorageName(
						args[0],              // context
						SharedStr(            // relFileName
							fileNameData[i].begin(), 
							fileNameData[i].end()  
						)
					)
				);
			lock.Commit();
		}
		return true;
	}
} ;

// *****************************************************************************
//											INSTANTIATION
// *****************************************************************************

namespace 
{
	oper_arg_policy oapFullPath[2] = { oper_arg_policy::calc_never, oper_arg_policy::calc_as_result };
	CommonOperGroup cog_CopyFile("CopyFile", oper_policy::is_transient | oper_policy::calc_requires_metainfo | oper_policy::has_external_effects);
	CommonOperGroup cog_CreateFile("CreateFile", oper_policy::is_transient | oper_policy::calc_requires_metainfo| oper_policy::has_external_effects);
	CommonOperGroup cog_InitFile("InitFile", oper_policy::is_transient | oper_policy::calc_requires_metainfo | oper_policy::has_external_effects);
	CommonOperGroup cog_UseFile("ExistingFile", oper_policy::is_transient | oper_policy::calc_requires_metainfo);

	const bool WHATEVER = false;

	CopyFileOperator g_CopyFileOperator(&cog_CopyFile, true, true, false);
	CopyFileOperator g_CreateFileOperator(&cog_CreateFile, false, true, false);
	CopyFileOperator g_InitFileOperator(&cog_InitFile, false, false, false);
	CopyFileOperator g_ExistingFileOperator(&cog_UseFile, WHATEVER, WHATEVER, true);

	CommonOperGroup cog_MakeDir("makeDir", oper_policy::has_external_effects);
	CommonOperGroup cog_CurrDir("currentDir", oper_policy::is_transient);
	SpecialOperGroup sog_FullPath("fullPathName", 2, oapFullPath, oper_policy::is_transient|oper_policy::calc_requires_metainfo);

	MakeDirOperator         g_MakeDirOperator(&cog_MakeDir);
	CurrentDirParamOperator g_CurrentDirParamOperator(&cog_CurrDir);
	FullPathNameOperator    g_FullPathNameOperator(&sog_FullPath);

	CommonOperGroup cog_Version("GeoDmsVersion", oper_policy::is_transient);
	CommonOperGroup cog_Platform("GeoDmsPlatform", oper_policy::is_transient);
	CommonOperGroup cog_BuildConfig("GeoDmsBuildConfig", oper_policy::is_transient);
	CommonOperGroup cog_TypeModel("GeoDmsTypeModel", oper_policy::is_transient);

	ConstParamOperator<Float64>            cpVersion(&cog_Version, DMS_GetVersionNumber());
	ConstParamOperator<SharedStr, CharPtr> cpPlatform(&cog_Platform, DMS_GetPlatform());
	ConstParamOperator<SharedStr, CharPtr> cpBuildConfig(&cog_BuildConfig, DMS_GetBuildConfig());
	ConstParamOperator<SharedStr, CharPtr> cpTypeModel(&cog_TypeModel, DMS_GetTypeModel());
}
