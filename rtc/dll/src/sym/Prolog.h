// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////
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
Answer SayAnswer(AssocListPtr env);
SYM_CALL RuleList RewriteGoals(const PredicateList& goals, const AnswerList& answers);
#endif // __MG_SYMBOL_PROLOG_H
