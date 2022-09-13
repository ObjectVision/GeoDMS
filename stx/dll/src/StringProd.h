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


#if !defined(__STX_STRINGPROD_H)
#define __STX_STRINGPROD_H

#include "StxInterface.h"
#include "TextPosition.h"

#include "ptr/SharedStr.h"

///////////////////////////////////////////////////////////////////////////////
//
//  Product Holder for multi purpose string grammar
//
///////////////////////////////////////////////////////////////////////////////


struct StringProdBase
{
	void ProdFirstStringValue() {}
	void ProdNextStringValue() {}

	void ProdStringLiteral1(CharPtr first, CharPtr last) {}
	void ProdStringLiteral2(CharPtr first, CharPtr last) {}
};

struct StringProd
{
	SharedStr m_StringValue, m_StringLiteral;

	void ProdFirstStringValue() { m_StringValue  = m_StringLiteral; }
	void ProdNextStringValue () { m_StringValue += m_StringLiteral; }

	SYNTAX_CALL void ProdStringLiteral1(CharPtr first, CharPtr last);
	SYNTAX_CALL void ProdStringLiteral2(CharPtr first, CharPtr last);

	CharPtr c_str() const { return m_StringValue.c_str(); }
	CharPtr begin() const { return m_StringValue.begin(); }
	CharPtr send () const { return m_StringValue.send (); }

	void clear()  { m_StringValue.clear(); m_StringLiteral.clear(); }
	bool empty() const { return m_StringValue.empty() && m_StringLiteral.empty(); }
};


#endif //!defined(__STX_STRINGPROD_H)

