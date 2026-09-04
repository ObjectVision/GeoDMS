// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////

#include "TicPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

// Calculation-layer support satellites of tic, merged from four small TUs
// (2026-08): LispContextHandle, ExprRewrite, StateChangeNotification and
// TreeItemContextHandle.

// ==== LispContextHandle ====

#include "LispContextHandle.h"

#include "dbg/SeverityType.h"
#include "ser/AsString.h"
#include "utl/StrFormat.h"

#if defined(MG_DEBUG)
#define MG_DEBUG_LISPCONTEXT
#endif

/********** LispContextHandle **********/

#if defined(MG_DEBUG_LISPCONTEXT)
THREAD_LOCAL UInt32 s_NrLispContexts = 0;
#endif

LispContextHandle::LispContextHandle(CharPtr expr, LispPtr ref)
	:	m_Expr(expr)
	,	m_Ref(ref)
{
#if defined(MG_DEBUG_LISPCONTEXT)
	++s_NrLispContexts;
	GetDescription();
//	reportF(ST_MinorTrace, "LCH::CTOR {}({})", s_NrLispContexts, GetDescription());
#endif
}

LispContextHandle::~LispContextHandle()
{
#if defined(MG_DEBUG_LISPCONTEXT)
//	reportF(ST_MinorTrace, "LCH::DTOR {}({})", s_NrLispContexts, GetDescription());
	s_NrLispContexts--;
#endif
}


void LispContextHandle::GenerateDescription()
{
	SetText(
		mySSPrintF("Expression={};\nLocal Lisp Tree={}.", 
			m_Expr, 
			AsString(m_Ref).c_str()
		)
	);
}




// ==== ExprRewrite ====

#include "ExprRewrite.h"

#include "dbg/DebugContext.h"
#include "ser/FileStreamBuff.h"
#include "utl/Environment.h"
#include "utl/FileSystem.h"
#include "utl/splitPath.h"

#include "Assoc.h"
#include "LispEval.h"
#include "LispContextHandle.h"

#include "Parser.h"

// *****************************************************************************
//											RESOURCES
// *****************************************************************************


SharedStr rewriteExprFileName()
{
	return DelimitedConcat(GetExeDir().c_str(), "RewriteExpr.lsp");
}


AssocList::ptr_type GetEnv()
{
	static AssocList s_Env;
	static bool s_HasRead = false;

	if (!s_HasRead)
	{
		CDebugContextHandle dch("RewriteExpr", "Read RewriteExpr.lsp", false);
		SharedStr fileName = rewriteExprFileName();
		FileInpStreamBuff in(fileName, true);
		if (!in.IsOpen())
			throwErrorD("RewriteRules", "Cannot open file RewriteExpr.lsp which is required for expression parsing");
		FormattedInpStream fin(&in);
		s_Env = AssocList( GetExpr(fin) );
		SetEnv(s_Env);
		s_HasRead = true;
	}
	return s_Env;
}

LispRef RewriteExprList(LispPtr orgList)
{
	lfs_assert(orgList.EndP() || orgList->GetRefCount());

	// stack friendly version; don't use recursion on list length but 
	// build a reverserd list whose elements are the to be processed ExprLists

	LispRef reversedList;
	LispPtr orgListIter = orgList;
	for(; !orgListIter.EndP(); orgListIter = orgListIter.Right())
		reversedList = LispRef(orgListIter, reversedList);

//	for (; !reversedList.EndP(); reversedList = reversedList.Right())
//		result = LispRef(RewriteExpr(reversedList.Left().Left()), result);
//  better avoid ListObj lookup as long as RewriteExpr is ineffective

	LispRef result;
	for (; !reversedList.EndP(); reversedList = reversedList.Right())
	{
		lfs_assert(result == orgListIter);
		MG_CHECK(reversedList.IsRealList());
		MG_CHECK(reversedList.Left().EndP() || reversedList.Left()->IsOwned());
		orgListIter = reversedList.Left();
		LispRef rewriteExpr = RewriteExpr(orgListIter->Left());
		if (rewriteExpr != orgListIter->Left() )
		{
			while (true) {
				result = LispRef(rewriteExpr, result);
				reversedList = reversedList.Right(); 
				if (reversedList.EndP())
					goto exit;
				rewriteExpr = RewriteExpr(reversedList.Left().Left());
			}
		}
		result = orgListIter;
	}
exit:
	return result;
}

LispRef RewriteExpr(LispPtr org)
{
//	MG_DEBUGCODE( LispContextHandle lch1("LispBeforeRewrite", org); )
	if (!org.IsRealList())
		return org;
	return RewriteExprTop(
		RewriteExprList(org)
	);
}

LispRef RewriteExprTop(LispPtr org)
{
	GetEnv();
//	MG_DEBUGCODE( LispContextHandle lch1("LispBeforeRewriteTop", org); )
	return ApplyTopEnv(org);
}

namespace {
	// a rule CAPTURES generic calls of its head iff every argument position of its
	// pattern is a plain variable (including a variable tail such as _T): any call
	// spelling then matches, so a same-named function would never be reached. A rule
	// with a structural or literal argument fires only on specific spellings and
	// COMPOSES with a same-named function (e.g. the retained MakeDefined idempotence
	// collapse and the median-interval destructuring rule: they normalize the source,
	// and their output resolves to the prelude function).
	bool IsCapturingPattern(LispPtr key)
	{
		LispPtr cursor = key.Right();
		while (true)
		{
			if (cursor.EndP())
				return true;
			if (!cursor.IsRealList())
				return cursor.IsVar(); // variable tail = generic; a structural tail is specific
			if (!cursor.Left().IsVar())
				return false;
			cursor = cursor.Right();
		}
	}
}

bool HasRewriteRuleForHead(TokenID headID)
{
	AssocListPtr cursor = GetEnv();
	while (!cursor.IsEmpty())
	{
		AssocPtr a = cursor.First();
		LispPtr key = a.Key();
		if (key.IsRealList() && key.Left().IsSymb() && key.Left().GetSymbID() == headID
			&& IsCapturingPattern(key))
			return true;
		cursor = cursor.Tail();
	}
	return false;
}



// ==== StateChangeNotification ====

//----------------------------------------------------------------------
// used modules and forward class references
//----------------------------------------------------------------------
#include <set>
#include <utility>

#include "StateChangeNotification.h"

#include "dbg/DmsCatch.h"
#include "ptr/StaticPtr.h"
#include "TreeItemContextHandle.h"

//----------------------------------------------------------------------

namespace { // local defs

	typedef std::pair<TStateChangeNotificationFunc, ClientHandle> TStateChangeNotificationSink;
	typedef std::set<TStateChangeNotificationSink>               TStateChangeNotificationSinkContainer;
	static_ptr<TStateChangeNotificationSinkContainer>             g_StateChangeNotifications;

	typedef std::pair<const TreeItem*, TStateChangeNotificationSink> TTreeItemStateChangeNotificationSink;
	typedef std::set<TTreeItemStateChangeNotificationSink>         TTreeItemStateChangeNotificationSinkContainer;

	static_ptr<TTreeItemStateChangeNotificationSinkContainer>       g_TreeItemStateChangeNotificationSink;

} // end anonymous namespace

//----------------------------------------------------------------------
// header implementation
//----------------------------------------------------------------------

CharPtr UpdateStateName(UInt32 nc)
{
	switch (nc) {
	case NC2_Invalidated: return "None";
	case NC2_MetaReady:   return "MetaInfoReady";
	case NC2_DataReady:   return "DataReady";
	case NC2_Validated:   return "Validated";
	case NC2_Committed:   return "Validated&Committed";
	}
	return "unrecognized";
}

void NotifyStateChange(const TreeItem* item, UInt32 state)
{
	if (TreeItem::s_NotifyChangeLockCount || (state < CC_First && item->IsPassor()))
		return;

	if (g_StateChangeNotifications)
	{
		TStateChangeNotificationSinkContainer::const_iterator 
			current = g_StateChangeNotifications->begin(),
			last    = g_StateChangeNotifications->end();
		while (current != last)
		{
			ClientHandle clientHandle = current->second;
			(current++)->first(clientHandle, item, NotificationCode(state) );
		}
	}
	if (state >= CC_First || !g_TreeItemStateChangeNotificationSink)
		return;

	TTreeItemStateChangeNotificationSinkContainer::const_iterator
		i = g_TreeItemStateChangeNotificationSink->lower_bound(TTreeItemStateChangeNotificationSink(item, TStateChangeNotificationSink(nullptr,ClientHandle()))),
		e = g_TreeItemStateChangeNotificationSink->end();
	while (g_TreeItemStateChangeNotificationSink && i != e && i->first == item)
	{
		ClientHandle clientHandle = i->second.second;
		(i++)->second.first(clientHandle, item, NotificationCode(state));
	}
}

//----------------------------------------------------------------------
// C-API implementation
//----------------------------------------------------------------------

#include "TicInterface.h"

extern "C" TIC_CALL void DMS_CONV DMS_RegisterStateChangeNotification(TStateChangeNotificationFunc fcb, ClientHandle clientHandle)
{
	DMS_CALL_BEGIN

		if (!g_StateChangeNotifications) 
			g_StateChangeNotifications.assign( new TStateChangeNotificationSinkContainer );
		dms_assert(g_StateChangeNotifications);
		g_StateChangeNotifications->insert(TStateChangeNotificationSink(fcb, clientHandle));

	DMS_CALL_END
}

extern "C" TIC_CALL void DMS_CONV DMS_ReleaseStateChangeNotification(TStateChangeNotificationFunc fcb, ClientHandle clientHandle)
{
	DMS_CALL_BEGIN

		// Release may be called several times to accomodate various exit paths for client
		if(!g_StateChangeNotifications)
			return;

		g_StateChangeNotifications->erase(TStateChangeNotificationSink(fcb, clientHandle));
		if (g_StateChangeNotifications->empty())
			g_StateChangeNotifications.reset();

	DMS_CALL_END
}

extern "C" TIC_CALL void DMS_CONV DMS_TreeItem_RegisterStateChangeNotification(TStateChangeNotificationFunc fcb, const TreeItem* self, ClientHandle clientHandle)
{
	DMS_CALL_BEGIN

		TreeItemContextHandle checkPtr(self, nullptr, "DMS_TreeItem_RegisterStateChangeNotification");
		if (!g_TreeItemStateChangeNotificationSink) 
			g_TreeItemStateChangeNotificationSink.assign( new TTreeItemStateChangeNotificationSinkContainer );

		assert(g_TreeItemStateChangeNotificationSink);

		g_TreeItemStateChangeNotificationSink->insert(
				TTreeItemStateChangeNotificationSink(self, TStateChangeNotificationSink(fcb, clientHandle))
		);

	DMS_CALL_END
}


extern "C" TIC_CALL void DMS_CONV DMS_TreeItem_ReleaseStateChangeNotification (TStateChangeNotificationFunc fcb, const TreeItem* self, ClientHandle clientHandle)
{
	DMS_CALL_BEGIN

		TreeItemContextHandle checkPtr(self, nullptr, "DMS_TreeItem_ReleaseStateChangeNotification");
		dms_assert(g_TreeItemStateChangeNotificationSink);
		g_TreeItemStateChangeNotificationSink->erase(TTreeItemStateChangeNotificationSink(self, TStateChangeNotificationSink(fcb, clientHandle)));

		if (g_TreeItemStateChangeNotificationSink->empty())
			g_TreeItemStateChangeNotificationSink.reset();

	DMS_CALL_END
}



// ==== TreeItemContextHandle ====

#include "TreeItemContextHandle.h"

#include "dbg/DmsCatch.h"
#include "utl/Environment.h"
#include "utl/Registry.h"
#include "utl/TimeFmt.h"
#include "utl/StrFormat.h" 

//----------------------------------------------------------------------
// class  : TreeItemContextHandle
//----------------------------------------------------------------------

TreeItemContextHandle::TreeItemContextHandle(const TreeItem* obj, CharPtr role)
	:	m_Role(role) 
{
	if (obj)
	{
		CheckPtr(obj, nullptr, role);
		m_Obj = obj;
	}
	assert(GetPrev() != this);
}

TreeItemContextHandle::TreeItemContextHandle(const TreeItem* obj, const Class* cls, CharPtr role)
	:	m_Role(role) 
{
	CheckPtr(obj, cls, role);
	m_Obj = obj;
	assert(m_Obj);

	assert(GetPrev() != this);
}

TreeItemContextHandle::~TreeItemContextHandle()
{}

auto TreeItemContextHandle::ItemAsStr() const -> SharedStr
{
	auto cci = GetItem();
	if (!cci)
		return {};
	return cci->GetSourceName();
}

void TreeItemContextHandle::GenerateDescription()
{
	CharPtr role = m_Role ? m_Role : "TreeItem";

	if (m_Obj)
	{
		SharedStr objNameStr = m_Obj->GetFullName();
		SetText(
			mgFormat2SharedStr("while in {0}( {1}: {2} )"
				,	role
				,	objNameStr.empty() ? m_Obj->GetName() : objNameStr
				,	m_Obj->GetClsName()
			)
		);
	}
	else
		SetText(SharedStr());
}

//----------------------------------------------------------------------
// SystemContext for providing system info in error messages
//----------------------------------------------------------------------

#include "xml/PropWriter.h"
#include "RtcInterface.h"
#include "SessionData.h"

void GenerateSystemInfo(AbstrPropWriter& apw, const TreeItem* curr)
{
	// Default handling when no configured MetaInfo
	assert(curr);

	apw.OpenSection("DefaultInfo");

	if (curr)
		apw.WriteKey("FullName", curr->GetFullName().c_str());

	apw.WriteKey("GeoDmsVersion", DMS_GetVersion());
	apw.WriteKey("SessionStartTime", GetSessionStartTimeStr());
	apw.WriteKey("CurrentTime", GetCurrentTimeStr());
	apw.WriteKey("StatusFlags",
		mySSPrintF("0x{:x} = {}{}{}{}{}{}{}{}"
			, GetRegStatusFlags()
			, (GetRegStatusFlags() & RSF_AdminMode) ? "AdminMode " : ""
			, (GetRegStatusFlags() & RSF_DebugMode) ? "DebugMode " : ""
			, (GetRegStatusFlags() & RSF_SuspendForGUI) ? "SuspendForGUI " : ""
			, (GetRegStatusFlags() & RSF_ShowStateColors) ? "ShowStateColors " : ""
			, (GetRegStatusFlags() & RSF_TraceLogFile) ? "TraceLogFile " : ""
			, (GetRegStatusFlags() & RSF_MultiThreading1) ? "MT1 " : ""
			, (GetRegStatusFlags() & RSF_MultiThreading2) ? "MT2 " : ""
			, (GetRegStatusFlags() & RSF_MultiThreading3) ? "MT3 " : ""
		)
	);
	apw.CloseSection();
}

