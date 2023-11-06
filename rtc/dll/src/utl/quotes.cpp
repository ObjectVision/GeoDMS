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

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

#include "utl/Quotes.h"
#include "dbg/debug.h"
#include "geo/SequenceArray.h"
#include "mem/FixedAlloc.h"
#include "ptr/SharedStr.h"
#include "ser/FormattedStream.h"

//================= private helper functions

//================= size funcs

static SizeT sizeSingleQuouteMiddle(CharPtr str)
{
	SizeT result = 0;
	
	while (true)
	{
		switch (*str++) 
		{
			case '\0': return result;
			case '\'':
			case '\\':
			case '\t':
			case '\r':
			case '\n': ++result;
		}
		++result;
	}
}

static SizeT sizeSingleQuouteMiddle(CharPtr begin, CharPtr end)
{
	dms_assert(end || !begin);
	SizeT result = 0;
	
	while (begin!=end)
	{
		switch (*begin++) 
		{
			case '\0':
			case '\'':
			case '\\':
			case '\t':
			case '\r':
			case '\n': ++result;
		}
		++result;
	}
	return result;
}

static SizeT sizeDoubleQuouteMiddle(CharPtr str)
{ 
	SizeT result = 0;
	
	while (true)
	{
		switch (*str++) 
		{
			case '\0': return result;
			case '\"':
			case '\\':
			case '\t':
			case '\r':
			case '\n': ++result;
		}
		++result;
	}
}

static SizeT sizeDoubleQuouteMiddle(CharPtr begin, CharPtr end)
{ 
	dms_assert(end || !begin);
	SizeT result = 0;
	
	while (begin!=end)
	{
		switch (*begin++) 
		{
			case '\0':
			case '\"':
			case '\\':
			case '\t':
			case '\r':
			case '\n': ++result;
		}
		++result;
	}
	return result;
}

//================= single quote

char* _SingleQuoteMiddle(char* buf, CharPtr str)
{
	dms_assert(str);
	char ch;
	while (true)
	{
		switch (ch = *str++)
		{
			case '\0': return buf;
			case '\'': *buf++ = '\''; break;
			case '\t': ch = 't'; goto placeExtraSlash;
			case '\r': ch = 'r'; goto placeExtraSlash;
			case '\n': ch = 'n';
		placeExtraSlash:
			[[fallthrough]];
			case '\\': *buf++ = '\\'; break;
		}
		*buf++ = ch;
	}
}

char* _SingleQuoteMiddle(char* buf, CharPtr begin, CharPtr end)
{
	dms_assert(end || !begin);
	char ch;
	while (begin != end)
	{
		switch (ch = *begin++) 
		{
			case '\'': *buf++ = '\''; break;
			case '\0': ch = '0'; goto placeExtraSlash;
			case '\t': ch = 't'; goto placeExtraSlash;
			case '\r': ch = 'r'; goto placeExtraSlash;
			case '\n': ch = 'n';
		placeExtraSlash:
			[[fallthrough]];
			case '\\': *buf++ = '\\'; break;
		}
		*buf++ = ch;
	}
	return buf;
}

inline char* _SingleQuote(char* buf, CharPtr str)
{
	*buf++ = '\''; 
	buf = _SingleQuoteMiddle(buf, str);
	*buf++ = '\''; 
	return buf;
}

inline char* _SingleQuote(char* buf, CharPtr begin, CharPtr end)
{
	dms_assert(end || !begin);
	*buf++ = '\''; 
	buf = _SingleQuoteMiddle(buf, begin, end);
	*buf++ = '\''; 
	return buf;
}

SharedStr SingleQuote(CharPtr str)
{
	SharedCharArray* result = SharedCharArray::CreateUninitialized(sizeSingleQuouteMiddle(str)+3);
	SharedStr resultStr(result);

	char* resEnd = _SingleQuote(result->begin(), str);
	*resEnd = 0;
	dms_assert(resultStr.ssize() == resEnd - result->begin());
	return resultStr;
}

void SingleQuote  (StringRef& result, CharPtr begin, CharPtr end)
{
	dms_assert(end || !begin);
	result.resize_uninitialized(sizeSingleQuouteMiddle(begin, end) + 2);
	end = _SingleQuote(&result[0], begin, end);
	dms_assert(result.size() == end - &(result[0]));
}

void SingleQuote  (SharedStr& result, CharPtr begin, CharPtr end)
{
	dms_assert(end || !begin);
	SharedCharArray* resPtr = SharedCharArray::CreateUninitialized( sizeSingleQuouteMiddle(begin, end)+3 );
	result = SharedStr(resPtr);
	char* resEnd = _SingleQuote(resPtr->begin(), begin, end);
	*resEnd = 0;
	dms_assert(result.ssize() == resEnd - result.begin());
}

SharedStr SingleQuote(CharPtr begin, CharPtr end)
{
	SharedStr result;
	SingleQuote(result, begin, end);
	return result;
}

void _ProcessSpecialChar(OutStreamBuff& buf, CharPtr& strBegin, CharPtr str, CharPtr strSpecial)
{
	buf.WriteBytes(strBegin, str - strBegin-1); 
	if (!strSpecial)
		return; // at end of processing

	strBegin = str;
	buf.WriteBytes(strSpecial,2); 
}

void _SingleQuoteMiddle(OutStreamBuff& buf, CharPtr str)
{
	CharPtr strBegin = str;
	while (true)
	{
		switch (*str++) 
		{
			case 0:    _ProcessSpecialChar(buf, strBegin, str, 0     ); return;
			case '\'': _ProcessSpecialChar(buf, strBegin, str, "\'\'"); break;
			case '\t': _ProcessSpecialChar(buf, strBegin, str, "\\t" ); break;
			case '\r': _ProcessSpecialChar(buf, strBegin, str, "\\r" ); break; 
			case '\n': _ProcessSpecialChar(buf, strBegin, str, "\\n" ); break; 
			case '\\': _ProcessSpecialChar(buf, strBegin, str, "\\\\"); break; 
		// default: do nothing, str is already incremented
		}
	}
}

void _SingleQuoteMiddle(OutStreamBuff& buf, CharPtr begin, CharPtr end)
{
	dms_assert(end || !begin);
	CharPtr strBegin = begin;
	while (begin != end)
	{
		switch (*begin++) 
		{
			case 0:    _ProcessSpecialChar(buf, strBegin, begin, "\\0" ); break;
			case '\'': _ProcessSpecialChar(buf, strBegin, begin, "\'\'"); break;
			case '\t': _ProcessSpecialChar(buf, strBegin, begin, "\\t" ); break;
			case '\r': _ProcessSpecialChar(buf, strBegin, begin, "\\r" ); break; 
			case '\n': _ProcessSpecialChar(buf, strBegin, begin, "\\n" ); break; 
			case '\\': _ProcessSpecialChar(buf, strBegin, begin, "\\\\"); break; 
		// default: do nothing, begin is already incremented
		}
	}
	_ProcessSpecialChar(buf, strBegin, ++end, 0);
}

inline void _SingleQuote(OutStreamBuff& buf, CharPtr str)
{
	buf.WriteByte('\''); 
	_SingleQuoteMiddle(buf, str);
	buf.WriteByte('\''); 
}

inline void _SingleQuote(OutStreamBuff& buf, CharPtr begin, CharPtr end)
{
	buf.WriteByte('\''); 
	_SingleQuoteMiddle(buf, begin, end);
	buf.WriteByte('\''); 
}

void SingleQuote(struct FormattedOutStream& os,CharPtr str)
{
	_SingleQuote(os.Buffer(), str);
}

void SingleQuote(struct FormattedOutStream& os, CharPtr begin, CharPtr end)
{
	_SingleQuote(os.Buffer(), begin, end);
}

//================= double quote

char* _DoubleQuoteMiddle(char* buf, CharPtr str)
{
	char ch;
	while (true)
	{
		switch (ch = *str++) 
		{
			case 0:    return buf;
			case '\t': ch = 't'; goto placeExtraSlash;
			case '\r': ch = 'r'; goto placeExtraSlash;
			case '\n': ch = 'n';
		placeExtraSlash:
			[[fallthrough]];
			case '\"':
			case '\\': *buf++ = '\\'; 
		}
		*buf++ = ch;
	}
}

char* _DoubleQuoteMiddle(char* buf, CharPtr begin, CharPtr end)
{
	dms_assert(end || !begin);
	char ch;
	while (begin != end)
	{
		switch (ch = *begin++)
		{
			case '\0': ch = '0'; goto placeExtraSlash;
			case '\t': ch = 't'; goto placeExtraSlash;
			case '\r': ch = 'r'; goto placeExtraSlash;
			case '\n': ch = 'n';
		placeExtraSlash:
			[[fallthrough]];
			case '\"':
			case '\\': *buf++ = '\\'; 
		}
		*buf++ = ch;
	}
	return buf;
}

inline char* _DoubleQuote(char* buf, CharPtr str)
{
	*buf++ = '\"'; 
	buf = _DoubleQuoteMiddle(buf, str);
	*buf++ = '\"'; 
	return buf;
}

inline char* _DoubleQuote(char* buf, CharPtr begin, CharPtr end)
{
	dms_assert(end || !begin);
	*buf++ = '\"'; 
	buf = _DoubleQuoteMiddle(buf, begin, end);
	*buf++ = '\"'; 
	return buf;
}

SharedStr DoubleQuote(CharPtr str)
{
	SharedCharArray* result = SharedCharArray::CreateUninitialized(sizeDoubleQuouteMiddle(str)+3);
	SharedStr resultStr(result);

	char* resEnd = _DoubleQuote(result->begin(), str);
	*resEnd = 0;
	dms_assert(result->size()-1 == resEnd - result->begin());
	return resultStr;
}


void DoubleQuote(sequence_array<char>::reference& result, CharPtr begin, CharPtr end)
{
	dms_assert(end || !begin);
	result.resize_uninitialized(sizeDoubleQuouteMiddle(begin, end)+2);
	end = _DoubleQuote(&(result[0]), begin, end);
	dms_assert(result.size() == end - &(result[0]));
}


void _DoubleQuoteMiddle(OutStreamBuff& buf, CharPtr str)
{
	CharPtr strBegin = str;
	while (true)
	{
		switch (*str++) 
		{
			case 0:    _ProcessSpecialChar(buf, strBegin, str, 0     ); return;
			case '\"': _ProcessSpecialChar(buf, strBegin, str, "\\\""); break;
			case '\t': _ProcessSpecialChar(buf, strBegin, str, "\\t" ); break;
			case '\r': _ProcessSpecialChar(buf, strBegin, str, "\\r" ); break; 
			case '\n': _ProcessSpecialChar(buf, strBegin, str, "\\n" ); break; 
			case '\\': _ProcessSpecialChar(buf, strBegin, str, "\\\\"); break; 
		// default: do nothing, str is already incremented
		}
	}
}

void DoubleQuoteMiddle(OutStreamBuff& buf, CharPtr begin, CharPtr end)
{
	dms_assert(end || !begin);
	CharPtr strBegin = begin;
	while (begin!=end)
	{
		switch (*begin++) 
		{
			case 0:    _ProcessSpecialChar(buf, strBegin, begin, "\\0" ); break;
			case '\"': _ProcessSpecialChar(buf, strBegin, begin, "\\\""); break;
			case '\t': _ProcessSpecialChar(buf, strBegin, begin, "\\t" ); break;
			case '\r': _ProcessSpecialChar(buf, strBegin, begin, "\\r" ); break; 
			case '\n': _ProcessSpecialChar(buf, strBegin, begin, "\\n" ); break; 
			case '\\': _ProcessSpecialChar(buf, strBegin, begin, "\\\\"); break; 
		// default: do nothing, begin is already incremented
		}
	}
	_ProcessSpecialChar(buf, strBegin, ++end, 0);
}

inline void _DoubleQuote(OutStreamBuff& buf, CharPtr str)
{
	buf.WriteByte('\"'); 
	_DoubleQuoteMiddle(buf, str);
	buf.WriteByte('\"'); 
}

inline void _DoubleQuote(OutStreamBuff& buf, CharPtr begin, CharPtr end)
{
	buf.WriteByte('\"'); 
	DoubleQuoteMiddle(buf, begin, end);
	buf.WriteByte('\"'); 
}

void DoubleQuote(struct FormattedOutStream& os, CharPtr str)
{
	_DoubleQuote(os.Buffer(), str);
}

void DoubleQuote(struct FormattedOutStream& os, CharPtr begin, CharPtr end)
{
	_DoubleQuote(os.Buffer(), begin, end);
}


//================= unquote middle

// counts nr of chars until 
// - single quote '\'' (not included in count)
// - end (if applicable)
// - '\0', even after non counted '\\'
// "\'\'" is counted as one
// '\\' is skipped in counting, even before '\0' or end

SizeT _SingleUnQuoteMiddleSize(CharPtr str)
{
	SizeT c = 0;
	while (true)  
	{
		switch (*str++) 
		{
			case '\\': 
				if (!*str++) // count only next ch if not '\0'
					goto exit;
				break;
			case '\'':   // stop at single quote '\'' unless followed by another one 
				if (*str++ == '\'')  // count first '\'' and skip second '\'' and continue
					break;
				[[fallthrough]];
			case 0:
				goto exit;  // stop at '\0' or swallow ch.
		}
		++c;
	}
exit:
	return c;
}

SizeT _SingleUnQuoteMiddleSize(CharPtr begin, CharPtr end)
{
	dms_assert(end || !begin);
	SizeT c = 0;
	while (begin != end)    // stop at end
	{
		switch (*begin++) 
		{
			case '\\': 
				if (begin == end || !*begin++) // count only next ch if not '\0'
					goto exit;
				break;
			case '\'':   // stop at single quote '\'' unless followed by another one 
				if (begin != end && *begin++ == '\'')  // count first '\'' and skip second '\'' and continue; notice end to avoid buggy unquotation of sequence arrays like 'a''b''c''d'
					break;
				[[fallthrough]];
			case 0:   
				goto exit;  // stop at '\0' or swallow ch.
		}
		++c;
	}
exit:
	return c;
}

char* _SingleUnQuoteMiddle(char* buf, CharPtr str)
{
	char ch;
	while (true)  
	{
		ch = *str++; // stop at '\0' or eat ch.
		switch (ch)
		{
			case '\\':
				ch = *str++; 
				switch (ch) // transform ch
				{
					case '\0': goto exit; // eat only next ch if not '\0' or end
					case '0': ch = '\0'; break;
					case 't': ch = '\t'; break;
					case 'r': ch = '\r'; break;
					case 'n': ch = '\n'; break;
				}
				break;
			case '\'':   // stop at single quote '\'' unless followed by another one 
				if(*str++ == '\'') // eat first if second '\'' available to be skipped else exit
					break;
				[[fallthrough]];
			case 0:
				goto exit;
		}
		*buf++ = ch; // swallow it
	}
exit:
	return buf;
}

char* _SingleUnQuoteMiddle(char* buf, CharPtr begin, CharPtr end)
{
	dms_assert(end || !begin);
	char ch;
	while (begin != end)    // stop at end
	{
		ch = *begin++; // stop at '\0' or eat ch.
		switch (ch)
		{
			case '\\':
				if (begin == end)
					goto exit;
				ch = *begin++; 
				switch (ch) // transform ch
				{
					case '\0': goto exit; // eat only next ch if not '\0' or end
					case '0': ch = '\0'; break;
					case 't': ch = '\t'; break;
					case 'r': ch = '\r'; break;
					case 'n': ch = '\n'; break;
				}
				break;
			case '\'':   // stop at single quote '\'' unless followed by another one 
				if(begin != end && *begin++ == '\'') // eat first if second '\'' available to be skipped else exit
					break;
				[[fallthrough]];
			case 0:
				goto exit;
		}
		*buf++ = ch; // swallow it
	}
exit:
	return buf;
}

SharedStr SingleUnQuoteMiddle(CharPtr str)
{
	auto sz = _SingleUnQuoteMiddleSize(str);
	if (!sz)
		return SharedStr();
	SharedCharArray* result = SharedCharArray::CreateUninitialized(sz+1);
	SharedStr resultStr(result);

	char* resEnd = _SingleUnQuoteMiddle(result->begin(), str);
	*resEnd = 0;
	dms_assert(result->size() == resEnd - result->begin());
	return resultStr;
}

void SingleUnQuoteMiddle(SharedStr& resStr, CharPtr begin, CharPtr end)
{
	dms_assert(!begin || (end && (*end == '\'' || !*end)));

	auto sz = _SingleUnQuoteMiddleSize(begin, end);
	if (!sz)
		resStr.clear();
	else
	{
		SharedCharArray* result = SharedCharArray::CreateUninitialized(sz+1);
		resStr = SharedStr(result);
		char* resEnd = _SingleUnQuoteMiddle(result->begin(), begin, end);
		*resEnd = 0;
		dms_assert(resStr.ssize() == resEnd - resStr.begin());
	}
}

SharedStr SingleUnQuoteMiddle(CharPtr begin, CharPtr end)
{
	SharedStr result;
	SingleUnQuoteMiddle(result, begin, end);
	return result;
}

SizeT _DoubleUnQuoteMiddleSize(CharPtr str)
{
	SizeT c = 0;
	while (true)
	{
		switch (*str++)
		{
			case '\\': if (*str++) break; // eat only next ch if not '\0' or end
				[[fallthrough]];
			case '\"':
			case 0: 
				return c;
		}
		++c;
	}
}

SizeT _DoubleUnQuoteMiddleSize(CharPtr begin, CharPtr end)
{
	dms_assert(end || !begin);

	SizeT c = 0;
	while (begin != end)
	{
		switch (*begin++)
		{
			case '\\': 
				if (begin != end && *begin++) break; // eat only next ch if not '\0' or end
				[[fallthrough]];
			case '\"':
			case 0: 
				goto exit;
		}
		++c;
	}
exit:
	return c;
}

char* _DoubleUnQuoteMiddle(char* buf, CharPtr str)
{
	char ch;
	while (true)
	{
		switch (ch = *str++) 
		{
			case 0: 
			case '\"':
				goto exit;
			case '\\':
				ch = *str++;
				switch (ch) // translate second char
				{
					case '\0': goto exit;
					case '0': ch = '\0'; break;
					case 't': ch = '\t'; break;
					case 'r': ch = '\r'; break;
					case 'n': ch = '\n'; break;
				}
		}
		*buf++ = ch;
	}
exit:
	return buf;
}

char* _DoubleUnQuoteMiddle(char* buf, CharPtr begin, CharPtr end)
{
	dms_assert(end || !begin);

	char ch;
	while (begin != end)
	{
		switch (ch = *begin++) 
		{
			case 0: 
			case '\"':
				goto exit;
			case '\\':
				if (begin == end) goto exit;
				ch = *begin++;
				switch (ch) // translate second char
				{
					case '\0': goto exit;
					case '0': ch = '\0'; break;
					case 'r': ch = '\r'; break;
					case 'n': ch = '\n'; break;
					case 't': ch = '\t'; break;
				}
		}
		*buf++ = ch;
	}
exit:
	return buf;
}

SharedStr DoubleUnQuoteMiddle(CharPtr str)
{
	auto sz = _DoubleUnQuoteMiddleSize(str);
	if (!sz)
		return SharedStr();
	SharedCharArray* result = SharedCharArray::CreateUninitialized(sz+1);
	SharedStr resStr(result);

	char* resEnd = _DoubleUnQuoteMiddle(result->begin(), str);
	*resEnd = 0;
	dms_assert(resStr.ssize() == resEnd - result->begin());
	return resStr;
}

void DoubleUnQuoteMiddle(SharedStr& resStr, CharPtr begin, CharPtr end)
{
	dms_assert(!begin || (end && (*end == '"' || !*end)));
	dms_assert(begin || !end);

	auto sz = _DoubleUnQuoteMiddleSize(begin, end);
	if (!sz)
		resStr.clear();
	else
	{
		SharedCharArray* result = SharedCharArray::CreateUninitialized(sz+1);
		resStr = result;
		char* resEnd = _DoubleUnQuoteMiddle(result->begin(), begin, end);
		*resEnd = 0;
		dms_assert(resStr.ssize() == resEnd - result->begin());
	}
}

//================= unquote

SharedStr SingleUnQuote(CharPtr str)
{
	CDebugContextHandle dct("SingleUnQuote", str, true);

	MG_PRECONDITION(str && *str == '\'');
	return SingleUnQuoteMiddle(str+1);
}

SharedStr SingleUnQuote(CharPtr begin, CharPtr end)
{
	CDebugContextHandle dct("SingleUnQuote", begin, true);

	MG_PRECONDITION(begin && begin[0] == '\'');
	MG_PRECONDITION(end   && end [-1] == '\'');
	return SingleUnQuoteMiddle(begin+1, end-1);
}

SharedStr DoubleUnQuote(CharPtr str)
{
	CDebugContextHandle dct("DoubleUnQuote", str, true);

	MG_PRECONDITION(str && *str == '"');
	return DoubleUnQuoteMiddle(str+1);
}

void SingleUnQuote(StringRef& result, CharPtr begin, CharPtr end)
{
	dms_assert(begin+2 <= end);
	MG_PRECONDITION(begin && end!=begin && *begin == '\'' && end && *(end-1) == '\'');

	result.resize_uninitialized(_SingleUnQuoteMiddleSize(++begin, --end));
	if (!result.empty())
	{
		end = _SingleUnQuoteMiddle(&(result[0]), begin, end);
		dms_assert(result.size() == end - &(result[0]));
	}
}

void DoubleUnQuote(StringRef& result, CharPtr begin, CharPtr end)
{
	dms_assert(begin+2 <= end);
	MG_PRECONDITION(begin && end!=begin && *begin == '\"' && end && *(end-1) == '\"');

	result.resize_uninitialized(_DoubleUnQuoteMiddleSize(++begin, --end));
	if (!result.empty())
	{
		end = _DoubleUnQuoteMiddle(&(result[0]), begin, end);
		dms_assert(result.size() == end - &(result[0]));
	}
}
