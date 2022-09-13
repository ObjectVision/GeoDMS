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
#pragma once

#ifndef __RTC_UTL_SCOPED_EXIT_H
#define __RTC_UTL_SCOPED_EXIT_H

#include "RtcBase.h"
#include "act/any.h"

template <typename Func>
struct scoped_exit {
	scoped_exit(Func&& f) : m_Func(std::move(f)) {}
	~scoped_exit() { m_Func(); }

	scoped_exit(const scoped_exit&) = delete;
	scoped_exit(scoped_exit&&) = delete;
	void operator = (const scoped_exit&) = delete;
	void operator = (scoped_exit&&) = delete;
private:
	Func m_Func;
};

template <typename Func>
struct movable_scoped_exit {
	movable_scoped_exit() 
	:	m_Active(false)
	{}
	movable_scoped_exit(Func&& f) : m_Func(std::move(f)) {}
	~movable_scoped_exit() { if (m_Active) m_Func(); }

	movable_scoped_exit(movable_scoped_exit&& rhs)
	:	m_Active(rhs.m_Active)
	,	m_Func(std::move(rhs.m_Func))
	{
		rhs.m_Active = false;
	}
	movable_scoped_exit(const movable_scoped_exit&) = delete;
	void operator = (const movable_scoped_exit&) = delete;
	void operator = (movable_scoped_exit&&) = delete;

protected:
	bool m_Active = true;
	Func m_Func;
};

template <typename Func>
struct releasable_scoped_exit : movable_scoped_exit<Func>
{
	using movable_scoped_exit<Func>::movable_scoped_exit;

	void release() { this->m_Active = false; }
};


template <typename Func>
auto make_scoped_exit(Func&& f) { return scoped_exit<Func>(std::forward<Func>(f)); }

template <typename Func>
auto make_movable_scoped_exit(Func&& f) { return movable_scoped_exit<Func>(std::forward<Func>(f)); }

template <typename Func>
auto make_releasable_scoped_exit(Func&& f) { return releasable_scoped_exit<Func>(std::forward<Func>(f)); }

template <typename Func>
std::any make_any_scoped_exit(Func&& f) { return make_noncopyable_any<movable_scoped_exit<Func>>(std::forward<Func>(f)); }

template <typename Func>
auto make_shared_exit(Func&& f) { return std::make_shared<scoped_exit<Func>>(std::forward<Func>(f)); }

template <typename Func>
auto make_shared_releasable_exit(Func&& f) { return std::make_shared<movable_scoped_exit<Func>>(std::forward<Func>(f)); }


#endif // __RTC_UTL_SCOPED_EXIT_H