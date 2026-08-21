// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////
#ifndef __ASMUTIL_H
#define __ASMUTIL_H 1

/*
 *	ASM_STATE
 *
 *	Abstract storage (error) state definitions
 */
typedef enum
{
	ASM_OK,
	ASM_ERR,
	ASM_E_FILENOTFOUND,
	ASM_E_PATHNOTFOUND,
	ASM_E_FILENOTOPENED,
	ASM_E_FILEALREADYEXISTS,
	ASM_E_LOCKVIOLATION,
	ASM_E_SHAREVIOLATION,
	ASM_E_ACCESSDENIED,
	ASM_E_DATAREAD,
	ASM_E_MEDIUMFULL,
	ASM_E_WRITEFAULT,
	ASM_E_INSUFFICIENTMEMORY,
	ASM_E_INVALIDOBJECT,
	ASM_E_UNKNOWNSTORAGECLASS
} ASM_STATE;


[[noreturn]] void throwStorageError(ASM_STATE state, const char* storageName);

#endif // __ASMUTIL_H