// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////
#pragma once


#ifndef __XML_XMLCONST_H
#define __XML_XMLCONST_H

#include "dbg/Diagnostics.h"

// The longest entity name XmlParser::TransformChar accepts. The registered names are 2 to 4
// characters and numeric references such as &#8212; are 5, so this only bounds the stack buffer
// against a run of text after an unterminated '&'.
const UInt32 MAX_TOKEN_LEN = 32;

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
