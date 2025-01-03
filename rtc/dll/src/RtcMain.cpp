// Copyright (C) 1998-2024 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "RtcPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

static RtcReportLock myLock;
#if defined(MG_DEBUG)
static IStringComponentLock iStringLock;
#endif
static TokenComponent s_tcl;

