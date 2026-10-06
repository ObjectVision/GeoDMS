// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////
#pragma once


#ifndef __XML_XMLCONST_H
#define __XML_XMLCONST_H

#include "dbg/Diagnostics.h"

// The longest entity name XmlParser::AppendEntity accepts. The registered names are 2 to 4
// characters and numeric references such as &#8212; are 5, so this only bounds the stack buffer
// against a run of text after an unterminated '&'.
const UInt32 MAX_TOKEN_LEN = 32;

// Orders entity names that are terminated by ';' OR by NUL: the keys in XmlConstMap are plain names,
// while SymbolGetChar may receive a ';'-terminated slice of a longer text. Both sides are compared
// as if truncated at their first ';' or NUL, so "amp;b" and "amp" are equal. The previous version
// treated the two terminators asymmetrically, "amp;b" ordered before "amp" but not the reverse, so a
// ';'-terminated name never found its key and every entity in an attribute value was unknown.
struct CompCharPtr
{
	static bool IsEnd(char c) { return c == 0 || c == ';'; }

	bool operator ()(CharPtr a, CharPtr b) const
	{
		for (;; ++a, ++b)
		{
			bool aEnd = IsEnd(*a), bEnd = IsEnd(*b);
			if (aEnd || bEnd)
				return aEnd && !bEnd; // a proper prefix orders first; equal names are not less
			if (*a != *b)
				return static_cast<unsigned char>(*a) < static_cast<unsigned char>(*b);
		}
	}
};

extern CharPtr XmlConstTable[256];

inline CharPtr CharGetSymbol(char ch) { return XmlConstTable[UInt8(ch)]; }

char SymbolGetChar(CharPtr symbol);

#endif // __XML_XMLCONST_H
