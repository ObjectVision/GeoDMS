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

#include <cstdio>

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

	// Read or write size bytes. When fewer go through, the error names the file, the number of bytes
	// asked for and the number that went through, and the reason: errno with its text and, on Windows,
	// the system error that the C runtime translated into errno, since errno alone lumps different
	// causes together. Reading a file that ends early says so instead.
	// (not ReadBytes and WriteBytes: DbfImpl derives from this class, and a member of that name would
	// hide the free WriteBytes of dbfImp.cpp in its member functions)
	STGDLL_CALL FileResult ReadBlock (void* data, SizeT size);
	STGDLL_CALL FileResult WriteBlock(const void* data, SizeT size);

	// Close a file that was written and report what CloseFH and the destructor cannot: a failing fclose,
	// which writes the last buffered part of the data and, on a network share, can be the first call to
	// hear of a refused write, and an earlier write failure that the stream recorded, such as one of an
	// unchecked fprintf. A writer calls this before it reports success.
	STGDLL_CALL FileResult CloseAfterWrite();

	STGDLL_CALL SizeT GetFileSize() const;

	bool IsOpen() const { return m_FP != nullptr; }

	// Don't use the following results to close a file yourself, let FilePtrHandle manage the resource
	FILE* GetFP()     const { return m_FP; }    
	operator FILE* () const { return GetFP(); }

private:

	FILE* m_FP = nullptr;
	SharedStr m_FileName; // as OpenFH opened it, for the errors of ReadBlock, WriteBlock and CloseAfterWrite
	bool  m_TranslateText :1 = false, m_CanRead : 1 = false, m_CanWrite : 1 = false;
};


#endif // __STGIMPL_FILEHANDLE_H
