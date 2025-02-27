// Copyright (C) 1998-2025 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "RtcPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

#include "act/ActorEnums.h"

//----------------------------------------------------------------------
// struct flag_set
//----------------------------------------------------------------------

UInt32 flag_set::GetBits(UInt32 sf) const
{
	return m_DW & sf;
}

void flag_set::Set(UInt32 sf)
{
	m_DW |= sf;
}

void flag_set::SetBits(UInt32 sf, UInt32 values)
{
	dms_assert(!(values & ~sf));

	UInt32 oldDW = m_DW;
	while (!m_DW.compare_exchange_weak(oldDW, (oldDW & ~sf) | values))
	{}
}

void flag_set::Clear(UInt32 sf) 
{
	m_DW &= ~sf;
}

void flag_set::Toggle (UInt32 sf) 
{ 
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

