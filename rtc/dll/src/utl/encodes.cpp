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

#if defined(_MSC_VER)
#pragma hdrstop
#endif

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

	auto result = SharedCharArray::CreateUninitialized(sz + 1);
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
	return SharedStr(result.c_str());
}

SharedStr from_utf(CharPtr first, CharPtr last)
{
	std::string result = boost::locale::conv::from_utf<char>(first, last, GetCp1250Locale()); // s_from_utf_locale);
	return SharedStr(result.c_str());
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

	auto resultPtr = SharedCharArray::Create(n+1, false); // size + zero termination
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
	//SizeT sz = UrlDecodeSize(filenameStr);

	std::string fileName(filenameStr.c_str());

	auto sz = filenameStr.ssize();

	std::string illegalChars = "\\/:?<>|*";
	std::string::iterator it;
	for (it = fileName.begin(); it < fileName.end(); ++it) {
		bool found = illegalChars.find(*it) != std::string::npos;
		if (found)
		{
			*it = '_';
		}
	}

	return SharedStr(fileName.c_str());
}