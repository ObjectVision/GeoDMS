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

#include <boost/locale.hpp>

//================= size funcs

const auto& GetCp1250Locale()
{
	static boost::locale::generator gen;
	static auto usLocale = gen("cp1250");
	return usLocale;
}

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
		return ch >= 32 && ch < 128 && impl::isSafe[ch - 32];
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
	//			throwErrorF("UrlDecode", "invalid character '%c'", *chPtr);
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

SharedStr to_utf(CharPtr first, CharPtr last)
{
	std::string result = boost::locale::conv::to_utf<char>(first, last, GetCp1250Locale()); //  s_to_utf_locale); // , boost::locale::conv::default_method);
	return SharedStr(result.c_str() MG_DEBUG_ALLOCATOR_SRC("to_utf"));
}

#ifndef _WIN32
// Map each UTF-8 accented Latin codepoint to its ASCII base character, matching
// the Windows WinAPI CP1250 best-fit mapping: exactly one output byte per input codepoint.
// Covers Latin-1 Supplement (U+00C0–U+00FF) and Œ/œ (U+0152–U+0153).
// Used instead of iconv(ASCII//TRANSLIT): glibc's built-in iconv has no transliteration
// tables for these characters and produces '?' for each one.
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
#endif

SharedStr from_utf(CharPtr first, CharPtr last)
{
#ifdef _WIN32
	std::string result = boost::locale::conv::from_utf<char>(first, last, GetCp1250Locale());
	return SharedStr(result.c_str() MG_DEBUG_ALLOCATOR_SRC("from_utf"));
#else
	return SharedStr(strip_to_ascii(first, last).c_str() MG_DEBUG_ALLOCATOR_SRC("from_utf"));
#endif
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
		throwErrorF("CheckTreeItemName", "Illegal character '%c' in item-name '%s'", *charPtr, name);
}

void CheckTreeItemPath(CharPtr name)
{
	auto charPtr = ParseTreeItemPath(name);
	if (*charPtr)
		throwErrorF("CheckTreeItemPath", "Illegal character '%c' in item-path '%s'", *charPtr, name);
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
