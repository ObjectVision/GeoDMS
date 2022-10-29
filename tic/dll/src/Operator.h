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
#if !defined(__TIC_OPERATOR_H)
#define __TIC_OPERATOR_H

#include <vector>
template <typename V> class Unit;
#include "set/StackUtil.h"

#include "OperGroups.h"
#include "TreeItemDualRef.h"
#include "ptr/OwningPtrSizedArray.h"
#include "Explain.h"

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
	virtual void CreateResultCaller(TreeItemDualRef& resultHolder, const ArgRefs& args, OperationContext*, LispPtr metaCallArgs = LispPtr()) const
	{
		dms_assert(!CanExplainValue()); // or this method should be overridden.
		auto argSeq = GetItems(args);
		bool actualResult = CreateResult(resultHolder, argSeq, false);
		dms_assert(actualResult);
		dms_assert(resultHolder);
	}

	virtual bool CalcResult(TreeItemDualRef& resultHolder, const ArgRefs& args, OperationContext*, Explain::Context* context = nullptr) const
	{
		dms_assert(resultHolder);
		dms_assert(!CanExplainValue()); // or this method should be overridden.
		auto argSeq = GetItems(args);
		return CreateResult(resultHolder, argSeq, true);
	}


	arg_index             NrSpecifiedArgs()        const { return m_ArgClassesEnd - m_ArgClassesBegin; }
	arg_index             NrOptionalArgs()         const { dms_assert(NrSpecifiedArgs() >= m_NrOptionalArgs);  return m_NrOptionalArgs; }
	ClassCPtr             GetArgClass(arg_index i) const { dms_assert(i<NrSpecifiedArgs()); return m_ArgClassesBegin[i]; }
	ClassCPtr             GetResultClass()         const { return m_ResultClass; }
	const AbstrOperGroup* GetGroup()               const { return m_Group;       }
	const Operator*       GetNextGroupMember()     const { return m_NextGroupMember; }

	TIC_CALL virtual oper_arg_policy GetArgPolicy(arg_index argNr, CharPtr firstArgValue) const;
	TIC_CALL oper_policy GetOperPolicy() const { return GetGroup()->m_Policy; }

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
	SharedPtr<AbstrOperGroup> m_Group;
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

class UndenaryOperator : public Operator
{
public:
	UndenaryOperator(AbstrOperGroup* gr, ClassCPtr resultCls,
		ClassCPtr arg1Cls, ClassCPtr arg2Cls, ClassCPtr arg3Cls, 
		ClassCPtr arg4Cls, ClassCPtr arg5Cls, ClassCPtr arg6Cls, 
		ClassCPtr arg7Cls, ClassCPtr arg8Cls, ClassCPtr arg9Cls,
		ClassCPtr arg10Cls, ClassCPtr arg11Cls)
	:	Operator(gr, resultCls, m_ArgClasses, 11)
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
		m_ArgClasses[10] = arg11Cls;
	}

private:
	ClassCPtr m_ArgClasses[11];
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
