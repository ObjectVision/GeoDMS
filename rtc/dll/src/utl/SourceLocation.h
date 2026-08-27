// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////
#pragma once

#if !defined(__RTC_UTL_SOURCELOCATION_H)
#define __RTC_UTL_SOURCELOCATION_H

//----------------------------------------------------------------------
// used modules and forward class references
//----------------------------------------------------------------------

#include "RtcBase.h"
#include "ptr/SharedBase.h"
#include "ptr/SharedStr.h"

//*****************************************************************
//**********         FileDescr Interface                 **********
//*****************************************************************

struct FileDescr : SharedBase
{
	RTC_CALL FileDescr(WeakStr str, FileDateTime fdt, UInt32 loadNumber);
	RTC_CALL ~FileDescr();

	WeakStr GetFileName() const { return m_FileName; }

	FileDateTime m_ReadFdt, m_LaterFdt = 0;
	UInt32 m_LoadNumber;

	FileDateTime LastFdt() const 
	{ 
		if (m_LaterFdt)
			return m_LaterFdt;
		return m_ReadFdt;
	}

	void Release() const 
	{ 
		assert(!IsOwned()); 
		delete this; 
	}

private:
	SharedStr m_FileName;
};

using FileDescrPtr = SharedPtr<FileDescr>;


struct SourceLocation : SharedBase
{
	RTC_CALL SourceLocation(FileDescrPtr configFileDescr, UInt32 lineNr, UInt32 colNr);

	SharedStr AsText() const;
	void Release() const;  //most descendant dtor visible from here

	SharedStr GetSourceName(WeakStr fullName, const Class* cls) const;

	FileDescrPtr m_ConfigFileDescr;
	UInt32       m_ConfigFileLineNr; 
	UInt32       m_ConfigFileColNr;
};

#endif // __RTC_UTL_SOURCELOCATION_H
