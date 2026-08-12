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

#include "utl/Encodes.h"

#include "dbg/debug.h"
#include "geo/SequenceArray.h"
#include "ptr/SharedStr.h"
#include "ser/FormattedStream.h"
#include "xct/DmsException.h"

#include <string>

namespace url
{

	/*	URL Encoding replaces spaces with "+" signs, and unsafe ASCII characters with "%" followed by their hex equivalent.
		Safe characters are defined in RFC2396. They are the 7-bit ASCII alphanumerics and the mark characters "-_.!~*'()".
		(Note that the standard JavaScript escape and unescape functions operate slightly differently: they encode space as "%20", and treat "+" as a safe character.)
		see: http://www.albionresearch.com/misc/urlencode.php
		and: http://www.ietf.org/rfc/rfc2396.txt
	*/
	namespace impl
	{
		static bool isInitialized = false;
		static bool isSafe[128 - 32];

		void SetRange(char first, char count)
		{
			dms_assert(first >= 32);
			first -= 32;
			dms_assert(count < 128 - 32 && first + count <= 128 - 32);
			bool* firstPtr = isSafe + first;
			fast_fill(firstPtr, firstPtr + count, true);
		}
		void SetChars(CharPtr chPtr)
		{
			while (*chPtr)
				isSafe[*chPtr++ - char(32)] = true;
		}
		void InitSafeChars()
		{
			if (isInitialized)
				return;
			SetRange('0', 10);
			SetRange('A', 26);
			SetRange('a', 26);
			SetChars("-_.!~*\'()");
			SetChars("+"); // count as one character; transforms to space 
			isInitialized = true;
		}
	}
	bool IsSafeChar(char ch)
	{
		impl::InitSafeChars();
		unsigned char uch = ch;
		return uch >= 32 && uch < 128 && impl::isSafe[uch - 32];
	}
}

bool isHex(char ch)
{
	return (ch >= '0' && ch <= '9') || (ch >= 'A' && ch <= 'F') || (ch >= 'a' && ch <= 'f');
}

char hexVal(char ch)
{
	dms_assert(isHex(ch));

	if (ch >= '0' && ch <= '9')
		return ch - '0';

	if (ch >= 'A' && ch <= 'F')
		return ch - ('A' - 10);

	if (ch >= 'a' && ch <= 'f')
		return ch - ('a' - 10);

	throwErrorD("RTC", "Unexpected char in UrlDecode.hexVal)");
}

SizeT UrlDecodeSize(WeakStr urlStr)
{
	SizeT c = 0;
	for (CharPtr chPtr = urlStr.begin(), chEnd = urlStr.send(); chPtr != chEnd; ++chPtr)
		if (*chPtr == '%')
		{
			++c;
			if (++chPtr == chEnd || !isHex(*chPtr) || ++chPtr == chEnd || !isHex(*chPtr))
				throwErrorD("UrlDecode", "invalid escape code");
		}
	//		else if (!url::IsSafeChar(*chPtr))
	//			throwErrorF("UrlDecode", "invalid character '{}'", *chPtr);
	assert(3 * c <= urlStr.ssize());
	return urlStr.ssize() - 2 * c;
}

SharedStr UrlDecode(WeakStr urlStr)
{
	SizeT sz = UrlDecodeSize(urlStr);
	if (sz == urlStr.ssize() && urlStr.find('+') == urlStr.send())
		return urlStr;

	auto result = SharedCharArray::CreateUninitialized(sz + 1 MG_DEBUG_ALLOCATOR_SRC("UrlDecode"));
	SharedStr resultStr(result); // assign ownership

	char* resultPtr = result->begin();
	for (CharPtr chPtr = urlStr.begin(), chEnd = urlStr.send(); chPtr != chEnd; ++resultPtr, ++chPtr)
	{
		char ch = *chPtr;
		if (ch == '%')
		{
			ch = (hexVal(*++chPtr) << 4);
			ch += hexVal(*++chPtr);
		}
		else if (ch == '+')
			ch = ' ';
		*resultPtr = ch;
	}
	assert(resultPtr == resultStr.csend());
	*resultPtr = char(0);
	return resultStr;
}

/*	UrlEncode is the exact inverse of UrlDecode above: every byte of the unreserved set of
	RFC2396 (the 7-bit alphanumerics plus the mark characters "-_.!~*'()") passes through, a
	space becomes '+', and every other byte becomes %XX with UPPERCASE hex digits. Hence
	UrlDecode(UrlEncode(s)) == s for EVERY byte string s, non-ASCII (e.g. UTF-8) included.

	NB url::IsSafeChar is deliberately NOT used here: its table also marks '+' as safe -- it
	exists to SIZE the decode result, where "+" is one character -- and letting a literal '+'
	through unescaped would silently decode back as a space. It is escaped as %2B instead.
*/
static bool IsUrlUnreservedChar(unsigned char uch)
{
	if ((uch >= '0' && uch <= '9') || (uch >= 'A' && uch <= 'Z') || (uch >= 'a' && uch <= 'z'))
		return true;
	switch (uch)
	{
		case '-': case '_': case '.': case '!': case '~': case '*': case '\'': case '(': case ')':
			return true;
	}
	return false;
}

static const char s_UpperHexDigits[] = "0123456789ABCDEF";

SharedStr UrlEncode(WeakStr urlStr)
{
	std::string result;
	result.reserve(urlStr.ssize());

	for (CharPtr chPtr = urlStr.begin(), chEnd = urlStr.send(); chPtr != chEnd; ++chPtr)
	{
		unsigned char uch = *chPtr;
		if (IsUrlUnreservedChar(uch))
			result += char(uch);
		else if (uch == ' ')
			result += '+';
		else
		{
			result += '%';
			result += s_UpperHexDigits[uch >> 4];
			result += s_UpperHexDigits[uch & 0x0F];
		}
	}
	return SharedStr(result MG_DEBUG_ALLOCATOR_SRC("UrlEncode"));
}

/*	HtmlEncode / HtmlDecode work on the five predefined XML/HTML entities -- the same set that
	the XML/HTML output stream escapes (see the RegisterConst table in xml/XmlConst.cpp), listed
	once here so that HtmlDecode is by construction the inverse of HtmlEncode. The table is NOT
	shared with XmlConst.cpp: its reverse lookup is a std::map::operator[] on a global map, which
	would INSERT on an unknown entity name (and race between worker threads).

	HtmlEncode touches nothing else: bytes >= 0x80 pass through unchanged, so UTF-8 input stays
	UTF-8 instead of becoming a stream of numeric character references.

	HtmlDecode additionally accepts what a browser writes but HtmlEncode never produces: the
	ubiquitous &nbsp;, and numeric character references &#DDD; and &#xHH; (emitted as UTF-8).
	It is lenient by design -- an unterminated, unknown or out-of-range reference is copied
	through verbatim rather than being an error, since that is what the input meant literally.
*/
static void append_utf8(std::string& out, unsigned cp); // defined with the CP1250 table below

namespace html
{
	struct entity_t { CharPtr m_Name; unsigned m_CodePoint; };

	static const entity_t s_Entities[] = {
		{ "lt"  , '<'    },
		{ "gt"  , '>'    },
		{ "amp" , '&'    },
		{ "apos", '\''   },
		{ "quot", '"'    },
		{ "nbsp", 0x00A0 }, // decode-only: not produced by HtmlEncode
	};

	// the encodable set is the table minus its decode-only tail
	static const entity_t* FindEntityByChar(char ch)
	{
		for (const auto& e : s_Entities)
			if (e.m_CodePoint == static_cast<unsigned char>(ch) && e.m_CodePoint < 0x80)
				return &e;
		return nullptr;
	}

	static const entity_t* FindEntityByName(CharPtr first, CharPtr last)
	{
		for (const auto& e : s_Entities)
		{
			CharPtr n = e.m_Name, p = first;
			while (p != last && *n && *p == *n)
				++p, ++n;
			if (p == last && !*n)
				return &e;
		}
		return nullptr;
	}

	// parse "#DDD" or "#xHH" (the part between '&' and ';'); returns false if malformed
	static bool ParseNumericRef(CharPtr first, CharPtr last, unsigned& cp)
	{
		assert(first != last && *first == '#');
		++first;
		unsigned base = 10;
		if (first != last && (*first == 'x' || *first == 'X'))
		{
			base = 16;
			++first;
		}
		if (first == last)
			return false;

		unsigned value = 0;
		for (; first != last; ++first)
		{
			unsigned digit;
			if (*first >= '0' && *first <= '9')
				digit = *first - '0';
			else if (base == 16 && isHex(*first))
				digit = hexVal(*first);
			else
				return false;
			value = value * base + digit;
			if (value > 0x10FFFF) // beyond the last Unicode code point
				return false;
		}
		cp = value;
		return true;
	}
} // namespace html

SharedStr HtmlEncode(WeakStr htmlStr)
{
	std::string result;
	result.reserve(htmlStr.ssize());

	for (CharPtr chPtr = htmlStr.begin(), chEnd = htmlStr.send(); chPtr != chEnd; ++chPtr)
	{
		if (const auto* e = html::FindEntityByChar(*chPtr))
		{
			result += '&';
			result += e->m_Name;
			result += ';';
		}
		else
			result += *chPtr;
	}
	return SharedStr(result MG_DEBUG_ALLOCATOR_SRC("HtmlEncode"));
}

SharedStr HtmlDecode(WeakStr htmlStr)
{
	std::string result;
	result.reserve(htmlStr.ssize());

	CharPtr chPtr = htmlStr.begin(), chEnd = htmlStr.send();
	while (chPtr != chEnd)
	{
		if (*chPtr != '&')
		{
			result += *chPtr++;
			continue;
		}

		// an entity reference is '&', a name or numeric reference, and a ';'
		CharPtr semiColon = chPtr + 1;
		while (semiColon != chEnd && *semiColon != ';' && *semiColon != '&')
			++semiColon;

		unsigned cp = 0;
		bool isResolved = false;
		if (semiColon != chEnd && *semiColon == ';' && semiColon != chPtr + 1)
		{
			if (chPtr[1] == '#')
				isResolved = html::ParseNumericRef(chPtr + 1, semiColon, cp);
			else if (const auto* e = html::FindEntityByName(chPtr + 1, semiColon))
			{
				cp = e->m_CodePoint;
				isResolved = true;
			}
		}

		if (isResolved)
		{
			append_utf8(result, cp);
			chPtr = semiColon + 1;
		}
		else
			result += *chPtr++; // not a reference we know: the '&' stands for itself
	}
	return SharedStr(result MG_DEBUG_ALLOCATOR_SRC("HtmlDecode"));
}

// Windows-1250 (Central European) single byte -> Unicode code point, for the
// upper half 0x80..0xFF (the lower half 0x00..0x7F maps to itself). 0xFFFD marks
// the five byte positions that are undefined in CP1250. This static table
// replaces boost::locale::conv::to_utf<char>(..., "cp1250").
static const unsigned cp1250_to_unicode[128] = {
	/*80*/ 0x20AC,0xFFFD,0x201A,0xFFFD,0x201E,0x2026,0x2020,0x2021,
	/*88*/ 0xFFFD,0x2030,0x0160,0x2039,0x015A,0x0164,0x017D,0x0179,
	/*90*/ 0xFFFD,0x2018,0x2019,0x201C,0x201D,0x2022,0x2013,0x2014,
	/*98*/ 0xFFFD,0x2122,0x0161,0x203A,0x015B,0x0165,0x017E,0x017A,
	/*A0*/ 0x00A0,0x02C7,0x02D8,0x0141,0x00A4,0x0104,0x00A6,0x00A7,
	/*A8*/ 0x00A8,0x00A9,0x015E,0x00AB,0x00AC,0x00AD,0x00AE,0x017B,
	/*B0*/ 0x00B0,0x00B1,0x02DB,0x0142,0x00B4,0x00B5,0x00B6,0x00B7,
	/*B8*/ 0x00B8,0x0105,0x015F,0x00BB,0x013D,0x02DD,0x013E,0x017C,
	/*C0*/ 0x0154,0x00C1,0x00C2,0x0102,0x00C4,0x0139,0x0106,0x00C7,
	/*C8*/ 0x010C,0x00C9,0x0118,0x00CB,0x011A,0x00CD,0x00CE,0x010E,
	/*D0*/ 0x0110,0x0143,0x0147,0x00D3,0x00D4,0x0150,0x00D6,0x00D7,
	/*D8*/ 0x0158,0x016E,0x00DA,0x0170,0x00DC,0x00DD,0x0162,0x00DF,
	/*E0*/ 0x0155,0x00E1,0x00E2,0x0103,0x00E4,0x013A,0x0107,0x00E7,
	/*E8*/ 0x010D,0x00E9,0x0119,0x00EB,0x011B,0x00ED,0x00EE,0x010F,
	/*F0*/ 0x0111,0x0144,0x0148,0x00F3,0x00F4,0x0151,0x00F6,0x00F7,
	/*F8*/ 0x0159,0x016F,0x00FA,0x0171,0x00FC,0x00FD,0x0163,0x02D9,
};

static void append_utf8(std::string& out, unsigned cp)
{
	if (cp < 0x80)
		out += static_cast<char>(cp);
	else if (cp < 0x800) {
		out += static_cast<char>(0xC0 | (cp >> 6));
		out += static_cast<char>(0x80 | (cp & 0x3F));
	} else if (cp < 0x10000) { // all CP1250 code points are in the BMP (max U+20AC)
		out += static_cast<char>(0xE0 | (cp >> 12));
		out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
		out += static_cast<char>(0x80 | (cp & 0x3F));
	} else { // only reachable from HtmlDecode's numeric character references
		out += static_cast<char>(0xF0 | (cp >> 18));
		out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
		out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
		out += static_cast<char>(0x80 | (cp & 0x3F));
	}
}

SharedStr to_utf(CharPtr first, CharPtr last)
{
	std::string result;
	result.reserve(last - first);
	for (CharPtr p = first; p != last; ++p) {
		unsigned char b = static_cast<unsigned char>(*p);
		append_utf8(result, b < 0x80 ? unsigned(b) : cp1250_to_unicode[b - 0x80]);
	}
	return SharedStr(result.c_str() MG_DEBUG_ALLOCATOR_SRC("to_utf"));
}

// Map each UTF-8 accented Latin codepoint to its ASCII base character, matching
// the Windows WinAPI CP1250 best-fit mapping: exactly one output byte per input codepoint.
// Covers Latin-1 Supplement (U+00C0–U+00FF) and Œ/œ (U+0152–U+0153).
// This is the from_utf transliteration used on all platforms (previously only on
// Linux; Windows formerly used boost::locale, which produced the same result for
// these characters).
static std::string strip_to_ascii(const char* first, const char* last)
{
	// Indexed by (second_byte - 0x80) for 0xC3-prefixed 2-byte sequences (U+00C0–U+00FF).
	static const char latin1_asc[64] = {
	//  À     Á     Â     Ã     Ä     Å     Æ     Ç
	    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'C',
	//  È     É     Ê     Ë     Ì     Í     Î     Ï
	    'E',  'E',  'E',  'E',  'I',  'I',  'I',  'I',
	//  Ð     Ñ     Ò     Ó     Ô     Õ     Ö     ×
	    'D',  'N',  'O',  'O',  'O',  'O',  'O',  '?',
	//  Ø     Ù     Ú     Û     Ü     Ý     Þ     ß
	    'O',  'U',  'U',  'U',  'U',  'Y',  '?',  '?',
	//  à     á     â     ã     ä     å     æ     ç
	    'a',  'a',  'a',  'a',  'a',  'a',  'a',  'c',
	//  è     é     ê     ë     ì     í     î     ï
	    'e',  'e',  'e',  'e',  'i',  'i',  'i',  'i',
	//  ð     ñ     ò     ó     ô     õ     ö     ÷
	    'd',  'n',  'o',  'o',  'o',  'o',  'o',  '?',
	//  ø     ù     ú     û     ü     ý     þ     ÿ
	    'o',  'u',  'u',  'u',  'u',  'y',  '?',  'y',
	};

	std::string out;
	out.reserve(last - first);
	const auto* p   = reinterpret_cast<const unsigned char*>(first);
	const auto* end = reinterpret_cast<const unsigned char*>(last);

	while (p < end) {
		unsigned b0 = *p;
		if (b0 < 0x80) {
			out += static_cast<char>(b0);
			++p;
		} else if (b0 == 0xC3 && p + 1 < end) {
			unsigned b1 = p[1];
			out += (b1 >= 0x80 && b1 <= 0xBF) ? latin1_asc[b1 - 0x80] : '?';
			p += 2;
		} else if (b0 == 0xC5 && p + 1 < end) {
			unsigned b1 = p[1];
			if      (b1 == 0x92) out += 'O'; // Œ U+0152
			else if (b1 == 0x93) out += 'o'; // œ U+0153
			else                 out += '?';
			p += 2;
		} else {
			// No ASCII mapping — skip the entire multibyte sequence
			++p;
			while (p < end && (*p & 0xC0) == 0x80)
				++p;
		}
	}
	return out;
}

SharedStr from_utf(CharPtr first, CharPtr last)
{
	return SharedStr(strip_to_ascii(first, last).c_str() MG_DEBUG_ALLOCATOR_SRC("from_utf"));
}

bool itemName_test(CharPtr p)
{
	if (!p || !*p)
		return false;
	if (!itemNameFirstChar_test(*p))
		return false;
	while (char ch = *++p)
	{
		if (!itemNameNextChar_test(ch))
			return false;
	}
	return true;
}

CharPtr ParseTreeItemName(CharPtr name)
{
	assert(name);
	if (itemNameFirstChar_test(*name))
	{
		++name;
		while (itemNameNextChar_test(*name))
			++name;
	}
	return name;
}

CharPtr ParseTreeItemPath(CharPtr name)
{
	assert(name);
	while (true)
	{
		name = ParseTreeItemName(name); // could be empty
		if (!*name)
			break;
		if (*name != '/')
			break;
		if (!name[1]) // don't allow an item-path to zero-terminate directly after '/'
			break;
		++name;
		if (*name == '/') // don't allow a 2nd '/'
			break;
		assert(*name);
	}
	return name;
}

void CheckTreeItemName(CharPtr name)
{
	CharPtr charPtr = ParseTreeItemName(name);
	if (*charPtr)
		throwErrorF("CheckTreeItemName", "Illegal character '{}' in item-name '{}'", *charPtr, name);
}

void CheckTreeItemPath(CharPtr name)
{
	auto charPtr = ParseTreeItemPath(name);
	if (*charPtr)
		throwErrorF("CheckTreeItemPath", "Illegal character '{}' in item-path '{}'", *charPtr, name);
}


SharedStr as_item_name(CharPtr first, CharPtr last)
{
	SizeT n = last - first;
	if (!n)
		return {};
	if (isdigit((unsigned char)*first))
		++n;

	auto resultPtr = SharedCharArray::Create(n+1, false MG_DEBUG_ALLOCATOR_SRC("as_item_name")); // size + zero termination
	auto resultStr = SharedStr(resultPtr);

	auto dstPtr = resultPtr->begin();
	if (isdigit((unsigned char)*first))
		*dstPtr++ = '_';

	auto dstEnd = fast_copy(first, last, dstPtr);

	for(;dstPtr != dstEnd; ++dstPtr)
		if (!itemNameNextChar_test(*dstPtr))
			*dstPtr = '_';
	dstPtr = resultPtr->begin()+1;
	for (; dstPtr != dstEnd; ++dstPtr)
	{
		if (dstPtr[0] == '_' && dstPtr[-1] == '_')
			break;
	}
	auto dstPtr2 = dstPtr;
	for (; dstPtr != dstEnd; ++dstPtr)
	{
		if (dstPtr[0] == '_' && dstPtr[-1] == '_')
			;
		else
			*dstPtr2++ = *dstPtr;
	}
	resultPtr->erase(dstPtr2, dstPtr);
	resultPtr->end()[-1] = char(0); // provide zero termination 
	return resultStr;
}


SharedStr AsFilename(WeakStr filenameStr)
{
	auto sz = filenameStr.ssize();
	auto resultPtr = SharedCharArray::Create(sz + 1, false MG_DEBUG_ALLOCATOR_SRC("AsFilename")); // size + zero termination
	auto resultStr = SharedStr(resultPtr);

	auto dstPtr = resultPtr->begin();

	static std::string illegalChars = "\\/:?<>|*";
	for (auto i = filenameStr.begin(), e = filenameStr.send(); i != e; ++i)
	{
		bool isIllegalChar = illegalChars.find(*i) != std::string::npos;
		*dstPtr  = (isIllegalChar) ? '_' : *i;
		++dstPtr;
	}
	*dstPtr = char(0); // provide zero termination

	return resultStr;
}
