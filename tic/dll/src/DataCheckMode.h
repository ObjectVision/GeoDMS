// Copyright (C) 1998-2024 Object Vision b.v. 
// License: GNU GPL 3

#if defined(_MSC_VER)
#pragma once
#endif


#if !defined(__TIC_DATACHECKMODE_H)
#define __TIC_DATACHECKMODE_H

//----------------------------------------------------------------------
// enumeration Check Modes
//----------------------------------------------------------------------

enum DataCheckMode{
	DCM_None = 0
,	DCM_CheckDefined= 1
,	DCM_CheckRange= 2
,	DCM_CheckBoth = 3

,	DCM_Count    = 4
};


#endif // __TIC_DATACHECKMODE_H
