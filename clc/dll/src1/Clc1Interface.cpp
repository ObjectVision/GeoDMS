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

#include "TicInterface.h"
#include "ClcInterface.h"
#include "StxInterface.h"

#include "dbg/debug.h"
#include "dbg/DmsCatch.h"

#include "StgBase.h"

#include "stg/StorageInterface.h"

#include "AbstrDataItem.h"
#include "AbstrUnit.h"
#include "SessionData.h"


CLC1_CALL void DMS_CONV DMS_Clc1_Load() 
{
	DMS_Stx_Load();
}

// *****************************************************************************
// Section:     OperatorSet
// *****************************************************************************

CLC1_CALL UInt32 DMS_CONV DMS_OperatorSet_GetNrOperatorGroups()
{
	DMS_CALL_BEGIN

		return AbstrOperGroup::GetNrOperatorGroups();

	DMS_CALL_END
	return 0;
}

CLC1_CALL const AbstrOperGroup* DMS_CONV DMS_OperatorSet_GetOperatorGroup(UInt32 i)
{
	DMS_CALL_BEGIN

		return AbstrOperGroup::GetOperatorGroup(i);

	DMS_CALL_END
	return 0;
}

CLC1_CALL const Operator* DMS_CONV DMS_OperatorGroup_GetFirstMember(const AbstrOperGroup* self)
{
	DMS_CALL_BEGIN

		dms_assert(self);
		return self->GetFirstMember();

	DMS_CALL_END
	return 0;
}

CLC1_CALL const Operator* DMS_CONV DMS_Operator_GetNextGroupMember(const Operator* self)
{
	DMS_CALL_BEGIN

		dms_assert(self);
		return self->GetNextGroupMember();

	DMS_CALL_END
	return 0;
}

CLC1_CALL CharPtr  DMS_CONV DMS_OperatorGroup_GetName(const AbstrOperGroup* self)
{
	DMS_CALL_BEGIN

		dms_assert(self);
		return self->GetNameID().GetStr().c_str();

	DMS_CALL_END
	return "";
}


CLC1_CALL UInt32  DMS_CONV DMS_Operator_GetNrArguments(const Operator* self)
{
	DMS_CALL_BEGIN

		MG_PRECONDITION(self);
		return self->NrSpecifiedArgs();

	DMS_CALL_END
	return 0;
}

CLC1_CALL const Class*  DMS_CONV DMS_Operator_GetArgumentClass (const Operator* self, UInt32 argnr)
{
	DMS_CALL_BEGIN

		MG_PRECONDITION(self);
		return self->GetArgClass(argnr);

	DMS_CALL_END
	return 0;
}


CLC1_CALL const Class*  DMS_CONV DMS_Operator_GetResultingClass(const Operator* self)
{
	DMS_CALL_BEGIN

		MG_PRECONDITION(self);
		return self->GetResultClass();

	DMS_CALL_END
	return 0;
}

// *****************************************************************************
// Section:     ClientDefinedOperator
// *****************************************************************************

struct ClientDefinedOperator : public VariadicOperator
{
  public:
	ClientDefinedOperator(AbstrOperGroup* group, const Class* resultCls, 
		UInt32 nrArgs, const ClassCPtr* argClsList,
		OperatorCreateFunc createFunc, 
		OperatorApplyFunc  applyFunc, 
		ClientHandle clientHandle=0
	)	:	VariadicOperator(group, resultCls, nrArgs)
		,	m_CreateFunc(createFunc)
		,	m_ApplyFunc(applyFunc)
		,	m_ClientHandle(clientHandle)
		,	m_OperGroup(group) // double call, can only be avoided by extra var or reordering members
	{
		m_ArgClassesBegin = m_ArgClasses.get();
		m_ArgClassesEnd   = std::copy(argClsList, argClsList+nrArgs, m_ArgClasses.get());
	}

	~ClientDefinedOperator()
	{
		// remove from linked list of operatorgroup
		// if group is empty; it was probably dynamically allocated and thus we destroy it
		m_OperGroup->UnRegister(this);
	}

	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		dms_assert(args.size() == NrSpecifiedArgs());
		if (!resultHolder)
			resultHolder = m_CreateFunc(m_ClientHandle, SessionData::Curr()->GetConfigRoot(), args.size(), begin_ptr( args ));
		dms_assert(resultHolder.IsNew());
		if (mustCalc)
			m_ApplyFunc(m_ClientHandle, resultHolder, args.size(), begin_ptr( args ));
		return true;
	}

  private:
	OperatorCreateFunc m_CreateFunc;
	OperatorApplyFunc  m_ApplyFunc;
	ClientHandle       m_ClientHandle;
	AbstrOperGroup*    m_OperGroup;
};

CLC1_CALL ClientDefinedOperator* DMS_CONV DMS_ClientDefinedOperator_Create(
	CharPtr name, ClassCPtr resultCls, 
	UInt32 nrArgs, const ClassCPtr* argClsList,
	OperatorCreateFunc createFunc, 
	OperatorApplyFunc  applyFunc, 
	ClientHandle clientHandle)
{
	DMS_CALL_BEGIN

		DBG_START("DMS_ClientOperator", "Create", true);

		return new ClientDefinedOperator(
			AbstrOperGroup::FindOrCreateCOG(GetTokenID_mt(name)),
			resultCls, nrArgs, argClsList,
			createFunc, applyFunc, clientHandle);

	DMS_CALL_END
	return nullptr;
}

CLC1_CALL void DMS_CONV DMS_ClientDefinedOperator_Release(ClientDefinedOperator* self)
{
	DMS_CALL_BEGIN

		DBG_START("DMS_ClientOperator", "Release", true);
		MG_PRECONDITION(self);

		delete self;

	DMS_CALL_END
}

