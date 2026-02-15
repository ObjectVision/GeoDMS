// Copyright (C) 1998-2026 Object Vision B.V. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif


#if !defined(__RTC_PTR_LIFETIMEPROTECTOR_H)
#define __RTC_PTR_LIFETIMEPROTECTOR_H

template <typename OBJECT>
struct LifetimeProtector
{
	template <typename ...Args>
	LifetimeProtector(Args&&... args)
		: m_Object(std::forward<Args>(args)...)
	{
		m_Object.AdoptRef();
	}
	~LifetimeProtector()
	{
		m_Object.DecRef();
		assert(!m_Object.IsOwned());
	}

	OBJECT* operator -> () { return &m_Object; }
	const OBJECT* operator -> () const { return &m_Object; }

	OBJECT& operator * () { return m_Object; }
	const OBJECT& operator * () const { return m_Object; }

private:
	OBJECT m_Object;
};

#endif // __RTC_PTR_LIFETIMEPROTECTOR_H
