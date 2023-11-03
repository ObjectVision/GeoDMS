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

#include "ClcPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

#include "act/MainThread.h"
#include "ptr/StaticPtr.h"
//#include "dbg/DebugContext.h"
#include "CalcFactory.h"
#include "DC_Ptr.h"

#include "ExprCalculator.h"
#include "ExprRewrite.h"
#include <boost/tuple/tuple.hpp>
#include <boost/tuple/tuple_comparison.hpp>


//----------------------------------------------------------------------
// instantiation and registration of ExprCalculator Class Factory
//----------------------------------------------------------------------


// register ExprCalculator ctor upon loading the .DLL.
CalcFactory::CalcFactory()
{
	AbstrCalculator::SetConstructor(this); 
}

CalcFactory::~CalcFactory()
{
	AbstrCalculator::SetConstructor(nullptr);
}

AbstrCalculatorRef CalcFactory::ConstructExpr(const TreeItem* context, WeakStr expr, CalcRole cr)
{
	dms_assert(IsMetaThread());
	if (expr.empty())
	{
		if (cr == CalcRole::Calculator)
			context->Fail("Invalid CalculationRule", FR_MetaInfo);
		return nullptr;
	}
	AbstrCalculatorRef exprCalc = new ExprCalculator(context, expr, cr); // hold resource for now; beware: this line can trigger new inserts/deletes in s_CalcFactory
	return exprCalc; // second alloc succeeded, release hold an from now on it's callers' responsibility to destroy the new ExprCalculator
}

#include "DataBlockTask.h"

AbstrCalculatorRef CalcFactory::ConstructDBT(AbstrDataItem* context, const AbstrCalculator* src)
{
	dms_assert(src);
	dms_assert(src->IsDataBlock());
	return new DataBlockTask(
		context, 
		*debug_cast<const DataBlockTask*>(src)
	);
}

LispRef CalcFactory::RewriteExprTop(LispPtr org)
{
	return ::RewriteExprTop(org);
}

//----------------------------------------------------------------------
// the Singleton
//----------------------------------------------------------------------

CalcFactory calcFactory;
