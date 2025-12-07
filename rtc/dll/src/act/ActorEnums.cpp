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
	assert(!(values & ~sf));

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

RTC_CALL CharPtr FailStateName(FailType fs)
{
	switch (fs)
	{
	case FailType::None: return "None";
	case FailType::Determine: return "DetermineState Failed";
	case FailType::MetaInfo: return "MetaInfo Failed";
	case FailType::Data: return "Primary Data Derivation Failed";
	case FailType::Validate: return "Validation (Integrity Check) Failed";
	case FailType::Committed: return "Committing Data (writing to storage) Failed";
	}
	return "unrecognized";
}

