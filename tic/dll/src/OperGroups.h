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
#if !defined(__TIC_OPERGROUPS_H)
#define __TIC_OPERGROUPS_H

#include "TicBase.h"

#include "OperArgPolicy.h"
#include "set/StackUtil.h"
#include "ptr/SharedStr.h"

#include <variant>
#include <optional>

#if defined MG_DEBUG
#define MG_DEBUG_SPECIAL_OPERATORS
#endif

// *****************************************************************************
// ArgRefs
// *****************************************************************************

struct Actor;

// template <typename ExpectedType> using expected = std::variant<ExpectedType, ErrMsg>;
using ArgRef = std::variant<FutureData, SharedTreeItem>;
using ArgRefs = std::vector<ArgRef>;
// using EArgInterest=  expected<ArgInterest>;
using OArgRefs = std::optional<ArgRefs>;

using FutureSuppliers = std::vector<FutureData>;

TIC_CALL const TreeItem* GetItem(const ArgRef& ar);
TIC_CALL const Actor* GetStatusActor(const ArgRef& ar);
TIC_CALL ArgSeqType GetItems(const ArgRefs& ar);
TIC_CALL const AbstrDataItem* AsDataItem(const ArgRef& ar);

// *****************************************************************************
// Section:     OperatorsGroup
// OperatorsGroups are unique per (name, nrArgs) pair. 
// They maintain a list of available operators for that name and nrArgs
// which can be different. 
// They provide argument policies and
// contain a list of concrete opearors with that name and argument count
// to handle resolution of overloaded operators
// operators can be overloaded on argument type and 
// *****************************************************************************

struct AbstrOperGroup : SharedObj
{
	TIC_CALL AbstrOperGroup(CharPtr   operName, oper_policy op);
	TIC_CALL AbstrOperGroup(SharedStr operName, oper_policy op);
	TIC_CALL AbstrOperGroup(TokenID nameID, oper_policy op);
	TIC_CALL virtual ~AbstrOperGroup(); 
	TIC_CALL void Release() const;

	TIC_CALL static const AbstrOperGroup* FindName  (TokenID operID);  // FindLB(operID, 0), but return theTemplGroup if incorrect operID
	TIC_CALL static AbstrOperGroup* FindOrCreateCOG(TokenID operID);      // calls FindLB, can be fed to Register(); must be matched with Unregister

	TIC_CALL static og_index GetNrOperatorGroups();
	TIC_CALL static AbstrOperGroup* GetOperatorGroup(og_index i);

	bool MustCacheResult      () const { return !(m_Policy & oper_policy::dont_cache_result); }
	bool IsTransient          () const { return m_Policy & oper_policy::is_transient; }
	bool CanResultToConfigItem() const { return m_Policy & oper_policy::existing; } 
	bool IsTemplateCall       () const { return m_Policy & oper_policy::is_template_call; }
	bool HasExternalEffects   () const { return m_Policy & oper_policy::has_external_effects; }
	bool HasTemplArg          () const { return m_Policy & oper_policy::has_template_arg; }
	bool HasSupplTreeArg      () const { return m_Policy & oper_policy::has_tree_args; }
	bool AllowExtraArgs       () const { return m_Policy & oper_policy::allow_extra_args; }
	bool CanExplainValue      () const { return m_Policy & oper_policy::can_explain_value; }
	bool HasDynamicArgPolicies() const { return m_Policy & oper_policy::dynamic_argument_policies; }
	bool IsObsolete           () const { return m_Policy & oper_policy::obsolete; }

	void SetCanExplainValue() { m_Policy = 	oper_policy(m_Policy | oper_policy::can_explain_value); }

	virtual oper_arg_policy GetArgPolicy(arg_index argNr, CharPtr firstArgValue) const =0;
	virtual CharPtr GetObsoleteMsg() const { return "NOT OBSOLETE"; }

	bool MaySubstArg(arg_index argNr, CharPtr firstArgValue) const { auto oap = GetArgPolicy(argNr, firstArgValue); return oap != oper_arg_policy::is_templ && oap!=oper_arg_policy::calc_never; }
	bool MustSupplyTree(arg_index argNr, CharPtr firstArgValue) const { return GetArgPolicy(argNr, firstArgValue) == oper_arg_policy::subst_with_subitems; }
	bool IsArgTempl (arg_index argNr, CharPtr firstArgValue) const { return GetArgPolicy(argNr, firstArgValue) == oper_arg_policy::is_templ; }

	TokenID   GetNameID()            const { return m_OperNameID; }
	TokenStr  GetName()              const { return GetTokenStr(GetNameID()); }
	const Operator* GetFirstMember() const { return m_FirstMember; }

	TIC_CALL void UpdateNameID();

	const Operator* FindOper      (arg_index nrArgs, const ClassCPtr* argType) const;
	TIC_CALL const Operator* FindOperByArgs(const ArgRefs& args) const;

	[[noreturn]] TIC_CALL void throwOperError (CharPtr msg) const;
	template <class ...Args>
	[[noreturn]] void throwOperErrorF(CharPtr fmt, Args&& ...args) const { throwOperError(mgFormat2SharedStr(fmt, std::forward<Args>(args)...).c_str()); }

	TIC_CALL ConstUnitRef CreateValuesUnit(const ArgSeqType& dataArgs) const;

protected: // friend Operator
	TIC_CALL void Register  (Operator* member); friend class  Operator;
	TIC_CALL void UnRegister(Operator* member); friend struct ClientDefinedOperator;

private:
	void Init();

	SharedStr       m_OperName;
	TokenID         m_OperNameID;
	const Operator* m_FirstMember;

protected:
	oper_policy     m_Policy;
};

struct CommonOperGroup: AbstrOperGroup
{
	TIC_CALL CommonOperGroup(CharPtr operName  , oper_policy op = oper_policy::none);
 	TIC_CALL CommonOperGroup(TokenID operNameID, oper_policy op = oper_policy::none);

	TIC_CALL oper_arg_policy GetArgPolicy(arg_index argNr, CharPtr firstArgValue) const override;
};

struct SpecialOperGroup: AbstrOperGroup
{
	TIC_CALL SpecialOperGroup(TokenID operName, arg_index maxNrArgs, oper_arg_policy* argPolicyArray, oper_policy op = oper_policy());
	TIC_CALL SpecialOperGroup(CharPtr operName, arg_index maxNrArgs, oper_arg_policy* argPolicyArray, oper_policy op = oper_policy())
		: SpecialOperGroup(GetTokenID_st(operName), maxNrArgs, argPolicyArray, op)
	{}

	oper_arg_policy GetArgPolicy(arg_index argNr, CharPtr firstArgValue) const override;

private:
	void DetermineOperPolicy();

	arg_index        m_MaxNrArgs;
	oper_arg_policy* m_ArgPolicyArray;
};

struct TemplOperGroup: AbstrOperGroup
{
	TemplOperGroup();

	oper_arg_policy GetArgPolicy(arg_index argNr, CharPtr firstArgValue) const override;
};

template <typename OperGroupType>
struct Obsolete : OperGroupType {

	template<typename ...OtherArgs>
	Obsolete(CharPtr obsMsg, OtherArgs&& ...args)
		: OperGroupType(std::forward<OtherArgs>(args)...)
		, m_ObsMsg(obsMsg)
	{}
	virtual CharPtr GetObsoleteMsg() const override
	{
		return m_ObsMsg;
	}
	CharPtr m_ObsMsg;
};
#endif // __TIC_OPERGROUPS_H
