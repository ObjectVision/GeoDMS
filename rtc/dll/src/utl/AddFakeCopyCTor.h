// Copyright (C) 1998-2024 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

#if !defined(__RTC_UTL_ADD_FAKE_COPY_CTOR_H)
#define __RTC_UTL_ADD_FAKE_COPY_CTOR_H

#include "RtcBase.h"

#if defined(MG_DEBUG)
#define MG_DEBUG_FAKE_COPY_CONSTRUCTOR
#endif

template <typename T>
class add_fake_copy_constructor
{
    mutable T     m_data;
#if defined(MG_DEBUG_FAKE_COPY_CONSTRUCTOR)
    mutable bool  m_hasValue = false;
#endif

public:
    // Default
    add_fake_copy_constructor() = default;

    // Construct from an rvalue T
    add_fake_copy_constructor(T&& v) 
        noexcept(std::is_nothrow_move_constructible_v<T>)
        : m_data(std::move(v))
#if defined(MG_DEBUG_FAKE_COPY_CONSTRUCTOR)
        , m_hasValue(true)
#endif
    {}

    // “Fake” copy‐ctor
    add_fake_copy_constructor(const add_fake_copy_constructor& rhs)
        noexcept(std::is_nothrow_move_constructible_v<T>)
        : m_data(std::move(rhs.m_data))
#if defined(MG_DEBUG_FAKE_COPY_CONSTRUCTOR)
        , m_hasValue(rhs.m_hasValue)
#endif
    {
#if defined(MG_DEBUG_FAKE_COPY_CONSTRUCTOR)
        rhs.m_hasValue = false;
#endif
    }

    // Move‐ctor
    add_fake_copy_constructor(add_fake_copy_constructor&& rhs)
        noexcept(std::is_nothrow_move_constructible_v<T>)
        : m_data(std::move(rhs.m_data))
#if defined(MG_DEBUG_FAKE_COPY_CONSTRUCTOR)
        , m_hasValue(rhs.m_hasValue)
#endif
    {
#if defined(MG_DEBUG_FAKE_COPY_CONSTRUCTOR)
        rhs.m_hasValue = false;
#endif
    }


    // “Fake” copy‐assign
    void operator=(const add_fake_copy_constructor& rhs)
        noexcept(std::is_nothrow_move_assignable_v<T>)
    {
        m_data = std::move(rhs.m_data);
#if defined(MG_DEBUG_FAKE_COPY_CONSTRUCTOR)
        m_hasValue = rhs.m_hasValue;
        rhs.m_hasValue = false;
#endif
    }

    //  Move‐assign
    void operator=(add_fake_copy_constructor&& rhs)
        noexcept(std::is_nothrow_move_assignable_v<T>)
    {
        if (this != &rhs) {
            m_data = std::move(rhs.m_data); // value or zombie, take it anyways
#if defined(MG_DEBUG_FAKE_COPY_CONSTRUCTOR)
            m_hasValue = rhs.m_hasValue;
            rhs.m_hasValue = false; // make sure the rhs is known as a zombie
#endif
        }
    }

    // Take out the value (non‐const)
    T take() const noexcept(std::is_nothrow_move_constructible_v<T>)
    {
        assert(m_hasValue);// throw std::runtime_error("Object not initialized");
#if defined(MG_DEBUG_FAKE_COPY_CONSTRUCTOR)
        m_hasValue = false; // make sure the object is not used again
#endif
        return std::move(m_data);
    }

    // value‐conversion
    operator T() const noexcept(std::is_nothrow_move_constructible_v<T>)
    {
        return take();   // reuse your non‐const take()
    }
};



#endif // __RTC_UTL_ADD_FAKE_COPY_CTOR_H
