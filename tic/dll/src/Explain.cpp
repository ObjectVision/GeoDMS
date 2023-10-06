// Copyright (C) 2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "TicPCH.h"
#pragma hdrstop

#include <memory>

#include "Explain.h"

#include "act/ActorVisitor.h"
#include "act/SupplierVisitFlag.h"
#include "act/SupplInterest.h"
#include "dbg/DmsCatch.h"
#include "mci/ValueWrap.h"
#include "mci/ValueClass.h"
#include "xct/DmsException.h"

#include "LockLevels.h"
#include "LispRef.h"

#include "AbstrDataItem.h"
#include "AbstrCalculator.h"
#include "AbstrDataObject.h"
#include "DataController.h"
#include "DataLocks.h"
#include "Unit.h"
#include "UnitClass.h"
#include "DisplayValue.h"
#include "Operator.h"
#include "LispTreeType.h"
#include "TreeItemProps.h"
#include "TreeItemClass.h"
#include "TreeItemContextHandle.h"
#include "xml/XmlTreeOut.h"

//  -----------------------------------------------------------------------
//  Name        : dms\tic\src\Explain.cpp
//  Descr       : contains various functions for item and value description
//  Uses        : html/xml formatting and DMS urls
//  -----------------------------------------------------------------------

#define MAX_LEVEL 1
typedef InterestPtr<DataControllerRef> CalcInterestPtr;

leveled_critical_section scs_ExplainAccess(item_level_type(0), ord_level_type::MOST_INNER_LOCK, "ExplainAccess");

//  -----------------------------------------------------------------------
//  struct CalcExplanation
//  -----------------------------------------------------------------------

typedef InterestPtr<const TreeItem*> TreeItemInterestPtr;

namespace Explain { // local defs
	using CoordinateCollectionType = std::vector<CoordinateType>;
	static auto calculatingStr = SharedStr("Calculating...");

	struct AbstrCalcExplanation
	{
		SharedDataItemInterestPtr             m_DataItem;
		const AbstrUnit*                      m_UltimateDomainUnit;
		const AbstrUnit*                      m_UltimateValuesUnit;
		CoordinateCollectionType              m_Coordinates;
		mutable SharedDataItemInterestPtrTuple m_Interests;
		mutable GuiReadLockPair                m_UnitLabelLocks;
		bool                                  m_IsExprOfExistingItem = false;
		AbstrCalcExplanation(const AbstrDataItem* dataItem)
			: m_DataItem(dataItem)
			, m_UltimateDomainUnit(AsUnit(dataItem->GetAbstrDomainUnit()->GetUltimateItem()))
			, m_UltimateValuesUnit(AsUnit(dataItem->GetAbstrValuesUnit()->GetUltimateItem()))
		{}
		virtual ~AbstrCalcExplanation()
		{
			reportD(SeverityTypeID::ST_MinorTrace, "Byte");
		}

		virtual ArgRef GetCalcDataItem(Context* context) const = 0;

		CoordinateType* AddIndex(SizeT index) // returns nullptr if abundant
		{
			if (!IsDefined(index))
				return nullptr;
			for (auto& c : m_Coordinates)
				if (c.first == index)
					return &c;

			if (m_Coordinates.size() >= MaxNrEntries)
				return nullptr;

			m_Coordinates.push_back(CoordinateType(index, AbstrValueRef())); // calculate later
			return &m_Coordinates.back();
		}

		const AbstrValue* CalcValue(Context* context); // returns nullptr if suspended

		void GetDescr(CalcExplImpl* self, OutStreamBase& stream, bool& isFirst, bool showHidden) const;
		void DescrValue(OutStreamBase& stream) const;

		virtual void GetDescrImpl(CalcExplImpl* self, OutStreamBase& stream, bool isFirst, bool showHidden) const = 0;
		virtual void PrintSeqNr(OutStreamBase& stream) const = 0;

		auto RelativeExprPath() const -> SharedStr
		{
			VectorOutStreamBuff buff;
			OutStream_DMS x(&buff, nullptr);
			PrintSeqNr(x);
			return SharedStr(buff.GetData(), buff.GetDataEnd());
		}
		auto MatchesExtraInfo(std::string_view extraInfo) const -> match_status
		{
			CheckEqualityOutStreamBuff buff(CByteRange(begin_ptr(extraInfo), end_ptr(extraInfo)));
			OutStream_DMS x(&buff, nullptr);
			PrintSeqNr(x);
			return buff.GetStatus();
		}

	protected:
		void GetDescrBase(CalcExplImpl* self, OutStreamBase& stream, bool isFirst, const AbstrUnit* domainUnit, const AbstrUnit* valuesUnit) const;
	};

	struct DataCalcExplanation : AbstrCalcExplanation
	{
		DataCalcExplanation(const AbstrDataItem* dataItem)
			:	AbstrCalcExplanation(dataItem)
		{}

		ArgRef GetCalcDataItem(Context* context) const override { return ArgRef(std::in_place_type<SharedTreeItem>, m_DataItem.get_ptr()); }

		void GetDescrImpl(CalcExplImpl* self, OutStreamBase& stream, bool isFirst, bool showHidden) const override
		{
			NewLine(stream);
			{
				XML_hRef supplRef(stream, ItemUrl(m_DataItem.get_ptr()).c_str());
				stream << m_DataItem->GetName().c_str();
			}
			stream << " := ";  GetExprOrSourceDescr(stream, m_DataItem.get_ptr());
			NewLine(stream);

			GetDescrBase(self, stream, isFirst, m_DataItem->GetAbstrDomainUnit(), m_DataItem->GetAbstrValuesUnit());

			if (!isFirst)
				return;

			NewLine(stream);
		}
		void PrintSeqNr(OutStreamBase& stream) const override
		{
			auto fullName = SharedStr(m_DataItem->GetFullName());
			stream << fullName.c_str();
		}
	};

	struct LispCalcExplanation : AbstrCalcExplanation
	{
		LispCalcExplanation(const AbstrCalculator* calcPtr, const AbstrCalcExplanation* parent, arg_index seqNr)
			: LispCalcExplanation(GetDC(calcPtr), calcPtr, parent, seqNr)

		{}

		LispCalcExplanation(DataControllerRef dc, const AbstrCalculator* calcPtr, const AbstrCalcExplanation* parent, arg_index seqNr)
			: AbstrCalcExplanation(AsDataItem(dc->MakeResult().get_ptr()))
			, m_DC(dc)
			, m_CalcPtr(calcPtr)
			, m_Parent(parent)
			, m_SeqNr(seqNr)
		{
			assert(calcPtr);
		}

		ArgRef GetCalcDataItem(Context* context) const override
		{
			ExplainResult(m_CalcPtr, context);
			return CalcResult(m_CalcPtr, AbstrDataItem::GetStaticClass());
		}

		void GetDescrImpl(CalcExplImpl* self, OutStreamBase& stream, bool isFirst, bool showHidden) const override;

		virtual void AddLispExplanations(CalcExplImpl* self, LispPtr lispExprPtr, UInt32 level);

		void PrintSeqNr(OutStreamBase& stream) const override
		{
			if (m_Parent) {
				m_Parent->PrintSeqNr(stream);
				if (dynamic_cast<const DataCalcExplanation*>(m_Parent))
					stream << "?";
			}
			stream << AsString(m_SeqNr).c_str() << ".";
		}

		DataControllerRef                m_DC;
		SharedPtr<const AbstrCalculator> m_CalcPtr;
		const AbstrCalcExplanation*      m_Parent;
		arg_index                        m_SeqNr;
		SupplInterestListPtr             m_SupplInterest;
	};

	using Factor = LispRef;
	using Term = std::vector<Factor>;
	using SignedTerm = std::pair<bool, Term>;
	using SumOfTermsExpr = std::vector<SignedTerm>;

	using Predicate = LispRef;
	using Intersection = std::vector<Predicate>;
	using Union = std::vector<Intersection>;

	void operator <<=(Term& left, Term&& right)
	{
		left.reserve(left.size() + right.size());
		for (auto& factor : right)
			left.emplace_back(std::move(factor));
	}


	struct SumOfTermsExplanation : LispCalcExplanation
	{
		SumOfTermsExplanation(const AbstrCalculator* calcPtr, const AbstrCalcExplanation* parent, arg_index seqNr)
			: LispCalcExplanation(calcPtr, parent, seqNr)
		{
			ProcessTerms(calcPtr->GetLispExprOrg(), true);
		}

		void ProcessTerms(LispPtr terms, bool additive)
		{
			assert(CanHandle(terms));
			if (!IsSumOfTerms(terms))
			{
				auto term = ProcessFactors(terms);
				m_Expr.emplace_back(additive, std::move(term));
				return;
			}

			bool addThisTerm = additive;
			bool isSub = (terms.Left().GetSymbID() == token::sub);
			bool addNextTerms = isSub ? (!addThisTerm) : addThisTerm;

			for (auto termListPtr = terms.Right(); termListPtr.IsRealList(); termListPtr = termListPtr.Right())
			{
				auto nextTerm = termListPtr.Left();
				if (CanHandle(nextTerm))
					ProcessTerms(nextTerm, addThisTerm);
				else
				{
					auto term = ProcessFactors(nextTerm);
					m_Expr.emplace_back(addThisTerm, std::move(term));
				}
				addThisTerm = addNextTerms;
			}
		}
		Term ProcessFactors(LispPtr factors)
		{
			Term result;
			if (IsProductOfFactors(factors))
				for (auto factorListPtr = factors.Right(); factorListPtr.IsRealList(); factorListPtr = factorListPtr.Right())
					result <<= ProcessFactors(factorListPtr.Left());
			else
				result.emplace_back(ProcessFactor(factors));
			return result;
		}

		Factor ProcessFactor(LispPtr factors)
		{
			return factors;
		}

		SumOfTermsExpr m_Expr;

		static bool CanHandle(LispPtr lispExpr)
		{
			return IsSumOfTerms(lispExpr) || IsProductOfFactors(lispExpr);
		}

		static bool IsSumOfTerms(LispPtr lispExpr)
		{
			if (!lispExpr.IsRealList())
				return false;
			if (!lispExpr.Left().IsSymb())
				return false;
			auto symbID = lispExpr.Left().GetSymbID();
			return symbID == token::add || symbID == token::sub;
		}

		static bool IsProductOfFactors(LispPtr lispExpr)
		{
			if (!lispExpr.IsRealList())
				return false;
			if (!lispExpr.Left().IsSymb())
				return false;
			auto symbID = lispExpr.Left().GetSymbID();
			return symbID == token::mul ;
		}

		void GetDescrImpl(CalcExplImpl* self, OutStreamBase& stream, bool isFirst, bool showHidden) const override;
		void AddLispExplanations(CalcExplImpl* self, LispPtr lispExprPtr, UInt32 level) override;
	};

	struct UnionOfAndsExplanation : LispCalcExplanation
	{
		UnionOfAndsExplanation(const AbstrCalculator* calcPtr, const AbstrCalcExplanation* parent, arg_index seqNr)
			: LispCalcExplanation(calcPtr, parent, seqNr)
		{
			Process(calcPtr->GetLispExprOrg(), 0);
		}

		void Process(LispPtr terms, UInt32 level)
		{
			assert(CanHandle(terms));
			if (!IsUnion(terms))
			{
				assert(IsIntersection(terms));
				auto term = ProcessIntersection(terms, level);
				m_Expr.emplace_back(std::move(term));
				return;
			}

			for (auto termListPtr = terms.Right(); termListPtr.IsRealList(); termListPtr = termListPtr.Right())
			{
				auto nextTerm = termListPtr.Left();
				if (CanHandle(nextTerm))
					Process(nextTerm, level+1);
				else
				{
					auto term = ProcessIntersection(nextTerm, level+1);
					m_Expr.emplace_back(std::move(term));
				}
			}
		}
		Intersection ProcessIntersection(LispPtr factors, UInt32 level)
		{
			Intersection result;
			if (IsIntersection(factors) && level < 3)
			{
				bool isRightIntersection = false;
				for (auto factorListPtr = factors.Right(); factorListPtr.IsRealList(); factorListPtr = factorListPtr.Right())
				{
					result <<= ProcessIntersection(factorListPtr.Left(), level + UInt32(isRightIntersection));
					isRightIntersection = true;
				}
			}
			else
				result.emplace_back(ProcessPredicate(factors));
			return result;
		}

		Factor ProcessPredicate(LispPtr factors)
		{
			return factors;
		}

		Union m_Expr;

		static bool CanHandle(LispPtr lispExpr)
		{
			return IsUnion(lispExpr) || IsIntersection(lispExpr);
		}

		static bool IsUnion(LispPtr lispExpr)
		{
			if (!lispExpr.IsRealList())
				return false;
			if (!lispExpr.Left().IsSymb())
				return false;
			auto symbID = lispExpr.Left().GetSymbID();
			return symbID == token::or_;
		}

		static bool IsIntersection(LispPtr lispExpr)
		{
			if (!lispExpr.IsRealList())
				return false;
			if (!lispExpr.Left().IsSymb())
				return false;
			auto symbID = lispExpr.Left().GetSymbID();
			return symbID == token::and_;
		}
		void GetDescrImpl(CalcExplImpl* self, OutStreamBase& stream, bool isFirst, bool showHidden) const override;
		void AddLispExplanations(CalcExplImpl* self, LispPtr lispExprPtr, UInt32 level) override;
	};


	/*
	Description of value(s) of <dataitemname>
	*   the value of the <x>-th <loc-unit> is <y> <values-unit>
	because is is calculated according to <expr> / because this is read from (storagefilename)
	It has the following supplier(s):
	*  <suppliers>
	*/

	
	using QueueEntry        = std::pair<const AbstrUnit*, SizeT>;
	using ExplArrayEntry    = OwningPtr<AbstrCalcExplanation>;
	using ExplArray         = std::vector<ExplArrayEntry>;
	using ItemInterestArray = std::vector<TreeItemInterestPtr>;
	using CalcInterestArray = std::vector<CalcInterestPtr>;

	struct CalcExplImpl
	{
		void Init(const AbstrDataItem* studyObject, SizeT studyIdx, CharPtr extraInfo);
		bool IsClear() const;
		void AddQueueEntry(const AbstrUnit* domain, SizeT index);
		bool ProcessQueue(); // returns false if suspended
		void AddExplanation(const AbstrDataItem* explItem);
		void AddLispExplanation(LispPtr lispExprOrg, UInt32 level, const AbstrCalcExplanation* parent, arg_index seqNr);
		void AddExplanations(const Actor* studyActor);
		void AddExplanations();
		bool IsKnownDomain(const AbstrUnit* valuesUnit);
		bool IsExplainable(const AbstrUnit* valuesUnit, SizeT index);
		auto FindExpl(LispPtr key) -> const LispCalcExplanation*;
		auto URL(const LispCalcExplanation* expl, SizeT recNo) -> SharedStr;

		ExplArray                m_Expl;
		std::set<LispRef>        m_KnownExpr;
		std::vector<QueueEntry > m_Queue;
		SizeT                    m_DoneQueueEntries = 0, m_DoneExpl = 0;

		ItemInterestArray        m_ItemInterests;
		CalcInterestArray        m_CalcInterests;

		SharedDataItem           m_StudyObject = 0;
		SizeT                    m_StudyIdx = -1;
		std::string              m_ExtraInfo;
		std::string_view         m_ExprRelPath;
		SizeT                    m_ExprLocationIdx = -1;
		arg_index                m_ExprSeqNr  = 0;
		arg_index                m_ExprLevel  = 0;
		TimeStamp                m_LastChange = 0;

		friend struct CalcExplanations;
	};

	void CalcExplImpl::Init(const AbstrDataItem* studyObject, SizeT studyIdx, CharPtr extraInfo)
	{
		if (!extraInfo)
			extraInfo = "";

		if (studyObject == m_StudyObject
			&& studyIdx == m_StudyIdx
			&& (strcmp(extraInfo, m_ExtraInfo.c_str()) == 0)
			&& (!studyObject || studyObject->GetLastChangeTS() == m_LastChange)
			)
			return;

		m_StudyObject = studyObject;
		m_StudyIdx = studyIdx;
		m_ExtraInfo = extraInfo;
		m_ExprSeqNr = 0;
		m_ExprLevel = 0;
		m_Queue.clear();
		m_DoneQueueEntries = m_DoneExpl = 0;
		if (m_ExtraInfo.contains(':'))
		{
			auto colonPos = m_ExtraInfo.find(':');
			m_ExprRelPath = std::string_view(m_ExtraInfo.data(), colonPos);
			m_ExprLocationIdx = Convert<SizeT>(std::string_view(m_ExtraInfo.begin() + colonPos + 1, m_ExtraInfo.end()));
		}
		else
		{
			m_ExprRelPath = m_ExtraInfo;
			m_ExprLocationIdx = m_StudyIdx;
		}

		auto oldExpl = std::move(m_Expl);
		auto oldLispRefSet = std::move(m_KnownExpr);
		auto oldItemInterests = std::move(m_ItemInterests);
		auto oldCalcInterests = std::move(m_CalcInterests);

		if (studyObject)
		{
			m_LastChange = studyObject->GetLastChangeTS();

			AddExplanations();

			if (m_ExtraInfo.empty())
				AddQueueEntry(m_Expl[0]->m_UltimateDomainUnit, studyIdx);
			AddQueueEntry(Unit<Void>::GetStaticClass()->CreateDefault(), 0);
		}
	}

	bool CalcExplImpl::IsClear() const
	{
		assert(m_Queue.empty());
		assert(m_Expl.empty());
		assert(m_ItemInterests.empty());
		assert(m_CalcInterests.empty());
		assert(m_StudyObject.is_null());
		return m_Expl.empty() && m_Queue.empty() && m_ItemInterests.empty() && m_CalcInterests.empty() && m_StudyObject.is_null();
	}

	void CalcExplImpl::AddQueueEntry(const AbstrUnit* domain, SizeT index)
	{
		leveled_critical_section::scoped_lock lock(scs_ExplainAccess);

		dms_assert(domain);
		domain = AsUnit(domain->GetUltimateItem());
		dms_assert(domain);
		for (auto entryPtr = m_Queue.begin(); entryPtr != m_Queue.end(); ++entryPtr)
			if (entryPtr->first == domain && entryPtr->second == index)
				return;

		if (IsExplainable(domain, index))
			m_Queue.push_back(QueueEntry(domain, index));
	}

	bool CalcExplImpl::ProcessQueue() // returns false if suspended
	{
		while (m_DoneQueueEntries < m_Queue.size())
		{
			QueueEntry entry = m_Queue[m_DoneQueueEntries];
			for (; m_DoneExpl < m_Expl.size(); ++m_DoneExpl)
			{
				AbstrCalcExplanation* explanation = m_Expl[m_DoneExpl].get();

				if (entry.first != explanation->m_UltimateDomainUnit)
					continue;

				CoordinateType* coordPtr = explanation->AddIndex(entry.second);
				if (!coordPtr || !IsDefined(coordPtr->first))
					continue;
				dms_assert(coordPtr->second.is_null()); // we don't expect to process the same entry twice

				Context context{ this, entry.first, coordPtr };

				const AbstrValue* value = explanation->CalcValue(&context);
				if (!value)
					return false; // suspend or NULL

				const AbstrUnit* valuesUnit = explanation->m_UltimateValuesUnit;
				if (!value->IsNull() && (value->GetValueClass() != ValueWrap<SharedStr>::GetStaticClass()) && IsKnownDomain(valuesUnit))
				{
					if (!valuesUnit->PrepareDataUsage(DrlType::Suspendible))
					{
						if (valuesUnit->WasFailed(FR_Data))
							continue;
						dms_assert(SuspendTrigger::DidSuspend());
						return false;
					}
					AddQueueEntry(valuesUnit, valuesUnit->GetIndexForAbstrValue(*value));
				}
			}

			m_DoneExpl = 0;
			++m_DoneQueueEntries;
		}
		return true;
	}

	void CalcExplImpl::AddExplanation(const AbstrDataItem* explItem)
	{
		for (const auto& explPtr : m_Expl)
			if (explPtr->m_DataItem == explItem)
				return;

		m_Expl.push_back(
			ExplArrayEntry(
				new DataCalcExplanation(explItem)
			)
		);
	}
	void CalcExplImpl::AddLispExplanation(LispPtr lispExprOrg, UInt32 level, const AbstrCalcExplanation* parent, arg_index seqNr)
	{
		if (!lispExprOrg.IsRealList() || !lispExprOrg.Left().IsSymb() || lispExprOrg.Left().GetSymbID() == token::sourceDescr)
			return;
		if (m_KnownExpr.contains(lispExprOrg))
			return;

		m_KnownExpr.insert(lispExprOrg);

		OwningPtr<LispCalcExplanation> newExpl;
		LispCalcExplanation* newExplPtr = nullptr;
		try {

			AbstrCalculatorRef calc = AbstrCalculator::ConstructFromLispRef(m_StudyObject, lispExprOrg, CalcRole::Calculator); // lispExpr already substitited ?
			auto metaInfo = calc->GetMetaInfo();

			//				if (metaInfo.index() == 2)
			//					AddLispExplanation(std::get<2>(metaInfo)->GetKeyExpr(CalcRole::Other), level, parent, seqNr);

			if (metaInfo.index() != 1)
				return;
			auto dc = GetExistingDataController(std::get<LispRef>(metaInfo));
			if (!dc)
				return;
			auto res = dc->MakeResult();
			if (!IsDataItem(res.get_ptr()))
				return;

			bool mustCalcNextLevel = true;
			if (SumOfTermsExplanation::CanHandle(lispExprOrg))
				newExpl = new  SumOfTermsExplanation(calc, parent, seqNr);
			else if (UnionOfAndsExplanation::CanHandle(lispExprOrg))
				newExpl = new  UnionOfAndsExplanation(calc, parent, seqNr);
			else
			{
				newExpl = new LispCalcExplanation(calc, parent, seqNr);
				mustCalcNextLevel = false;
			}
			auto matchInfo = newExpl->MatchesExtraInfo(m_ExprRelPath);
			if (matchInfo == match_status::different)
				return;

			if (matchInfo > match_status::partial)
				if (mustCalcNextLevel || (level + 1 < MaxLevel))
					++level;

			// check if already covered by visible supplying data item
			for (const auto& oldExpl : m_Expl)
			{
				//					if (oldExpl->MatchesExtraInfo(m_ExprRelPath) <= match_status::partial)
				//						continue;
				const AbstrDataItem* adi = oldExpl->m_DataItem;
				if (adi && !adi->IsCacheItem() && adi->HasCalculator())
					if (adi->GetCheckedKeyExpr() == dc->GetLispRef())
					{
						newExpl->m_IsExprOfExistingItem = true;
						break;
					}
			}

			newExplPtr = newExpl.release();
			m_Expl.push_back(ExplArrayEntry(newExplPtr));
			m_CalcInterests.push_back(dc);
			if (matchInfo == match_status::full)
				AddQueueEntry(newExplPtr->m_UltimateDomainUnit, m_ExprLocationIdx);
		}
		catch (const DmsException&)
		{
		}

		if (level < MaxLevel && newExplPtr)
			newExplPtr->AddLispExplanations(this, lispExprOrg, level);
	}

	void CalcExplImpl::AddExplanations(const Actor* studyActor)
	{
		VisitSupplProcImpl(studyActor, SupplierVisitFlag::Explain,
			[&](const Actor* supplier)
			{
				const AbstrDataItem* supplDI = AsDynamicDataItem(supplier);
				if (supplDI && !supplDI->IsCacheItem())
					AddExplanation(supplDI);
				else if (dynamic_cast<const AbstrCalculator*>(supplier))
					AddExplanations(supplier);
			}
		);
	}
	void CalcExplImpl::AddExplanations()
	{
		AddExplanation(m_StudyObject);
		AddExplanations(m_StudyObject);

		if (m_StudyObject->HasCalculator())
		{
			//				auto keyExpr = m_StudyObject->GetCalculator()->GetLispExprOrg();
			//				AddLispExplanation(keyExpr, 0, nullptr, ++m_ExprSeqNr);
			auto metaInfo = m_StudyObject->GetCurrMetaInfo({});
			if (metaInfo.index() == 2)
				AddExplanation(AsDataItem(std::get<SharedTreeItem>(metaInfo).get_ptr()));
			if (metaInfo.index() == 1)
				AddLispExplanation(std::get<LispRef>(metaInfo), 0, nullptr, ++m_ExprSeqNr);
		}
	}


	bool CalcExplImpl::IsKnownDomain(const AbstrUnit* valuesUnit)
	{
		for (const auto& expl : m_Expl)
			if (expl->m_UltimateDomainUnit == valuesUnit)
				return true;
		return false;
	}

	bool CalcExplImpl::IsExplainable(const AbstrUnit* valuesUnit, SizeT index)
	{
		bool result = false;
		for (const auto& expl : m_Expl)
			if (expl->m_UltimateDomainUnit == valuesUnit)
				if (expl->AddIndex(index))
					result = true;

		return result;
	}

	auto CalcExplImpl::FindExpl(LispPtr key) -> const LispCalcExplanation*
	{
		for (const auto& expl : m_Expl)
		{
			const auto* lispCalc = dynamic_cast<const LispCalcExplanation*>(expl.get());
			if (!lispCalc)
				continue;
			if (lispCalc->m_CalcPtr->GetLispExprOrg() == key)
				return lispCalc;
		}
		return nullptr;
	}
	auto CalcExplImpl::URL(const LispCalcExplanation* expl, SizeT recNo) -> SharedStr
	{
		auto fullPath = SharedStr(m_StudyObject->GetFullName());
		auto relExprPath = expl->RelativeExprPath();
		return mySSPrintF("dms:dp.vi.attr!%d:%s?%s:%d", m_StudyIdx, fullPath.c_str(), relExprPath.c_str(), recNo);
	}


	struct CalcExplanations
	{
		CalcExplanations(OutStreamBase& xmlOutStr, bool bShowHidden, CalcExplImpl* calcExplImplPtr)
			:	m_OutStream(xmlOutStr)
			,	m_bShowHidden(bShowHidden)
			,	m_CalcExplImplPtr(calcExplImplPtr)
		{}

		bool MakeExplanationIdx(const AbstrDataItem* studyObject, SizeT index, CharPtr extraInfo)
		{
			try {

				m_CalcExplImplPtr->Init(studyObject, index, extraInfo);
				return m_CalcExplImplPtr->ProcessQueue();

			}
			catch (...) 
			{
				m_LastErrorPtr = catchException(true);
			}
			return true; // don't come back
		}

		bool MakeExplanationLoc(const AbstrDataItem* studyObject, const AbstrValue& location, CharPtr extraInfo)
		{
			return 
				MakeExplanationIdx(
					studyObject, 
					studyObject->GetAbstrDomainUnit()->GetIndexForAbstrValue(location),
					extraInfo
				);
		}

		void GetDescr(const AbstrDataItem* studyObject) const
		{
			SuspendTrigger::FencedBlocker nowProvideValuesWithLabelsWithoutSuspension;

			assert(m_CalcExplImplPtr->m_Expl.size() >= 1);

			NewLine(m_OutStream);
			bool isFirst = true;
			for (const auto& expl: m_CalcExplImplPtr->m_Expl)
			{
				if (expl->m_Coordinates.empty())
					continue;
				expl->GetDescr(m_CalcExplImplPtr, m_OutStream, isFirst, m_bShowHidden);
			}
			GetSupplDescr(studyObject);

			if (m_LastErrorPtr && !IsDefaultValue(*m_LastErrorPtr))
			{
				NewLine(m_OutStream);
				m_OutStream << *m_LastErrorPtr;
			}
		}

		void GetSupplDescr(const AbstrDataItem* studyObject) const
		{
			bool headerShown = false;

			VisitSupplProcImpl(studyObject, SupplierVisitFlag::Explain,
				[&] (const Actor* supplier)
				{
					if (supplier->IsPassorOrChecked())
						return;
					const TreeItem* suppl = dynamic_cast<const TreeItem*>(supplier);
					if (!suppl)
						return;

					if (suppl->GetTSF(TSF_InHidden) && !m_bShowHidden )
						return;

					if (IsDataItem(suppl))
						return;

					if (!headerShown)
					{
						NewLine(m_OutStream);
						m_OutStream << " Other suppliers:";
						NewLine(m_OutStream);
						headerShown = true;
					}

					XML_hRef supplRef(m_OutStream, ItemUrl(suppl).c_str());
					m_OutStream << supplier->GetFullName().c_str();
					NewLine(m_OutStream);
				}
			);
		}
		OutStreamBase&  m_OutStream;
		bool            m_bShowHidden;;
		ErrMsgPtr       m_LastErrorPtr;
		CalcExplImpl*   m_CalcExplImplPtr;
	};

	// ========================== impl struct NonStaticCalcExplanation

	NonStaticCalcExplanations::NonStaticCalcExplanations(OutStreamBase& xmlOutStr, const AbstrDataItem* studyObject, SizeT index, CharPtr extraInfo)
		: m_Impl(std::make_unique<CalcExplImpl>())
		, m_Interface(std::make_unique<CalcExplanations>(xmlOutStr, true, m_Impl.get()))
		, m_StudyObject(studyObject)
	{
		m_Impl->Init(studyObject, index, extraInfo);
	}

	bool NonStaticCalcExplanations::ProcessQueue()
	{
		try {
			return m_Impl->ProcessQueue();
		}
		catch (...)
		{
			m_Interface->m_LastErrorPtr = catchException(true); // will be processed in WriteDescr
		}
		return true; // don't come back
	}

	void NonStaticCalcExplanations::WriteDescr()
	{
		m_Interface->GetDescr(m_StudyObject);
	}


	// ========================== impl struct LispCalcExplanation, SumOfTermsExplanation, UnionOfAndsExplanation

	void LispCalcExplanation::AddLispExplanations(CalcExplImpl* self, LispPtr lispExprPtr, UInt32 level)
	{
		assert(self);		
		for (arg_index seqNr = 0; lispExprPtr.IsRealList(); lispExprPtr = lispExprPtr->Right())
			self->AddLispExplanation(lispExprPtr->Left(), level, this, ++seqNr);

		dms_assert(lispExprPtr.EndP());
	}

	void SumOfTermsExplanation::AddLispExplanations(CalcExplImpl* self, LispPtr lispExprPtr, UInt32 level)
	{
		arg_index seqNr = 0;
		for (auto& term: m_Expr)
			for (auto& factor: term.second)
				self->AddLispExplanation(factor, level, this, ++seqNr);
	}

	void UnionOfAndsExplanation::AddLispExplanations(CalcExplImpl* self, LispPtr lispExprPtr, UInt32 level)
	{
		arg_index seqNr = 0;
		for (auto& term : m_Expr)
			for (auto& factor : term)
				self->AddLispExplanation(factor, level, this, ++seqNr);
	}

	const AbstrValue* AbstrCalcExplanation::CalcValue(Context* context) // returns nullptr if suspended
	{
		dms_assert(context);
		dms_assert(context->m_CalcExpl);
		dms_assert(context->m_Domain == m_UltimateDomainUnit);
		CoordinateType* crd = context->m_Coordinate;
		dms_assert(crd);
		dms_assert(!crd->second);

		SizeT entityNr = crd->first;
		assert(IsDefined(entityNr));

		try {
			auto result = GetCalcDataItem(context);
			auto resultData = GetItem(result);
			if (!resultData)
			{
				dms_assert(SuspendTrigger::DidSuspend());
				return nullptr; // calc suspended
			}
			dms_assert(resultData);
			resultData->UpdateMetaInfo();

			if (!resultData->PrepareDataUsage(DrlType::Suspendible))
			{
				if (resultData->WasFailed(FR_Data))
					resultData->ThrowFail();
				dms_assert(SuspendTrigger::DidSuspend());
				return nullptr;
			}

			if (!IsDefined(entityNr))
			{
				static SharedStr nullStr("<null>");
				crd->second.assign(new ValueWrap<SharedStr>(nullStr));
			}
			else
			{
				if (!WaitForReadyOrSuspendTrigger(resultData->GetCurrUltimateItem()))
					return nullptr;

				DataReadLock dlr(AsDataItem(resultData));
				dms_assert(dlr.IsLocked());

				const AbstrDataObject* resultObj = dlr.get_ptr();
				if (not (entityNr < resultObj->GetTiledRangeData()->GetRangeSize()))
				{
					static SharedStr oorStr("<OutOfRange>");
					crd->second = new ValueWrap<SharedStr>(oorStr);
				}
				else
				{
					OwningPtr<AbstrValue> valuePtr(resultObj->CreateAbstrValue());

					resultObj->GetAbstrValue(crd->first, *valuePtr);

					crd->second = std::move(valuePtr);
				}
			}
		}
		catch (const DmsException& x)
		{
			crd->second = OwningPtr<AbstrValue>(new ValueWrap<SharedStr>(x.GetAsText()));
		}

		dms_assert(crd->second);
		return crd->second;
	}

	TokenStr ItemOrValueTypeName(const AbstrUnit* au)
	{
		assert(au);
		return au->GetID().empty() ? au->GetValueType()->GetName() : au->GetName();
	}

	void AbstrCalcExplanation::GetDescrBase(CalcExplImpl* self, OutStreamBase& stream, bool isFirst, const AbstrUnit* domainUnit, const AbstrUnit* valuesUnit) const
	{
		if (domainUnit->IsKindOf(Unit<Void>::GetStaticClass()))
			domainUnit = nullptr;

		SizeT n = m_Coordinates.size();
		if (!n)
			return;

		XML_Table tab(stream);
		{
			XML_Table::Row row(tab);
			if (domainUnit)
				row.ClickableCell(domainUnit->GetName().c_str(), ItemUrl(domainUnit).c_str());
			row.ClickableCell(ItemOrValueTypeName(valuesUnit).c_str(), ItemUrl(valuesUnit).c_str());
		}

		for (SizeT i = 0; i != n; ++i)
		{
			SizeT recno = m_Coordinates[i].first;

			SharedStr locStr;
			if (domainUnit)
				locStr = DisplayValue(
					domainUnit,
					recno,
					true,
					m_Interests.m_DomainLabel, MAX_TEXTOUT_SIZE, m_UnitLabelLocks.first
				);

			SharedStr valStr;
			const AbstrValue* valuesValue = m_Coordinates[i].second;
			if (valuesValue)
				valStr = DisplayValue(valuesUnit, valuesValue, true, m_Interests.m_valuesLabel, MAX_TEXTOUT_SIZE, m_UnitLabelLocks.second);
			else
				valStr = calculatingStr;

			if (n == 1 && isFirst)
			{
				XML_Table::Row row(tab);
				if (domainUnit)
					row.ValueCell(locStr.c_str());
				row.ValueCell(valStr.c_str());
				return;
			}

			XML_Table::Row row(tab);
			stream.WriteAttr("bgcolor", CLR_HROW);

			SharedStr explainUrl;
			if (m_DataItem->IsCacheItem())
			{
				auto lispExpr = dynamic_cast<const LispCalcExplanation*>(this);
				if (lispExpr)
					explainUrl = self->URL(lispExpr, recno);
			}
			else
				explainUrl = mySSPrintF("dms:dp.vi.attr!%d:%s", recno, m_DataItem->GetFullName().c_str());
			if (domainUnit)
				row.ClickableCell(locStr.c_str(), explainUrl.c_str());
			row.ClickableCell(valStr.c_str(), explainUrl.c_str());
		}
	}

	void AbstrCalcExplanation::GetDescr(CalcExplImpl* self, OutStreamBase& stream, bool& isFirst, bool showHidden) const
	{
		if (MatchesExtraInfo(self->m_ExprRelPath) < match_status::full)
			return;

		auto parent = this;
		do {
			if (parent->m_IsExprOfExistingItem)
			{
				if (parent != this)
				{
					if (MatchesExtraInfo(self->m_ExprRelPath) >= match_status::full)
						return;
				}
				else if (   dynamic_cast<const SumOfTermsExplanation*>(this) == nullptr
					&& dynamic_cast<const UnionOfAndsExplanation*>(this) == nullptr)
					return;
			}
			auto castedParent = dynamic_cast<const LispCalcExplanation*>(parent);
			if (!castedParent)
				break;
			parent = castedParent->m_Parent;

		} while (parent);

		if (self->m_ExprLevel > 3)
			return;

		DynamicIncrementalLock lock(self->m_ExprLevel);
		GetDescrImpl(self, stream, isFirst, showHidden);
		isFirst = false;
	}

	void LispCalcExplanation::GetDescrImpl(CalcExplImpl* self, OutStreamBase& stream, bool isFirst, bool showHidden) const
	{
		if (!isFirst)
			NewLine(stream);
		stream << "Expression ";
		PrintSeqNr(stream);
		stream << ": "; stream.WriteTrimmed(m_CalcPtr->GetAsFLispExprOrg(FormattingFlags::ThousandSeparator).c_str());
		NewLine(stream);

		GetDescrBase(self, stream, isFirst, m_UltimateDomainUnit, m_UltimateValuesUnit);
	}

	void AbstrCalcExplanation::DescrValue(OutStreamBase& stream) const
	{
		auto domain = m_UltimateDomainUnit;
		assert(domain);
		if (domain->IsKindOf(Unit<Void>::GetStaticClass()))
			domain = nullptr;

		SizeT n = m_Coordinates.size(); if (!n) return;

		stream << "Value ";
		SizeT recno = m_Coordinates[0].first;
		if (domain)
		{
			auto locStr = DisplayValue(domain, recno, true, m_Interests.m_DomainLabel, MAX_TEXTOUT_SIZE, m_UnitLabelLocks.first);
			stream << "at " << locStr.c_str() << " ";
		}

		SharedStr valStr;
		const AbstrValue* valuesValue = m_Coordinates[0].second;
		if (valuesValue)
			valStr = DisplayValue(m_UltimateValuesUnit, valuesValue, true, m_Interests.m_valuesLabel, MAX_TEXTOUT_SIZE, m_UnitLabelLocks.second);
		else
		{
			static auto calculatingStr = SharedStr("being calculated...");
			valStr = calculatingStr;
		}
		stream << "is " << valStr.c_str();
		NewLine(stream);
	}

	void SumOfTermsExplanation::GetDescrImpl(CalcExplImpl* self, OutStreamBase& stream, bool isFirst, bool showHidden) const
	{
		if (!isFirst)
			NewLine(stream);
		switch(m_Expr.size())
		{
		case 0:
			return;
		case 1:
			stream << mySSPrintF("Summary of %d factors", m_Expr[0].second.size()).c_str();
		default:
			stream << mySSPrintF("Summary of %d terms ", m_Expr.size()).c_str();
		}
		PrintSeqNr(stream);
		stream << ": " << m_CalcPtr->GetAsFLispExprOrg(FormattingFlags::ThousandSeparator).c_str();
		NewLine(stream);

		DescrValue(stream);

		XML_Table tab(stream);
		UInt32 rowCounter = 0;
		for (const auto& signedTerm : m_Expr)
		{
			XML_Table::Row row(tab);
			row.ValueCell(signedTerm.first ? (rowCounter>0 ? "+" : " ") : "-" );
			++rowCounter;

			UInt32 colCounter = 0;
			for (const auto& factor : signedTerm.second)
			{
				if (colCounter > 0)
					row.ValueCell(CharPtr(u8"⋅"));

				++colCounter;
				auto expl = self->FindExpl(factor);
				if (expl)
				{
					auto valueURL = self->URL(expl, expl->m_Coordinates[0].first);
					auto valuePtr = expl->m_Coordinates[0].second.get();
					if (valuePtr)
						row.ClickableCell(valuePtr->AsString().c_str(), valueURL.c_str());
					else
						row.ClickableCell(calculatingStr.c_str(), valueURL.c_str());
				}
				else
				{
					auto factorStr = AsFLispSharedStr(factor, FormattingFlags::ThousandSeparator);
					row.ValueCell(factorStr.c_str());
				}
			}
		}
	}

	void UnionOfAndsExplanation::GetDescrImpl(CalcExplImpl* self, OutStreamBase& stream, bool isFirst, bool showHidden) const
	{
		if (!isFirst)
			NewLine(stream);

		switch (m_Expr.size())
		{
		case 0:
			return;
		case 1:
			stream << mySSPrintF("Summary of %d conjunctions of boolean values", m_Expr[0].size()).c_str();
		default:
			stream << mySSPrintF("Summary of %d disjuctions of (conjuncted) boolean values", m_Expr.size()).c_str();
		}

		PrintSeqNr(stream);
		stream << ": " << m_CalcPtr->GetAsFLispExprOrg(FormattingFlags::ThousandSeparator).c_str();
		NewLine(stream);

		DescrValue(stream);

		XML_Table tab(stream);
		UInt32 rowCounter = 0;
		for (const auto& signedTerm : m_Expr)
		{
			XML_Table::Row row(tab);
			row.ValueCell((rowCounter>0) ? CharPtr(u8"∨") : "");
			rowCounter++;

			UInt32 colCounter = 0;
			for (const auto& factor : signedTerm)
			{

				if (colCounter > 0)
					row.ValueCell(CharPtr(u8"∧"));
				colCounter++;
				auto expl = self->FindExpl(factor);
				assert(expl);
				if (expl)
				{
					auto valueURL = self->URL(expl, expl->m_Coordinates[0].first);
					auto valuePtr = expl->m_Coordinates[0].second.get();
					if (valuePtr)
						row.ClickableCell(valuePtr->AsString().c_str(), valueURL.c_str());
					else
						row.ClickableCell(calculatingStr.c_str(), valueURL.c_str());
				}
				else
				{
					auto factorStr = AsFLispSharedStr(factor, FormattingFlags::ThousandSeparator);
					row.ValueCell(factorStr.c_str());
				}
			}
		}
	}
} // end of anonymous namespace

//  -----------------------------------------------------------------------
//  extern "C" interface functions
//  -----------------------------------------------------------------------


namespace Explain
{
	void DeleteContext(Explain::CalcExplImpl* self)
	{
		delete self;
	}

	context_handle CreateContext()
	{
		return context_handle{ new Explain::CalcExplImpl, DeleteContext };
	}

	void AddQueueEntry(Explain::CalcExplImpl* explImpl, const AbstrUnit * domain, SizeT index)
	{
		assert(explImpl);
		assert(domain);
		//	dms_assert(explImpl == &Explain::g_CalcExplImpl); // single threading singleton hack.
		explImpl->AddQueueEntry(domain, index);
	}

	bool AttrValueToXML(Explain::CalcExplImpl* context, const AbstrDataItem* studyObject, OutStreamBase* xmlOutStrPtr, SizeT index, CharPtr extraInfo, bool bShowHidden)
	{
		XML_ItemBody itemBody(*xmlOutStrPtr, "Value Info", "", studyObject);
		try {
			TreeItemContextHandle hnd(studyObject, AbstrDataItem::GetStaticClass(), "DMS_DataItem_ExplainAttrValue");

			assert(!SuspendTrigger::DidSuspend());

			assert(IsMainThread());

			Explain::CalcExplanations expl(*xmlOutStrPtr, bShowHidden, context);

			bool result = expl.MakeExplanationIdx(studyObject, index, extraInfo);

			expl.GetDescr(studyObject);

			return result;
		}
		catch (...) {
			auto result = catchException(true);
			if (result)
				*xmlOutStrPtr << "Error occured during EplainAttrValue: " << *result;
		}
		return true;
	}

	CalcExplImpl g_CalcExplImpl;
}

extern "C"
TIC_CALL void DMS_CONV DMS_ExplainValue_Clear()
{
	DMS_CALL_BEGIN

		Explain::g_CalcExplImpl.Init(nullptr, 0, nullptr);
		
	DMS_CALL_END
}

#if defined(MG_DEBUG)
bool ExplainValue_IsClear()
{
	return Explain::g_CalcExplImpl.IsClear();
}
#endif

extern "C"
TIC_CALL bool DMS_CONV DMS_DataItem_ExplainAttrValueToXML(const AbstrDataItem* studyObject, OutStreamBase* xmlOutStrPtr, SizeT index, CharPtr extraInfo, bool bShowHidden)
{
	DMS_CALL_BEGIN

		return Explain::AttrValueToXML(&Explain::g_CalcExplImpl, studyObject, xmlOutStrPtr, index, extraInfo, bShowHidden);

	DMS_CALL_END
	return true;
}

extern "C"
TIC_CALL bool DMS_CONV DMS_DataItem_ExplainGridValueToXML(const AbstrDataItem* studyObject, OutStreamBase* xmlOutStrPtr, 
	Int32 row, Int32 col, CharPtr extraInfo, bool bShowHidden)
{
	DMS_CALL_BEGIN

		try {

			TreeItemContextHandle hnd(studyObject, AbstrDataItem::GetStaticClass(), "DMS_DataItem_ExplainGridValue");

			Explain::CalcExplanations expl(*xmlOutStrPtr, bShowHidden, &Explain::g_CalcExplImpl);

			ValueWrap<SPoint> value(SPoint(row, col));
			bool result = expl.MakeExplanationLoc(studyObject, value, extraInfo);

			expl.GetDescr(studyObject);

			return result;
		}
		catch (...) {
			auto result = catchException(true);
			if (result)	
				*xmlOutStrPtr << "Error occured during EplainGridValue: " << *result;
		}

	DMS_CALL_END
	return true;
}