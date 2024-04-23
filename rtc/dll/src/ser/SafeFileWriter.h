// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

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
		return stricmp(a, b) < 0;
	}
};

// ------------------------------------------------------------------------
// struct SafeFileWriter
// ------------------------------------------------------------------------

struct SafeFileWriter
{
	RTC_CALL SafeFileWriter(CharPtr fileName);
	RTC_CALL SafeFileWriter(SafeFileWriter&& rhs) = delete;
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
	using value_type = OwningPtr<SafeFileWriter>;
	using index_type = std::map<CharPtr, value_type, CharPtrCaseInsensitiveCompare>;

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
