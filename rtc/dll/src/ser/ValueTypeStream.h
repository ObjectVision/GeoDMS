// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////
#ifndef __SER_VALUETYPESTREAM_H
#define __SER_VALUETYPESTREAM_H
#pragma once

#include "mci/ValueClass.h"
#include "ser/FormattedStream.h"

//----------------------------------------------------------------------
// serialization operations
//----------------------------------------------------------------------

RTC_CALL FormattedOutStream& operator <<(FormattedOutStream& os, const ValueClass& vc);

#endif // __SER_VALUETYPESTREAM_H

