// Copyright (C) 1998-2026 Object Vision B.V. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif


#if !defined(__RTC_DBG_SEVERITYTYPE_H)
#define __RTC_DBG_SEVERITYTYPE_H

//----------------------------------------------------------------------
// enumeration type for tracing and Exception handling
//----------------------------------------------------------------------

// The priority of an error is given by the following classification:

enum class SeverityTypeID : UInt8 {
	ST_MinorTrace = 0,
	ST_MajorTrace = 1,
	ST_Warning    = 2,
	ST_Error      = 3,
	ST_FatalError = 4,
	ST_DispError  = 5,
	ST_Nothing    = 6
};

enum class MsgCategory : UInt8 {
	progress,
	storage_read,
	storage_write,
	background_layer_connection,
	background_layer_request,
	other,
	memory,
	commands,
};

RTC_CALL CharPtr AsString(MsgCategory);

#endif // __RTC_DBG_SEVERITYTYPE_H
