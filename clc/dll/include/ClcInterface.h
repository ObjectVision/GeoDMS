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
#pragma once

#ifndef __CLC_INTERFACE_H
#define __CLC_INTERFACE_H

// *****************************************************************************

#include "ClcBase.h"
class  Operator;
struct AbstrOperGroup;

// *****************************************************************************

extern "C" {

CLC1_CALL void         DMS_CONV DMS_Clc1_Load();
CLC2_CALL void         DMS_CONV DMS_Clc2_Load();
CLC1_CALL CharPtr      DMS_CONV DMS_NumericDataItem_GetStatistics(const TreeItem* item, bool* donePtr);


CLC1_CALL const AbstrCalculator* DMS_CONV DMS_TreeItem_GetParseResult(const TreeItem* context);
CLC1_CALL const AbstrCalculator* DMS_CONV DMS_TreeItem_CreateParseResult(TreeItem* context, CharPtr expr, bool contextIsTarget);
CLC1_CALL void         DMS_CONV DMS_ParseResult_Release                (AbstrCalculator* self);

CLC1_CALL bool           DMS_CONV DMS_ParseResult_CheckSyntax            (AbstrCalculator* self);
CLC1_CALL bool           DMS_CONV DMS_ParseResult_HasFailed              (AbstrCalculator* self);
CLC1_CALL CharPtr        DMS_CONV DMS_ParseResult_GetFailReason          (AbstrCalculator* self);
CLC1_CALL CharPtr        DMS_CONV DMS_ParseResult_GetAsSLispExpr         (AbstrCalculator* self, bool afterRewrite);
CLC1_CALL const LispObj* DMS_CONV DMS_ParseResult_GetAsLispObj   (AbstrCalculator* self, bool afterRewrite);

CLC1_CALL const TreeItem* DMS_CONV DMS_ParseResult_CreateResultingTreeItem(AbstrCalculator* self);
CLC1_CALL bool            DMS_CONV DMS_ParseResult_CheckResultingTreeItem (AbstrCalculator* self, TreeItem* resultHolder);
CLC1_CALL const TreeItem* DMS_CONV DMS_ParseResult_CalculateResultingData (AbstrCalculator* self);

CLC1_CALL UInt32                DMS_CONV DMS_OperatorSet_GetNrOperatorGroups();
CLC1_CALL const AbstrOperGroup* DMS_CONV DMS_OperatorSet_GetOperatorGroup(UInt32 i);
CLC1_CALL CharPtr               DMS_CONV DMS_OperatorGroup_GetName       (const AbstrOperGroup* self);
CLC1_CALL const Operator*       DMS_CONV DMS_OperatorGroup_GetFirstMember(const AbstrOperGroup* self);
CLC1_CALL const Operator*       DMS_CONV DMS_Operator_GetNextGroupMember (const Operator*       self);

CLC1_CALL UInt32                DMS_CONV DMS_Operator_GetNrArguments(const Operator* self);
CLC1_CALL const Class*          DMS_CONV DMS_Operator_GetArgumentClass (const Operator* self, arg_index argnr);
CLC1_CALL const Class*          DMS_CONV DMS_Operator_GetResultingClass(const Operator* self);

// function pointers are used to register a client defined operator
typedef TreeItem* (DMS_CONV *OperatorCreateFunc)(ClientHandle clientHandle, const TreeItem* configRoot, arg_index nrArgs, const TreeItem*const* args);
typedef void      (DMS_CONV *OperatorApplyFunc )(ClientHandle clientHandle, TreeItem* result, arg_index nrArgs, const TreeItem*const* args);

CLC1_CALL ClientDefinedOperator* DMS_CONV DMS_ClientDefinedOperator_Create(
	CharPtr name, ClassCPtr resultCls, 
	arg_index nrArgs, const ClassCPtr* argClsList,
	OperatorCreateFunc createFunc, 
	OperatorApplyFunc applyFunc, 
	ClientHandle clientHandle
);

CLC1_CALL void DMS_CONV DMS_ClientDefinedOperator_Release(ClientDefinedOperator* self);

CLC2_CALL void DMS_CONV XML_ReportOperator     (OutStreamBase* xmlStr, const Operator* oper);
CLC2_CALL void DMS_CONV XML_ReportOperGroup    (OutStreamBase* xmlStr, const AbstrOperGroup* gr);
CLC2_CALL void DMS_CONV XML_ReportAllOperGroups(OutStreamBase* xmlStr);

} // end extern "C"

#endif