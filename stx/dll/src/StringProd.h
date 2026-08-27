// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////


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

	void ProdStringLiteral1(CharPtr first, CharPtr last, const text_position* pos = nullptr) {}
	void ProdStringLiteral2(CharPtr first, CharPtr last, const text_position* pos = nullptr) {}
};

struct StringProd
{
	SharedStr m_StringValue, m_StringLiteral;

	void ProdFirstStringValue() { m_StringValue  = m_StringLiteral; }
	void ProdNextStringValue () { m_StringValue += m_StringLiteral; }

	void ProdStringLiteral1(CharPtr first, CharPtr last, const text_position* pos = nullptr);
	void ProdStringLiteral2(CharPtr first, CharPtr last, const text_position* pos = nullptr);

	CharPtr c_str() const { return m_StringValue.c_str(); }
	CharPtr begin() const { return m_StringValue.begin(); }
	CharPtr send () const { return m_StringValue.send (); }

	void clear()  { m_StringValue.clear(); m_StringLiteral.clear(); }
	bool empty() const { return m_StringValue.empty() && m_StringLiteral.empty(); }
};


#endif //!defined(__STX_STRINGPROD_H)

