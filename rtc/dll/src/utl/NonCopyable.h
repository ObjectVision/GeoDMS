// Copyright (C) 2025 Object Vision b.v. 
// License: GNU GPL 3
///////////////////////////////////////////////////////////////////////////// 

#if defined(_MSC_VER)
#pragma once
#endif

#if !defined(__RTC_UTL_NONCOPYABLE_H)
#define __RTC_UTL_NONCOPYABLE_H

namespace geodms { namespace rtc {

struct copyable {};


class noncopyable {
protected:
    noncopyable() = default;
    ~noncopyable() = default;

    noncopyable(const noncopyable&) = delete;
    noncopyable& operator=(const noncopyable&) = delete;
};

} // end of namespace rtc
} // end of namespace geodms

#endif // __RTC_UTL_NONCOPYABLE_H
