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

#include "RtcPCH.h"
#pragma hdrstop

#include "act/ActorEnums.h"

//----------------------------------------------------------------------
// struct flag_set
//----------------------------------------------------------------------
#define MG_DEBUG_FLAGSET_CONTENTION

using FlagSetCriticalSection = std::shared_mutex;
using ScopedFlagSetLock = std::scoped_lock<FlagSetCriticalSection>;
using SharedFlagSetLock = std::shared_lock<FlagSetCriticalSection>;

#if defined(MG_DEBUG_FLAGSET_CONTENTION)

FlagSetCriticalSection cs_flagSet;

FlagSetCriticalSection& GetFlagSetCS(const flag_set* ptr)
{
	return cs_flagSet;
}

#else

FlagSetCriticalSection cs_flagSet[256];

FlagSetCriticalSection& GetFlagSetCS(const flag_set* ptr)
{
	UInt32 hashKey32 = UInt32(SizeT(ptr));
	UInt16 hashKey16 = hashKey32 ^ (hashKey32 >> 16);
	UInt8  hashKey8 = hashKey16 ^ (hashKey16 >> 8);

	return cs_flagSet[hashKey8];
}

#endif

UInt32 flag_set::GetBits(UInt32 sf) const
{
	SharedFlagSetLock sectionLock(GetFlagSetCS(this));
	return m_DW & sf;
}

void flag_set::Set(UInt32 sf)
{
	ScopedFlagSetLock lock(GetFlagSetCS(this));
	m_DW |= sf;
}

void flag_set::SetBits(UInt32 sf, UInt32 values)
{
	ScopedFlagSetLock lock(GetFlagSetCS(this));
	dms_assert(!(values & ~sf));
	(m_DW &= ~sf) |= values;
}

void flag_set::Clear(UInt32 sf) 
{
	ScopedFlagSetLock lock(GetFlagSetCS(this));
	m_DW &= ~sf;
}

void flag_set::Toggle (UInt32 sf) 
{ 
	ScopedFlagSetLock lock(GetFlagSetCS(this));
	m_DW ^= sf;
}

RTC_CALL CharPtr FailStateName(UInt32 fs)
{
	switch (fs)
	{
	case FR_None: return "None";
	case FR_Determine: return "DetermineState Failed";
	case FR_MetaInfo: return "MetaInfo Failed";
	case FR_Data: return "Primary Data Derivation Failed";
	case FR_Validate: return "Validation (Integrity Check) Failed";
	case FR_Committed: return "Committing Data (writing to storage) Failed";
	}
	return "unrecognized";
}

