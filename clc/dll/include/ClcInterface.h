// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#pragma once

#ifndef __CLC_INTERFACE_H
#define __CLC_INTERFACE_H

// *****************************************************************************

#include "ClcBase.h"
class  Operator;
struct AbstrOperGroup;

// *****************************************************************************

extern "C" {

CLC_CALL void         DMS_CONV DMS_Clc_Load();
CLC_CALL CharPtr      DMS_CONV DMS_NumericDataItem_GetStatistics(const TreeItem* item, bool* donePtr);

CLC_CALL bool NumericDataItem_GetStatistics(const TreeItem* item, vos_buffer_type& statisticsBuffer);

CLC_CALL void DMS_CONV XML_ReportOperator     (OutStreamBase* xmlStr, const Operator* oper);
CLC_CALL void DMS_CONV XML_ReportOperGroup    (OutStreamBase* xmlStr, const AbstrOperGroup* gr);
CLC_CALL void DMS_CONV XML_ReportAllOperGroups(OutStreamBase* xmlStr);

} // end extern "C"

#endif