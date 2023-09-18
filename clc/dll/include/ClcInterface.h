// Copyright (C) 2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#pragma once

#ifndef __CLC_INTERFACE_H
#define __CLC_INTERFACE_H

// *****************************************************************************

#include "ClcBase.h"
class  Operator;
struct AbstrOperGroup;
struct ClientDefinedOperator;

// *****************************************************************************

extern "C" {

CLC_CALL void         DMS_CONV DMS_Clc_Load();
CLC_CALL CharPtr      DMS_CONV DMS_NumericDataItem_GetStatistics(const TreeItem* item, bool* donePtr);

// function pointers are used to register a client defined operator
typedef TreeItem* (DMS_CONV *OperatorCreateFunc)(ClientHandle clientHandle, const TreeItem* configRoot, arg_index nrArgs, const TreeItem*const* args);
typedef void      (DMS_CONV *OperatorApplyFunc )(ClientHandle clientHandle, TreeItem* result, arg_index nrArgs, const TreeItem*const* args);

CLC_CALL ClientDefinedOperator* DMS_CONV DMS_ClientDefinedOperator_Create(
	CharPtr name, ClassCPtr resultCls, 
	arg_index nrArgs, const ClassCPtr* argClsList,
	OperatorCreateFunc createFunc, 
	OperatorApplyFunc applyFunc, 
	ClientHandle clientHandle
);

CLC_CALL void DMS_CONV DMS_ClientDefinedOperator_Release(ClientDefinedOperator* self);
CLC_CALL bool NumericDataItem_GetStatistics(const TreeItem* item, vos_buffer_type& statisticsBuffer);

CLC_CALL void DMS_CONV XML_ReportOperator     (OutStreamBase* xmlStr, const Operator* oper);
CLC_CALL void DMS_CONV XML_ReportOperGroup    (OutStreamBase* xmlStr, const AbstrOperGroup* gr);
CLC_CALL void DMS_CONV XML_ReportAllOperGroups(OutStreamBase* xmlStr);

} // end extern "C"

#endif