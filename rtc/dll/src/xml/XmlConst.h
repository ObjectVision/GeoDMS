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


#ifndef __XML_XMLCONST_H
#define __XML_XMLCONST_H

#include "dbg/Check.h"

const UInt32 MAX_TOKEN_LEN = 4;

struct CompCharPtr
{
	bool operator ()(CharPtr a, CharPtr b) const
	{
		while ((signed char&)*a >= (signed char&)*b && *a != ';')
		{
			if (!*b || *b == ';' || (signed char&)*a++ > (signed char&)*b++) 
				return false;
		}
		return *b != ';';
	}
};

extern CharPtr XmlConstTable[256];

inline CharPtr CharGetSymbol(char ch) { return XmlConstTable[UInt8(ch)]; }

char SymbolGetChar(CharPtr symbol);

#endif // __XML_XMLCONST_H
