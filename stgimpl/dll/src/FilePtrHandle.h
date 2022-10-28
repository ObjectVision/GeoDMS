//<HEADER> 
/*
Data & Model Server (DMS) is a server written in C++ for DSS applications. 
Version: see srv/dms/rtc/dll/src/RtcVersion.h for version info.

Copyright (C) 1998-2004  YUSE GSO Object Vision BV. 

Documentation on using the Data & Model Server software can be found at:
http://www.ObjectVision.nl/DMS/

See additional guidelines and notes in srv/dms/Readme-srv.txt 

This library is free software; you can use, redistribute, and/or
modify it under the terms of the GNU General Public License version 2 
(the License) as published by the Free Software Foundation,
provided that this entire header notice and readme-srv.txt is preserved.

See LICENSE.TXT for terms of distribution or look at our web site:
http://www.objectvision.nl/DMS/License.txt
or alternatively at: http://www.gnu.org/copyleft/gpl.html

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
General Public License for more details. However, specific warranties might be
granted by an additional written contract for support, assistance and/or development
*/
//</HEADER>
#pragma once

#if !defined(__STGIMPL_FILEHANDLE_H)
#define __STGIMPL_FILEHANDLE_H

#include "ImplMain.h"

#include "ptr/WeakPtr.h"
#include "ser/FileCreationMode.h"
struct SafeFileWriterArray;

// ============================= FilePtrHandle (non-polymorphic base class for specific handles

typedef struct _iobuf FILE; // defined in stdio.h

#define NR_PAGES_DATFILE 16
#define NR_PAGES_HDRFILE  1
#define NR_PAGES_DIRECTIO 0

class FilePtrHandle
{
public:
	STGIMPL_CALL FilePtrHandle ();
	STGIMPL_CALL ~FilePtrHandle();

	STGIMPL_CALL bool OpenFH  (WeakStr name, SafeFileWriterArray* sfwa, FileCreationMode fm, bool translate, UInt32 nrPagesInBuffer);
	STGIMPL_CALL void CloseFH ();

	STGIMPL_CALL SizeT GetFileSize() const;

	bool IsOpen() const { return m_FP; }

	STGIMPL_CALL FILE* GetFP()     const { return m_FP; } // Don't use this result to close a file yourself, let FilePtrHandle manage the resource
	STGIMPL_CALL operator FILE* () const { return GetFP(); } // Don't use this result to close a file yourself, let FilePtrHandle manage the resource

private:
	void SetBufferSize(UInt32 nrPagesInBuffer);

	FILE* m_FP = nullptr;
	bool  m_TranslateText :1 = false, m_CanRead : 1 = false, m_CanWrite : 1 = false;
};


#endif // __STGIMPL_FILEHANDLE_H
