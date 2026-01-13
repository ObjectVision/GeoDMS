// Copyright (C) 1998-2024 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "ClcPch.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif

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


CLC_CALL void DMS_CONV DMS_Clc_Load() 
{
	DMS_Stx_Load();
}

