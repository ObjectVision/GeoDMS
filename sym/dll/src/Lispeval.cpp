// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "SymPch.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

/****************** Lisp interpreter              *******************/

#include "LispEval.h"
#include "dbg/debug.h"
#include "ser/AsString.h"

#if defined(MG_DEBUG)
	#include "utl/IncrementalLock.h"
	std::atomic<UInt32> gd_LispEvalLevel = 0;
	#define MaxAllowedLevel 20000
	#define MG_TRACE_LISP false
#endif

#if defined(MG_DEBUG_LISPEVAL)
#	include <iostream.h>
#endif

#include <functional>
#include "set/Cache.h"

using cache_key_t = std::pair<LispRef, AssocList> ;

/*************** Elementary functions  ******/

bool HasAnyVar(LispPtr expr)
{
	if (expr.IsRealList())
		return HasAnyVar(expr.Left()) || HasAnyVar(expr.Right());
	else
		return expr.IsVar();
}


/*************** Elementary functions for Eval ******/

#if defined(MG_USE_LISPFUNCS)
const TokenID T_Cond  = GetTokenID_st("CASE");
const TokenID T_Let   = GetTokenID_st("LET");
const TokenID T_Fail  = GetTokenID_st("FAIL");

const TokenID T_IsList= GetTokenID_st("IsList");
const TokenID T_Cons  = GetTokenID_st("Cons");
const TokenID T_Head  = GetTokenID_st("Head");
const TokenID T_Tail  = GetTokenID_st("Tail");

const TokenID T_IsInt = GetTokenID_st("IsNumb");
const TokenID T_Times = GetTokenID_st("*");
const TokenID T_Plus  = GetTokenID_st("+");
const TokenID T_LT    = GetTokenID_st("<");
const TokenID T_EQ    = GetTokenID_st("=");

const TokenID T_IsSymb= GetTokenID_st("IsSymb");
const TokenID T_Bound = GetTokenID_st("Bound");
const TokenID T_Free  = GetTokenID_st("Free");

const TokenID T_Prolog= GetTokenID_st("Pro");
const TokenID T_ProMod= GetTokenID_st("ProMod");
const TokenID T_Renum = GetTokenID_st("Renum");
const TokenID T_GetEnv= GetTokenID_st("GetEnv");

const TokenID T_MaxNrDefTokens = 20;

const LispRef ZeroElem(Number(0));
const LispRef  OneElem(Number(1));

const LispRef CondSymbol(T_Cond);
const LispRef LetSymbol (T_Let);
const LispRef FailSymbol(T_Fail);

/*************** Elementary functions using special symbols ******/

inline LispPtr MakeBool(bool B)
{
	return (B)
  	?	LispRef::s_null
    :	FailSymbol;
}


/****************** Numerical expressions         ******************/

LispRef Plus (LispPtr one, LispPtr two)
{
	return LispRef(one.GetNumbVal() + two.GetNumbVal());
}

LispRef Minus(LispPtr one, LispPtr two)
{
	return LispRef(one.GetNumbVal() - two.GetNumbVal());
}

LispRef Times(LispPtr one, LispPtr two)
{
	return LispRef(one.GetNumbVal() * two.GetNumbVal());
}

/****************** Conditional expressions      ******************/

struct LispClause : public Assoc
{
	LispClause(LispPtr cond, LispPtr expr)
		:	Assoc(cond, expr) {}
};

typedef LispList<LispClause> LispClauseList;

struct LispCondExpr : public Assoc
{
	LispCondExpr(const LispClauseList& l) :	Assoc(CondSymbol,l) {}
	const LispClauseList& ClauseList() const
	{
		return reinterpret_cast<const LispClauseList&>(Val());
	}
	static LispRef Failed;
};

LispRef LispCondExpr::Failed = FailSymbol;

bool IsCondExpr(LispPtr expr)
{
	if (!expr.IsRealList())
		return false;
	return expr.Left() == CondSymbol;
}

/*
LispRef EvalCondList(LispPtr condList)
  {
	if (EndP(condList)) return condList;           // condList OK: Return ()
	LispRef FirstCond=Eval(First(condList));
	if (FirstCond==FailSymbol)  return FailSymbol; // 1 cond FAIL: Return FAIL
	LispRef RestCond= EvalCondList(Rest(condList));
	if (EndP(FirstCond)) return RestCond;
	return Cons(FirstCond,RestCond);               // return Reduced condList
  }
*/

LispRef EvalClauseList(const LispClauseList& clauseList, AssocListPtr env)
{
	if (clauseList.IsEmpty())
		return LispCondExpr::Failed;
	LispClause FirstClause=clauseList.First();

#ifdef MG_DEBUG_LISPEVAL
  if (debugStream)
		(*debugStream) << "\nEvalClauseList: FirstClause" << FirstClause;
#endif

	LispRef cond = Eval(FirstClause.Key(), env);
	LispRef expr = FirstClause.Val();

	if (cond.EndP())    // true;
		return Eval(expr, env);
	LispRef RestCond = EvalClauseList(clauseList.Tail(), env);
	if (cond==FailSymbol) // false
		return RestCond;
	LispClause ResultClause=LispClause(cond,Eval(expr, env));
	if (RestCond == LispCondExpr::Failed)
		return LispCondExpr(List1(ResultClause));
	if (IsCondExpr(RestCond))
	{
		LispCondExpr RestCondExpr = reinterpret_cast<const LispCondExpr&>(RestCond);
		return LispCondExpr(LispClauseList(ResultClause, RestCondExpr.ClauseList()));
	}
	return
		LispCondExpr(List2(ResultClause, LispClause(LispRef::s_null,RestCond) ));
}

/****************** Let         expresstions      ******************/

Assoc MakeLetExpr(AssocListPtr assocList, LispPtr expr)
{
	return Assoc(LetSymbol,Assoc(assocList,expr));
}

LispRef EvalLet(LispPtr LetList, AssocListPtr env)
{
	AssocListPtr letTable =
  		reinterpret_cast<AssocListPtr>(LetList.Left());
	return Eval(letTable.ApplyOnce(LetList.Right()), env);
}

#endif //defined(MG_USE_LISPFUNCS)

/******************  Match                        ******************/

/* Match tests if the constant expression expr can match header */

AssocList Match(AssocListPtr aList, LispPtr header, LispPtr expr)
{
	if (header.IsVar())
		return Insert(aList, Assoc(header, expr) );
	if (!header.IsRealList())
		if (header==expr)
			return aList;
		else
			return AssocListPtr::failed();
	if (!expr.IsRealList())
		return AssocListPtr::failed();
	AssocList newAssocList = Match(aList, header.Left(), expr.Left());
	if (newAssocList.IsFailed())
		return newAssocList;
	return Match(newAssocList, header.Right(), expr.Right());
}

/*
LispRef ShortEvalCondList(LispPtr condList)
{
	if (condList.EndP()) return condList;           // Conditions OK: ()
	LispRef FirstCond=Eval(condList.Left());
	if (FirstCond.EndP()) return ShortEvalCondList(condList.Right());
	return FailSymbol;                             // If not all cond reduces
}   						 									// to (): return FAIL
*/
/****************** Global eval functions         *******************/

/*
	The following function does the searching and substitution for
	a constant expression expr.
	using	Match -> assocList	(for constant expressions)
		and	ShortEvalCondList	(for constant expressions)
	 if no matching pattern is found, FailSymbol is returned.
*/

LispRef ApplySubstList(LispPtr expr, AssocListPtr substList)
{
	DBG_START("LispEval", "ApplySubstList", false);
	DBG_TRACE(("expr  = %s", AsString(expr).c_str()));

	while (!substList.IsEmpty())
	{
		AssocPtr subst  = substList.Head();   // a substitution rule
		LispPtr  header = subst.Key();   // header part
		DBG_TRACE(("header = %s", AsString(header).c_str()));
		AssocList assocList = Match(AssocList(), header, expr);
		if (!assocList.IsFailed())
		{
/*
			DBG_START("LispEval", "ApplySubstList", false);
			DBG_TRACE(("expr   = %s", AsString(expr).c_str()));
			DBG_TRACE(("subst  = %s", AsString(subst).c_str()));
			DBG_TRACE(("unifier= %s", AsString(assocList).c_str()));
*/
			dms_assert(expr == assocList.ApplyOnce(subst.Key()));
			LispRef result = assocList.ApplyOnce(subst.Val());

			DBG_TRACE(("result = %s", AsString(result).c_str()));

//	 		dms_assert(!HasAnyVar(result));
			return result;
		}
		substList = substList.Tail();
	}
	return expr;
}

LispRef ApplyList(LispPtr expr, AssocListPtr env)
{
	if (!expr.IsRealList())
		return expr;

	return LispRef(
		Apply(expr.Left(), env), 
		ApplyList(expr.Right(), env)
	);
}

inline LispRef ApplyStep(LispPtr expr, AssocListPtr env)
{
	return ApplySubstList(ApplyList(expr, env), env); // bottom up
}

#if defined(MG_USE_LISPFUNCS)

LispRef EvalList(LispPtr expr, AssocListPtr env)
{
	if (expr.IsRealList())
		return LispRef(Eval(expr.Left(), env), EvalList(expr.Right(), env));
	return expr;
}

LispRef EvalStep(LispPtr expr, AssocListPtr env)
{
	if (expr.IsRealList())
	{
  		const LispRefList& exprList = reinterpret_cast<const LispRefList&>(expr);
  		LispPtr functor = exprList.Head();
		if (functor.IsSymb())
		{
			const LispRefList& tail=exprList.Tail();
			TokenID t = functor.GetSymbID();
//			if (t < T_MaxNrDefTokens)
//			{
			if (t == T_Cond ) return EvalClauseList(expr.Right(), env);
			if (t == T_Let  ) return EvalLet       (expr.Right(), env);

			if (t == T_IsInt) return MakeBool( Eval(tail.First(), env).IsNumb());

			if (t == T_IsSymb)return MakeBool( Eval(tail.First(), env).IsSymb());
			if (t == T_Bound) return MakeBool(!Eval(tail.First(), env).IsVar ());
			if (t == T_Free)  return MakeBool( Eval(tail.First(), env).IsVar ());

			if (t == T_IsList)return MakeBool( Eval(tail.First(), env).IsList());
			if (t == T_Head)  return Eval(tail.First(), env).Left();
			if (t == T_Tail)  return Eval(tail.First(), env).Right();
			if (t == T_Cons)  return LispRef(Eval(tail.First(), env), Eval(tail.Second(), env));
			if (t == T_Times) return Times(  Eval(tail.First(), env), Eval(tail.Second(), env));
			if (t == T_Plus ) return Plus(   Eval(tail.First(), env), Eval(tail.Second(), env));
/*
				if (T==T_Prolog) return Solve(Tail);
				if (T==T_Renum)
				{
					LispRef assocList;
					int Nr=0;
					return Renum(assocList,First(Tail),Nr);
				}
  */
//			}
		}
	}
	else if (expr.IsSymb())
	{
		TokenID t = expr.GetSymbID();
		if (t == T_GetEnv) return env;
	}
	return ApplySubstList(EvalList(expr, env), env);
}

struct EvalStepFunc
{
	typedef cache_key_t argument_type;
	typedef LispRef     result_type;

	LispRef operator ()(const cache_key_t& exprEnvPair) const
	{
		return EvalStep(exprEnvPair.first, exprEnvPair.second);
	}
};

Cache<EvalStepFunc> g_evalCache;

LispRef Eval(LispPtr expr, AssocListPtr env)
{
#if defined(MG_DEBUG)
	IncrementalLock levelLock(gd_Level);
	dms_assert(gd_Level <= MaxAllowedLevel);
#endif
//	reportF(ST_MinorTrace, "Eval: %s", AsString(expr).c_str()); // DEBUG

	return g_evalCache(Assoc(env, expr))
	return result;
}

LispRef RepeatedEval(LispPtr expr, AssocListPtr env)
{
	DBG_START("LispEval", "RepeatedEval", false);
	DBG_TRACE(("expr  = %s", AsString(expr).c_str()));

	LispRef result=expr;
	LispRef prevResult;
	while (!(result == prevResult))
	{
		prevResult = result;
		result = Eval(prevResult, env);
	}
	return result;
}

LispRef MakeVarsOfUnderscores(LispPtr expr)
{
	return expr.IsSymb() && expr.GetSymbStr()[0]=='_'
		? LispRef(expr.GetSymbID(), 1)
		: expr.IsRealList()
			? LispRef(MakeVarsOfUnderscores(expr.Left()), MakeVarsOfUnderscores(expr.Right()))
			: expr;
}

#endif //defined(MG_USE_LISPFUNCS)


struct ApplyStepFunc
{
	typedef cache_key_t argument_type;
	typedef LispRef     result_type;

	LispRef operator ()(const cache_key_t& exprEnvPair) const
	{
		return ApplyStep(exprEnvPair.first, exprEnvPair.second);
	}
};

LispComponent s_LispServiceSubscription;
Cache<ApplyStepFunc> g_applyCache;

LispRef Apply(LispPtr expr, AssocListPtr env)
{
#if defined(MG_DEBUG)
	StaticMtIncrementalLock<gd_LispEvalLevel> levelLock;
	dms_assert(gd_LispEvalLevel <= MaxAllowedLevel);
#endif
//	reportF(ST_MinorTrace, "Apply: %s", AsString(expr).c_str()); // DEBUG

	return g_applyCache.apply(cache_key_t(expr, env));
}

LispRef RepeatedApply(LispPtr expr, AssocListPtr env)
{
	DBG_START("LispEval", "RepeatedApply", false);
	DBG_TRACE(("expr  = %s", AsString(expr).c_str()));

	LispRef result=expr;
	while (true)
	{
		LispRef newResult = Apply(result, env);
		if (result == newResult)
			return result;
		result = std::move(newResult);
	}
}

//==============================

#include "RewriteRules.h"
#include "ptr/OwningPtr.h"

static OwningPtr<RewriteRuleSet> g_RewriteRuleSetPtr;

void SetEnv(AssocListPtr env)
{
	g_RewriteRuleSetPtr.assign( new RewriteRuleSet(env) );
}
LispRef AssocList_RepApplyTopEnv(AssocListPtr unifier, LispPtr templExpr); // forward decl 

LispRef AssocList_RepApplyTopEnvList(AssocListPtr unifier, LispPtr templExprPtr)
{
	DBG_START("AssocList", "RepApplyTopEnvList", MG_TRACE_LISP);
	DBG_TRACE(("templExprList   = %s", AsString(templExprPtr).c_str()));

	if (templExprPtr.IsVar()) // TAIL var
	{
		const Assoc& found = unifier.FindByKey(templExprPtr);
		dms_assert( !found.IsFailed() ); // all vars should be matched
		return found.Val();
	}
	dms_assert(templExprPtr.IsList()); // constants cannot be tail in resulting list
	if (!templExprPtr.IsRealList())
		return templExprPtr;
	return
 	LispRef(
		AssocList_RepApplyTopEnv    (unifier, templExprPtr.Left ()), // EXPR
		AssocList_RepApplyTopEnvList(unifier, templExprPtr.Right())  // TAIL
	);
}

LispRef AssocList_RepApplyTopEnv(AssocListPtr unifier, LispPtr templExpr)
{
	DBG_START("AssocList", "RepApplyTopEnv", MG_TRACE_LISP);
	DBG_TRACE(("templExpr   = %s", AsString(templExpr).c_str()));

	if (templExpr.IsVar())
	{
		AssocPtr found = unifier.FindByKey(templExpr);
		dms_assert( !found.IsFailed() ); // all vars should be matched
		return found.Val();
	}
	if (!templExpr.IsRealList())
		return templExpr;

	return ApplyTopEnv(
		LispRef(
			templExpr.Left() // FUNC-HEAD
		,	AssocList_RepApplyTopEnvList(unifier, templExpr.Right())
		)
	);
}

struct ApplyTopEnvFunc
{
	typedef LispRef argument_type;
	typedef LispRef result_type;

	LispRef operator ()(LispPtr expr) const
	{
		DBG_START("LispEval", "ApplyEnv", MG_TRACE_LISP);
		DBG_TRACE(("expr   = %s", AsString(expr).c_str()));

		LispPtr head = expr.Left(); // caller (= ApplyTopEnv) guarantees exp.IsRealList()

		RewriteRuleSet::const_iterator
			rewriteRulePtr = g_RewriteRuleSetPtr->FindLowerBoundByFuncName(head),
			rewriteRuleEnd = g_RewriteRuleSetPtr->End();

		while (rewriteRulePtr != rewriteRuleEnd)
		{
			LispPtr pattern = rewriteRulePtr->Key();   // pattern part
			if (pattern.Left() != head)
				break;

			AssocList unifier = Match(AssocList(), pattern, expr);
			if (!unifier.IsFailed())
			{
/*
				DBG_START("LispEval", "ApplyEnv", MG_TRACE_LISP);
				DBG_TRACE(("expr     = %s", AsString(expr).c_str()));
				DBG_TRACE(("pattern  = %s", AsString(rewriteRulePtr->Key()).c_str()));
				DBG_TRACE(("unifier  = %s", AsString(unifier).c_str()));
				DBG_TRACE(("templExpr= %s", AsString(rewriteRulePtr->Val()).c_str()));
*/
//				dms_assert(expr == unifier.ApplyOnce(pattern)); POST_CONDITION, but MUTATING through LispRef coy-ctor

				LispRef result = AssocList_RepApplyTopEnv(unifier, rewriteRulePtr->Val());

				DBG_TRACE(("result = %s", AsString(result).c_str()));

				return result;
			}

			++rewriteRulePtr;
		}
		return expr;
	}
};

Cache<ApplyTopEnvFunc> g_applyTopEnvCache;

#if defined(MG_DEBUG)
std::atomic<UInt32> gd_ApplyTopLevel = 0;
#endif

LispRef ApplyTopEnv(LispPtr expr)
{
	assert(IsMainThread());
#if defined(MG_DEBUG)
	StaticMtIncrementalLock<gd_ApplyTopLevel> levelLock;
	assert(gd_ApplyTopLevel <= MaxAllowedLevel);
#endif
	assert(expr.IsRealList());

	return g_applyTopEnvCache.apply(expr);
}

//==============================

LispRef MakeVarsOfUnderscores(LispPtr expr)
{
	return expr.IsSymb() && expr.GetSymbStr().c_str()[0]=='_'
		? LispRef(expr.GetSymbID(), 1)
		: expr.IsRealList()
			? LispRef(MakeVarsOfUnderscores(expr.Left()), MakeVarsOfUnderscores(expr.Right()))
			: expr;
}

/****************** renumerate vars               *******************/

/*
LispRef MakeVar(int VarNr)
{
	char name[10];
	sprintf(name,"_%d",VarNr);
	return MakeSymb(name);
}

LispRef Renum(AssocListPtr assocList, LispPtr expr, int& NextFreeVarNum)
{
	if (IsVar(expr))
	{
		Assoc envDef=assocList.Find(expr);
		if (envDef.Empty())
		{
			LispRef newVar=MakeVar(NextFreeVarNum++);
			assocList = assocList.Add(Assoc(expr,newVar));
			return newVar;
		}
		else
		  return envDef.Val();
	}
	if (!expr.IsRealList())   return expr;
	return Cons(Renum(assocList, expr.Left(),NextFreeVarNum),
					Renum(assocList, expr.Right(),NextFreeVarNum));
}
*/
/****************** class Query                   *******************/

/*
bool HasExpr(LispPtr expr, const LispRef Has)
{
	if (expr==Has)
  	return true;
	if (expr.IsRealList())
		if (!HasExpr(expr.Left(),Has))
			return HasExpr(expr.Right(),Has);
		else
			return true;
  	return false;
}
*/
/*
bool PrologMatch(	  LispRef& assocList,
				LispPtr Goal,
				LispPtr Patt)
  {
  //cout << "PMSR" << Source << "\n";
  //cout << "PMDS" << Dest   << "\n";
	if (IsVar(Goal)||IsVar(Patt))
	  {
		LispRef GoalDef=UseAssoc(assocList,Goal);
		LispRef PattDef=UseAssoc(assocList,Patt);
		if (IsVar(GoalDef)||IsVar(PattDef))
		  {
			if (IsVar(GoalDef))
				if (IsVar(PattDef))
				  {
					if (GoalDef!=PattDef)
						AddAssoc(assocList,GoalDef,PattDef);
					return true;
				  }
				else
				  {
					if (HasExpr(PattDef,GoalDef))
						return false;
					AddAssoc(assocList,GoalDef,PattDef);
					return true;
				  } 
			else
			  {
				if (HasExpr(GoalDef,PattDef))
					return false;
				AddAssoc(assocList,PattDef,GoalDef);
				return true;
			  }
		  }
		else return PrologMatch(assocList,GoalDef,PattDef);
	  }
	if (!IsRealList(Goal)) return Goal==Patt;
	if (!IsRealList(Patt)) return false;
	if (	   PrologMatch(assocList,First(Goal),First(Patt)))
		return PrologMatch(assocList,Rest (Goal),Rest (Patt));
	return false;
  }

LispRef LockedCond;

int NrLockedGoals=0;

inline void PushLock(LispPtr Goal)
  {
	NrLockedGoals++;
	AddAssoc(LockedCond,Goal,EmptyList);
  }

inline void PopLock(LispPtr Goal)
  {
	dms_assert(First(First(LockedCond))==Goal);
	dms_assert(NrLockedGoals>0);
	LockedCond=Rest(LockedCond);
	NrLockedGoals--;
  }

class Query {
  protected:
	Query(const VarNrType NFV):NFVExpr(NFV),NFVRslt(NFV),Active(false) {};
	virtual void Call(LispPtr expr) =0;
  public:
    virtual void Redo()                =0;
	 virtual ~Query()                   {};
	int  IsActive()                   { return Active; };
	LispRef  GetLast()  		       	  { return LastResult; };
    VarNrType GetNFV()				  { return NFVRslt; };
  protected:
    bool Active;
	 LispRef LastResult;
	VarNrType NFVExpr;	// Next Free Vars.
    VarNrType NFVRslt;
  };


class CondListQuery:public Query {
  public:
	CondListQuery(LispPtr expr, const VarNrType NFV):Query(NFV)
	  {
		FirstCondQ=NULL;
		RestCondQ =NULL;
		Call(expr);
      };
	virtual void Redo();
   ~CondListQuery()
  	  {
		if (FirstCondQ) delete FirstCondQ;
		if (RestCondQ ) delete RestCondQ;
	  };
  private:
	virtual void Call(LispPtr expr);
	LispRef FirstCond;
	 LispRef RestCond;
    Query* FirstCondQ;
	Query* RestCondQ;
  };

class CondQuery:public Query {
  public:
	CondQuery(LispPtr expr, const NFV):Query(NFV)
	  {
		CondListQ=NULL;
		HadAll=false;
		Call(expr);
      };
	virtual void Redo();
   ~CondQuery()
	  {
		if (CondListQ) delete CondListQ;
	  };

  private:
	virtual void Call(LispPtr expr);
	LispRef AssocP;		// assoc from Prolog match
	LispRef Goal;
	LispRef RestEnv;
	Query* CondListQ;
	bool HadAll;
  };

void CondQuery::Call(LispPtr expr)
  {
	if (NrLockedGoals>5) return;						// Protect looping
	Active  =true;
	Goal    = expr;
	cout << "Call:" << Goal << "\n"; //DEBUG
	if (!HasAnyVar(Goal))  			// Hook to Eval!!!
		if (EndP(Eval(Goal)))
		  {
			HadAll=true;
			return;
		  }
 //if (!EndP(GetAssoc(LockedCond,Goal))) return;		// Protect looping

	RestEnv = GlobalEnv;
	HadAll  =false;
	Redo();
  };

void CondQuery::Redo()
  {
	if (HadAll)  Active=false;
	if (!Active) return;
	cout << "CondG" << ++Level << ":" << Goal << "\n";  //DEBUG
	while (!EndP(RestEnv))
	  {
		PushLock(Goal);
		if (!CondListQ)
		  {
			LispRef envDef=First(RestEnv);		// V: env
			AssocP=EmptyList;               // V: env+goal
			if (PrologMatch(AssocP,Goal,First(envDef)))
			  {
				LispRef RenumP;          // D: env; B: new
				NFVRslt=NFVExpr;
				envDef=Renum(RenumP,envDef,NFVRslt);	// V: new
					 AssocP=ApplyAssoc(RenumP,AssocP); 			// V: goal+new
				CondListQ = CheckNew(CondListQuery,
							(UseAssoc(AssocP,Rest(envDef)),
							NFVRslt));
							  // V: krn(g+b); New vars must be above
		      }
		  }
		else
			CondListQ->Redo();
	    PopLock(Goal);

		if (CondListQ)
		  {
		  	if (CondListQ->IsActive())
			  {
				LastResult =
					UseAssoc(
						CondListQ->GetLast(),
						AssocP);
				NFVRslt=CondListQ->GetNFV();		//Optimize more                
//				HadAll=(LastResult==EmptyList);
				cout << "CondR" << Level-- << ":" << LastResult << "\n";   //DEBUG
				return;
			  }
			delete CondListQ; CondListQ=NULL;
		  }
		RestEnv=Rest(RestEnv);
		if (EndP(RestEnv))
			if (EndP(EvalDirect(Goal)))
			  {
				LastResult=EmptyList;
				NFVRslt=NFVExpr;
				HadAll=true;
				cout << "CondD" << Level-- << ":" << LastResult << "\n";  //DEBUG
				return;
			  }
	  }
	cout << "CondE" << Level-- << ":" << LastResult << "\n"; //DEBUG
	Active=0;
  }

LispRef ConcatLists(LispPtr A, LispPtr B)
  {
	 if (EndP(A)) return B;
    return Cons(First(A),ConcatLists(Rest(A),B));
  }

void CondListQuery::Call(LispPtr expr)
  {
	Active  =true;
	if (EndP(expr))
	  {
		 FirstCond = FailSymbol;
    	return;
  	  }
	FirstCond = First(expr);
	RestCond  = Rest(expr);
  	FirstCondQ=CheckNew(CondQuery,(FirstCond,NFVExpr));
	Redo();
  }

void CondListQuery::Redo()
  {
	 if (FirstCond==FailSymbol) Active=false;
    if (!Active) return;

  	while (FirstCondQ->IsActive())
	  {
		 if (!RestCondQ)
		  {
			RestCondQ=CheckNew(
				CondListQuery,
				(UseAssoc(FirstCondQ->GetLast(),RestCond),
				 FirstCondQ->GetNFV()));
		  }
		else
			RestCondQ->Redo();
		if (RestCondQ)
		  {
			 if (RestCondQ->IsActive())
			  {
				LastResult=ConcatLists(FirstCondQ->GetLast(),
										RestCondQ ->GetLast());
					 NFVRslt=RestCondQ->GetNFV();
				  return;
				}
			delete RestCondQ; RestCondQ=NULL;
				FirstCondQ->Redo();
		  }
	  }
	Active=false;
  }
*/
/****************** Prolog like query             *******************/
/*
VarNrType NextGoalVar = 1000;


LispRef BoolEvalCond(LispPtr Goal)
  // Precondition: Goal bevat geen vars boven NextGoalVar
  //		reden: bij het terugsubsitueren zouden deze conflicteren
  //				met free vars van het resultaat.
  //				Goal mag wel env-variabelen bevatten; deze komen in
  //				het resultaat niet voor.
  {
	LispRef       result;
	ProMod = 1;
	cout << "BlEvS" << ++Level << ":" << Goal << "\n";		//DEBUG

	VarNrType NFV = NextGoalVar;
	LispRef AssocG;
	LispRef NewGoal=Renum(AssocG,Goal,NFV);
	LispRef AssocR=ReverseAssoc(AssocG);

	CondQuery G(NewGoal,NFV);
	while (G.IsActive())
	  {
		cout << "BlEvR" << Level << ":" << G.GetLast() << "\n";
		result = Cons(ApplyAssoc(AssocR,G.GetLast()),result);
		G.Redo();
	  }
	cout << "BlEvE" << Level-- << "\n";
	ProMod = 0;
	return result;
  }

LispRef BoolEvalCondList(LispPtr Goal)
  // Precondition: Goal bevat geen vars boven NextGoalVar
  //		reden: bij het terugsubsitueren zouden deze conflicteren
  //				met free vars van het resultaat.
  //				Goal mag wel env-variabelen bevatten; deze komen in
  //				het resultaat niet voor.
  {
	LispRef       result;
	ProMod = 1;
	cout << "BlEvS" << ++Level << ":" << Goal << "\n";		//DEBUG

	VarNrType NFV = NextGoalVar;
	LispRef AssocG;
	LispRef NewGoal=Renum(AssocG,Goal,NFV);
	LispRef AssocR=ReverseAssoc(AssocG);

	CondListQuery G(NewGoal,NFV);
	while (G.IsActive())
	  {
		cout << "BlEvR" << Level << ":" << G.GetLast() << "\n";
		result = Cons(ApplyAssoc(AssocR,G.GetLast()),result);
		G.Redo();
	  }
	cout << "BlEvE" << Level-- << "\n";
	ProMod = 0;
	return result;
  }

*/


