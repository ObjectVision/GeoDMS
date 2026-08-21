// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////
#pragma once

#ifndef __SER__STREAMBUFFINTERFACE_H
#define __SER__STREAMBUFFINTERFACE_H

#include "RtcBase.h"

//----------------------------------------------------------------------
// Creating and reading OutStreamBuff's
//----------------------------------------------------------------------

extern "C" {

	RTC_CALL VectorOutStreamBuff* DMS_CONV DMS_VectOutStreamBuff_Create();
	RTC_CALL FileOutStreamBuff*   DMS_CONV DMS_FileOutStreamBuff_Create(CharPtr fileName, bool isAsciiFile);

	RTC_CALL void DMS_CONV DMS_OutStreamBuff_Destroy(OutStreamBuff* );
	RTC_CALL void DMS_CONV DMS_OutStreamBuff_WriteBytes(OutStreamBuff* self, const Byte* source, streamsize_t sourceSize);
	RTC_CALL void DMS_CONV DMS_OutStreamBuff_WriteChars(OutStreamBuff* self, CharPtr source);

	// note: apply the following function only on VectOutstreamBuffs
	// note: the returning string might not be null-terminated; use CurrPos as length-indicator to avoid 'GPF'
	RTC_CALL CharPtr DMS_CONV DMS_VectOutStreamBuff_GetData(VectorOutStreamBuff* self);
	RTC_CALL streamsize_t DMS_CONV DMS_OutStreamBuff_CurrPos(OutStreamBuff* self);

} // extern "C"

#endif // __SER__STREAMBUFFINTERFACE_H
