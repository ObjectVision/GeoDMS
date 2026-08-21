// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////

#if !defined(__TIC_LISPCONTEXTHANDLE_H)
#define __TIC_LISPCONTEXTHANDLE_H

#include "TicInterface.h"

#include "dbg/DebugContext.h"

#include "LispRef.h"

/********** LispContextHandle **********/

struct LispContextHandle : ContextHandle
{
	TIC_CALL LispContextHandle(CharPtr expr, LispPtr ref);
	TIC_CALL ~LispContextHandle();

protected:
	void GenerateDescription() override;

private:
	CharPtr m_Expr;
	LispPtr m_Ref;
};


#endif // __TIC_LISPCONTEXTHANDLE_H
