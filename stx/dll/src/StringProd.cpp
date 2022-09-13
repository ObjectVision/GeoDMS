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
#include "StxPch.h"
#pragma hdrstop

#include "StringProd.h"

#include "dbg/Check.h"
#include "utl/Quotes.h"
#include "Parallel.h"

///////////////////////////////////////////////////////////////////////////////
//
//  Product Holder for multi purpose string grammar
//
///////////////////////////////////////////////////////////////////////////////

void StringProd::ProdStringLiteral1(CharPtr first, CharPtr last) 
{ 
	dms_assert(last);
	if (*last != '\'')
		throwErrorD("ParseString", "single quoted string terminator expected");
	SingleUnQuoteMiddle(m_StringLiteral, first, last); 
}

void StringProd::ProdStringLiteral2(CharPtr first, CharPtr last) 
{ 
	dms_assert(last);
	if (*last != '\"')
		throwErrorD("ParseString", "double quoted string terminator expected");
	DoubleUnQuoteMiddle(m_StringLiteral, first, last); 
}

