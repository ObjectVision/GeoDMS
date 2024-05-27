// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "SafeFileWriter.h"

#include "dbg/debug.h"
#include "dbg/DebugContext.h"
#include "utl/mySPrintF.h"
#include "utl/Environment.h"

// ------------------------------------------------------------------------
// struct SafeFileWriterContextHandle
// ------------------------------------------------------------------------

struct SafeFileWriterContextHandle : ContextHandle
{
	SafeFileWriterContextHandle(SharedStr* fileNamePtr): m_FileNamePtr(fileNamePtr) {}

protected:
	void GenerateDescription() override
	{
		SetText(
			mySSPrintF("during SafeFileWriter::Commit of file '%s'\n"
			"New data (with suffix: .tmp)and possibly its previous version (with suffix: .old) may still reside on disk.\n"
				"Check all altered result files for version consistency before restarting the system.\n"
				"In case of doubt: remove the contents of the CalcCache folder",
				m_FileNamePtr->c_str()
			)
		);
	}
	SharedStr* m_FileNamePtr;

};

// ------------------------------------------------------------------------
// struct SafeFileWriter
// ------------------------------------------------------------------------

SafeFileWriter::SafeFileWriter(CharPtr fileName)
	:	m_FileName(fileName), m_IsOpen(false), m_DidExist(false), m_UseCopy(false), m_IsRenamed(false), m_IsRenamedOld(false)
{
}

void SafeFileWriter::Open(FileCreationMode fcm)
{
	if (!m_IsOpen)
		m_DidExist = IsFileOrDirAccessible(m_FileName);

	if (m_DidExist && fcm == FCM_CreateNew)
		throwDmsErrF("SafeFileWriter[%s].Open(FCM_CreateNew): Cannot overwrite existing file", m_FileName.c_str());

	bool wasUsingCopy = m_UseCopy;
	m_UseCopy |= (m_DidExist && fcm != FCM_OpenReadOnly);

	if ((!wasUsingCopy && m_UseCopy) // else we don't care about making tmp version ready
		|| fcm == FCM_Delete)
	{
		DBG_START("SafeFileWriter", "Open", true);
		DBG_TRACE(("filename= %s", m_FileName.c_str()));

		SharedStr tmpName = GetWorkingFileName();
		if (IsFileOrDirAccessible(tmpName))
		{
			DBG_TRACE(("Removing %s", tmpName.c_str()));
			KillFileOrDir(tmpName);
		}
		if (UseExisting(fcm))
		{
			DBG_TRACE(("Copying %s to open as %s", m_FileName.c_str(), tmpName.c_str()));
			CopyFileOrDir(m_FileName.c_str(), tmpName.c_str(), false);
		}
	}
	if (!m_DidExist && fcm == FCM_OpenReadOnly)
		throwDmsErrF("SafeFileWriter[%s].Open(FCM_ReadOny): File Does Not exist", m_FileName.c_str());
	if (fcm != FCM_Delete)
		if (!m_DidExist) GetWritePermission(GetWorkingFileName());

	m_IsOpen = true;
}

SharedStr SafeFileWriter::GetOldFileName() const
{
	return m_FileName + ".old";
}

SharedStr SafeFileWriter::GetWorkingFileName() const
{
	return (m_UseCopy)
		?	m_FileName + ".tmp"
		:	m_FileName;
}

SharedStr SafeFileWriter::KillOldFile()
{
	SharedStr oldName = GetOldFileName();
	if (IsFileOrDirAccessible(oldName))
		KillFileOrDir(oldName);
	return oldName;
}

void SafeFileWriter::PostCommit()
{
	assert(!m_IsOpen);
	if (m_IsRenamedOld)
	{
		assert(m_UseCopy);
		KillOldFile(); // clean-up the old mess
	}
}

void SafeFileWriter::Commit()
{
	assert(m_IsOpen);
	if (m_UseCopy) // implies WorkingFileName != fileName and implies that IsFileOrDirAccessible(m_FileName)) did return true earlier
	{
		SafeFileWriterContextHandle ch(&m_FileName);

		SharedStr oldName = KillOldFile();
		assert(IsFileOrDirAccessible(m_FileName));
		m_IsRenamedOld = MoveFileOrDir(m_FileName.c_str(), oldName .c_str(), true); // It did exist.
		try {
			SharedStr workingFileName = GetWorkingFileName();
			if (IsFileOrDirAccessible(workingFileName)) // maybe not created (yet) or (already) Dropped
			{
				MoveFileOrDir(workingFileName.c_str(), m_FileName.c_str(), false);
				m_IsRenamed = true;
			}
		}
		catch (...)
		{
			UnCommitOld();
			throw;
		}
	}
	m_IsOpen = false;
}

void SafeFileWriter::UnCommit()
{
	SafeFileWriterContextHandle ch(&m_FileName);
	if (m_UseCopy) // implies WorkingFileName != fileName
{
		assert(!m_IsOpen);
		if (m_IsRenamed && IsFileOrDirAccessible(m_FileName))
			MoveFileOrDir(m_FileName.c_str(), GetWorkingFileName().c_str(), true);
		m_IsRenamed = false;
		m_IsOpen = true; // Rollback later
		UnCommitOld();
	}
	else if (!m_DidExist)
		m_IsOpen = true;
}

void SafeFileWriter::UnCommitOld()
{
	assert(m_UseCopy); // PRECONDITION
	assert(m_IsOpen);
	if (m_IsRenamedOld)
	{
		assert(m_UseCopy); // DEBUG
		MoveFileOrDir(GetOldFileName().c_str(), m_FileName.c_str(), true); // It did exist.
		m_IsRenamedOld = false;
	}
}

void SafeFileWriter::Rollback() // before Commit or after UnCommit
{
	if (!m_IsOpen)
		return;

	if (m_DidExist && !m_UseCopy)
		return;

	// Kill either the .tmp name or the newly created original file name
	SharedStr workingFileName = GetWorkingFileName();
	if (IsFileOrDirAccessible(workingFileName)) // maybe not created (yet) or already Dropped
		KillFileOrDir(workingFileName, false); 

	m_IsOpen = false;
}

SafeFileWriter::~SafeFileWriter()
{
	if (m_IsOpen)
		Rollback();
}

// ------------------------------------------------------------------------
// struct SafeFileWriterArray
// ------------------------------------------------------------------------

#include "LockLevels.h"

static leveled_critical_section s_sfwaSection(item_level_type(0), ord_level_type::SafeFileWriterArray, "SafeFileWriterArray");

SafeFileWriterArray::SafeFileWriterArray()
{}

SafeFileWriterArray::~SafeFileWriterArray()
{
	Clear();
}

SafeFileWriter* SafeFileWriterArray::FindExisting(CharPtr fileName)
{
	leveled_critical_section::scoped_lock lock(s_sfwaSection);
	index_type::const_iterator i = m_Index.find(fileName);
	if (i == m_Index.end())
		return nullptr;
	else
		return i->second; 
}

SafeFileWriter* SafeFileWriterArray::FindOrCreate(CharPtr fileName, FileCreationMode fcm)
{
	leveled_critical_section::scoped_lock lock(s_sfwaSection);

	SafeFileWriter* sfw;
	index_type::const_iterator i = m_Index.find(fileName);

	if (i != m_Index.end() &&  !index_type::key_compare()(fileName, i->first))
		sfw = i->second.get_ptr();
	else
	{
		sfw = new SafeFileWriter(fileName);
		m_Index.insert(i, index_type::value_type(sfw->GetFileName().c_str(), sfw)); // use SharedStr filename of sfw; destroys sfw if insert throws, or moves it
	}
	assert(sfw);
	return sfw;
}

SafeFileWriter* SafeFileWriterArray::OpenOrCreate(CharPtr fileName, FileCreationMode fcm)
{
	SafeFileWriter* sfw = FindOrCreate(fileName, fcm);
	assert(sfw);
	sfw->Open(fcm);
	return sfw;
}

void SafeFileWriterArray::Commit()
{
	leveled_critical_section::scoped_lock lock(s_sfwaSection);

	for (index_type::iterator i = m_Index.begin(), e = m_Index.end(); i!=e; ++i)
	{
		try {
			(*i).second->Commit();
		}
		catch (...)
		{
			assert(  (*i).second->m_IsOpen ); // commit failed
			while (i != m_Index.begin()) {
				--i;
				assert(!((*i).second->m_IsOpen)); // this i was already committed;
				(*i).second->UnCommit();
				assert(  (*i).second->m_IsOpen ); // postcondition of UnCommit();
			}
			throw; // all SFW are open again, ready for Rollback().
		}
	}
	// now that all changes seem to have been safely committed we can remove the original files of which the names have been extended with '.old'.
	for (index_type::iterator i = m_Index.begin(), e = m_Index.end(); i!=e; ++i)
		(*i).second->PostCommit(); 
}

void SafeFileWriterArray::Clear()
{
	leveled_critical_section::scoped_lock lock(s_sfwaSection);
	m_Index.clear();
}

SharedStr SafeFileWriterArray::GetWorkingFileName(WeakStr fileName, FileCreationMode fcm)
{
	SafeFileWriter* sfw = (fcm==FCM_OpenReadOnly)
		?	FindExisting(fileName.c_str())
		:	OpenOrCreate(fileName.c_str(), fcm);

	if (!sfw)
		return fileName;

	return sfw->GetWorkingFileName();
}

SharedStr GetWorkingFileName(SafeFileWriterArray* sfwa, WeakStr fileName, FileCreationMode fcm)
{
	if (!sfwa)
		return fileName;
	return sfwa->GetWorkingFileName(fileName, fcm);
}
