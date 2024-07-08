// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

#ifndef __CLC_CLCBASE_H
#define __CLC_CLCBASE_H

// *****************************************************************************
// Section:     DLL Build settings
// *****************************************************************************

#include "TicBase.h"

#if defined(DMCLC_EXPORTS)
#	define CLC_CALL __declspec(dllexport)
#else
#	define CLC_CALL __declspec(dllimport)
#endif

#include "OperGroups.h"


#endif