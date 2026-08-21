// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////
#pragma once

#if !defined(__STACKPROTECTOR_H)
#define __STACKPROTECTOR_H

// StackProtector is used by Objects that are designed to be shared owned, but whose lifetime is determined 
// by the stack-frame of the current function (auto-var). It protects against premature and incorrent destruction

template <class T>
class StackProtecter 
{
  public:
      StackProtecter (const T& stack_object): m_StackObject(stack_object)
      {
			++(m_StackObject.RefCount());
      }

      ~StackProtecter () { --(m_StackObject.RefCount()); }

  private:
      const T& m_StackObject;
};



#endif // !defined(__STACKPROTECTOR_H)
