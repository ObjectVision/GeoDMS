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


#if !defined(__RTC_SER_SAFEFILEWRITER_H)
#define __RTC_SER_SAFEFILEWRITER_H

#include "ptr/OwningPtr.h"
#include "ptr/SharedStr.h"
#include "ser/FileCreationMode.h"
#include "set/QuickContainers.h"

struct CharPtrCaseInsensitiveCompare
{
	bool operator()(CharPtr a, CharPtr b) const 
	{
		return _stricmp(a, b) < 0;
	}
};

// ------------------------------------------------------------------------
// struct SafeFileWriter
// ------------------------------------------------------------------------

struct SafeFileWriter
{
	RTC_CALL SafeFileWriter(CharPtr fileName);
//	RTC_CALL SafeFileWriter(SafeFileWriter&& rhs);
	RTC_CALL ~SafeFileWriter();

	void Open(FileCreationMode fcm);

	RTC_CALL SharedStr GetWorkingFileName() const;
	WeakStr GetFileName() const { return m_FileName; }


private: friend struct SafeFileWriterArray;
	SafeFileWriter(const SafeFileWriter&); // never call copy ctor
	void  Commit();
	void  PostCommit();
	void  UnCommit();
	void  UnCommitOld();
	void  Rollback();

	SharedStr GetOldFileName() const;
	SharedStr KillOldFile();

	SharedStr m_FileName;
	bool   m_DidExist    : 1; // m_FileName did exist at the creation of SafeFileWriter
	bool   m_UseCopy     : 1; // use a copy (with a .tmp suffix) which can be a new file or a copy of the existing file. m_UseCopy can only be true when m_DidExist is true
	bool   m_IsOpen      : 1; // afer open, before commit, of after uncommit.
	bool   m_IsRenamed   : 1; // .tmp is renamed to base name 
	bool   m_IsRenamedOld: 1; // base name is renamed to basename.old
};

// ------------------------------------------------------------------------
// struct SafeFileWriterArray
// ------------------------------------------------------------------------

struct SafeFileWriterArray
{
	typedef OwningPtr<SafeFileWriter>   value_type;
	typedef std::map<CharPtr, value_type, CharPtrCaseInsensitiveCompare> index_type;

	RTC_CALL SafeFileWriterArray();
	RTC_CALL ~SafeFileWriterArray();

	RTC_CALL SafeFileWriter* OpenOrCreate(CharPtr fileName, FileCreationMode fcm);
	RTC_CALL SharedStr       GetWorkingFileName(WeakStr fileName, FileCreationMode fcm);
	RTC_CALL SafeFileWriter* FindExisting(CharPtr fileName);

	RTC_CALL void Commit();
	RTC_CALL void Clear();

	inline void swap(SafeFileWriterArray& oth) { m_Index.swap(oth.m_Index); }

private:
	SafeFileWriter* FindOrCreate(CharPtr fileName, FileCreationMode fcm);
	
	index_type m_Index;
};

RTC_CALL SharedStr GetWorkingFileName(SafeFileWriterArray* sfwa, WeakStr fileName, FileCreationMode fcm);

#endif //!defined(__RTC_SER_SAFEFILEWRITER_H)
