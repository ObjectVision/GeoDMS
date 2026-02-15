// Copyright (C) 1998-2026 Object Vision B.V. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif


#ifndef __SER_STREAMEXCEPTION
#define __SER_STREAMEXCEPTION 1

#include "xct/DmsException.h"

// *****************************************************************************
// Section:     streaming exceptions
// *****************************************************************************

[[noreturn]] void throwStreamException(CharPtr name, CharPtr msg);
[[noreturn]] void throwEndsException  (CharPtr name);

#endif // __SER_STREAMEXCEPTION
