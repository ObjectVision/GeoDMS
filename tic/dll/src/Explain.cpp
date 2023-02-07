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
	typedef std::vector<CoordinateType>       CoordinateCollectionType;

	struct AbstrCalcExplanation
	{
		SharedDataItemInterestPtr             m_DataItem;
		const AbstrUnit*                      m_UltimateDomainUnit;
		const AbstrUnit*                      m_UltimateValuesUnit;
		CoordinateCollectionType              m_Coordinates;
		mutable SharedDataItemInterestPtrTuple m_Interests;
		mutable GuiReadLockPair               m_UnitLabelLocks;
		AbstrCalcExplanation(const AbstrDataItem* dataItem)
			:	m_DataItem(dataItem)
			,	m_UltimateDomainUnit(AsUnit(dataItem->GetAbstrDomainUnit()->GetUltimateItem()))
			,	m_UltimateValuesUnit(AsUnit(dataItem->GetAbstrValuesUnit()->GetUltimateItem()))

		{}
		virtual ~AbstrCalcExplanation() 
		{
			reportD(SeverityTypeID::ST_MinorTrace, "Byte");
		}

		virtual ArgRef GetCalcDataItem(Context* context) const =0;

		CoordinateType* AddIndex(SizeT index) // returns nullptr if abundant
		{
			for (auto& c : m_Coordinates)
				if (c.first == index)
					return &c;

			if (m_Coordinates.size() >= MaxNrEntries)
				return nullptr;

			m_Coordinates.push_back(CoordinateType(index, AbstrValueRef() ) ); // calculate later
			return &m_Coordinates.back();
		}

		const AbstrValue* CalcValue(Context* context) // returns nullptr if suspended
		{
			dms_assert(context);
			dms_assert(context->m_CalcExpl);
			dms_assert(context->m_Domain == m_UltimateDomainUnit);
			CoordinateType* crd = context->m_Coordinate;
			dms_assert(crd);
			dms_assert(!crd->second);

			SizeT entityNr = crd->first;
			if (!IsDefined(entityNr))
				context = nullptr;

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
					crd->second.assign( new ValueWrap<SharedStr>(nullStr) );
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
						OwningPtr<AbstrValue> valuePtr( resultObj->CreateAbstrValue() );

						resultObj->GetAbstrValue(crd->first, *valuePtr);

						crd->second = std::move( valuePtr );
					}
				}
			}
			catch (const DmsException& x)
			{
				crd->second = OwningPtr<AbstrValue>( new ValueWrap<SharedStr>(x.GetAsText()) );
			}

			dms_assert(crd->second);
			return crd->second;
		}

		virtual void GetDescr(CalcExplImpl* self, OutStreamBase& stream, bool isFirst, bool showHidden) const = 0;

	protected:
		void GetDescrBase(OutStreamBase& stream, bool isFirst, const AbstrUnit* domainUnit, const AbstrUnit* valuesUnit) const
		{
			if (domainUnit->IsKindOf(Unit<Void>::GetStaticClass()))
				domainUnit = nullptr;

			SizeT n = m_Coordinates.size();
			stream << "Selected value"; if (n!=1) stream << "s";
			NewLine(stream);

			XML_Table tab(stream);
			{
				XML_Table::Row row(tab);
				row.ClickableCell(
					domainUnit ? domainUnit->GetName().c_str() : "",
					domainUnit ? ItemUrl(domainUnit).c_str() : "");
				row.ClickableCell(valuesUnit->GetName().c_str(), ItemUrl(valuesUnit).c_str());
			}

			for (SizeT i=0; i!=n; ++i)
			{
				SizeT recno = m_Coordinates[i].first;

				SharedStr locStr = (domainUnit)
					?	DisplayValue(
							domainUnit, 
							recno,
							true,
							m_Interests.m_DomainLabel, MAX_TEXTOUT_SIZE, m_UnitLabelLocks.first
						)
					:	SharedStr();

				const AbstrValue* valuesValue = m_Coordinates[i].second;

				SharedStr valStr;
				if (valuesValue)
					valStr = DisplayValue(valuesUnit, valuesValue, true, m_Interests.m_valuesLabel, MAX_TEXTOUT_SIZE, m_UnitLabelLocks.second);
				else
				{
					static auto calculatingStr = SharedStr("Calculating...");
					valStr = calculatingStr;
				}

				if (n == 1 && isFirst || m_DataItem->IsCacheItem())
					tab.NameValueRow(locStr.c_str(), valStr.c_str());
				else
				{
					XML_Table::Row row(tab);
					stream.WriteAttr("bgcolor", CLR_HROW);
					SharedStr explainUrl = mySSPrintF("dms:dp.VI.ATTR!%d:%s", recno, m_DataItem->GetFullName().c_str());
					row.ClickableCell(locStr.c_str(), explainUrl.c_str());
					row.ClickableCell(valStr.c_str(), explainUrl.c_str());
				}
			}
		}
	};

	struct DataCalcExplanation : AbstrCalcExplanation
	{
		DataCalcExplanation(const AbstrDataItem* dataItem)
			:	AbstrCalcExplanation(dataItem)
		{}

		ArgRef GetCalcDataItem(Context* context) const override { return ArgRef(std::in_place_type<SharedTreeItem>, m_DataItem.get_ptr()); }

		void GetDescr(CalcExplImpl* self, OutStreamBase& stream, bool isFirst, bool showHidden) const override
		{
			NewLine(stream);
			{
				XML_hRef supplRef(stream, ItemUrl(m_DataItem.get_ptr()).c_str());
				stream << m_DataItem->GetFullName().c_str();
			}
			NewLine(stream);


			GetDescrBase(stream, isFirst, m_DataItem->GetAbstrDomainUnit(), m_DataItem->GetAbstrValuesUnit());

			if (!isFirst)
				return;

			if (m_DataItem->HasCalculator())
			{
				stream << "Calculated as:";
				NewLine(stream);
			}
			GetExprOrSourceDescr(stream, m_DataItem.get_ptr());
			NewLine(stream);
		}
	};

	struct LispCalcExplanation : DataControllerRef, AbstrCalcExplanation
	{
		LispCalcExplanation(const AbstrCalculator* calcPtr, const LispCalcExplanation* parent, arg_index seqNr)
			: DataControllerRef(GetDC(calcPtr))
			, AbstrCalcExplanation(AsDataItem(get()->MakeResult().get_ptr()))
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
		void GetDescr(CalcExplImpl* self, OutStreamBase& stream, bool isFirst, bool showHidden) const override
		{
			dms_assert(!isFirst);
			NewLine(stream);
			stream << "Expr ";
			PrintSeqNr(stream);
			stream << " (in FLisp format): " << m_CalcPtr->GetAsFLispExprOrg().c_str();
			NewLine(stream);

			GetDescrBase(stream, isFirst, m_UltimateDomainUnit, m_UltimateValuesUnit);
		}
		virtual void AddLispExplanations(CalcExplImpl* self, LispPtr lispExprPtr, UInt32 level);

		void PrintSeqNr(OutStreamBase& stream) const
		{
			if (m_Parent) {
				m_Parent->PrintSeqNr(stream);
				stream << ".";
			}
			stream << AsString(m_SeqNr).c_str();
		}
		auto RelativeExprPath() const -> SharedStr
		{
			VectorOutStreamBuff buff;
			OutStream_DMS x(&buff, nullptr);
			PrintSeqNr(x);
			return SharedStr(buff.GetData(), buff.GetDataEnd());
		}

		SharedPtr<const AbstrCalculator> m_CalcPtr;
		const LispCalcExplanation*       m_Parent;
		arg_index                        m_SeqNr;
		SupplInterestListPtr             m_SupplInterest;
	};

	using Factor = LispRef;
	using Term = std::vector<Factor>;
	using SignedTerm = std::pair<bool, Term>;
	using SumOfTermsExpr = std::vector<SignedTerm>;

	void operator <<=(Term& left, Term&& right)
	{
		left.reserve(left.size() + right.size());
		for (auto& factor : right)
			left.emplace_back(std::move(factor));
	}

	struct SumOfTermsExplanation : LispCalcExplanation
	{
		SumOfTermsExplanation(const AbstrCalculator* calcPtr, const LispCalcExplanation* parent, arg_index seqNr)
			: LispCalcExplanation(calcPtr, parent, seqNr)
		{
			ProcessTerms(calcPtr->GetLispExprOrg(), true);
		}

		void ProcessTerms(LispPtr terms, bool additive)
		{
			assert(IsSumOfTerms(terms));
			bool negateThisTerm = additive;
			bool isSub = (terms.Left().GetSymbID() == token::sub);
			bool negateNextTerms = additive ^ isSub;

			for (auto termListPtr = terms.Right(); termListPtr.IsRealList(); termListPtr = termListPtr.Right())
			{
				auto nextTerm = termListPtr.Left();
				if (IsSumOfTerms(nextTerm))
					ProcessTerms(nextTerm, negateThisTerm);
				else
				{
					auto term = ProcessFactors(nextTerm);
					m_Expr.emplace_back(negateThisTerm, std::move(term));
				}
				negateThisTerm = negateNextTerms;
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

		void GetDescr(CalcExplImpl* self, OutStreamBase& stream, bool isFirst, bool showHidden) const override;
		void AddLispExplanations(CalcExplImpl* self, LispPtr lispExprPtr, UInt32 level) override;
	};

	/*
	Description of value(s) of <dataitemname>
	*   the value of the <x>-th <loc-unit> is <y> <values-unit>
	because is is calculated according to <expr> / because this is read from (storagefilename)
	It has the following supplier(s):
	*  <suppliers>
	*/


	typedef std::pair<const AbstrUnit*, SizeT>    QueueEntry;
	typedef OwningPtr<AbstrCalcExplanation>       ExplArrayEntry;
	typedef std::vector<ExplArrayEntry>           ExplArray;
	typedef std::vector<TreeItemInterestPtr>      ItemInterestArray;
	typedef std::vector<CalcInterestPtr>          CalcInterestArray;

	struct CalcExplImpl
	{
		void Init(const AbstrDataItem* studyObject, SizeT studyIdx)
		{
			if (	studyObject == m_StudyObject
				&&	studyIdx    == m_StudyIdx
				&& (!studyObject || studyObject->GetLastChangeTS() == m_LastChange))
				return;

			m_StudyObject = studyObject;
			m_StudyIdx    = studyIdx;
			m_Queue.clear();
			m_DoneQueueEntries = m_DoneExpl = 0;

			auto oldExpl          = std::move(m_Expl);
			auto oldLispRefSet    = std::move(m_KnownExpr);
			auto oldItemInterests = std::move(m_ItemInterests);
			auto oldCalcInterests = std::move(m_CalcInterests);

			if (studyObject)
			{
				m_LastChange  = studyObject->GetLastChangeTS();

				AddExplanations();

				AddQueueEntry(m_Expl[0]->m_UltimateDomainUnit, studyIdx);
				AddQueueEntry(Unit<Void>::GetStaticClass()->CreateDefault(), 0);
			}
		}
#if defined(MG_DEBUG)
		bool IsClear() const
		{
			dms_assert(m_Queue.empty());
			dms_assert(m_Expl.empty());
			dms_assert(m_ItemInterests.empty());
			dms_assert(m_CalcInterests.empty());
			dms_assert(m_StudyObject.is_null());
			return m_Expl.empty() && m_Queue.empty() && m_ItemInterests.empty() && m_CalcInterests.empty() && m_StudyObject.is_null();
		}
#endif
		void AddQueueEntry(const AbstrUnit* domain, SizeT index)
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

		bool ProcessQueue() // returns false if suspended
		{
			while (m_DoneQueueEntries < m_Queue.size())
			{
				QueueEntry entry = m_Queue[m_DoneQueueEntries];
				for (; m_DoneExpl < m_Expl.size(); ++m_DoneExpl)
				{
					AbstrCalcExplanation* explanation = m_Expl[ m_DoneExpl ].get();

					if (entry.first != explanation->m_UltimateDomainUnit)
						continue;

					CoordinateType* coordPtr = explanation->AddIndex(entry.second);
					if (!coordPtr)
						continue;
					dms_assert( coordPtr->second.is_null() ); // we don't expect to process the same entry twice

					Context context { this, entry.first, coordPtr };

					const AbstrValue* value = explanation->CalcValue(&context);
					if (! value )
						return false; // suspend
		
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

		void AddExplanation(const AbstrDataItem* explItem)
		{
			for (const auto& explPtr: m_Expl)
				if (explPtr->m_DataItem == explItem)
					return;

			m_Expl.push_back(
				ExplArrayEntry(
					new DataCalcExplanation(explItem) 
				)
			);
		}
		void AddLispExplanation(LispPtr lispExpr, UInt32 level, const LispCalcExplanation* parent, arg_index seqNr)
		{
			if (!lispExpr.IsRealList() || !lispExpr.Left().IsSymb() || lispExpr.Left().GetSymbID() == token::sourceDescr)
				return;
			if (m_KnownExpr.contains(lispExpr))
				return;
			m_KnownExpr.insert(lispExpr);

//			DisplayAuthLock suppressErrorDisplay;
			OwningPtr<LispCalcExplanation> newExpl;
			LispCalcExplanation* newExplPtr = nullptr;
			try {

				AbstrCalculatorRef calc = AbstrCalculator::ConstructFromLispRef(m_StudyObject, lispExpr, CalcRole::Calculator); // lispExpr already substitited ?
				auto metaInfo = calc->GetMetaInfo();
//				if (metaInfo.index() == 2)
//					AddLispExplanation(std::get<2>(metaInfo)->GetKeyExpr(CalcRole::Other), level, parent, seqNr);
				if (metaInfo.index() != 1)
					return;
				auto dc = GetExistingDataController(lispExpr);
				auto res = dc->MakeResult();
				if (!IsDataItem(res.get_ptr()))
					return;

				if (SumOfTermsExplanation::IsSumOfTerms(lispExpr))
					newExpl = new  SumOfTermsExplanation(calc, parent, seqNr);
				else
					newExpl = new LispCalcExplanation(calc, parent, seqNr);
				newExplPtr = newExpl.release();
				m_Expl.push_back( ExplArrayEntry(newExplPtr) );
				m_CalcInterests.push_back(dc);
			} 
			catch (const DmsException&)
			{}

			if (level < MaxLevel && newExplPtr)
				newExplPtr->AddLispExplanations(this, lispExpr, level+1);
		}

		void AddExplanations(const Actor* studyActor)
		{			
			VisitSupplProcImpl(studyActor, SupplierVisitFlag::Explain, 
				[&] (const Actor* supplier)
				{
					const AbstrDataItem* supplDI= AsDynamicDataItem( supplier );
					if	( supplDI)
						AddExplanation(supplDI);
					if (dynamic_cast<const AbstrCalculator*>( supplier ))
						AddExplanations( supplier );
				}
			);
		}
		void AddExplanations()
		{
			AddExplanation (m_StudyObject);
			AddExplanations(m_StudyObject);

			if (m_StudyObject->HasCalculator())
			{
				auto metaInfo = m_StudyObject->GetCurrMetaInfo({});
				if (metaInfo.index() == 2)
					AddExplanation(AsDataItem(std::get<SharedTreeItem>(metaInfo).get_ptr()));
				if (metaInfo.index() == 1)
					AddLispExplanation(std::get<LispRef>(metaInfo), 0, nullptr, 1);
			}
		}


		bool IsKnownDomain(const AbstrUnit* valuesUnit)
		{
			for (const auto& expl: m_Expl)
				if (expl->m_UltimateDomainUnit == valuesUnit) 
					return true;
			return false;
		}

		bool IsExplainable(const AbstrUnit* valuesUnit, SizeT index)
		{
			bool result = false;
			for (const auto& expl : m_Expl)
				if (expl->m_UltimateDomainUnit == valuesUnit)
					if (expl->AddIndex(index))
						result = true;

			return result;
		}

		auto FindExpl(LispPtr key) -> const LispCalcExplanation*
		{
			for (const auto& expl : m_Expl)
			{
				const auto* lispCalc = dynamic_cast<const LispCalcExplanation*>(expl.get());
				if (!lispCalc)
					continue;
				if (lispCalc->m_CalcPtr->GetLispExprOrg() == key)
					return lispCalc;
			}
			auto errMsg = mySSPrintF("Unexpected key: %s", AsFLispSharedStr(key).c_str());
			throwCheckFailed(MG_POS, "Unexpected key");
		}
		auto URL(const LispCalcExplanation* expl) -> SharedStr
		{
			auto fullPath = SharedStr(m_StudyObject->GetFullName());
			auto relExprPath = expl->RelativeExprPath();
			return mySSPrintF("dms:dp.VI.ATTR!%d:%s?%s"
				, m_StudyIdx
				, fullPath.c_str()
				, relExprPath.c_str()
			);
		}

		ExplArray                m_Expl;
		std::set<LispRef>        m_KnownExpr;
		std::vector<QueueEntry > m_Queue;
		SizeT                    m_DoneQueueEntries = 0, m_DoneExpl = 0;

		ItemInterestArray        m_ItemInterests;
		CalcInterestArray        m_CalcInterests;

		SharedDataItem           m_StudyObject = 0;
		SizeT                    m_StudyIdx = 0;
		TimeStamp                m_LastChange = 0;

		friend struct CalcExplanations;
	};

	CalcExplImpl g_CalcExplImpl;

	struct CalcExplanations
	{
		CalcExplanations(OutStreamBase& xmlOutStr, bool bShowHidden)
			:	m_OutStream(xmlOutStr)
			,	m_bShowHidden(bShowHidden)
		{}

		bool MakeExplanationIdx(const AbstrDataItem* studyObject, SizeT index, CharPtr extraInfo)
		{
			try {

				Explain::g_CalcExplImpl.Init(studyObject, index);
				return Explain::g_CalcExplImpl.ProcessQueue();

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

			dms_assert(g_CalcExplImpl.m_Expl.size() >= 1);

			bool isFirst = true;
			for (const auto& expl: g_CalcExplImpl.m_Expl)
			{
				if (expl->m_Coordinates.empty())
					continue;
				if (!isFirst) NewLine(m_OutStream);
				expl->GetDescr(&g_CalcExplImpl, m_OutStream, isFirst, m_bShowHidden);
				isFirst = false;
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
	};

	void LispCalcExplanation::AddLispExplanations(CalcExplImpl* self, LispPtr lispExprPtr, UInt32 level)
	{
		assert(self);		
		for (arg_index seqNr = 0; lispExprPtr.IsRealList(); lispExprPtr = lispExprPtr->Right())
			self->AddLispExplanation(lispExprPtr->Left(), level, m_Parent, ++seqNr);

		dms_assert(lispExprPtr.EndP());
	}

	void SumOfTermsExplanation::AddLispExplanations(CalcExplImpl* self, LispPtr lispExprPtr, UInt32 level)
	{
		arg_index seqNr = 0;
		for (auto& term: m_Expr)
			for (auto& factor: term.second)
				self->AddLispExplanation(factor, level, m_Parent, ++seqNr);
	}

	void SumOfTermsExplanation::GetDescr(CalcExplImpl* self, OutStreamBase& stream, bool isFirst, bool showHidden) const
	{
		dms_assert(!isFirst);
		NewLine(stream);
		stream << "Expr ";
		PrintSeqNr(stream);
		stream << " (in FLisp format): " << m_CalcPtr->GetAsFLispExprOrg().c_str();
		NewLine(stream);


		auto domain = m_UltimateDomainUnit;
		if (domain->IsKindOf(Unit<Void>::GetStaticClass()))
			domain = nullptr;

		SizeT n = m_Coordinates.size(); if (!n) return;

		stream << ((n != 1) ? "First selected value " : "Selected value ");
		SizeT recno = m_Coordinates[0].first;
		if (domain)
		{
			auto locStr = DisplayValue(domain, recno, true, m_Interests.m_DomainLabel, MAX_TEXTOUT_SIZE, m_UnitLabelLocks.first);
			stream << "at " << locStr.c_str() << " ";
		}
		stream << "=";

		const AbstrValue* valuesValue = m_Coordinates[0].second;

		SharedStr valStr;
		if (valuesValue)
			valStr = DisplayValue(m_UltimateValuesUnit, valuesValue, true, m_Interests.m_valuesLabel, MAX_TEXTOUT_SIZE, m_UnitLabelLocks.second);
		else
		{
			static auto calculatingStr = SharedStr("Calculating...");
			valStr = calculatingStr;
		}

		NewLine(stream);

		XML_Table tab(stream);
		for (const auto& signedTerm : m_Expr)
		{
			XML_Table::Row row(tab);
			row.ValueCell(signedTerm.first ? "+" : "-");

			for (const auto& factor : signedTerm.second)
			{
				auto expl = self->FindExpl(factor);
				auto valuePtr = expl->m_Coordinates[0].second.get();
				auto valueURL = self->URL(expl);

				row.ClickableCell(valuePtr->AsString().c_str(), valueURL.c_str());
			}
		}
	}
} // end of anonymous namespace

//  -----------------------------------------------------------------------
//  extern "C" interface functions
//  -----------------------------------------------------------------------

extern "C"
void DMS_CalcExpl_AddQueueEntry(Explain::CalcExplImpl* explImpl, const AbstrUnit* domain, SizeT index)
{
	dms_assert(explImpl);
	dms_assert(domain);	
	dms_assert(explImpl == &Explain::g_CalcExplImpl); // single threading singleton hack.
	explImpl->AddQueueEntry(domain, index);
}

extern "C"
TIC_CALL void DMS_CONV DMS_ExplainValue_Clear()
{
	DMS_CALL_BEGIN

		Explain::g_CalcExplImpl.Init(nullptr, 0);
		
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

		XML_ItemBody itemBody(*xmlOutStrPtr, studyObject);

		try {
			TreeItemContextHandle hnd(studyObject, AbstrDataItem::GetStaticClass(), "DMS_DataItem_ExplainAttrValue");

			dms_assert(!SuspendTrigger::DidSuspend());

			dms_assert(IsMainThread());

			Explain::CalcExplanations expl(*xmlOutStrPtr, bShowHidden);

			bool result = expl.MakeExplanationIdx(studyObject, index, extraInfo);

			expl.GetDescr(studyObject);

			return result;
		}
		catch (...) {
			auto result = catchException(true);
			if (result)
				*xmlOutStrPtr << "Error occured during EplainAttrValue: " << *result;
		}

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

			Explain::CalcExplanations expl(*xmlOutStrPtr, bShowHidden);

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