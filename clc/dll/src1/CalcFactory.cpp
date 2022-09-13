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
#pragma hdrstop

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
/*
typedef boost::tuple<TreeItem*, SharedStr, CalcRole> CalcKey;
typedef std::map<CalcKey, const AbstrCalculator*> CalcFactoryCache;

template <typename Cache>
struct Lockable_static_ptr : static_ptr<Cache>
{
	void ConsiderBuildup() // can throw
	{
		if (!this->get_ptr())
			this->assign(new Cache);
		dms_assert(this->has_ptr()); // postcondition for a normal exit
	}
	void ConsiderCleanup() // nothrow
	{
		if (this->has_ptr() && this->get_ptr()->empty() && !m_ConstructionCounter)
			this->reset();
	}

	struct PendingCreationLock : ref_base<Lockable_static_ptr, noncopyable>
	{
		PendingCreationLock(Lockable_static_ptr& self)
			:	ref_base<Lockable_static_ptr, noncopyable>(&self)
		{
			dms_assert(this->has_ptr());
			self.ConsiderBuildup();
			++self.m_ConstructionCounter;
		}
		~PendingCreationLock() // nothrow
		{
			dms_assert(this->has_ptr()); // guaranteed by constructor
			--(this->get_ptr()->m_ConstructionCounter);
			this->get_ptr()->ConsiderCleanup();
		}
	};

	UInt32 m_ConstructionCounter;
};

static Lockable_static_ptr<CalcFactoryCache> s_CalcFactoryCache;
static std::recursive_mutex s_CalcFactoryCacheSection;
*/

AbstrCalculatorRef CalcFactory::ConstructExpr(const TreeItem* context, WeakStr expr, CalcRole cr)
{
	dms_assert(IsMainThread());
	if (expr.empty())
		return nullptr;
//	CalcKey key(context, expr, calcRole);

//	std::lock_guard guard(s_CalcFactoryCacheSection);

//	Lockable_static_ptr<CalcFactoryCache>::PendingCreationLock lock(s_CalcFactoryCache);

//	dms_assert(s_CalcFactoryCache);

//	CalcFactoryCache::iterator entryPtr = s_CalcFactoryCache->lower_bound(key);
//	if (entryPtr != s_CalcFactoryCache->end() && entryPtr->first == key)
//		return entryPtr->second;

	AbstrCalculatorRef exprCalc = new ExprCalculator(context, expr, cr); // hold resource for now; beware: this line can trigger new inserts/deletes in s_CalcFactory
//	entryPtr = s_CalcFactoryCache->insert(entryPtr, CalcFactoryCache::value_type(key, exprCalc));
//	exprCalc->MakeDataController();
	return exprCalc; // second alloc succeeded, release hold an from now on it's callers' responsibility to destroy the new ExprCalculator
}

/*
void CalcFactory::DestructExpr(TreeItem* context, CalcRole cr, AbstrCalculator* exprCalculator)
{
	garbage_t garbage;
	std::lock_guard guard(s_CalcFactoryCacheSection);

	dms_assert(s_CalcFactoryCache);
	CalcKey key(context, exprCalculator->GetExpr(), cr);
	CalcFactoryCache::iterator entryPtr = s_CalcFactoryCache->find(key);

	dms_assert(entryPtr != s_CalcFactoryCache->end());
	if (entryPtr == s_CalcFactoryCache->end())
		return;

	dms_assert(entryPtr->first == key);
	dms_assert(entryPtr->second == exprCalculator);
	s_CalcFactoryCache->erase(entryPtr);

	s_CalcFactoryCache.ConsiderCleanup();
}
*/

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
