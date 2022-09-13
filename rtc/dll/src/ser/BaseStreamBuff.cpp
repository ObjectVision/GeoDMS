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


