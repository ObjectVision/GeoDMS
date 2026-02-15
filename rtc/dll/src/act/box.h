// Copyright (C) 1998-2026 Object Vision B.V. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif


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
