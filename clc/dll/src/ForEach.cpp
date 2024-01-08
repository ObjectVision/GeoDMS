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

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

#include "act/MainThread.h"
#include "dbg/SeverityType.h"
#include "mci/CompositeCast.h"

#include "DataItemClass.h"
#include "TreeItemClass.h"
#include "DataArray.h"
#include "Unit.h"
#include "UnitClass.h"
#include "CopyTreeContext.h"
#include "TreeItemProps.h"

enum param_t { 
	pt_excluded    = 0, 
	pt_contextonly = 1, 
	pt_contextname = 2
};

enum field_spec {
	FS_EXPR               = 1,
	FS_CHECK              = 2,
	FS_HASDOMAINVALUESPEC = 4,
	FS_DOMAIN_AS_SUBITEM  = 8,
	FS_VALUES_AS_SUBITEM  = 0x10,
	FS_TEMPLATE           = 8,
	FS_TEMPL_AS_SUBITEM   = 0x10,
	FS_LABEL              = 0x20,
	FS_DESCR              = 0x40,
	FS_STORAGENAME        = 0x80,
	FS_STORAGETYPE        = 0x100,
	FS_STORAGE_RO         = 0x4000,
	FS_SQLSTRING          = 0x200,
	FS_CDF                = 0x400,
	FS_URL                = 0x800,
	FS_UNIT               = 0x1000,
	FS_UNIT_AS_SUBITEM    = 0x2000,
};

inline field_spec operator |(field_spec a, field_spec b) { return field_spec(int(a)|int(b)); }

param_t TemplMode(field_spec fs)
{
	if (fs & FS_HASDOMAINVALUESPEC)
		return pt_excluded;
	dms_assert((fs & (FS_TEMPLATE|FS_TEMPL_AS_SUBITEM)) != (FS_TEMPLATE|FS_TEMPL_AS_SUBITEM)); 

	switch(fs & (FS_TEMPLATE|FS_TEMPL_AS_SUBITEM))
	{
		case FS_TEMPLATE:         return pt_contextonly;
		case FS_TEMPL_AS_SUBITEM: return pt_contextname;
	}
	return pt_excluded;
}

param_t DomainMode(field_spec fs)
{
	if ((fs & FS_HASDOMAINVALUESPEC) == 0)
		return pt_excluded;
	if (fs & FS_DOMAIN_AS_SUBITEM)
		return pt_contextname;
	return pt_contextonly;
}

param_t ValuesMode(field_spec fs)
{
	if ((fs & FS_HASDOMAINVALUESPEC) == 0)
		return pt_excluded;
	if (fs & FS_VALUES_AS_SUBITEM)
		return pt_contextname;
	return pt_contextonly;
}

param_t UnitMode(field_spec fs)
{
	switch(fs & (FS_UNIT|FS_UNIT_AS_SUBITEM))
	{
		case FS_UNIT:            return pt_contextonly;
		case FS_UNIT_AS_SUBITEM: return pt_contextname;
	}
	return pt_excluded;
}

bool IsParam(const AbstrDataItem* adi)
{
	return adi && adi->HasVoidDomainGuarantee();
}

typedef DataArray<SharedStr> SharedStrArray;

// *****************************************************************************
//									ForEach_CreateRessult
// *****************************************************************************

const TreeItem* FindName(const TreeItem* context, const DataArray<SharedStr>* nameArray, UInt32 i, CharPtr role)
{
	dms_assert(context);
	if (!nameArray) 
		return context;

	SharedStr templName = nameArray->GetIndexedValue(i);
	const TreeItem* result = context->FindItem(templName);
	if (!result)
		context->throwItemErrorF(
			"%s %s cannot be found in %s container",
			role, templName.c_str(), role
		);

	return result;
}

bool ForEach_CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, arg_index argCount, TokenID groupNameID, field_spec fs)
{
	dbg_assert(resultHolder);

	param_t withTempl  = TemplMode (fs);
	param_t withDomain = DomainMode(fs); 
	param_t withValues = ValuesMode(fs);
	param_t withUnit   = UnitMode  (fs);

	const AbstrDataItem* nameArrayItemA =                   AsCertainDataItem(args[argCount++]);
	const AbstrDataItem* optExprsA      = (fs & FS_EXPR ) ? AsCertainDataItem(args[argCount++]) : nullptr;
	const AbstrDataItem* optChecksA     = (fs & FS_CHECK) ? AsCertainDataItem(args[argCount++]) : nullptr;
	const TreeItem*      optTempl       = withTempl  != pt_excluded     ? args[argCount++] : nullptr;
	const AbstrDataItem* optTemplNamesA = withTempl  == pt_contextname  ? AsCertainDataItem(args[argCount++]) : nullptr;
	const TreeItem*      optDuContext   = withDomain != pt_excluded     ? args[argCount++] : nullptr;
	const AbstrDataItem* optDuNamesA    = withDomain == pt_contextname  ? AsCertainDataItem(args[argCount++]) : nullptr;
	const TreeItem*      optVuContext   = withValues != pt_excluded     ? args[argCount++] : nullptr;
	const AbstrDataItem* optVuNamesA    = withValues == pt_contextname  ? AsCertainDataItem(args[argCount++]) : nullptr;
	const TreeItem*      optUnitContext = withUnit   != pt_excluded     ? args[argCount++] : nullptr;
	const AbstrDataItem* optUnitNamesA  = withUnit   == pt_contextname  ? AsCertainDataItem(args[argCount++]) : nullptr;
	const AbstrDataItem* optLabelsA     = (fs & FS_LABEL      ) ? AsCertainDataItem(args[argCount++]) : nullptr;
	const AbstrDataItem* optDescrsA     = (fs & FS_DESCR      ) ? AsCertainDataItem(args[argCount++]) : nullptr;
	const AbstrDataItem* optStorageNameA= (fs & FS_STORAGENAME) ? AsCertainDataItem(args[argCount++]) : nullptr;
	const AbstrDataItem* optStorageTypeA= (fs & FS_STORAGETYPE) ? AsCertainDataItem(args[argCount++]) : nullptr;
	const AbstrDataItem* optStorageRoA  = (fs & FS_STORAGE_RO)  ? AsCertainDataItem(args[argCount++]) : nullptr;
	const AbstrDataItem* optSqlStringsA = (fs & FS_SQLSTRING  ) ? AsCertainDataItem(args[argCount++]) : nullptr;
	const AbstrDataItem* optCdfA =        (fs & FS_CDF        ) ? AsCertainDataItem(args[argCount++]) : nullptr;
	const AbstrDataItem* optUrlA =        (fs & FS_URL        ) ? AsCertainDataItem(args[argCount++]) : nullptr;

	DataReadLock lock1(nameArrayItemA);
	DataReadLock lock2(optExprsA);
	DataReadLock lock2a(optChecksA);
	DataReadLock lock4(optTemplNamesA);
	DataReadLock lock4a(optDuNamesA);
	DataReadLock lock4b(optVuNamesA);
	DataReadLock lock4c(optUnitNamesA);
	DataReadLock lock5(optLabelsA);
	DataReadLock lock6(optDescrsA);
	DataReadLock lock7(optStorageNameA);
	DataReadLock lock8(optStorageTypeA);
	DataReadLock lock9(optStorageRoA);
	DataReadLock lock10(optSqlStringsA);
	DataReadLock lock11(optCdfA);
	DataReadLock lock12(optUrlA);

	const SharedStrArray* nameArrayItem  =                  const_array_checkedcast<SharedStr>(lock1 );
	const SharedStrArray* optExprs       = optExprsA      ? const_array_checkedcast<SharedStr>(lock2 ) : nullptr;
	const SharedStrArray* optChecks      = optChecksA     ? const_array_checkedcast<SharedStr>(lock2a) : nullptr;
	const SharedStrArray* optTemplNames  = optTemplNamesA ? const_array_checkedcast<SharedStr>(lock4 ) : nullptr;
	const SharedStrArray* optUnitNames   = optUnitNamesA  ? const_array_checkedcast<SharedStr>(lock4c) : nullptr;
	const SharedStrArray* optDuNames     = optDuNamesA    ? const_array_checkedcast<SharedStr>(lock4a) : nullptr;
	const SharedStrArray* optVuNames     = optVuNamesA    ? const_array_checkedcast<SharedStr>(lock4b) : nullptr;
	const SharedStrArray* optLabels      = optLabelsA     ? const_array_checkedcast<SharedStr>(lock5 ) : nullptr;
	const SharedStrArray* optDescrs      = optDescrsA     ? const_array_checkedcast<SharedStr>(lock6 ) : nullptr;
	const SharedStrArray* optStorageName = optStorageNameA? const_array_checkedcast<SharedStr>(lock7 ) : nullptr;
	const SharedStrArray* optStorageType = optStorageTypeA? const_array_checkedcast<SharedStr>(lock8 ) : nullptr;
	const DataArray<Bool>* optStorageRo   = optStorageRoA  ? const_array_checkedcast<Bool     >(lock9) : nullptr;
	const SharedStrArray* optSqlStrings  = optSqlStringsA ? const_array_checkedcast<SharedStr>(lock10) : nullptr;
	const SharedStrArray* optCdf         = optCdfA        ? const_array_checkedcast<SharedStr>(lock11) : nullptr;
	const SharedStrArray* optUrl         = optUrlA        ? const_array_checkedcast<SharedStr>(lock12) : nullptr;

	dms_assert(nameArrayItem);

	const AbstrUnit* domainUnit = nameArrayItemA->GetAbstrDomainUnit();
	dms_assert(domainUnit);

	MG_USERCHECK2(!optExprs || domainUnit->UnifyDomain(optExprsA->GetAbstrDomainUnit(), "", "", UM_AllowVoidRight),
		"for_each operator: nameArray and exprArray don't have the same domain");
	bool optExprsIsParam = IsParam(optExprsA);

	MG_USERCHECK2(!optChecks || domainUnit->UnifyDomain(optChecksA->GetAbstrDomainUnit(), "", "", UM_AllowVoidRight),
		"for_each operator: nameArray and checkArray don't have the same domain");
	bool optChecksIsParam = IsParam(optChecksA);

	MG_USERCHECK2(!optTemplNames || domainUnit->UnifyDomain(optTemplNamesA->GetAbstrDomainUnit(), "", ""),
		"for_each operator: nameArray and templNameArray don't have the same domain");

	MG_USERCHECK2(!optDuNames || domainUnit->UnifyDomain(optDuNamesA->GetAbstrDomainUnit(), "", ""),
		"for_each operator: nameArray and domainUnitNameArray don't have the same domain");

	MG_USERCHECK2(!optVuNames || domainUnit->UnifyDomain(optVuNamesA->GetAbstrDomainUnit(), "", ""),
		"for_each operator: nameArray and valuesUnitNameArray don't have the same domain");

	MG_USERCHECK2(!optUnitNames || domainUnit->UnifyDomain(optUnitNamesA->GetAbstrDomainUnit(), "", ""),
		"for_each operator: nameArray and unitNameArray don't have the same domain");

	MG_USERCHECK2(!optLabels || domainUnit->UnifyDomain(optLabelsA->GetAbstrDomainUnit(), "", "", UM_AllowVoidRight),
		"for_each operator: nameArray and labelArray don't have the same domain");
	bool optLabelsIsParam = IsParam(optLabelsA);

	MG_USERCHECK2(!optDescrs || domainUnit->UnifyDomain(optDescrsA->GetAbstrDomainUnit(), "", "", UM_AllowVoidRight),
		"for_each operator: nameArray and descrArray don't have the same domain");
	bool optDescrsIsParam = IsParam(optDescrsA);

	MG_USERCHECK2(!optStorageName || domainUnit->UnifyDomain(optStorageNameA->GetAbstrDomainUnit(), "", "", UM_AllowVoidRight),
		"for_each operator: nameArray and storageName don't have the same domain");
	bool optStorageNamesIsParam = IsParam(optStorageNameA);

	MG_USERCHECK2(!optStorageType || domainUnit->UnifyDomain(optStorageTypeA->GetAbstrDomainUnit(), "", "", UM_AllowVoidRight),
		"for_each operator: nameArray and storageType don't have the same domain");
	bool optStorageTypesIsParam = IsParam(optStorageTypeA);

	MG_USERCHECK2(!optStorageRo || domainUnit->UnifyDomain(optStorageRoA->GetAbstrDomainUnit(), "", "", UM_AllowVoidRight),
		"for_each operator: nameArray and storageReadOnly don't have the same domain");
	bool optStorageRoIsParam = IsParam(optStorageRoA);

	MG_USERCHECK2(!optSqlStrings || domainUnit->UnifyDomain(optSqlStringsA->GetAbstrDomainUnit(), "", "", UM_AllowVoidRight),
		"for_each operator: nameArray and sqlStringArray don't have the same domain");
	bool optSqlStringsIsParam  = IsParam(optSqlStringsA);

	MG_USERCHECK2(!optCdf || domainUnit->UnifyDomain(optCdfA->GetAbstrDomainUnit(), "", "", UM_AllowVoidRight),
		"for_each operator: nameArray and CdfArray don't have the same domain");
	bool optCdfIsParam  = IsParam(optCdfA);

	MG_USERCHECK2(!optUrl || domainUnit->UnifyDomain(optUrlA->GetAbstrDomainUnit(), "", "", UM_AllowVoidRight),
		"for_each operator: nameArray and UrlArray don't have the same domain");
	bool optUrlIsParam  = IsParam(optUrlA);

	UInt32 maxNrIter = domainUnit->GetCount();

	if (!resultHolder)
		resultHolder = TreeItem::CreateCacheRoot();

	for (arg_index i=0; i!= maxNrIter; ++i)
	{
		SharedStr subItemName = nameArrayItem->GetIndexedValue(i);
		if (!subItemName.IsDefined())
			continue;
		if (subItemName.empty())
			continue;

		TreeItem* iter = 0;
		if (optTempl)
		{
			dms_assert(!optDuContext);
			dms_assert(!optVuContext);

			const TreeItem* templ = FindName(optTempl, optTemplNames, i, "template");
			if (optExprs && !templ->_GetExprStr().empty())
				reportF(SeverityTypeID::ST_Warning,
					"%s: this template has calculation rule '%s'\n"
					"which conflicts with the explicit given expressions for the item domain set %s;\n"
					"this is considered as a potential error",
					groupNameID.GetStr().c_str(),
					templ->_GetExprStr().c_str(),
					domainUnit->GetFullName().c_str()
				);

			iter = CopyTreeContext(resultHolder, templ, subItemName.c_str(), DataCopyMode::CopyExpr).Apply();
		}
		else if (optDuContext)
		{
			dms_assert(optVuContext);
			const AbstrUnit* du = debug_cast<const AbstrUnit*>(FindName(optDuContext, optDuNames, i, "domain unit")->CheckObjCls(AbstrUnit::GetStaticClass()));
			const AbstrUnit* vu = debug_cast<const AbstrUnit*>(FindName(optVuContext, optVuNames, i, "values unit")->CheckObjCls(AbstrUnit::GetStaticClass()));

			iter = CreateDataItemFromPath(resultHolder, subItemName.c_str(), du, vu, ValueComposition::Single);
		}
		else if (optUnitContext)
		{
			const AbstrUnit* unit = debug_cast<const AbstrUnit*>(FindName(optUnitContext, optUnitNames, i, "unit")->CheckObjCls(AbstrUnit::GetStaticClass()));

			iter = unit->GetUnitClass()->CreateUnitFromPath(resultHolder, subItemName.c_str());
		}
		else
		{
			dms_assert(!optVuContext);
			iter = resultHolder.GetNew()->CreateItemFromPath(subItemName.c_str());
		}

		dms_assert(iter);

		if (optExprs)       iter->SetExpr (                          SharedStr(optExprs      ->GetIndexedValue(optExprsIsParam       ? 0 : i)));
		if (optDescrs)      iter->SetDescr(                          SharedStr(optDescrs     ->GetIndexedValue(optDescrsIsParam      ? 0 : i)));
		if (optChecks)      integrityCheckPropDefPtr ->SetValue(iter, SharedStr(optChecks     ->GetIndexedValue(optChecksIsParam      ? 0 : i)));
		if (optLabels)      labelPropDefPtr          ->SetValue(iter, SharedStr(optLabels     ->GetIndexedValue(optLabelsIsParam      ? 0 : i)));
		if (optStorageName) storageNamePropDefPtr    ->SetValue(iter, SharedStr(optStorageName->GetIndexedValue(optStorageNamesIsParam? 0 : i)));
		if (optStorageType) storageTypePropDefPtr    ->SetValue(iter, TokenID  (optStorageType->GetIndexedValue(optStorageTypesIsParam? 0 : i)));
		if (optStorageRo)   storageReadOnlyPropDefPtr->SetValue(iter, optStorageRo->GetIndexedValue(optStorageRoIsParam ? 0 : i));
		if (optSqlStrings)  sqlStringPropDefPtr      ->SetValue(iter, SharedStr(optSqlStrings ->GetIndexedValue(optSqlStringsIsParam  ? 0 : i)));
		if (optCdf)         cdfPropDefPtr            ->SetValue(iter, SharedStr(optCdf        ->GetIndexedValue(optCdfIsParam         ? 0 : i)));
		if (optUrl)         urlPropDefPtr            ->SetValue(iter, SharedStr(optUrl        ->GetIndexedValue(optUrlIsParam         ? 0 : i)));
	}
	resultHolder->SetIsInstantiated();
	return true;
}

static arg_index CalcNrArgs(field_spec fs)
{
	arg_index nrParams = 1;
	if (fs & FS_EXPR)        nrParams++;
	if (fs & FS_CHECK)       nrParams++;
	nrParams += UInt32(TemplMode(fs));
	nrParams += UInt32(DomainMode(fs));
	nrParams += UInt32(ValuesMode(fs));
	nrParams += UInt32(UnitMode(fs));
	if (fs & FS_LABEL)       nrParams++;
	if (fs & FS_DESCR)       nrParams++;
	if (fs & FS_STORAGENAME) nrParams++;
	if (fs & FS_STORAGETYPE) nrParams++;
	if (fs & FS_SQLSTRING) nrParams++;
	if (fs & FS_CDF) nrParams++;
	if (fs & FS_URL) nrParams++;
	return nrParams;
}

// *****************************************************************************
//									ForEachInd operator
// *****************************************************************************

static field_spec ScanFirstArg(const AbstrOperGroup* og, CharPtr argSpecPtr)
{
	CharPtr argSpecBegin = argSpecPtr;
	UInt32 fs = 0;
	if (!argSpecPtr || !*argSpecPtr)
		og->throwOperError("argument specification expected");

	if (*argSpecPtr == 'n') { ++argSpecPtr; }
	else goto parseEnd;
	if (*argSpecPtr == 'e') { fs |= FS_EXPR;               ++argSpecPtr; }
	if (*argSpecPtr == 'i') { fs |= FS_CHECK;              ++argSpecPtr; }
	if (*argSpecPtr == 'd') {
		fs |= FS_HASDOMAINVALUESPEC; ++argSpecPtr;
		if (*argSpecPtr == 'n') { fs |= FS_DOMAIN_AS_SUBITEM; ++argSpecPtr; }
		if (*argSpecPtr != 'v') goto parseEnd; ++argSpecPtr;
		if (*argSpecPtr == 'n') { fs |= FS_VALUES_AS_SUBITEM; ++argSpecPtr; }
	}
	if (*argSpecPtr == 'x') { fs |= FS_UNIT;               ++argSpecPtr; }
	if (*argSpecPtr == 'l') { fs |= FS_LABEL;              ++argSpecPtr; }
	if (*argSpecPtr == 'd') { fs |= FS_DESCR;              ++argSpecPtr; }
	if (*argSpecPtr == 'a') { fs |= FS_STORAGENAME;        ++argSpecPtr; }
	if (*argSpecPtr == 't') { fs |= FS_STORAGETYPE;        ++argSpecPtr; }
	if (*argSpecPtr == 'r') { fs |= FS_STORAGE_RO;         ++argSpecPtr; }
	if (*argSpecPtr == 's') { fs |= FS_SQLSTRING;          ++argSpecPtr; }
	if (*argSpecPtr == 'c') { fs |= FS_CDF;                ++argSpecPtr; }
	if (*argSpecPtr == 'u') { fs |= FS_URL;                ++argSpecPtr; }

parseEnd:
	if (*argSpecPtr)
		og->throwOperErrorF("argument specification, unexpected token(s): '%s' in %s", argSpecPtr, argSpecBegin);
	return field_spec(fs);
}

class ForEachIndOperator : public BinaryOperator
{
public:
	ForEachIndOperator(AbstrOperGroup* gr)
		:	BinaryOperator(gr, TreeItem::GetStaticClass(), SharedStrArray::GetStaticClass(), SharedStrArray::GetStaticClass())
	{}

	// Override Operator
	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		assert(args.size() >= NrSpecifiedArgs());
		SharedStr argSpec = GetValue<SharedStr>(args[0], 0);
		auto fs = ScanFirstArg(GetGroup(), argSpec.begin());
		auto nrExpectedArgs = CalcNrArgs(fs) + 1;
		if (args.size() != nrExpectedArgs)
			throwDmsErrF(
				"number of given arguments doesn't match the specification '%s': %d arguments given (including the specification), but %d expected"
				, argSpec.c_str()
				, args.size()
				, nrExpectedArgs
			);

		return ForEach_CreateResult(resultHolder, args, 1, GetGroup()->GetNameID(), fs);
	}
};

// *****************************************************************************
//									ForEach operator
// *****************************************************************************

class ForEachOperator : public VariadicOperator
{
	static void AddClassInfo(param_t pt, ClassCPtr*& argClsIter, ClassCPtr desiredClass)
	{
		if (pt == pt_excluded) return;
		if (pt == pt_contextonly)
			*argClsIter++  = desiredClass;
		else
		{
			*argClsIter++ = TreeItem::GetStaticClass(); // context for finding provided names
			*argClsIter++ = DataArray<SharedStr>::GetStaticClass();
		}
	}

public:
	ForEachOperator(AbstrOperGroup* gr, field_spec fs)	
		:	VariadicOperator(gr, TreeItem::GetStaticClass(), CalcNrArgs(fs))
		,	m_FS(fs)
	{
		assert(!(m_FS & (FS_CDF | FS_URL | FS_STORAGE_RO)));

		ClassCPtr* argClsIter = m_ArgClasses.get();

		                         *argClsIter++ = SharedStrArray::GetStaticClass();
		if (fs & FS_EXPR)        *argClsIter++ = SharedStrArray::GetStaticClass();
		if (fs & FS_CHECK)       *argClsIter++ = SharedStrArray::GetStaticClass();

		AddClassInfo(TemplMode (fs), argClsIter, TreeItem ::GetStaticClass());
		AddClassInfo(DomainMode(fs), argClsIter, AbstrUnit::GetStaticClass());
		AddClassInfo(ValuesMode(fs), argClsIter, AbstrUnit::GetStaticClass());
		AddClassInfo(UnitMode  (fs), argClsIter, AbstrUnit::GetStaticClass());

		if (fs & FS_LABEL)       *argClsIter++ = SharedStrArray::GetStaticClass();
		if (fs & FS_DESCR)       *argClsIter++ = SharedStrArray::GetStaticClass();
		if (fs & FS_STORAGENAME) *argClsIter++ = SharedStrArray::GetStaticClass();
		if (fs & FS_STORAGETYPE) *argClsIter++ = SharedStrArray::GetStaticClass();
		if (fs & FS_SQLSTRING)   *argClsIter++ = SharedStrArray::GetStaticClass();
		if (fs & FS_CDF      )   *argClsIter++ = SharedStrArray::GetStaticClass();
		if (fs & FS_URL      )   *argClsIter++ = SharedStrArray::GetStaticClass();
		dms_assert(m_ArgClassesEnd == argClsIter);
	}

	// Override Operator
	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		dms_assert(args.size() == NrSpecifiedArgs());
		dms_assert(!mustCalc);

/* NYI: add obsolete warnings, once argument dependent argument policy determination is implemented.
		if (m_FS & (FS_STORAGENAME | FS_STORAGETYPE | FS_SQLSTRING | FS_DESCR | FS_LABEL))
		{
			reportF(ST_Warning, 
				"use of obsolete %s, replace by for_each_ind, see: http://www.objectvision.nl/geodms/operators-a-functions/metascript/for-each-indirect", 
				GetGroup()->GetName()
			);
		}
*/
		return ForEach_CreateResult(resultHolder, args, 0, GetGroup()->GetNameID(), m_FS);
	}

private:
	field_spec m_FS;
};

// *****************************************************************************
//											INSTANTIATION
// *****************************************************************************

oper_arg_policy CalcArgPolicy(arg_index argNr, field_spec fs)
{
	dms_assert(argNr < 11);
	if (argNr--)                        // ! NAME
		if (!(fs & FS_EXPR) || argNr--)  // ! EXPR
			if (!(fs & FS_CHECK) || argNr--)  // ! CHECK
				if (fs & FS_HASDOMAINVALUESPEC)
				{
					if (!argNr--)
						return
						(fs & FS_DOMAIN_AS_SUBITEM)
						? oper_arg_policy::calc_never // Domain Unit Container
						: oper_arg_policy::calc_never;               // Domain Unit
					if (!(fs & FS_DOMAIN_AS_SUBITEM) || argNr--) // ! DOMAIN NAME
						if (!argNr--)
							return
							(fs & FS_VALUES_AS_SUBITEM)
							? oper_arg_policy::calc_never // Values Unit Container
							: oper_arg_policy::calc_never;               // Values Unit 
				}
				else if (!argNr)
				{
					if (fs & FS_TEMPL_AS_SUBITEM) return oper_arg_policy::calc_never;
					if (fs & FS_UNIT_AS_SUBITEM) return oper_arg_policy::calc_never;
					if (fs & FS_TEMPLATE) return oper_arg_policy::is_templ;
					if (fs & FS_UNIT) return oper_arg_policy::calc_never;
				}
	return oper_arg_policy::calc_always;
}



struct ForEachIndOperGroup : AbstrOperGroup
{
	ForEachIndOperGroup()
		:	AbstrOperGroup("for_each_ind", 
				oper_policy::dont_cache_result
			|	oper_policy::dynamic_argument_policies
			|	oper_policy::allow_extra_args
			|	oper_policy::has_tree_args
			|	oper_policy::has_template_arg
			)
	{}

	oper_arg_policy GetArgPolicy(arg_index argNr, CharPtr firstArgValue) const override
	{
		dms_assert(firstArgValue || !argNr);
		if (!argNr)
			return oper_arg_policy::calc_always;
		dms_assert(firstArgValue);
		auto fs = ScanFirstArg(this, firstArgValue);

		return CalcArgPolicy(argNr-1, fs);
	}
};

struct ForEachOperGroup : AbstrOperGroup
{
	ForEachOperGroup(TokenID id, field_spec fs)
		:	AbstrOperGroup(
				id, 
						oper_policy::dont_cache_result 
					|	oper_policy::calc_requires_metainfo
					|	(	(TemplMode(fs) == pt_contextonly)
							?	oper_policy::has_template_arg
							:	oper_policy()
						)
					|	(	(fs & (FS_HASDOMAINVALUESPEC|FS_TEMPLATE|FS_TEMPL_AS_SUBITEM|FS_UNIT_AS_SUBITEM) )
							?	oper_policy::has_tree_args
							:	oper_policy()
						)
			)
		,	m_FS(fs)
	{}

	oper_arg_policy GetArgPolicy(arg_index argNr, CharPtr firstArgValue) const override
	{
		assert(firstArgValue == nullptr || *firstArgValue == char(0));
		return CalcArgPolicy(argNr, m_FS);
	}

private:
	field_spec m_FS;
};

namespace 
{
	struct FE_SpecName
	{
		static void NameComponent(char*& bufPtr, param_t pt, char base)
		{
			if (pt != pt_excluded)
			{
				*bufPtr++ = base;
				if (pt != pt_contextonly)
					*bufPtr++ = 'n';
			}
		}

		static TokenID CreateName(field_spec fs)
		{
			dms_assert(IsMainThread());

			const int BUFSIZE = 10 + 9 + 2*4;
			static char buffer[ BUFSIZE ] = "for_each_n";
			char* bufPtr = buffer + 10; // point directly after "for_each_n";
			if (fs & FS_EXPR)   *bufPtr++ = 'e';
			if (fs & FS_CHECK)  *bufPtr++ = 'i';

			NameComponent(bufPtr, TemplMode (fs), 't');
			NameComponent(bufPtr, DomainMode(fs), 'd');
			NameComponent(bufPtr, ValuesMode(fs), 'v');
			NameComponent(bufPtr, UnitMode  (fs), 'x');

			if (fs & FS_LABEL      ) *bufPtr++ = 'l';
			if (fs & FS_DESCR      ) *bufPtr++ = 'd';
			if (fs & FS_STORAGENAME) *bufPtr++ = 'a';
			if (fs & FS_STORAGETYPE) *bufPtr++ = 't';
			if (fs & FS_SQLSTRING  ) *bufPtr++ = 's';
			if (fs & FS_CDF        ) *bufPtr++ = 'c';
			if (fs & FS_URL        ) *bufPtr++ = 'u';

			dms_assert(bufPtr <= buffer + BUFSIZE);
			return GetTokenID_st(buffer, bufPtr);
		}

		FE_SpecName(field_spec fs)
			:	m_Group(CreateName(fs),	fs)
			,	m_Oper(&m_Group, fs)
		{}

		ForEachOperGroup m_Group;
		ForEachOperator  m_Oper;
	};

	struct FE_dl
	{
		FE_dl(field_spec fs)
			:	m_FE   (fs)          
			,	m_FE_d (fs|FS_DESCR)
			,	m_FE_l (fs|FS_LABEL)         
			,	m_FE_ld(fs|FS_LABEL|FS_DESCR)
		{}

		FE_SpecName m_FE, m_FE_d, m_FE_l, m_FE_ld;
	};

	struct FE_ec
	{
		FE_ec(field_spec fs)
			:	m_FE   (fs)
			,	m_FE_e (fs|FS_EXPR)
			,	m_FE_c (fs|FS_CHECK)
			,	m_FE_ec(fs|FS_EXPR|FS_CHECK)
		{}

		FE_dl
			m_FE, m_FE_e, m_FE_c, m_FE_ec;
	};

	struct FE_at
	{
		FE_at(field_spec fs)
			:	m_FE   (fs)
			,	m_FE_a (fs|FS_STORAGENAME)
			,	m_FE_at(fs|FS_STORAGENAME|FS_STORAGETYPE)
		{}

		FE_ec
			m_FE, 
			m_FE_a,
			m_FE_at;
	};

	struct FE_tn
	{
		FE_tn(field_spec fs)
			:	m_FE     (fs)
			,	m_FE_t   (fs|FS_TEMPLATE)
			,	m_FE_tn  (fs|FS_TEMPL_AS_SUBITEM)
			,	m_FE_u   (fs|FS_UNIT)
			,	m_FE_un  (fs|FS_UNIT_AS_SUBITEM)
			,	m_FE_dv  (fs|FS_HASDOMAINVALUESPEC)
			,	m_FE_dnv (fs|FS_HASDOMAINVALUESPEC|FS_DOMAIN_AS_SUBITEM)
			,	m_FE_dvn (fs|FS_HASDOMAINVALUESPEC|FS_VALUES_AS_SUBITEM)
			,	m_FE_dnvn(fs|FS_HASDOMAINVALUESPEC|FS_DOMAIN_AS_SUBITEM|FS_VALUES_AS_SUBITEM)
		{}

		FE_at
			m_FE, 
			m_FE_t, 
			m_FE_tn,
			m_FE_u,
			m_FE_un,
			m_FE_dv,
			m_FE_dnv,
			m_FE_dvn,
			m_FE_dnvn;
	};

	struct FE_s
	{
		FE_s(field_spec fs = field_spec())
			:	m_FE(fs)
			,	m_FE_s(fs|FS_SQLSTRING) {}

		FE_tn
			m_FE, 
			m_FE_s;
	};

	ForEachIndOperGroup sopFEI;
	ForEachIndOperator feInd(&sopFEI);

	FE_s s_AllNamedForEachOperators;

/*  MAKE THIS LINE A COMMENT_LINE TO GET THE SIZE OF FE_cu REPORTED

	template <int N> struct Array {};
	void sayit(Array<1>& x);
	void sayit()
	{
		Array<sizeof(FE_cu)> x;
		sayit(x);
	}

//*/
}

/******************************************************************************/

