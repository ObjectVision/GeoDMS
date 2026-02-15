// Copyright (C) 1998-2026 Object Vision B.V. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif


#if !defined(__RTC_DBG_DEBUGREPORTER_H)
#define __RTC_DBG_DEBUGREPORTER_H

//----------------------------------------------------------------------
// used modules and forward class references
//----------------------------------------------------------------------

#if defined(MG_DEBUGREPORTER)

struct linked_list
{
	const linked_list* GetNext() const { return m_Next; }

protected:
	linked_list(linked_list** firstPtr) : m_Next(*firstPtr)
	{
		*firstPtr = this;
	}

	~linked_list()
	{
		dms_assert(!m_Next); // remove must have been called (by embedder for example)
	}

	void remove_from_list(linked_list** firstPtr) 
	{
		dms_assert(m_Next);
		while (*firstPtr != this)
		{
			dms_assert(*firstPtr); // we know for certain that we're in list and thus can be found, since m_Next had value
			firstPtr = &((*firstPtr)->m_Next);
		}
		// remove from list
		*firstPtr = m_Next;
		m_Next = nullptr;
	}

private:
	linked_list* m_Next;
};

/********** DebugReporter interface  **********/

struct DebugReporter : linked_list
{
	virtual void Report() const =0;
	RTC_CALL static void ReportAll();

protected:
	RTC_CALL DebugReporter();
	RTC_CALL ~DebugReporter();
	RTC_CALL void clear();
};


/********** DebugReporter interface  **********/

template <typename Func>
struct DebugCaller : DebugReporter {
	DebugCaller(Func&& func)
		: m_Func(std::move(func))
	{}
	void Report() const override
	{
		m_Func();
	}

	Func m_Func;
};

template <typename Func>
auto MakeDebugCaller(Func&& f)
{
	return DebugCaller<Func>(std::forward<Func>(f));
}

#endif

/* EXAMPLE CODE, only in .cpp
 
 
#if defined(MG_DEBUGREPORTER)

auto test = MakeDebugCaller(
	[]() { dms_assert(1 + 1 == 2);  }
);

#endif defined(MG_DEBUGREPORTER)


*/

/********** Various debug related defined **********/

void ReportExistingObjects();


#endif // __RTC_DBG_DEBUGREPORTER_H