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
#if !defined(__TIC_MOREDATACONTROLLERS_H)
#define __TIC_MOREDATACONTROLLERS_H

#include "ptr\Resource.h" 
#include "ptr\WeakPtr.h" 

#include "AbstrCalculator.h"
#include "DataController.h"
#include "OperGroups.h" 

// *****************************************************************************
// Section:     specific DataController definitions
// *****************************************************************************

struct NumbDC : DataController
{
	typedef DataController base_type;

	NumbDC(LispPtr keyExpr): DataController(keyExpr) {}

	SharedTreeItem MakeResult() const override;
	FutureData CalcResult(Explain::Context* context) const override;
};

struct StringDC : DataController
{
	typedef DataController base_type;

	StringDC(LispPtr keyExpr): DataController(keyExpr) {}

	SharedTreeItem MakeResult() const override;
	FutureData CalcResult(Explain::Context* context) const override;

};

// *****************************************************************************
// struct SymbDC : DataController
// *****************************************************************************

struct SymbDC : DataController
{
	typedef DataController base_type;

	SymbDC(LispPtr keyExpr, const TokenID fullNameID);

	SharedTreeItem MakeResult() const override;
	FutureData CalcResult(Explain::Context* context) const override;

	ActorVisitState VisitSuppliers(SupplierVisitFlag svf, const ActorVisitor& visitor) const override;
	bool CanResultToConfigItem() const override { return true; }

	bool IsSymbDC() const override { return true; }

private:
	TokenID m_FullNameID;
};

// *****************************************************************************
// struct FuncDC : DataController
// *****************************************************************************

struct FuncDC : DataController
{
	typedef DataController base_type;

	FuncDC(LispPtr keyExpr, const AbstrOperGroup* og);

	~FuncDC();

	SharedTreeItem MakeResult() const override;
	FutureData CalcResult(Explain::Context* context) const override;
	bool                      CanResultToConfigItem() const override
	{ 
		dms_assert(m_OperatorGroup);
		return m_OperatorGroup->CanResultToConfigItem();
	}
	TokenID GetID          () const override { return GetLispRef()->Left()->GetID(); }

	DcRefListElem* GetArgList() const { return m_Args; }
	const DataController* GetArgDC(arg_index i) const
	{
		DcRefListElem* dcRef = m_Args;
		while (dcRef)
		{
			if (!i--)
				return dcRef->m_DC;
			dcRef = dcRef->m_Next;
		}
		dms_assert(0);
		return nullptr;
	}
	bool MustCalcArg(arg_index argNr, bool doCalc, CharPtr firstArgValue) const { return MustCalcArg(GetArgPolicy(argNr, firstArgValue), doCalc); }

	arg_index GetNrArgs() const
	{
		arg_index result = 0;
		for (DcRefListElem* dcRef = m_Args; dcRef; dcRef = dcRef->m_Next)
			++result;
		return result;
 	}
	const Operator* GetCurrOperator() const { return m_Operator; }
	void SetOperator(const Operator* oper) const { m_Operator = oper; }
	bool IsSubItemCall() const { return GetArgPolicy(0, nullptr) == oper_arg_policy::calc_subitem_root; }

	static bool MustCalcArg(oper_arg_policy ap, bool doCalc);

//protected: // override Actor virtuals
	ActorVisitState VisitSuppliers(SupplierVisitFlag svf, const ActorVisitor& visitor) const override;
//	void IncDataInterestCount() const override;
	void DoInvalidate() const override;
	bool IsCalculating() const override;
	garbage_t StopInterest () const noexcept override;

//protected: 
	
	virtual bool MakeResultImpl() const;
	void CalcResultImpl(Explain::Context* context) const;
	const Class* GetResultCls () const override;
	OArgRefs GetArgs(bool doUpdateMetaInfo, bool doUpdateData) const;

	oper_arg_policy GetArgPolicy(arg_index argNr, CharPtr firstArgValue) const;
	bool IsSupplArg   (arg_index argNr, CharPtr firstArgValue) const {return MustCalcArg(argNr, true, firstArgValue); }
	const Operator* GetOperator() const;
	void CancelOperContext() const;

	std::shared_ptr<OperationContext> GetOperContext() const;
	std::shared_ptr<OperationContext> ResetOperContextImpl() const;
	garbage_t ResetOperContextImplAndStopSupplInterest() const;

	OwningPtr<DcRefListElem>      m_Args;
	WeakPtr<const AbstrOperGroup> m_OperatorGroup;
	mutable WeakPtr<const Operator>  m_Operator;
private: friend struct OperationContext;
	mutable std::shared_ptr<OperationContext> m_OperContext;
	mutable std::vector<DataControllerRef> m_OtherSuppliersCopy;
};

#endif // __TIC_MOREDATACONTROLLERS_H
