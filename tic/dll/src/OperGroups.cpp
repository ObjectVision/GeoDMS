// Copyright (C) 1998-2024 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "TicPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

#include "OperGroups.h"
#include "Operator.h"

#include "act/TriggerOperator.h"
#include "dbg/DebugCast.h"
#include "dbg/DebugContext.h"
#include "dbg/SeverityType.h"
#include "mci/Class.h"
#include "ptr/LifetimeProtector.h"
#include "set/VectorFunc.h"
#include "set/VectorMap.h"
#include "utl/mySPrintF.h"
#include "xct/DmsException.h"

#include "AbstrDataItem.h"
#include "AbstrUnit.h"
#include "DataLocks.h"
#include "DataStoreManager.h"
#include "ItemLocks.h"
#include "OperationContext.h"
#include "SessionData.h"
#include "TicInterface.h"


// *****************************************************************************
// Section:     OperGroupReg
// Purpose:     Binary search for OperatorGroup
// *****************************************************************************

namespace AbstrOperGroupRegImpl {

	const UInt32 REG_SIZE = 4000;

	typedef std::vector<AbstrOperGroup*> RegType;
	typedef RegType::const_iterator     const_iterator;

	RegType* s_Reg               = nullptr;
	bool     s_OperGroupRegDirty = false;

	struct OperGroupCompare {
		bool operator()(const AbstrOperGroup* a, const AbstrOperGroup* b) const
		{
			dms_assert(a && b);
			return a->GetNameID() < b->GetNameID();
		}
		bool operator()(const AbstrOperGroup* a, TokenID bID) const
		{
			dms_assert(a);
			return a->GetNameID() < bID;
		}
	};

	void UpdateOperatorGroupReg()
	{
		dms_check_not_debugonly;
		assert(IsMetaThread());

		if (s_OperGroupRegDirty)
		{
			if (s_Reg != nullptr)
			{
				assert(s_Reg->size() <= REG_SIZE);
				assert(s_Reg->size() > REG_SIZE - 1000);

				auto b = s_Reg->begin(), e = s_Reg->end();
				for (auto i = b; i != e; ++i)
					(*i)->UpdateNameID();
				std::stable_sort(b, e, OperGroupCompare());
				auto check = check_order(b, e, OperGroupCompare());
				if (!check.first)
					throwDmsErrF("OperatorGroupReg has unordered elements: %s", (*check.second)->GetNameStr());
			}
			s_OperGroupRegDirty = false;
		}
	}

	RegType& GetReg()
	{
		if (s_Reg == 0)
		{
			s_Reg = new RegType;
			s_Reg->reserve(REG_SIZE);
		}
		s_OperGroupRegDirty = true;
		return *s_Reg;
	}

	const RegType& GetSortedReg()
	{
		dms_assert(s_Reg);
		UpdateOperatorGroupReg();
		return *s_Reg;
	}

	const_iterator  FindLB(TokenID operID)
	{
		Operator* checkedVersion  =0;
		Operator* uncheckedVersion=0;

		AbstrOperGroupRegImpl::const_iterator
			b = GetSortedReg().begin(),
			e = GetSortedReg().end();

		OperGroupCompare comp;

		UInt32 n = e - b;
		while (n)
		{
			UInt32 n2 = n / 2;
			AbstrOperGroupRegImpl::const_iterator
				m = b + n2;
			dms_assert(*m);
			if (comp(*m, operID))
				b = ++m, n -= n2 + 1;
			else
				n = n2; 
		}
		return b;
	}

	TemplOperGroup theTemplGroup;

} // end local namespace 

// *****************************************************************************
// Section:     OperatorGroup
// *****************************************************************************

AbstrOperGroup::AbstrOperGroup(CharPtr operName, oper_policy op)
	:	m_OperName(operName)
	,	m_OperNameID(GetTokenID_st(operName))
//	,	m_FirstMember(0) assume static initialization and possible usage before construction
	,	m_Policy(op)
{
	Init();
}

AbstrOperGroup::AbstrOperGroup(SharedStr operName, oper_policy op)
	: m_OperName(operName)
	, m_OperNameID(GetTokenID_st(operName))
//	,	m_FirstMember(0) assume static initialization and possible usage before construction
	, m_Policy(op)
{
	Init();
}

AbstrOperGroup::AbstrOperGroup(TokenID nameID, oper_policy op)
	:	m_OperName(nameID.str_range_st())
	,	m_OperNameID(nameID)
//	,	m_FirstMember(0) assume static initialization and possible usage before construction
	,	m_Policy(op)
{
	Init();
}

AbstrOperGroup::~AbstrOperGroup()
{
	dms_assert(AbstrOperGroupRegImpl::s_Reg);
	vector_erase(*AbstrOperGroupRegImpl::s_Reg, this);
	if (AbstrOperGroupRegImpl::s_Reg->size() == 0)
	{
		delete AbstrOperGroupRegImpl::s_Reg;
		AbstrOperGroupRegImpl::s_Reg = 0;
	}
}

void AbstrOperGroup::Init()
{
	AbstrOperGroupRegImpl::GetReg().push_back(this);
/*
#if defined(MG_DEBUG_SPECIAL_OPERATORS)
	auto op = m_Policy;
	if (op != oper_policy::none)
	{
		// TODO WIKI
		reportF(SeverityTypeID::ST_MinorTrace, "OperGroup %s %s %s %s %s %s %s %s %s %s %s %s %s %s %s "
			, m_OperName.c_str()
			, op & oper_policy::dont_cache_result ? "dont_cache_result" : ""
			, op & oper_policy::existing ? "existing" : ""
			, op & oper_policy::has_external_effects ? "has_external_effects" : ""
			, op & oper_policy::allow_extra_args ? "allow_extra_args" : ""
			, op & oper_policy::is_template_call ? "is_template_call" : ""
			, op & oper_policy::has_template_arg ? "has_template_arg" : ""
			, op & oper_policy::has_tree_args ? "has_tree_args" : ""
			, op & oper_policy::is_transient ? "is_transient" : ""
			, op & oper_policy::dynamic_argument_policies ? "dynamic argument policies" : ""
			, op & oper_policy::dynamic_result_class ? "dynamic_result_class" : ""
			, op & oper_policy::calc_requires_metainfo ? "calc_requires_metainfo" : ""
			, op & oper_policy::is_dynamic_group ? "is_dynamic_group" : ""
			, op & oper_policy::can_explain_value ? "can_explain_value" : ""
			, op & oper_policy::obsolete ? "obsolete" : ""
		);
	}
#endif
*/
}

void AbstrOperGroup::Release() const
{
	if (!DecRef()) 
	{
		if (m_Policy & oper_policy::is_dynamic_group)
			delete this;	
	}
}

void AbstrOperGroup::UpdateNameID()
{
	if (!m_OperNameID)
		m_OperNameID = GetTokenID_mt(m_OperName.cbegin(), m_OperName.csend());
}

void AbstrOperGroup::Register(Operator* member)
{
	dms_assert(member && (member->m_Group == this));
	member->m_NextGroupMember = m_FirstMember;
	m_FirstMember = member;
}

void AbstrOperGroup::UnRegister(Operator* member)
{
	dms_assert(member);
	// remove from linked list of operatorgroup
	const Operator** opPtr = &m_FirstMember;
	while (*opPtr != member)
	{
		if (*opPtr == 0) return; // unexpected end of list? 
		// can happen in debug since dtors of CliendDefinedOper AND Operator UnRegister; in release only the former

		opPtr = &((*opPtr)->m_NextGroupMember);
	}
	*opPtr = member->GetNextGroupMember(); // actual removal;

	// if group is empty; it was probably dynamically allocated and thus we destroy it
	if (!m_FirstMember)
		delete this;
}

AbstrOperGroup* AbstrOperGroup::FindOrCreateCOG(TokenID operID)
// calls FindLB, can be fed to Register(); must be matched with Unregister
{
	dms_assert(IsMainThread());

	AbstrOperGroupRegImpl::const_iterator result = AbstrOperGroupRegImpl::FindLB(operID);
	if	(result != AbstrOperGroupRegImpl::GetSortedReg().end() && (*result)->GetNameID() == operID)
		return *result;

	CommonOperGroup* cog = new CommonOperGroup(operID);
	cog->m_FirstMember = nullptr; // take care of non-static initialization
	cog->m_Policy = oper_policy(cog->m_Policy | oper_policy::is_dynamic_group);
	return cog;
}

const AbstrOperGroup* AbstrOperGroup::FindName(TokenID operID)
// FindLB(operID, 0), but return theTemplGroup if incorrect operID
{
	AbstrOperGroupRegImpl::const_iterator result = AbstrOperGroupRegImpl::FindLB(operID);
	if	(result != AbstrOperGroupRegImpl::GetSortedReg().end() && (*result)->GetNameID() == operID)
		return *result;

	return &AbstrOperGroupRegImpl::theTemplGroup;
}

UInt32 AbstrOperGroup::GetNrOperatorGroups()
{
	return (AbstrOperGroupRegImpl::s_Reg)
		?	AbstrOperGroupRegImpl::s_Reg->size()
		:	0;
}

AbstrOperGroup* AbstrOperGroup::GetOperatorGroup(og_index i)
{
	dms_assert(i < GetNrOperatorGroups());
	return AbstrOperGroupRegImpl::GetSortedReg()[i];
}

// class CommonOperGroup: public AbstrOperGroup

CommonOperGroup::CommonOperGroup(CharPtr operName, oper_policy op)
	: AbstrOperGroup(operName, op)
{}

CommonOperGroup::CommonOperGroup(TokenID operNameID, oper_policy op)
	: AbstrOperGroup(operNameID, op)
{}

oper_arg_policy CommonOperGroup::GetArgPolicy(arg_index argNr, CharPtr firstArgValue) const
{ 
	dms_assert(firstArgValue == nullptr || !*firstArgValue);
	return oper_arg_policy::calc_as_result;
}


// class SpecialOperGroup: public AbstrOperGroup

SpecialOperGroup::SpecialOperGroup(TokenID operNameID, arg_index maxNrArgs, oper_arg_policy* argPolicyArray, oper_policy op)
	: AbstrOperGroup(operNameID, op)
	, m_MaxNrArgs(maxNrArgs)
	, m_ArgPolicyArray(argPolicyArray)
{
	DetermineOperPolicy();
#if defined(MG_DEBUG_SPECIAL_OPERATORS)
	CharPtr calcability[7] = { "never", "as_result", "subitem_root", "always", "template", "supply_tree", "calc_at_subitem"};

	if (op != oper_policy::none)
	{
		// TODO WIKI
		auto operNameStr = SharedStr(operNameID.str_range_st());
		DBG_START("SpecialOperGroup", operNameStr.c_str(), false);
		for (auto i = 0; i != maxNrArgs; ++i)
		{
			auto oap = argPolicyArray[i];
			dms_assert(oap <= oper_arg_policy::calc_at_subitem);
			DBG_TRACE(("arg %d calc %s", i, calcability[int(oap)]));
		}
	}
#endif
}

bool IsTreeArg(oper_arg_policy oap)
{
	return oap == oper_arg_policy::is_templ || oap == oper_arg_policy::subst_with_subitems || oap == oper_arg_policy::calc_subitem_root;
}

void SpecialOperGroup::DetermineOperPolicy()
{
	dms_assert(MustCacheResult() || !CanResultToConfigItem());
	// set m_HasSupplTreeArg to true if any arg has the oap_suppl_tree policy;
	// destroys the maxNrArgs value and the argPolicyArray pointer
	oper_arg_policy* argPolicyPtr = m_ArgPolicyArray;
	oper_arg_policy* argPolicyEnd = argPolicyPtr + m_MaxNrArgs;
	for (; argPolicyPtr != argPolicyEnd; ++argPolicyPtr)
	{
		if (IsTreeArg(* argPolicyPtr))
		{
			m_Policy = m_Policy | oper_policy::has_tree_args;
			break;
		}
	}
}

oper_arg_policy SpecialOperGroup::GetArgPolicy(arg_index argNr, CharPtr firstArgValue) const
{ 
	dms_assert(firstArgValue == nullptr || *firstArgValue == char(0));
	dms_assert(argNr < m_MaxNrArgs || AllowExtraArgs()); 
	if (argNr <  m_MaxNrArgs)
		return m_ArgPolicyArray[argNr]; 
	return oper_arg_policy::calc_never;
}

TemplOperGroup::TemplOperGroup()
	:	AbstrOperGroup(TokenID::GetEmptyID(), oper_policy(oper_policy::dont_cache_result|oper_policy::is_template_call))
{}

oper_arg_policy TemplOperGroup::GetArgPolicy(arg_index argNr, CharPtr firstArgValue) const
{
	assert(firstArgValue == nullptr || *firstArgValue == char(0));
	return oper_arg_policy::calc_never;
}


SharedStr GenerateArgClsDescription(arg_index nrArgs, const ClassCPtr* args)
{
	int c=0;
	SharedStr msg;

	while (nrArgs--)
	{
		msg += mySSPrintF(
			"arg%d of type %s\n", 
				++c, 
				(*args)->GetName().c_str()
		);
		++args;
	}

	return msg;
}

UInt32 AbstrOperGroup::GetNrMembers() const
{
	UInt32 count = 0;
	for (auto b = GetFirstMember(); b; b = b->GetNextGroupMember())
		++count;

	return count;
}

const Operator* AbstrOperGroup::FindOper(arg_index nrArgs, const ClassCPtr* argTypes) const
{
	dms_assert(!IsTemplateCall());

	auto b = GetFirstMember();

	if (!b)
	{
		auto nameStr = SharedStr(GetName());
		throwDmsErrF("There is no implemented operator for operator name '%s'. This operator name name might have been reserved for future implementation or might be obsolete."
			, nameStr);
	}

	auto best_oper = b; arg_index best_count = 0;
	UInt32 nr_best_match = 0;

	for (; b; b = b->GetNextGroupMember())
	{
		assert(b->m_Group == this);
		arg_index nrSpecifiedArgs = b->NrSpecifiedArgs();
		if (nrSpecifiedArgs > nrArgs)
		{
			// allow for skipped trailing args of select_xxx that are processed as lispExpr only for now 
			if (!MustCacheResult())
				while (nrSpecifiedArgs > nrArgs && GetArgPolicy(nrSpecifiedArgs - 1, nullptr) == oper_arg_policy::calc_as_result)
					--nrSpecifiedArgs;
//			if (nrArgs < nrSpecifiedArgs - b->NrOptionalArgs())
//				continue;
//			nrSpecifiedArgs = nrArgs;
		}

		const ClassCPtr* requiredTypePtr = b->m_ArgClassesBegin;
		const ClassCPtr* givenTypePtr    = argTypes;

		arg_index match_count = 0, nrRequiredArgs = nrSpecifiedArgs - b->NrOptionalArgs();
		for (arg_index i=0, ie = nrSpecifiedArgs; i!= ie; ++i, ++requiredTypePtr, ++givenTypePtr)
		{
			if (i >= nrArgs)
			{
				if (i >= nrRequiredArgs)
					break;
				goto nextGroupMember;
			}
			else if (!(*givenTypePtr)->IsDerivedFrom(*requiredTypePtr))
				goto nextGroupMember;

			++match_count;
			if (match_count > best_count)
			{
				best_count = match_count;
				best_oper = b;
				nr_best_match = 1;
			}
			else if (match_count == best_count)
				++nr_best_match;
		}
		if (nrSpecifiedArgs >= nrArgs || AllowExtraArgs())
			return b;
	nextGroupMember:;
	}
	auto nameStr = SharedStr(GetName());
	throwErrorF(nameStr.c_str(), "Cannot find operator for these arguments:\n"
		"%s"
		"Possible cause: argument type mismatch. Check the types of the used arguments.\n"
		"\nThere are %d operators registered for the %s operator-group."
		"\n%d operator%s correspond%s with these arguments for the first %d argument%s, %s the following signature:\n"
		"%s%s%s"
		, GenerateArgClsDescription(nrArgs, argTypes).c_str()
		, GetNrMembers(), nameStr.c_str()
		, nr_best_match, (nr_best_match==1 ? "": "s"), (nr_best_match == 1 ? "s" : ""), best_count, (best_count == 1 ? "" : "s"), (nr_best_match == 1 ? "with" : "of which the first operator has")
		, GenerateArgClsDescription(best_oper->NrSpecifiedArgs(), best_oper->m_ArgClassesBegin).c_str()
		, AllowExtraArgs() ? "\nand supplemental args" : ""
		, HasAnnotation() ? mySSPrintF("\n\n%s", GetAnnotation()).c_str() : ""
	);

	return nullptr;
}

const Operator* AbstrOperGroup::FindOperByArgs(const ArgRefs& args) const // REMOVE, OBSOLETE?, NOW IN FuncDC::GetOperator? RELOCATE check on tile support
{
	arg_index n = args.size();
	if (!n)
		return FindOper(0, 0);

	std::vector<const Class*> operandTypeSeq; operandTypeSeq.reserve(n);

	for (arg_index i = 0; i<n; ++i)
	{
		const TreeItem* arg = GetItem(args[i]);
		operandTypeSeq.push_back( arg->GetDynamicObjClass() );
	}

	dms_assert( operandTypeSeq.size() == args.size() );
	const Operator* result = FindOper(args.size(), begin_ptr( operandTypeSeq ));
	dms_assert(result);
	return result;
}


// CreateValuesUnit() wordt alleen (deels indirect) gebruikt door operated_unit_creator
// met cog_div, cog_mul, en cog_sqrt

ConstUnitRef AbstrOperGroup::CreateValuesUnit(const ArgSeqType& dataArgs) const
{
	dms_assert(IsMetaThread());

	ArgRefs unitSeq;

	arg_index nrDataArgs = dataArgs.size();
	unitSeq.reserve(nrDataArgs);

	TimeStamp ts = UpdateMarker::tsBereshit;
	for (arg_index i=0; i!=nrDataArgs; i++)
	{
		const AbstrUnit* unit_i;
		const TreeItem* ti = dataArgs[i];
		if (IsDataItem(ti))
		{
			const AbstrDataItem* di = AsDataItem(ti);
			dms_assert(di);
			unit_i = di->GetAbstrValuesUnit();
		}
		else
		{
			dms_assert(IsUnit(ti));			
			unit_i = AsUnit(ti);
		}
		dms_assert(unit_i);
		unitSeq.emplace_back(std::in_place_type<SharedTreeItem>, unit_i);
		MakeMax(ts, unit_i->GetLastChangeTS());
	}

	LifetimeProtector<TreeItemDualRef> resultRef;
	resultRef->MarkTS(ts);
	UpdateMarker::ChangeSourceLock changeStamp(&*resultRef, "CreateValuesUnit");

	const Operator* oper = FindOperByArgs(unitSeq);
	MG_CHECK(oper);

	oper->CreateResultCaller(*resultRef, unitSeq, nullptr, LispPtr());

	dms_assert(resultRef->GetOld());
	ConstUnitRef u = debug_cast<const AbstrUnit*>(resultRef->GetOld());
	dms_assert(u);
	return u;
}

#include <stdarg.h>
#include "utl/mySPrintF.h"

[[noreturn]] void AbstrOperGroup::throwOperError(CharPtr msg) const
{
	auto errMsg = mySSPrintF("Operator %s", GetName().c_str());
	throwErrorD(errMsg.c_str(), msg);
}

// *****************************************************************************
// shared OperGroups
// *****************************************************************************

#include "LispTreeType.h"

CommonOperGroup
	cog_mul(token::mul),
	cog_div(token::div),
	cog_add(token::add), // used for addition and string concatenation
	cog_sub(token::sub),
	cog_bitand("bitand"),
	cog_bitor("bitor"),
	cog_bitxor("bitxor"),
	cog_pow("pow"),
	cog_eq(token::eq),
	cog_ne(token::ne),
	cog_substr("substr");


