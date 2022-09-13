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

#if !defined(__RTC_ACT_BOX_H)
#define __RTC_ACT_BOX_H

#include <memory>

//----------------------------------------------------------------------
//	class  : Wrap
//----------------------------------------------------------------------
namespace rtc {
	namespace box {

		//----------------------------------------------------------------------
		//	class  : BoxBase
		//----------------------------------------------------------------------

		struct BoxBase {
			virtual ~BoxBase() {}

			std::unique_ptr<BoxBase> m_Next;

			template <typename T, typename ...Args>
			void AddFiduciary(Args&& ...args)
			{
				m_Next = std::make_unique<Box<T>>(std::move(m_Next), std::forward<Args>(args)...);
			}
			template <typename T>
			T& GetRef()
			{
				auto self = dynamic_cast<Box<T>*>(this);
				if (self)
					return self->m_Value;
				if (!m_Next)
					throwIllegalAbstract(MG_POS, "BoxBase::GetRef");
				return m_Next->GetRef<T>();
			}
			void Clear()
			{
				m_Next.reset();
			}
		};

		//----------------------------------------------------------------------
		//	class  : Box
		//----------------------------------------------------------------------

		template <typename T>
		struct Box : BoxBase {
			template <typename ...Args>
			Box(std::unique_ptr<BoxBase>&& next, Args&& ...args)
				: m_Value{ std::forward<Args>(args)... }
			{
				m_Next = std::move(next);
			}

			T m_Value;
		};

	} // namespace box
} // namespace rtc

#endif //!defined(__RTC_ACT_BOX_H)
