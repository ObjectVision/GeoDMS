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

#include "TicPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

#include "ExprRewrite.h"

#include "dbg/DebugContext.h"
#include "ser/FileStreamBuff.h"
#include "utl/Environment.h"
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
		FileInpStreamBuff in(fileName, 0, true);
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

