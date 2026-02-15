// Copyright (C) 1998-2026 Object Vision B.V. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "RtcPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

/*
 *  Name        : ser\BaseStreamBuff.cpp
 */

#include "ser/BaseStreamBuff.h"
#include "ser/FormattedStream.h"
#include "ser/StreamException.h"
#include "utl/mySPrintF.h"
#include "geo/Conversions.h"

// *****************************************************************************
// Section:     streaming exceptions
// *****************************************************************************

[[noreturn]] void throwStreamException(CharPtr name, CharPtr msg)
{
	throwErrorF("Stream","Stream %s has exception '%s'", name, msg);
}

[[noreturn]] void throwEndsException(CharPtr name)
{ 
	throwStreamException(name, "unexpected end of stream");
}

// *****************************************************************************
// Section:     Basic streambuffer Interface
// *****************************************************************************


/********** InpStreamBuff CODE **********/

InpStreamBuff:: InpStreamBuff() {}
InpStreamBuff::~InpStreamBuff() {}

WeakStr InpStreamBuff::FileName() { return SharedStr(); }

CharPtr InpStreamBuff::GetDataBegin()
{
	throwIllegalAbstract(MG_POS, "InpStreamBuff::GetDataBegin()");
}

CharPtr InpStreamBuff::GetDataEnd()
{
	throwIllegalAbstract(MG_POS, "InpStreamBuff::GetDataEnd()");
}

void InpStreamBuff::SetCurrPos(streamsize_t pos)
{
	throwIllegalAbstract(MG_POS, "InpStreamBuff::SetCurrPos(streamsize_t pos)");
}

/********** OutStreamBuff CODE **********/

OutStreamBuff:: OutStreamBuff() {}
OutStreamBuff::~OutStreamBuff() {}
WeakStr OutStreamBuff::FileName() { return SharedStr(); }

void OutStreamBuff::WriteBytes(CharPtr cstr)
{
	WriteBytes(cstr, StrLen(cstr)); 
}

