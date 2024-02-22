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

#if !defined(__RTC_SER_BASESTREAMBUFF)
#define __RTC_SER_BASESTREAMBUFF

using BytePtr = Byte*;
using CBytePtr = const Byte*;

// *****************************************************************************
// Section:     Basic streambuffer Interface
// *****************************************************************************

class InpStreamBuff
{
public:
	RTC_CALL InpStreamBuff();
	RTC_CALL virtual ~InpStreamBuff();
	RTC_CALL virtual void ReadBytes (BytePtr data, streamsize_t size) const=0;
	RTC_CALL virtual streamsize_t CurrPos() const=0;
	RTC_CALL virtual bool   AtEnd  () const=0;
	RTC_CALL virtual WeakStr FileName();

	RTC_CALL virtual CharPtr GetDataBegin(); 
	RTC_CALL virtual CharPtr GetDataEnd(); 
	RTC_CALL virtual void    SetCurrPos(streamsize_t pos); 
};

class OutStreamBuff
{
public:
	RTC_CALL OutStreamBuff();
	RTC_CALL virtual ~OutStreamBuff();
	RTC_CALL virtual void WriteBytes(CBytePtr data, streamsize_t size) =0;
	RTC_CALL virtual streamsize_t CurrPos() const=0;
	RTC_CALL virtual WeakStr FileName();
	RTC_CALL virtual bool    AtEnd() const = 0;
	void WriteByte(Byte data) { WriteBytes(&data, 1); }
	RTC_CALL void WriteBytes(CharPtr cstr);
};

#endif // __RTC_SER_BASESTREAMBUFF
