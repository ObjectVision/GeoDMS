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

#ifndef __MCI_ABSTRVALUE_H
#define __MCI_ABSTRVALUE_H

//----------------------------------------------------------------------
// used modules and forward class references
//----------------------------------------------------------------------

#include "mci/Object.h"
class ValueClass;
enum class FormattingFlags;

//----------------------------------------------------------------------
// class  : AbstrValue
//----------------------------------------------------------------------

class AbstrValue : public Object
{
	using base_type = Object;
public:
	virtual const ValueClass* GetValueClass() const = 0;

	virtual bool      AsCharArray(char* sink, SizeT buflen, FormattingFlags ff) const=0;
	virtual SizeT     AsCharArraySize(SizeT maxLen, FormattingFlags ff) const = 0;
	virtual SharedStr AsString()  const = 0;
	virtual Float64   AsFloat64() const = 0;
	virtual bool      IsNull()    const = 0;

	virtual void AssignFromCharPtr (CharPtr data) =0;
	virtual void AssignFromCharPtrs(CharPtr first, CharPtr last) =0;

	DECL_ABSTR(RTC_CALL, Class)
};

#endif // __MCI_ABSTRVALUE_H

