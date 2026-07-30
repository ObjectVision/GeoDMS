// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

#if !defined(__TIC_OPERATOR_H)
#define __TIC_OPERATOR_H

#include <vector>
template <typename V> class Unit;
#include "set/StackUtil.h"

#include "OperGroups.h"
#include "TreeItemDualRef.h"
#include "ptr/OwningPtrSizedArray.h"
#include "Explain.h"
#include "DataController.h"
#include "ItemLocks.h"
struct ItemReadLock;

// *****************************************************************************
// Section:     enums
// *****************************************************************************

enum ArgFlags
{
	AF1_ISPARAM     = 0x001,
	AF2_ISPARAM     = 0x010,
	AF3_ISPARAM     = 0x100,
	AF1_HASUNDEFINED= 0x002,
	AF2_HASUNDEFINED= 0x020,
	AF3_HASUNDEFINED= 0x200,
};
// *****************************************************************************
// Section:     PerformanceEstimationData
// *****************************************************************************

// estimate_confidence and materialization live in TicBase.h: units and storage managers describe
// themselves with them too. See doc/development/schedule-with-lookahead.md §4.1 and §4.4.

struct PerformanceEstimationData
{
	calc_time_t expectedCalcTime = 0;
	SizeT ioBytes = 0;               // storage traffic: what a read (or write) moves across the boundary
	SizeT inputSize = 0, inputSizePerChore = 0;
	SizeT workingMemorySize = 0, workingMemorySizePerChore = 0;
	SizeT resultingMemory = 0;       // the eventual full result volume, once every tile exists
	SizeT resultingMemoryUpperBound = 0; // sound ceiling: from a declared SizeUpperbound where present
	SizeT residentMemory = 0;        // what a ledger would charge; per regime, see materialization
	SizeT choreMemory = 0;           // one tile's worth of the result
	SizeT resultingNrElements = 0;

	UInt32 nrChores = 1;             // tiles this result is produced in
	UInt16 extraTasks = 0;
	materialization regime = materialization::eager;
	estimate_confidence confidence = estimate_confidence::assumed;
};

// *****************************************************************************
// Section:     Operator
// *****************************************************************************

class Operator
{
protected:
	TIC_CALL Operator(AbstrOperGroup* gr, ClassCPtr resultCls);
	TIC_CALL Operator(AbstrOperGroup* gr, ClassCPtr resultCls, const ClassCPtr* argClassesBegin, arg_index nrArgs);
    TIC_CALL virtual ~Operator();

	TIC_CALL virtual bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const;
public:
//	Returns FALSE in case of suspension; throw on matching failure
	virtual void CreateResultCaller(TreeItemDualRef& resultHolder, const ArgRefs& args, LispPtr metaCallArgs = LispPtr()) const
	{
		if (resultHolder && !resultHolder.IsTmp())
			return;

		dms_assert(!CanExplainValue()); // or this method should be overridden.
		auto argSeq = GetItems(args);
		bool actualResult = CreateResult(resultHolder, argSeq, false);
		dms_assert(actualResult);
		dms_assert(resultHolder);
	}

	virtual bool PreCalcUpdate(TreeItemDualRef& resultHolder, ArgRefs& args) const { return true; };

	virtual bool CalcResult(TreeItemDualRef& resultHolder, const ArgRefs& args, std::vector<ItemReadLock> readLocks, Explain::Context* context = nullptr) const
	{
		dms_assert(resultHolder);
		dms_assert(!CanExplainValue()); // or this method should be overridden.
		auto argSeq = GetItems(args);
		return CreateResult(resultHolder, argSeq, true);
	}
	// Predict this operator's cost and footprint from the result skeleton and the argument
	// meta-info, without calculating anything. Not consumed by scheduling decisions yet: P0 of
	// doc/development/schedule-with-lookahead.md only logs it against the measured actuals.
	TIC_CALL virtual auto EstimatePerformance(TreeItemDualRef& resultHolder, const ArgRefs& args) const -> PerformanceEstimationData;

	arg_index             NrSpecifiedArgs()        const { return m_ArgClassesEnd - m_ArgClassesBegin; }
	arg_index             NrOptionalArgs()         const { dms_assert(NrSpecifiedArgs() >= m_NrOptionalArgs);  return m_NrOptionalArgs; }
	ClassCPtr             GetArgClass(arg_index i) const { dms_assert(i<NrSpecifiedArgs()); return m_ArgClassesBegin[i]; }
	ClassCPtr             GetResultClass()         const { return m_ResultClass; }
	const AbstrOperGroup* GetGroup()               const { return m_Group;       }
	const Operator*       GetNextGroupMember()     const { return m_NextGroupMember; }

	TIC_CALL virtual oper_arg_policy GetArgPolicy(arg_index argNr, CharPtr firstArgValue) const;
	TIC_CALL oper_policy GetOperPolicy() const { return GetGroup()->m_Policy; }

	// Describe this operator's unit constraints declaratively (OperSignature.h).
	// Returns false (the default) when undescribed: every consumer then defers.
	// Family bases override this ONCE for all their instantiations; the description
	// must mirror the member's CreateResult (the §9 drift defenses guard the pair).
	TIC_CALL virtual bool DescribeSignature(struct AbstrSignatureBuilder& sb) const;

	// §12.7 (K13 closed-spec): describe this member's CONCRETE signature for a
	// definition-time-known value of its meta-directing argument. The walker calls
	// this when a DynamicShape member's spec argument is CLOSED over the checked
	// function's formals and was evaluated at definition scan; the emitted record
	// (no DynamicShape) carries exactly the spec-implied position count, which the
	// walker enforces as the ruled honest arity check. False makes the walker defer
	// exactly as without the spec. A THROW must be the member's own spec validation
	// (the very predicate CreateResult applies first — ParseDijkstraString/
	// CheckFlags): the walker PROPAGATES it as an honest definition-time error.
	TIC_CALL virtual bool DescribeSpecSignature(struct AbstrSignatureBuilder& sb, CharPtr specValue) const;

	// §12.7 (for_each tranche): describe the argument LAYOUT of a container-
	// GENERATING meta member (OperSignature.h MetaMemberLayout) so the walker
	// can pseudo-expand the generated member set when the meta-directing
	// arguments are CLOSED over a checked function's formals. For a group whose
	// layout is directed by its first argument's value (dynamic_argument_policies,
	// for_each_ind), the walker passes that value once closed-evaluated;
	// layout-static members ignore it. False (the default: no describable
	// container) makes the walker defer as before. A THROW must be the member's
	// own spec validation (ScanFirstArg — CreateResult's first predicate): the
	// walker PROPAGATES it as an honest definition-time error.
	TIC_CALL virtual bool DescribeMetaSignature(struct MetaMemberLayout& layout, CharPtr optSpecValue) const;

	inline bool HasRegisteredResultClass() const { return !(GetOperPolicy() & oper_policy::dynamic_result_class); }
	inline bool CalcRequiresMetaInfo()     const { return GetOperPolicy() & oper_policy::calc_requires_metainfo; }
	inline bool CanRunParallel()           const { return !(GetGroup()->HasExternalEffects() ||  CalcRequiresMetaInfo()); }
	inline bool CanExplainValue()          const { return GetGroup()->CanExplainValue(); }

protected:
	const ClassCPtr*       m_ArgClassesBegin = nullptr;
	const ClassCPtr*       m_ArgClassesEnd   = nullptr;
	ClassCPtr              m_ResultClass     = nullptr;
	arg_index              m_NrOptionalArgs  = 0;

private:
	AbstrOperGroup* m_Group;
	mutable const Operator*   m_NextGroupMember = nullptr; friend struct AbstrOperGroup;
};


class NullaryOperator : public Operator
{
public:
	NullaryOperator(AbstrOperGroup* gr, ClassCPtr resultCls)
		: Operator(gr, resultCls, 0, 0)
	{}
};

class UnaryOperator : public Operator
{
public:
	UnaryOperator(AbstrOperGroup* gr, ClassCPtr resultCls, ClassCPtr arg1Cls)
		: Operator(gr, resultCls, &m_ArgClass, 1)
	{
		m_ArgClass = arg1Cls;
	}
private:
	ClassCPtr m_ArgClass;
};

class BinaryOperator : public Operator
{
public:
	BinaryOperator(AbstrOperGroup* gr, ClassCPtr resultCls, 
		ClassCPtr arg1Cls, ClassCPtr arg2Cls
	)	: Operator(gr, resultCls, m_ArgClasses, 2)
	{
		m_ArgClasses[0] = arg1Cls;
		m_ArgClasses[1] = arg2Cls;
	}

private:
	ClassCPtr m_ArgClasses[2];
};

class TernaryOperator : public Operator
{
public:
	TernaryOperator(AbstrOperGroup* gr, ClassCPtr resultCls, 
		ClassCPtr arg1Cls, ClassCPtr arg2Cls, ClassCPtr arg3Cls)
		: Operator(gr, resultCls, m_ArgClasses, 3)
	{
		m_ArgClasses[0] = arg1Cls;
		m_ArgClasses[1] = arg2Cls;
		m_ArgClasses[2] = arg3Cls;
	}

private:
	ClassCPtr m_ArgClasses[3];
};

class QuaternaryOperator : public Operator
{
public:
	QuaternaryOperator(AbstrOperGroup* gr, ClassCPtr resultCls,
		ClassCPtr arg1Cls, ClassCPtr arg2Cls, ClassCPtr arg3Cls, ClassCPtr arg4Cls)
	:	Operator(gr, resultCls, m_ArgClasses, 4)
	{
		m_ArgClasses[0] = arg1Cls;
		m_ArgClasses[1] = arg2Cls;
		m_ArgClasses[2] = arg3Cls;
		m_ArgClasses[3] = arg4Cls;
	}

private:
	ClassCPtr m_ArgClasses[4];
};

class QuinaryOperator : public Operator
{
public:
	QuinaryOperator(AbstrOperGroup* gr, ClassCPtr resultCls,
		ClassCPtr arg1Cls, ClassCPtr arg2Cls, ClassCPtr arg3Cls, ClassCPtr arg4Cls, ClassCPtr arg5Cls)
	:	Operator(gr, resultCls, m_ArgClasses, 5)
	{
		m_ArgClasses[0] = arg1Cls;
		m_ArgClasses[1] = arg2Cls;
		m_ArgClasses[2] = arg3Cls;
		m_ArgClasses[3] = arg4Cls;
		m_ArgClasses[4] = arg5Cls;
	}

private:
	ClassCPtr m_ArgClasses[5];
};

class SexenaryOperator : public Operator
{
public:
	SexenaryOperator(AbstrOperGroup* gr, ClassCPtr resultCls,
		ClassCPtr arg1Cls, ClassCPtr arg2Cls, ClassCPtr arg3Cls, 
		ClassCPtr arg4Cls, ClassCPtr arg5Cls, ClassCPtr arg6Cls)
	:	Operator(gr, resultCls, m_ArgClasses, 6)
	{
		m_ArgClasses[0] = arg1Cls;
		m_ArgClasses[1] = arg2Cls;
		m_ArgClasses[2] = arg3Cls;
		m_ArgClasses[3] = arg4Cls;
		m_ArgClasses[4] = arg5Cls;
		m_ArgClasses[5] = arg6Cls;
	}

private:
	ClassCPtr m_ArgClasses[6];
};

class SeptenaryOperator : public Operator
{
public:
	SeptenaryOperator(AbstrOperGroup* gr, ClassCPtr resultCls,
		ClassCPtr arg1Cls, ClassCPtr arg2Cls, ClassCPtr arg3Cls, 
		ClassCPtr arg4Cls, ClassCPtr arg5Cls, ClassCPtr arg6Cls, ClassCPtr arg7Cls)
	:	Operator(gr, resultCls, m_ArgClasses, 7)
	{
		m_ArgClasses[0] = arg1Cls;
		m_ArgClasses[1] = arg2Cls;
		m_ArgClasses[2] = arg3Cls;
		m_ArgClasses[3] = arg4Cls;
		m_ArgClasses[4] = arg5Cls;
		m_ArgClasses[5] = arg6Cls;
		m_ArgClasses[6] = arg7Cls;
	}

private:
	ClassCPtr m_ArgClasses[7];
};

class OctalOperator : public Operator
{
public:
	OctalOperator(AbstrOperGroup* gr, ClassCPtr resultCls,
		ClassCPtr arg1Cls, ClassCPtr arg2Cls, ClassCPtr arg3Cls, 
		ClassCPtr arg4Cls, ClassCPtr arg5Cls, ClassCPtr arg6Cls, 
		ClassCPtr arg7Cls, ClassCPtr arg8Cls)
	:	Operator(gr, resultCls, m_ArgClasses, 8)
	{
		m_ArgClasses[0] = arg1Cls;
		m_ArgClasses[1] = arg2Cls;
		m_ArgClasses[2] = arg3Cls;
		m_ArgClasses[3] = arg4Cls;
		m_ArgClasses[4] = arg5Cls;
		m_ArgClasses[5] = arg6Cls;
		m_ArgClasses[6] = arg7Cls;
		m_ArgClasses[7] = arg8Cls;
	}

private:
	ClassCPtr m_ArgClasses[8];
};

class NonaryOperator : public Operator
{
public:
	NonaryOperator(AbstrOperGroup* gr, ClassCPtr resultCls,
		ClassCPtr arg1Cls, ClassCPtr arg2Cls, ClassCPtr arg3Cls, 
		ClassCPtr arg4Cls, ClassCPtr arg5Cls, ClassCPtr arg6Cls, 
		ClassCPtr arg7Cls, ClassCPtr arg8Cls, ClassCPtr arg9Cls)
	:	Operator(gr, resultCls, m_ArgClasses, 9)
	{
		m_ArgClasses[0] = arg1Cls;
		m_ArgClasses[1] = arg2Cls;
		m_ArgClasses[2] = arg3Cls;
		m_ArgClasses[3] = arg4Cls;
		m_ArgClasses[4] = arg5Cls;
		m_ArgClasses[5] = arg6Cls;
		m_ArgClasses[6] = arg7Cls;
		m_ArgClasses[7] = arg8Cls;
		m_ArgClasses[8] = arg9Cls;
	}

private:
	ClassCPtr m_ArgClasses[9];
};

class DecimalOperator : public Operator
{
public:
	DecimalOperator(AbstrOperGroup* gr, ClassCPtr resultCls,
		ClassCPtr arg1Cls, ClassCPtr arg2Cls, ClassCPtr arg3Cls, 
		ClassCPtr arg4Cls, ClassCPtr arg5Cls, ClassCPtr arg6Cls, 
		ClassCPtr arg7Cls, ClassCPtr arg8Cls, ClassCPtr arg9Cls,
		ClassCPtr arg10Cls)
	:	Operator(gr, resultCls, m_ArgClasses, 10)
	{
		m_ArgClasses[0] = arg1Cls;
		m_ArgClasses[1] = arg2Cls;
		m_ArgClasses[2] = arg3Cls;
		m_ArgClasses[3] = arg4Cls;
		m_ArgClasses[4] = arg5Cls;
		m_ArgClasses[5] = arg6Cls;
		m_ArgClasses[6] = arg7Cls;
		m_ArgClasses[7] = arg8Cls;
		m_ArgClasses[8] = arg9Cls;
		m_ArgClasses[9] = arg10Cls;
	}

private:
	ClassCPtr m_ArgClasses[10];
};


class VariadicOperator : public Operator
{
public:
	VariadicOperator(AbstrOperGroup* gr, ClassCPtr resultCls, arg_index nrArgClasses)
	:	Operator(gr, resultCls)
	,	m_ArgClasses(new ClassCPtr[nrArgClasses])
	{
		m_ArgClassesBegin = m_ArgClasses.get();
		m_ArgClassesEnd   = m_ArgClassesBegin + nrArgClasses;
	}

protected:
	std::unique_ptr<ClassCPtr[]> m_ArgClasses;
};

#endif // __TIC_OPERATOR_H
