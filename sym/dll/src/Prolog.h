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
#ifndef __MG_SYMBOL_PROLOG_H
#define __MG_SYMBOL_PROLOG_H

#include "LispList.h"
#include "Assoc.h"


//	constructs used in prolog processor

typedef LispRef             Predicate;
typedef LispList<Predicate> PredicateList;
typedef PredicateList       Rule;
typedef LispList<Rule>      RuleList;

typedef AssocList           Answer;
typedef LispList<Answer>    AnswerList;

// global storage of rules

// params: List of goals and assocs
//         are in Renumbered-style with all timestamps < chr
//	returns: a solution, or fail
//				if a solution is found, it is returned; to see all
//				solutions, the Answer function should return fail
//				to let the processor continue searching.
//	See: [Boizumault93], p.45

SYM_CALL AnswerList Solve(const RuleList& ruleBase, const PredicateList& goals); // List of goals; NOT renumbered (at 1)
SYM_CALL Answer SayAnswer(AssocListPtr env);
SYM_CALL RuleList RewriteGoals(const PredicateList& goals, const AnswerList& answers);
#endif // __MG_SYMBOL_PROLOG_H
