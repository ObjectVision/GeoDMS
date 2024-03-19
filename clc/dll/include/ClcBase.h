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

// *****************************************************************************
// oper groups that are used in multiple untis
// *****************************************************************************

// defined in UnitCreators.cpp
extern CLC_CALL CommonOperGroup cog_mul;
extern CLC_CALL CommonOperGroup cog_div;
extern CLC_CALL CommonOperGroup cog_add;
extern CLC_CALL CommonOperGroup cog_sub;
extern CLC_CALL CommonOperGroup cog_bitand;
extern CLC_CALL CommonOperGroup cog_bitor;
extern CLC_CALL CommonOperGroup cog_bitxor;
extern CLC_CALL CommonOperGroup cog_pow;
extern CLC_CALL CommonOperGroup cog_eq;
extern CLC_CALL CommonOperGroup cog_ne;
extern CLC_CALL CommonOperGroup cog_substr;

#endif