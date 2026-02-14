// Copyright (C) 1998-2026 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif


#if !defined(__STGIMPL_FILEHANDLE_H)
#define __STGIMPL_FILEHANDLE_H

#include "ImplMain.h"
#include "FileResult.h"
#include "ptr/SharedStr.h"
#include "ser/FileCreationMode.h"

// ============================= FilePtrHandle (non-polymorphic base class for specific handles

using FILE = struct _iobuf; // defined in stdio.h

#define NR_PAGES_DATFILE 16
#define NR_PAGES_HDRFILE  1
#define NR_PAGES_DIRECTIO 0

class FilePtrHandle
{
public:
	STGDLL_CALL FilePtrHandle ();
	STGDLL_CALL ~FilePtrHandle();

	STGDLL_CALL FileResult OpenFH(WeakStr name, FileCreationMode fm, bool translate, UInt32 nrPagesInBuffer);
	STGIMPL_CALL void CloseFH ();

	STGDLL_CALL SizeT GetFileSize() const;

	bool IsOpen() const { return m_FP != nullptr; }

	// Don't use the following results to close a file yourself, let FilePtrHandle manage the resource
	FILE* GetFP()     const { return m_FP; }    
	operator FILE* () const { return GetFP(); }

private:

	FILE* m_FP = nullptr;
	bool  m_TranslateText :1 = false, m_CanRead : 1 = false, m_CanWrite : 1 = false;
};


#endif // __STGIMPL_FILEHANDLE_H
