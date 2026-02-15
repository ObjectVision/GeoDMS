// Copyright (C) 1998-2026 Object Vision B.V. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "RtcPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

#include "xml/XmlConst.h"

CharPtr XmlConstTable[256];
std::map<CharPtr, Char, CompCharPtr> XmlConstMap;


char SymbolGetChar(CharPtr symbol)
{
	return XmlConstMap[symbol];
}

struct RegisterConst {
	RegisterConst(char ch, CharPtr token)
	{
		XmlConstTable[ch] = token;
		XmlConstMap[token] = ch;
	}
};
namespace {

RegisterConst lt('<',  "lt" );
RegisterConst gt('>',  "gt" );
RegisterConst amp('&', "amp");
RegisterConst apos('\'', "apos");
RegisterConst quot('"',"quot");

// etc.

} // namespace
