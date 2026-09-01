// Copyright (C) 1998-2024 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

#if !defined(__TIC_OPERGROUPS_H)
#define __TIC_OPERGROUPS_H

#include "TicBase.h"
#include "utl/StrFormat.h" // mgFormat2SharedStr

#include "OperArgPolicy.h"
#include "set/StackUtil.h"
#include "ptr/SharedStr.h"

#include <memory>
#include <variant>
#include <optional>

#if defined MG_DEBUG
#define MG_DEBUG_SPECIAL_OPERATORS
#endif

// *****************************************************************************
// ArgRefs
// *****************************************************************************

using calc_time_t = double;

// template <typename ExpectedType> using expected = std::variant<ExpectedType, ErrMsg>;
using ArgRef = std::variant<FutureData, SharedTreeItem>;
using ArgRefs = std::vector<ArgRef>;
// using EArgInterest=  expected<ArgInterest>;
using OArgRefs = std::optional<ArgRefs>;

using FutureSuppliers = std::vector<FutureData>;

TIC_CALL const TreeItem* GetItem(const ArgRef& ar);
const Actor* GetStatusActor(const ArgRef& ar); // TreeItem args are now Actors (no longer SharedActor)
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

struct AbstrOperGroup
{
	TIC_CALL AbstrOperGroup(CharPtr   operName, oper_policy op);
	TIC_CALL AbstrOperGroup(SharedStr operName, oper_policy op);
	TIC_CALL AbstrOperGroup(TokenID nameID, oper_policy op);
	TIC_CALL virtual ~AbstrOperGroup(); 

	TIC_CALL static const AbstrOperGroup* FindName  (TokenID operID);  // FindLB(operID, 0), but return theTemplGroup if incorrect operID

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
	bool IsDepreciated        () const { return m_Policy & oper_policy::depreciated; }
	bool IsObsolete           () const { return m_Policy & oper_policy::obsolete; }
	bool HasAnnotation        () const { return m_Policy & oper_policy::has_annotation; }
	bool IsBetterNotInMetaScripting() const { return m_Policy & oper_policy::better_not_in_meta_scripting; }
	bool AllowsAsFunctionResult() const { return m_Policy & oper_policy::allow_as_function_result; }

	auto GetCalcFactor        () const { return m_CalcFactor; }

	void SetCanExplainValue() { m_Policy = 	oper_policy(m_Policy | oper_policy::can_explain_value); }
	void SetBetterNotInMetaScripting () { m_Policy = oper_policy(m_Policy | oper_policy::better_not_in_meta_scripting); }

	virtual oper_arg_policy GetArgPolicy(arg_index argNr, CharPtr firstArgValue) const =0;
	virtual CharPtr GetObsoleteMsg() const { return "NO OBSOLETE MSG PROVIDED"; }
	virtual CharPtr GetAnnotation() const { return ""; }

	bool MaySubstArg(arg_index argNr, CharPtr firstArgValue) const 
	{ 
		auto oap = GetArgPolicy(argNr, firstArgValue); 
		return oap != oper_arg_policy::is_templ && oap != oper_arg_policy::calc_never && oap != oper_arg_policy::calc_at_subitem;
	}
	bool MustSupplyTree(arg_index argNr, CharPtr firstArgValue) const { return GetArgPolicy(argNr, firstArgValue) == oper_arg_policy::subst_with_subitems; }
	bool IsSubItemRoot (arg_index argNr, CharPtr firstArgValue) const { return GetArgPolicy(argNr, firstArgValue) == oper_arg_policy::calc_subitem_root; }
	bool IsArgTempl (arg_index argNr, CharPtr firstArgValue) const { return GetArgPolicy(argNr, firstArgValue) == oper_arg_policy::is_templ; }

	TokenID   GetNameID()            const { return m_OperNameID; }
	// GetName() shares m_OperName (a refcount bump, no allocation); GetNameLock() hands out a
	// TokenStr, which HOLDS THE TOKEN REGISTRY until it dies -- see mci/Object.h and issue #1227.
	SharedStr GetName()              const { return SharedStr(m_OperName); }
	TokenStr  GetNameLock()          const { return GetTokenStrLock(GetNameID()); }
	CharPtr   GetNameStr()           const { return m_OperName.c_str(); }
	const Operator* GetFirstMember() const { return m_FirstMember; }
	UInt32    GetNrMembers() const;

	void UpdateNameID();

	const Operator* FindOper      (arg_index nrArgs, const ClassCPtr* argType) const;
	TIC_CALL const Operator* FindOperByArgs(const ArgRefs& args) const;

	// operator-signature descriptions (batch 0, OperSignature.h): the lazily built,
	// per-group merge of the members' DescribeSignature records. nullptr when no
	// member describes itself -- consumers then defer, as before the description
	// layer existed. Register() invalidates the cache (late-loading DLLs).
	TIC_CALL const struct OperGroupSignatures* GetSignatures() const;

	// arity-aware head dispatch (§5.14 successor): false ONLY when no member's arity
	// range can cover nrArgs -- FindOper is then guaranteed to throw, and a same-named
	// function (scope/prelude) may serve the foreign arity. Conservative: non-caching
	// groups (fluid effective arity: the trailing calc_as_result drop) always accept;
	// allow_extra_args groups accept any count >= their minimum required.
	bool AcceptsArity(arg_index nrArgs) const;
	// the cached member-arity envelope [minRequired, maxSpecified]; false when the
	// group effectively accepts everything (non-caching). allow_extra_args groups
	// report maxSpecified = the arg_index maximum.
	TIC_CALL bool GetArityEnvelope(arg_index& minRequired, arg_index& maxSpecified) const;

	[[noreturn]] TIC_CALL void throwOperError (CharPtr msg) const;
	template <class ...Args>
	[[noreturn]] void throwOperErrorF(CharPtr fmt, Args&& ...args) const { throwOperError(mgFormat2SharedStr(fmt, std::forward<Args>(args)...).c_str()); }

	TIC_CALL ConstUnitRef CreateValuesUnit(const ArgSeqType& dataArgs) const;

protected: // friend Operator
	void Register  (Operator* member); friend class  Operator;

private:
	void Init();

	SharedStr       m_OperName;
	TokenID         m_OperNameID;
	const Operator* m_FirstMember;
	mutable arg_index m_MinReqArgs = 0, m_MaxSpecArgs = 0; // cached member-arity envelope
	mutable bool      m_ArityEnvelopeValid = false;        // invalidated by Register (late-loading DLLs)
	mutable bool      m_SignaturesValid = false;           // idem, for the signature cache
	mutable std::unique_ptr<const struct OperGroupSignatures> m_Signatures;

protected:
	oper_policy     m_Policy;
	calc_time_t     m_CalcFactor = 1.0;
};

struct CommonOperGroup: AbstrOperGroup
{
	TIC_CALL CommonOperGroup(CharPtr operName  , oper_policy op = oper_policy::none);
 	TIC_CALL CommonOperGroup(TokenID operNameID, oper_policy op = oper_policy::none);

	TIC_CALL oper_arg_policy GetArgPolicy(arg_index argNr, CharPtr firstArgValue) const override;
};

// *****************************************************************************
// oper groups that are used in multiple untis
// *****************************************************************************

extern TIC_CALL CommonOperGroup cog_mul;
extern TIC_CALL CommonOperGroup cog_div;
extern TIC_CALL CommonOperGroup cog_add;
extern TIC_CALL CommonOperGroup cog_sub;
extern TIC_CALL CommonOperGroup cog_bitand;
extern TIC_CALL CommonOperGroup cog_bitor;
extern TIC_CALL CommonOperGroup cog_bitxor;
extern TIC_CALL CommonOperGroup cog_pow;
extern TIC_CALL CommonOperGroup cog_eq;
extern TIC_CALL CommonOperGroup cog_ne;
extern TIC_CALL CommonOperGroup cog_substr;


// *****************************************************************************
// SpecialOperGroup
// *****************************************************************************

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
	{
		MG_CHECK(this->IsDepreciated() || this->IsObsolete());
	}
	virtual CharPtr GetObsoleteMsg() const override
	{
		return m_ObsMsg;
	}
	CharPtr m_ObsMsg;
};

template <typename OperGroupType>
struct Annotated : OperGroupType {

	template<typename ...OtherArgs>
	Annotated(CharPtr obsMsg, OtherArgs&& ...args)
		: OperGroupType(std::forward<OtherArgs>(args)...)
		, m_Annotation(obsMsg)
	{
		this->m_Policy |= oper_policy::has_annotation;
	}

	virtual CharPtr GetAnnotation() const override
	{
		return m_Annotation;
	}
	CharPtr m_Annotation;
};

#endif // __TIC_OPERGROUPS_H
