// Copyright (C) 1998-2024 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif


#if !defined(__TIC_ABSTR_CALCULATOR_H)
#define __TIC_ABSTR_CALCULATOR_H

#include "TicBase.h"

#include "MetaInfo.h"

#include "OperArgPolicy.h"
#include "TreeItemSet.h"

enum class CalcRole : UInt8
{
	Other       = 0,
	Calculator  = 1,
	ArgCalc     = 2,
	Checker     = 3,
	SourceSubst = 4,
//	MASK       = 0x0003
};

struct ActorVisitor;
enum class SupplierVisitFlag;

// *****************************************************************************
// struct SubstitutionBuffer 
// *****************************************************************************

struct SubstitutionBuffer
{
	using TTreeItemSet = std::map<const TreeItem*, UInt32>;
	using LispRefAssoc = std::map<LispRef, LispRef>;

	const ActorVisitor* optionalVisitor = nullptr;
	SupplierVisitFlag svf = SupplierVisitFlag();
	ActorVisitState avs = AVS_Ready;

	LispRefAssoc m_SubstituteBuffer[3];
	TTreeItemSet m_SupplierSet;

	bool m_CollectSuppliers;

	SubstitutionBuffer(bool mustCollectSuppliers = false)
		: m_CollectSuppliers(mustCollectSuppliers)
	{}

	LispRef& BufferedLispRef(metainfo_policy_flags mpf, LispPtr key);
};

// *****************************************************************************
// Section:     Calculator interface with Constructor Factory
// *****************************************************************************

TIC_CALL AbstrCalculatorRef CreateCalculatorForTreeItem(TreeItem* context, const TreeItem* sourceObject, const CopyTreeContext& copyContext);

//----------------------------------------------------------------------
// now independent Calculator related function; TODO G8: dismantle Calculators
//----------------------------------------------------------------------

using make_result_t = DataControllerRef; // std::pair<DataControllerRef, SharedTreeItem>; second is optionallu owned by first
using calc_result_t = FutureData; // std::pair<DataControllerRef, SharedTreeItemInterestPtr>; second is optionallu owned by first

TIC_CALL auto GetDC(const AbstrCalculator* calculator)->DataControllerRef;
auto MakeResult(const AbstrCalculator* calculator)->make_result_t;
auto CalcResult(const AbstrCalculator* calculator, const Class* cls)->calc_result_t;
void ExplainResult(const AbstrCalculator* calculator, Explain::Context* context);

void CheckResultingTreeItem(const TreeItem* refItem, const Class* desiredResultingClass);

TIC_CALL void InstantiateTemplate(TreeItem* holder, const TreeItem* applyItem, LispPtr templCallArgList);
TIC_CALL void ApplyAsMetaFunction(TreeItem* holder, const AbstrCalculator* ac, const AbstrOperGroup* og,  LispPtr metaCallArgs);

//----------------------------------------------------------------------
// AcConstructor
//----------------------------------------------------------------------

struct AcConstructor
{
	virtual AbstrCalculatorRef ConstructExpr    (const TreeItem* context, WeakStr expr, CalcRole cr) =0;
	virtual AbstrCalculatorRef ConstructDBT     (AbstrDataItem* context, const AbstrCalculator* src) =0;
	virtual LispRef RewriteExprTop(LispPtr org) =0;
};

//----------------------------------------------------------------------
// AbstrCalculator
//----------------------------------------------------------------------

class AbstrCalculator: public SharedObj // TODO G8: RENAME TO AbstrExprKey
{
public:

	TIC_CALL AbstrCalculator(const TreeItem* context, CalcRole cr);
	TIC_CALL AbstrCalculator(const TreeItem* context, LispPtr lispRefOrg, CalcRole cr);

	TIC_CALL ~AbstrCalculator();

	TIC_CALL static AcConstructor* GetConstructor();
	TIC_CALL static void SetConstructor(AcConstructor* constructor);

	TIC_CALL static AbstrCalculatorRef ConstructFromStr      (const TreeItem* context, WeakStr expr, CalcRole cr);
	TIC_CALL static AbstrCalculatorRef ConstructFromDirectStr(const TreeItem* context, WeakStr expr, CalcRole cr);
	TIC_CALL static AbstrCalculatorRef ConstructFromLispRef  (const TreeItem* context, LispPtr lispExpr, CalcRole cr);
	TIC_CALL static AbstrCalculatorRef ConstructFromDBT      (AbstrDataItem* context, const AbstrCalculator* src);

	static bool MustEvaluate(CharPtr expr) { dms_assert(expr); return *expr == '='; }
	static LispRef RewriteExprTop(LispPtr org) { return GetConstructor()->RewriteExprTop(org); }

	TIC_CALL static BestItemRef GetErrorSource(const TreeItem* context, WeakStr expr);
	TIC_CALL static SharedStr EvaluatePossibleStringExpr(const TreeItem* context, WeakStr expr, CalcRole cr);
	TIC_CALL static SharedStr EvaluateExpr(const TreeItem* context, CharPtrRange expr, CalcRole cr, UInt32 nrEvals);

	TIC_CALL virtual ActorVisitState VisitSuppliers(SupplierVisitFlag svf, const ActorVisitor& visitor) const;
	TIC_CALL static ActorVisitState VisitImplSuppl(SupplierVisitFlag svf, const ActorVisitor& visitor, const TreeItem* context, WeakStr expr, CalcRole cr);
	TIC_CALL static const TreeItem* GetSearchContext(const TreeItem* holder, CalcRole cr);
	auto VisitSourceItem(TokenID supplRefID, SupplierVisitFlag svf, const ActorVisitor& visitor) const->std::optional<SharedTreeItem>;

	TIC_CALL virtual bool CheckSyntax () const;
	TIC_CALL BestItemRef FindErrorneousItem() const;
	TIC_CALL BestItemRef FindPrimaryDataFailedItem() const;

	TIC_CALL auto GetSourceItem() const->SharedPtr<const TreeItem>;  // directly referred persistent object.

	TIC_CALL virtual SharedStr GetExpr() const;
	TIC_CALL virtual void      WriteHtmlExpr(OutStreamBase& stream) const;
	virtual TIC_CALL MetaInfo  GetMetaInfo() const;
	virtual LispRef   GetLispExprOrg() const { return m_LispExprOrg; } // overridable, but with default behaviour

	TIC_CALL virtual bool IsSourceRef() const;

	TIC_CALL SharedStr GetAsFLispExpr(FormattingFlags ff)    const;
	TIC_CALL SharedStr GetAsFLispExprOrg(FormattingFlags ff) const;

	virtual bool        IsDataBlock() const { return false; }
	virtual bool        IsDcPtr    () const { return false; }

	TIC_CALL bool            HasTemplSource() const;
	TIC_CALL const TreeItem* GetTemplSource() const;

	TIC_CALL bool            IsForEachTemplHolder()  const;
	TIC_CALL const TreeItem* GetForEachTemplSource() const;

	const TreeItem* GetHolder() const { return m_Holder; }

	TIC_CALL const TreeItem* SearchContext() const;
	TIC_CALL const TreeItem* FindItem(TokenID itemRef) const;
	TIC_CALL auto FindOrVisitItem(SubstitutionBuffer& buff, TokenID itemRef) const -> SharedTreeItem;
	TIC_CALL BestItemRef FindBestItem(TokenID itemRef) const;
	MetaInfo SubstituteExpr(SubstitutionBuffer& substBuff, LispPtr localExpr) const;

	LispRef slSupplierExpr(SubstitutionBuffer& substBuff, LispPtr supplRef, metainfo_policy_flags mpf) const;
	LispRef slSupplierExprImpl(SubstitutionBuffer& substBuff, const TreeItem* supplier, metainfo_policy_flags mpf) const;

//	void ScanArgsForSuppliers(SubstitutionBuffer& substBuff, LispPtr localArgs) const;

	LispRef SubstituteArgs(SubstitutionBuffer& substBuff, LispPtr localArgs, const AbstrOperGroup* og, arg_index argNr, SharedStr firstArgValue) const;
	LispRef SubstituteExpr_impl(SubstitutionBuffer& substBuff, LispRef localExpr, metainfo_policy_flags mpf) const;

	mutable WeakPtr<const TreeItem>   m_Holder;
	mutable SharedPtr<const TreeItem> m_SearchContext; // parent of instantiatior in case of argument binding

//protected:
	mutable LispRef m_LispExprOrg; // TODO G8: required for ExprCalculator and DC_Ptr(ArgCalc), but not for DataBlockTask and maybe also not for DC_Ptr(Calc)
	mutable MetaInfo m_LispExprSubst;
	
	mutable bool     
		m_HasParsed      : 1 = false,
		m_HasSubstituted : 1 = false,
		m_HasCollectedNamedSuppliers: 1 = false;

	CalcRole m_CalcRole : 2 = CalcRole::Calculator;

	mutable BestItemRef m_BestGuessErrorSuppl;
	mutable std::vector<SharedTreeItem> m_NamedSuppliers;

	friend struct TreeItem;
	friend struct InterestReporter;
};


#endif // __TIC_ABSTR_CALCULATOR_H
