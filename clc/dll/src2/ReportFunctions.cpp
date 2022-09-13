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
#include "ClcPch.h"

#include "geo/StringArray.h"
#include "mci/CompositeCast.h"
#include "xml/XmlOut.h"

#include "DataArray.h"
#include "DataItemClass.h"
#include "LispTreeType.h"
#include "TreeItemClass.h"
#include "Unit.h"
#include "UnitClass.h"


// =========================================================
// TreeItemOperator
// =========================================================

namespace {
	CommonOperGroup containerGroup(token::container, oper_policy::dynamic_result_class|oper_policy::allow_extra_args);
}

struct TreeItemOperator : Operator
{
	TreeItemOperator()
		: Operator(&containerGroup, TreeItem::GetStaticClass())
	{}

	// Override Operator
	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		dms_check_not_debugonly;

		if (args.size() > 0)
			throwNYI(MG_POS, "container with args");

		if (!resultHolder)
			resultHolder = TreeItem::CreateCacheRoot();

		resultHolder->SetIsInstantiated();
		return true;
	}
};


// =========================================================
// DocData creates the following:
//	this = Expr = "DocData()";
//	ObjectTypes
//	Operators
//		name
//	OperatorArgs
//		nr_Operator
//		nrArg
//		objectType

namespace {
	UInt32 GetNrObjectTypes() 
	{ 
		return Class::Size();    
	}

	UInt32 GetNrGroups()      
	{ 
		return AbstrOperGroup::GetNrOperatorGroups(); 
	}

	UInt32 GetNrOperators(UInt32 grID)
	{ 
		dms_assert(grID < GetNrGroups());
		const AbstrOperGroup* gr = AbstrOperGroup::GetOperatorGroup(grID);
		const Operator* op = gr->GetFirstMember();
		UInt32 result = 0; 
		while (op)
		{
			result++;
			op = op->GetNextGroupMember();
		}
		return result;
	}

	UInt32 GetNrOperators()
	{ 
		UInt32 result = 0;
		UInt32 i = GetNrGroups();
		while (i--)
			result += GetNrOperators(i);
		return result;
	}

	UInt32 GetNrArgs()
	{
		UInt32 c = 0;
		for( UInt32 grID=0, n=GetNrGroups(); grID<n; ++grID)
		{
			const AbstrOperGroup* gr = AbstrOperGroup::GetOperatorGroup(grID);
			const Operator* op = gr->GetFirstMember();
			while (op)
			{
				c += op->NrSpecifiedArgs();
				op = op->GetNextGroupMember();
			}
		}
		return c;
	}

	CommonOperGroup docDataGroup("DocData", oper_policy::calc_requires_metainfo);
	CommonOperGroup docSmGroup  ("DocStorageManagers");
} // namespace

class DocDataOperator : public Operator
{
	typedef TreeItem ResultType;
	typedef DataArray<SharedStr> StringDataItem;
	typedef DataArray<UInt32>    UInt32DataItem;

public:
	DocDataOperator()
		: Operator(&docDataGroup, ResultType::GetStaticClass())
		 {}

	// Override Operator
	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		dms_check_not_debugonly; 

		dms_assert(args.size() == 0);

		if (!resultHolder)
		{
			resultHolder = TreeItem::CreateCacheRoot();
			resultHolder.GetNew()->SetKeepDataState(true);
		}

		TreeItem* result = resultHolder;

//		table of ObjectTypes
		std::map<ClassCPtr, UInt32> classNrs;

			AbstrUnit* auTableTypes = Unit<UInt32>::GetStaticClass()->CreateUnit(result, GetTokenID_mt("ObjectTypes"));
			{
				AbstrDataItem* adiTypeName = CreateDataItem(
						auTableTypes
					,	GetTokenID_mt("name")
					,	auTableTypes
					,	Unit<SharedStr>::GetStaticClass()->CreateDefault()
					);

				if (mustCalc)
				{
					UInt32 nrClasses = GetNrObjectTypes();
					auTableTypes->SetCount(nrClasses);

					DataWriteLock resLock(adiTypeName);
					auto diTypeName = mutable_array_cast<SharedStr>(resLock);
					auto nameData = diTypeName->GetDataWrite();
					if (nrClasses)
					{
						const ClassCPtr* src = Class::Begin();
						StringDataItem::iterator
							b = nameData.begin(),
							e = nameData.end();
						UInt32 i=0;
						for(; b!=e; ++src, ++i, ++b) 
						{
							classNrs[*src] = i;
							Assign(*b, (*src)->GetName().c_str());
						}
					}
					resLock.Commit();
				}
			}
//		table of OperGroups
			AbstrUnit* auTableGroups = Unit<UInt32>::GetStaticClass()->CreateUnit(result, GetTokenID_mt("OperatorGroups"));
			{
				AbstrDataItem* adiGroupName = CreateDataItem(auTableGroups, GetTokenID_mt("name"       ), auTableGroups, Unit<SharedStr>::GetStaticClass()->CreateDefault());
				AbstrDataItem* adiNrOper    = CreateDataItem(auTableGroups, GetTokenID_mt("NrOperators"), auTableGroups, Unit<UInt32>::GetStaticClass()->CreateDefault());

				if (mustCalc)
				{
					auTableGroups->SetCount(GetNrGroups());

					DataWriteLock res1Lock(adiGroupName); 
					DataWriteLock res2Lock(adiNrOper   ); 

					auto nameData = mutable_array_cast<SharedStr>(res1Lock)->GetDataWrite();
					auto nrOpData = mutable_array_cast<UInt32>(res2Lock)->GetDataWrite();

					UInt32 srcI = 0;
					StringDataItem::iterator
						iName = nameData.begin(),
						eName = nameData.end();
					UInt32DataItem::iterator iNrOper = nrOpData.begin();
					while (iName!=eName) 
					{
						*iNrOper++ = GetNrOperators(srcI);
						const AbstrOperGroup* agr = AbstrOperGroup::GetOperatorGroup(srcI++); 
						Assign(*iName++, agr->GetName().c_str());
					}

					res1Lock.Commit();
					res2Lock.Commit();
				}
			}
			AbstrUnit* auTableFuncs = Unit<UInt32>::GetStaticClass()->CreateUnit(result, GetTokenID_mt("Operators"));
			{
				AbstrDataItem* adiNrGroup    = CreateDataItem(auTableFuncs, GetTokenID_mt("nr_Group"  ), auTableFuncs, auTableGroups);
				AbstrDataItem* adiResultType = CreateDataItem(auTableFuncs, GetTokenID_mt("nr_ResType"), auTableFuncs, auTableTypes );

				if (mustCalc)
				{
					auTableFuncs->SetCount(GetNrOperators());

					DataWriteLock res1Lock(adiNrGroup);
					DataWriteLock res2Lock(adiResultType);

					auto nrGroupData = mutable_array_cast<UInt32>(res1Lock)->GetDataWrite();
					auto resTypeData = mutable_array_cast<UInt32>(res2Lock)->GetDataWrite();

					auto nrGroupDataPtr = nrGroupData.begin();
					auto resTypeDataPtr = resTypeData.begin();

					UInt32 grI = 0, grN = GetNrGroups();
					while (grI!=grN)
					{
						const AbstrOperGroup* agr = AbstrOperGroup::GetOperatorGroup(grI); 
						const Operator*      oper = agr->GetFirstMember();
						while(oper)
						{
							*nrGroupDataPtr++ = grI;
							*resTypeDataPtr++  = classNrs[oper->GetResultClass()];
							oper = oper->GetNextGroupMember();
						}

						grI++;
					}
					dms_assert(nrGroupDataPtr == nrGroupData.end());
					dms_assert(resTypeDataPtr == resTypeData.end());

					res1Lock.Commit();
					res2Lock.Commit();
				}
			}
			AbstrUnit* auTableArgs = Unit<UInt32>::GetStaticClass()->CreateUnit(result, GetTokenID_mt("OperatorArgs"));
			{
				AbstrDataItem* adiNrOper = CreateDataItem(auTableArgs, GetTokenID_mt("nr_Operator"), auTableArgs, auTableFuncs);
				AbstrDataItem* adiNrType = CreateDataItem(auTableArgs, GetTokenID_mt("nr_ArgType" ), auTableArgs, auTableTypes);

				if (mustCalc)
				{
					auTableArgs->SetCount(GetNrArgs());

					DataWriteLock res1Lock(adiNrOper);
					DataWriteLock res2Lock(adiNrType);

					auto nrOperData = mutable_array_cast<UInt32>(res1Lock)->GetDataWrite();
					auto nrTypeData = mutable_array_cast<UInt32>(res2Lock)->GetDataWrite();

					auto nrOperDataPtr = nrOperData.begin();
					auto nrTypeDataPtr = nrTypeData.begin();

					UInt32 grI = 0, opI = 0, grN = GetNrGroups();
					while (grI!=grN)
					{
						const AbstrOperGroup* agr = AbstrOperGroup::GetOperatorGroup(grI); 
						const Operator*      oper = agr->GetFirstMember();
						while(oper)
						{
							UInt32 argI = 0, argN = oper->NrSpecifiedArgs();
							while (argI != argN)
							{
								*nrOperDataPtr++ = opI;
								*nrTypeDataPtr++ = classNrs[const_cast<ClassCPtr>(oper->GetArgClass(argI++))];
							}
							opI++;
							oper = oper->GetNextGroupMember();
						}

						grI++;
					}
					dms_assert(nrOperDataPtr == nrOperData.end());
					dms_assert(nrTypeDataPtr == nrTypeData.end());

					res1Lock.Commit();
					res2Lock.Commit();
				}
			}

		resultHolder->SetIsInstantiated();
		return true;
	}
};

#include "stg/StorageClass.h"
#include "gdal/gdal_base.h"

class DocStorageManagersOperator : public Operator
{
	typedef TreeItem ResultType;
	typedef DataArray<SharedStr> StringDataItem;
	typedef DataArray<UInt32>    UInt32DataItem;

public:
	DocStorageManagersOperator()
		: Operator(&docSmGroup, ResultType::GetStaticClass())
		{}

	// Override Operator
	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		dms_assert(args.size() == 0);

		if (!resultHolder)
			resultHolder = TreeItem::CreateCacheRoot();

		TreeItem* result = resultHolder;

	//	table of StorageManagers
		{
			AbstrUnit* auSmTypes = Unit<UInt32>::GetStaticClass()->CreateUnit(result, GetTokenID_mt("StorageManagers"));

			AbstrDataItem* adiTypeName = CreateDataItem(
					auSmTypes
				,	GetTokenID_mt("name")
				,	auSmTypes
				,	Unit<SharedStr>::GetStaticClass()->CreateDefault()
				);

			if (mustCalc)
			{
				SizeT nrSm = StorageClass::GetNrClasses();
				auSmTypes->SetCount(nrSm);

				DataWriteLock resLock(adiTypeName, dms_rw_mode::write_only_mustzero);
				StringDataItem* diTypeName = mutable_array_cast<SharedStr>(resLock);
				StringDataItem::locked_seq_t nameData = diTypeName->GetLockedDataWrite();
				StringDataItem::iterator
					b = nameData.begin(),
					e = nameData.end();
				for (SizeT i=0; i!=nrSm; ++b, ++i)
				{
					StorageClass* sc = StorageClass::Get(i);
					dms_assert(sc);
					Assign(*b, sc->GetName().c_str());
				}
				resLock.Commit();
			}
		}

		//gdalComponent test;
		gdalComponent::CreateMetaInfo(result, mustCalc);

		resultHolder->SetIsInstantiated();
		return true;
	}
};

namespace {
	TreeItemOperator treeItemOperator;
	DocDataOperator docDataOper;
	DocStorageManagersOperator docSmOper;
}	// end anonymous namespace

#include "ClcInterface.h"

CLC2_CALL void DMS_CONV XML_ReportOperator(OutStreamBase* xmlStr, const Operator* oper)
{
	XML_OutElement xml_oper(*xmlStr, "Function", GetTokenStr(oper->GetGroup()->GetNameID()).c_str());
	{
		XML_OutElement xml_resulttype(*xmlStr, "ResultType", "");
		XML_ReportSchema(xmlStr, DMS_Operator_GetResultingClass(oper), false);
	}
	{
		XML_OutElement xml_argtypes(*xmlStr, "ArgTypes", "");
		UInt32 nrArgs = DMS_Operator_GetNrArguments(oper);
		xmlStr->WriteAttr("NrSpecifiedArgs", nrArgs);
		for (UInt32 i=0; i<nrArgs; ++i)
			XML_ReportSchema(xmlStr, DMS_Operator_GetArgumentClass(oper, i), false);
	}
}

CLC2_CALL void DMS_CONV XML_ReportOperGroup(OutStreamBase* xmlStr, const AbstrOperGroup* gr)
{
	XML_OutElement xml_oper(*xmlStr, "OperatorGroup", GetTokenStr(gr->GetNameID()).c_str());
	const Operator* oper = gr->GetFirstMember();
	while(oper)
	{
		XML_ReportOperator(xmlStr, oper);
		oper = oper->GetNextGroupMember();
	}
}



CLC2_CALL void DMS_CONV XML_ReportAllOperGroups(OutStreamBase* xmlStr)
{
	XML_OutElement xml_funcs(*xmlStr, "FunctionList", "");
	UInt32 nrFuncs = GetNrOperators();
	xmlStr->WriteAttr("NrFunctions", nrFuncs);
	for (UInt32 i=0; i<nrFuncs; ++i)
		XML_ReportOperGroup(xmlStr, DMS_OperatorSet_GetOperatorGroup(i));
}
