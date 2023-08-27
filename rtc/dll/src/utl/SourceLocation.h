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
	RTC_CALL FileDescr(WeakStr str, FileDateTime fdt = 0);
	RTC_CALL ~FileDescr();

	WeakStr GetFileName() const { return m_FileName; }

	FileDateTime m_Fdt;

	void Release() const { if (!DecRef()) delete this; }

private:
	SharedStr m_FileName;
};

using FileDescrPtr = SharedPtr<FileDescr>;


struct SourceLocation : SharedBase
{
	RTC_CALL SourceLocation(FileDescrPtr configFileDescr, UInt32 lineNr, UInt32 colNr);

	RTC_CALL SharedStr AsText() const;
	RTC_CALL void Release() const;  //most descendant dtor visible from here

	SharedStr GetSourceName(WeakStr fullName, const Class* cls) const;

	FileDescrPtr m_ConfigFileDescr;
	UInt32       m_ConfigFileLineNr; 
	UInt32       m_ConfigFileColNr;
};

#endif // __RTC_UTL_SOURCELOCATION_H
