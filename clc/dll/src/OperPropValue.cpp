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

// string x:= PropValue(treeitem, propName)

#include "geo/StringArray.h"
#include "mci/CompositeCast.h"
#include "mci/PropDef.h"
#include "mci/ValueWrap.h"
#include "utl/mySPrintF.h"

#include "Param.h"
#include "UnitClass.h"
#include "DataItemClass.h"
#include "TreeItemClass.h"
#include "AbstrCalculator.h"

struct AbstrItemSet
{
	AbstrItemSet(const TreeItem* currItem) : m_CurrItem(currItem) {}

	const TreeItem* operator *() const { return m_CurrItem; }

	virtual void operator ++() = 0;

protected:
	const TreeItem* m_CurrItem;
};

struct AbstrItemSetProvider
{
	virtual AbstrItemSet* CreateItemSet(const TreeItem* focus) const =0;

	virtual SizeT CountItems(const TreeItem* focus) const
	{
		OwningPtr<AbstrItemSet> set = CreateItemSet(focus);
		SizeT count = 0;
		for (; **set; ++*set)
			++count;
		return count;
	}
};

//	direct, tree, explicit-suppliers, using,
//	direct
//	inherited.
//	namespace

struct SubItemSet: AbstrItemSet
{
	SubItemSet(const TreeItem* focus)
		:	AbstrItemSet( focus->GetFirstSubItem() )
	{}

	void operator ++() override
	{
		dms_assert(m_CurrItem);
		m_CurrItem = m_CurrItem->GetNextItem();
	}
};

struct SubTreeSet: AbstrItemSet
{
	SubTreeSet(const TreeItem* focus)
		:	AbstrItemSet( focus->GetFirstSubItem() )
		,	m_Focus(focus)
	{}

	void operator ++() override
	{
		dms_assert(m_CurrItem);
		m_CurrItem = const_cast<TreeItem*>(m_Focus)->WalkCurrSubTree(const_cast<TreeItem*>(m_CurrItem));
	}
	const TreeItem* m_Focus;
};

struct InheritedSet: AbstrItemSet
{
	InheritedSet(const TreeItem* focus)
		:	AbstrItemSet( GetFirstSubOrInheritedItem(focus) )
		,	m_Focus(focus)
	{}

	const TreeItem* GetFirstSubOrInheritedItem(const TreeItem*& focus)
	{
		while (focus)
		{
			auto firstSubItem = focus->GetFirstSubItem();
			if (firstSubItem)
				return firstSubItem;
			focus = focus->GetCurrRefItem();
		}
		return nullptr;
	}

	void operator ++() override
	{
		assert(m_CurrItem);
		m_CurrItem = m_CurrItem->GetNextItem();
		if (!m_CurrItem)
		{
			m_Focus = m_Focus->GetCurrRefItem();
			m_CurrItem = GetFirstSubOrInheritedItem(m_Focus);
		}
	}
	const TreeItem* m_Focus;
};

template <typename ItemSet>
struct ItemSetProvider : AbstrItemSetProvider
{
	AbstrItemSet* CreateItemSet(const TreeItem* focus) const override
	{
		return new ItemSet(focus);
	}
};

//	DECL + DEF

struct PropValueOperator : public BinaryOperator
{
	typedef DataArray<SharedStr> ResultType;
	typedef DataArray<SharedStr> Arg2Type;

	PropValueOperator(AbstrOperGroup* gr, const AbstrItemSetProvider* itemSetProvider = nullptr)
		:	BinaryOperator(gr
			,	itemSetProvider ? typesafe_cast<const Class*>(Unit<UInt32>::GetStaticClass()) : typesafe_cast<const Class*>(ResultType::GetStaticClass())
			,	TreeItem::GetStaticClass()
			,	Arg2Type::GetStaticClass()
			)
		,	m_ItemSetProvider(itemSetProvider)
		{}

	OwningPtr<const AbstrItemSetProvider> m_ItemSetProvider;

	// Override Operator
	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		dms_assert(args.size() == 2);

		const TreeItem* arg1  = args[0];
		const AbstrDataItem* arg2A = debug_cast<const AbstrDataItem*>(args[1]);

		dms_assert(arg1);

		const AbstrUnit* argDomain = arg2A->GetAbstrDomainUnit();

		if (!resultHolder)
			if (m_ItemSetProvider)
			{
				resultHolder = Unit<UInt32>::GetStaticClass()->CreateResultUnit(resultHolder); // count subitems.
				resultHolder->SetTSF(TSF_Categorical);

				resultHolder.GetNew()->SetKeepDataState(true);
				for (SizeT p=0, pe = argDomain->GetCount(); p!=pe; ++p)
					AbstrDataItem* resAttr = CreateDataItem(
						resultHolder.GetNew(), TokenID(GetValue<SharedStr>(arg2A, p)), 
						AsUnit(resultHolder.GetNew()),  
						Unit<SharedStr>::GetStaticClass()->CreateDefault()
					);
			}
			else
				resultHolder = CreateCacheDataItem(argDomain, Unit<SharedStr>::GetStaticClass()->CreateDefault());

		if (!mustCalc) 
			return true;

		if (m_ItemSetProvider)
		{
			AsUnit(resultHolder.GetNew())->SetCount(m_ItemSetProvider->CountItems(arg1));
		}

		SizeT ri = 0;

		DataWriteLock resLock;  AbstrDataItem* lockedRes = nullptr;
		
		for (SizeT p=0, pe = argDomain->GetCount(); p!=pe; ++p) // for all properties
		{
			TokenID propName = TokenID(GetCurrValue<SharedStr>(arg2A, p));

			AbstrDataItem* res = AsDataItem(
				m_ItemSetProvider 
				?	resultHolder.GetNew()->GetSubTreeItemByID(propName)
				:	resultHolder.GetNew()
			);

			if (res != lockedRes)
			{
				resLock = DataWriteLock(res);
				lockedRes = res;
			}
			ResultType* result = mutable_array_cast<SharedStr>(resLock);
			dms_assert(result);

			auto resData = result->GetDataWrite();

			const TreeItem* item = arg1;
			OwningPtr<AbstrItemSet> itemSet;
			if (m_ItemSetProvider)
			{
				itemSet.assign(m_ItemSetProvider->CreateItemSet(arg1));
				item = **itemSet;
				ri = 0;
			}
			while (item)
			{
				const Class* cls = item->GetDynamicClass();
				dms_assert(cls);
				const AbstrPropDef* propDef = cls->FindPropDef(propName);

				SharedStr value;
				if (!propDef)
					value = mySSPrintF("%s::%s unknown"
					,	item->GetCurrentObjClass()->GetName()
					,	propName
					);
				else while (true)
				{
					value = propDef->GetValueAsSharedStr(item);
					if (!value.empty())
						break;
					const TreeItem* source = item->GetSourceItem();
					if (!source)
						break;
					item = source;
				}
	//		TODO, string contents scannen in FuncDC::VisitSuppliers, evt ook specifieke Eval functie maken
	//			value = AbstrCalculator::EvaluatePossibleStringExpr(arg1, value.c_str());

				dms_assert(ri < resData.size());

				Assign(resData[ri], value);

				if (!itemSet)
					break;
				++*itemSet;
				item = **itemSet;
				++ri;
			}
			resLock.Commit();
		}
		return true;
	}
};


struct GenerateOperator : public UnaryOperator
{
	GenerateOperator(AbstrOperGroup* gr, bool doAsMetaInfo)
		:	UnaryOperator(gr, TreeItem::GetStaticClass(), TreeItem::GetStaticClass())
		,	m_DoAsMetaInfo(doAsMetaInfo)
		{}

	// Override Operator
	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		dms_assert(args.size() == 1);

		const TreeItem* arg1  = args[0];

		dms_assert(arg1);

		if (!resultHolder)
			resultHolder.SetOld(arg1);

		if (m_DoAsMetaInfo || mustCalc)
		{
			ActorVisitState result = arg1->SuspendibleUpdate(PS_Committed);

			if (result == AVS_SuspendedOrFailed)
			{
				dms_assert(!(m_DoAsMetaInfo && SuspendTrigger::DidSuspend()));
				dms_assert( SuspendTrigger::DidSuspend() || arg1->WasFailed());
				return false;
			}
		}
		return true;
	}
	bool m_DoAsMetaInfo;
};


// *****************************************************************************
//											INSTANTIATION
// *****************************************************************************
namespace {

	oper_arg_policy oap_PropValue [2] = { oper_arg_policy::calc_never, oper_arg_policy::calc_as_result };
	oper_arg_policy oap_PropValues[2] = { oper_arg_policy::subst_with_subitems, oper_arg_policy::calc_always };

	oper_arg_policy oap_Generate[1] = { oper_arg_policy::calc_never };

	SpecialOperGroup sog_PropValue("PropValue", 2, oap_PropValue, oper_policy::calc_requires_metainfo | oper_policy::is_transient); // transient because of for example 'CaseDir'
	SpecialOperGroup sog_PropValuessi("SubItem_PropValues", 2, oap_PropValues, oper_policy::calc_requires_metainfo | oper_policy::is_transient);
	SpecialOperGroup sog_PropValuesst("SubTree_PropValues", 2, oap_PropValues, oper_policy::calc_requires_metainfo | oper_policy::is_transient);
	SpecialOperGroup sog_PropValuesih("Inherited_PropValues", 2, oap_PropValues, oper_policy::calc_requires_metainfo | oper_policy::is_transient);


	PropValueOperator propValueOper(&sog_PropValue);
	PropValueOperator propValueOperssi(&sog_PropValuessi, new ItemSetProvider<SubItemSet>);
	PropValueOperator propValueOpersst(&sog_PropValuesst, new ItemSetProvider<SubTreeSet>);
	PropValueOperator propValueOpersih(&sog_PropValuesih, new ItemSetProvider<InheritedSet>);

	SpecialOperGroup sog_Generate("Generate", 1, oap_Generate, oper_policy::calc_requires_metainfo | oper_policy::dynamic_result_class | oper_policy::existing | oper_policy::has_external_effects);
	SpecialOperGroup sog_GenerateDirect("GenerateDirect", 1, oap_Generate, oper_policy::dynamic_result_class | oper_policy::existing);

	GenerateOperator  generateOper(&sog_Generate, false);
	GenerateOperator  generateDirectOper(&sog_GenerateDirect, true);
}