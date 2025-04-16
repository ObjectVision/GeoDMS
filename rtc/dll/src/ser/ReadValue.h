// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#pragma once

#if !defined(__RTC_SER_READVALUE_H)
#define __RTC_SER_READVALUE_H


// *****************************************************************************
// Section:     Helper Funcs
// *****************************************************************************

inline void SkipSpace(CharPtr& src, CharPtr end)
{
	while ((src != end) && isspace(UChar(*src)))
	{
		dms_assert(*src); // NULL is not a space
		++src;
	}
}

inline void SkipSpace(CharPtr& src)
{
	while (isspace(UChar(*src)))
	{
		dms_assert(*src); // NULL is not a space
		++src;
	}
}

inline void SkipSpace(FormattedInpStream& str)
{
	while (isspace(UChar(str.NextChar())))
	{
		dms_assert(str.NextChar()); // NULL is not a space
		str.ReadChar();
	}
}


template <typename T> const SizeT max_scan_size_v = 40;




template <typename T>
T ReadValue(CharPtr src, CharPtr end)
{
	T value;
	auto fromResult = std::from_chars(src, end, value);
	if (fromResult.ec == std::errc())
		return value;
	return UNDEFINED_VALUE(T);
}

template <typename T>
T ReadValue(FormattedInpStream& str)
{
	T value;
	assert(str.CurrPos() != -1);
	if (str.IsCommaDecimalSeparator())
	{
		char buffer[max_scan_size_v<T>];
		char* bufferPtr = buffer;
		char* bufferEnd = buffer + sizeof(buffer);
		auto f = str.Buffer().GetDataBegin() + str.CurrPos();
		auto e = str.Buffer().GetDataEnd();
		while (f != e && bufferPtr != bufferEnd)
		{
			if (*f == ',')
			{
				*bufferPtr++ = '.'; // replace comma with period
				++f;
			}
			else	
				*bufferPtr++ = *f++;
		}
		auto fromResult = std::from_chars(buffer, bufferPtr, value);
		if (fromResult.ec != std::errc())
			return UNDEFINED_VALUE(T);
		str.SetCurrPos(str.CurrPos() + (bufferPtr - buffer));
		return value;
	}

	auto  f = str.Buffer().GetDataBegin() + str.CurrPos();
	auto fromResult = std::from_chars(f, str.Buffer().GetDataEnd(), value);

	if (fromResult.ec != std::errc())
		return UNDEFINED_VALUE(T);
	str.SetCurrPos(fromResult.ptr - str.Buffer().GetDataBegin());
	return value;
}

bool ReadNull(FormattedInpStream& str)
{
	if (str.NextChar() != 'n')
		return false;
	str.ReadChar();
	if (str.NextChar() == 'u')
	{
		str.ReadChar();
		if (str.NextChar() == 'l')
		{
			str.ReadChar();
			if (str.NextChar() == 'l')
				str.ReadChar();
		}
	}
	return true;
}

template <typename T>
T ReadValueOrNull(FormattedInpStream& str)
{
	if (ReadNull(str))
		return UNDEFINED_VALUE(T);
	return ReadValue<T>(str);
}

template <typename T>
T ReadValueAfterSpace(CharPtr src, CharPtr end)
{
	SkipSpace(src, end);
	return ReadValue<T>(src, end);
}

template <typename T>
T ReadValueAfterSpace(CharPtr src)
{
	SkipSpace(src);
	return ReadValue<T>(src, src+max_scan_size_v<T>);
}

template <typename T>
T ReadValueAfterSpace(FormattedInpStream& str)
{
	SkipSpace(str);
	return ReadValue<T>(str);
}

template <typename T>
T ReadValueOrNullAfterSpace(FormattedInpStream& str)
{
	SkipSpace(str);
	return ReadValueOrNull<T>(str);
}


#endif //!defined(__RTC_SER_READVALUE_H)
