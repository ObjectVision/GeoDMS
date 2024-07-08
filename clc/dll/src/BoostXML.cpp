// Copyright (C) 1998-2024 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "ClcPch.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif

#define BOOST_PROPERTY_TREE_RAPIDXML_STATIC_POOL_SIZE 8*1024

#include <boost/property_tree/detail/rapidxml.hpp>

#include "mci/ValueClass.h"
#include "set/IndexedStrings.h"
#include "utl/mySPrintF.h"
#include "utl/splitPath.h"

#include "Abstrunit.h"
#include "SessionData.h"
#include "TreeItemClass.h"
#include "Unit.h"
#include "UnitClass.h"

#include "MoreDataControllers.h"

#include "VersionComponent.h"
static VersionComponent s_BoostPropTree("boost::property_tree::detail::rapidxml");

namespace { // anonymous

// *****************************************************************************
//	Helper funcs
// *****************************************************************************

template <typename OutIter>
void MakeItemName(const char* name, const char* nameEnd, OutIter res)
{
	for (;name != nameEnd; ++name)
		switch (*name) 
		{
			case 0: return;
			case ':': *res++ = '_'; break;
			case '_': *res++ = '_'; break;
			default:
				if (isalnum(UChar(*name)))
					*res++ = *name;
		}
}

typedef UInt32 entity_index;
typedef std::pair<entity_index, entity_index> entity_id;

struct Entity; // forward decl

struct Element : SharedBase
{
	Element() {}

	Element(Entity* parent, entity_index entityIndex, TreeItem* item)
		:	m_Parent(parent)
		,	m_EntityIndex(entityIndex)
	{
		if (!item) 
			return;
		m_DmsFullName = item->GetFullName();
	}
	virtual ~Element() {}
	void Release() const { if (!DecRef()) delete this;	}
	virtual void AddValue(entity_id parentID, CharPtr begin, CharPtr end) {}

	SharedStr GetName() const { return m_DmsFullName.empty() ? SharedStr() : SharedStr(m_DmsFullName.begin()+1, m_DmsFullName.send()); }

	Entity*      m_Parent = nullptr;
	entity_index m_EntityIndex = UNDEFINED_VALUE(entity_index);
	SharedStr    m_DmsFullName;
};

struct Entity : Element
{
	Entity() {}

	Entity(AbstrUnit* domain, Entity* parent, entity_index entityIndex)
		:	Element(parent, entityIndex, domain)
		,	m_Domain(domain)
	{}
	void AddValue(entity_id parentID, CharPtr begin, CharPtr end) override
	{
		if(!m_Parent)
			m_ParentEntityTableRel.push_back(parentID.first);
		m_ParentRel.push_back(parentID.second);
		m_ValueIndex.push_back( m_Values.GetOrCreateID_mt(begin, end) );
	}

	SizeT GetCount() const { return m_ValueIndex.size(); }

	SharedPtr<AbstrUnit>      m_Domain;

	std::vector<entity_index> m_ParentEntityTableRel;
	std::vector<entity_index> m_ParentRel;
	std::vector<entity_index> m_ValueIndex;
	IndexedStringValues       m_Values;
};

struct Attribute : Element
{
	Attribute(AbstrDataItem* adi, Entity* parent, entity_index entityIndex)
		:	Element(parent, entityIndex, adi)
		,	m_Attr(adi)
	{
		MG_CHECK(parent);
	}

	void AddValue(entity_id parentID, CharPtr begin, CharPtr end) override
	{
		SizeT currCount = m_Data.size();
		SizeT entityCount = m_Parent->GetCount();

		dms_assert(parentID.first == m_Parent->m_EntityIndex);
		dms_assert(parentID.second == entityCount - 1);

		if (currCount >= entityCount)
			m_Attr->throwItemErrorF("Too many occurences of attribute %s in entity %s #%I64u", GetName(), m_Parent->GetName(), UInt64(entityCount));
		for (SizeT i= entityCount - currCount - 1; i;--i)
			m_Data.push_back(Undefined());
		m_Data.push_back_seq(begin, end);
		dms_assert(m_Data.size() == entityCount);
	}

	SharedPtr<AbstrDataItem>  m_Attr;
	StringVector m_Data;
};

typedef std::map<Point<TokenID>, SharedPtr<Element> > entity_map;

struct ParseContext
{
	ParseContext(TreeItemDualRef& resultHolder)
		:	m_ResultHolder(resultHolder)
	{}

	TreeItemDualRef& m_ResultHolder;
	entity_map m_Map;
	StringVector m_EntityNames;
	std::map<TokenID, const Element*> m_KnownEntities;

	Element* CreateElement(TreeItemDualRef& resultHolder, Entity* parent, TokenID id, char* name, char* nameEnd, entity_index entityIndex)
	{
		std::vector<char> nameBuffer;
		nameBuffer.reserve(nameEnd - name);
		MakeItemName(name, nameEnd, std::back_inserter(nameBuffer));
		auto nameRange = CharPtrRange(begin_ptr(nameBuffer), end_ptr(nameBuffer));

		TreeItem* container = parent ? parent->m_Domain.get_ptr() : resultHolder.GetNew();
		TreeItem* item = container->GetItem(nameRange);
		if (!item && parent)
		{
			item = resultHolder.GetNew()->GetItem(nameRange);
			if (item && IsUnit(item))
			{
				entity_map::key_type key(TokenID::GetEmptyID(), id);
				auto iter = m_Map.lower_bound(key);
				if (iter != m_Map.end() && iter->first == key)
					return iter->second;
				parent = nullptr;
			}
		}

		if (IsUnit(item))
			return new Entity(AsUnit(item), parent, entityIndex);

		if (IsDataItem(item))
			return new Attribute(AsDataItem(item), parent, entityIndex);

		return new Element(parent, entityIndex, nullptr);
	};

	Entity* ProcessBase(boost::property_tree::detail::rapidxml::xml_base<char>* basePtr, Entity* context, entity_id parentID)
	{
		char* name = basePtr->name();
		char* nameEnd = name+basePtr->name_size();
		TokenID ns = context ? GetTokenID(context->GetName()) : TokenID::GetEmptyID();
		TokenID id = GetTokenID_mt(name, nameEnd);

		entity_map::key_type key(ns, id);
		auto iter = m_Map.lower_bound(key);
		if (iter == m_Map.end() || iter->first != key)
		{
			iter = m_Map.insert(iter, entity_map::value_type(key, CreateElement(m_ResultHolder, context, id, name, nameEnd, m_EntityNames.size())));
			SharedStr elementName = mySSPrintF("%s.%s", ns.GetStr().c_str(), id.GetStr().c_str());
			m_EntityNames.push_back_seq(elementName.cbegin(), elementName.csend());
			if (!iter->second->m_DmsFullName.empty())
				m_KnownEntities.insert(std::make_pair(iter->second->m_DmsFullName, iter->second.get_ptr()));
		}
		Element* element = iter->second;
		dms_assert(element);
		CharPtr value = basePtr->value(), valueEnd = value + basePtr->value_size();
		element->AddValue(parentID, value, valueEnd);
		return dynamic_cast<Entity*>(element);
	}

	void ProcessNode(boost::property_tree::detail::rapidxml::xml_node<char>* nodePtr, Entity* context, entity_id parentID)
	{
		Entity* entity = ProcessBase(nodePtr, context, parentID);
		if (entity)
		{
			parentID.first  = entity->m_EntityIndex;
			parentID.second = entity->m_ValueIndex.size() -1;
			context = entity;
			for (auto attr = nodePtr->first_attribute(); attr; attr = attr->next_attribute())
				ProcessBase(attr, context, parentID);
		}

		for (auto subNodePtr = nodePtr->first_node(); subNodePtr; subNodePtr = subNodePtr->next_sibling())
			ProcessNode(subNodePtr, context, parentID);
	}
};

template <typename V, typename Container >
void StoreValues(AbstrDataItem* adi, Container& c)
{
	DataWriteLock lock(adi);
	auto data = mutable_array_cast<V>(lock)->GetLockedDataWrite();
	SizeT i=0;
	for (auto v: c)
		Assign(data[i++], v);
	lock.Commit();
}

template <typename Container >
void StoreAttrValues(AbstrDataItem* adi, Container& c)
{
	DataWriteLock lock(adi, dms_rw_mode::write_only_mustzero); 
	OwningPtr<AbstrValue> aval = lock->GetValuesType()->CreateValue();
	SizeT i=0;
	for (auto& v: c)
	{
		aval->AssignFromCharPtrs(v.begin(), v.end());
		lock->SetAbstrValue(i++, *aval);
	}
	lock.Commit();
}

struct Tokens {
	TokenID 
		valuesTableID,
		valuesID, 
		entityTableID,
		parentEntityTableRelID, // WILL BECOME OBSOLETE.
		parentRelID, 
		valueRelID;

	Tokens()
		: valuesTableID(GetTokenID_st("_ValuesTable")) 
		, valuesID(GetTokenID_st("Values"))
		, entityTableID(GetTokenID_st("_EntityTable"))
		, parentEntityTableRelID(GetTokenID_st("Parent_EntityTable_rel"))  // WILL BECOME OBSOLETE.
		, parentRelID(GetTokenID_st("Parent_rel"))
		, valueRelID(GetTokenID_st("value_rel"))
	{}

};
static Tokens theTokens;

Tokens& GetTokens()
{
	return theTokens;
}

TreeItem* WalkNextElementOrContainer(TreeItem* context, TreeItem* walker)
{
	walker = walker ? context->WalkCurrSubTree(walker) : context;
	while (walker && (
			walker->GetID() == GetTokens().valuesTableID || 
			walker->GetID() == GetTokens().entityTableID || 
			walker->GetID() == GetTokens().parentRelID   ||
			walker->GetID() == GetTokens().valueRelID   ||
			walker->GetID() == GetTokens().parentEntityTableRelID
		))
	{
		walker = context->WalkNext(walker);
	}
	return walker;
}

// *****************************************************************************
//	RapidXML
// *****************************************************************************

oper_arg_policy rapidXmlArgs[] = { oper_arg_policy::calc_as_result, oper_arg_policy::is_templ };
SpecialOperGroup rapidXmlOg("parse_xml", 2, rapidXmlArgs, oper_policy::calc_requires_metainfo|oper_policy::allow_extra_args|oper_policy::has_template_arg);

struct RapidXmlOperator : public BinaryOperator
{
	RapidXmlOperator()
		: BinaryOperator(&rapidXmlOg, TreeItem::GetStaticClass()
		, DataArray<SharedStr>::GetStaticClass()
			,	TreeItem::GetStaticClass()
			)
	{}

	void CreateResultCaller(TreeItemDualRef& resultHolder, const ArgRefs& argRegs, OperationContext*, LispPtr metaCallArgs) const override
	{
		dms_assert(!CanExplainValue()); // or this method should be overridden.
		auto args = GetItems(argRegs);
		dms_assert(args.size() >= 2);

		if (!resultHolder)
			resultHolder = TreeItem::CreateCacheRoot();

		dms_assert(metaCallArgs);
		InstantiateTemplate(resultHolder.GetNew(), args[1], metaCallArgs.Right().Right()); // GetArgList()->m_Next->m_//Next); // but why?
//		TemplDC::Instantiate(resultHolder, args[1], debug_cast<FuncDC*>(&resultHolder)->GetArgList()->m_Next->m_Next);

		AbstrUnit* entityTable = Unit<UInt32>::GetStaticClass()->CreateUnit(resultHolder, GetTokens().entityTableID);
		CreateDataItem(entityTable, GetTokens().valuesID, entityTable, Unit<SharedStr>::GetStaticClass()->CreateDefault());

		TreeItem* walkRoot = resultHolder.GetNew();
		TreeItem* walker = nullptr;
		while (walker = WalkNextElementOrContainer(walkRoot, walker))
		{
			if (walker != walkRoot && walker->HasCalculatorImpl())
			{
				GetGroup()->throwOperErrorF("illegal calculation rule in xml schema at %s", walker->GetFullName());
			}
			AbstrUnit* entityDomain = AsDynamicUnit(walker);
			if (entityDomain)
			{
				dms_assert(entityDomain->GetID() != GetTokens().valuesTableID);
				dms_assert(entityDomain->GetID() != GetTokens().entityTableID);
				if (entityDomain->GetTreeParent() == resultHolder.GetNew())
					CreateDataItem(entityDomain, GetTokens().parentEntityTableRelID, entityDomain, entityTable);
				CreateDataItem(entityDomain, GetTokens().parentRelID, entityDomain, Unit<entity_index>::GetStaticClass()->CreateDefault());

				AbstrUnit* valueSet = Unit<entity_index>::GetStaticClass()->CreateUnit(entityDomain, GetTokens().valuesTableID);
				CreateDataItem(valueSet, GetTokens().valuesID, valueSet, Unit<SharedStr>::GetStaticClass()->CreateDefault());
				CreateDataItem(entityDomain, GetTokens().valueRelID, entityDomain, valueSet);
			}
		}
		dms_assert(resultHolder);
	}

	bool CalcResult(TreeItemDualRef& resultHolder, ArgRefs argRefs, std::vector<ItemReadLock> readLocks, OperationContext*, Explain::Context* context = nullptr) const override
	{
		dms_assert(resultHolder);
		dms_assert(!CanExplainValue()); // or this method should be overridden.
		auto args = GetItems(argRefs);
		dms_assert(args.size() >= 2);

		DataReadLock stringLock(AsDataItem(args[0]));
		auto stringArray = const_array_cast<SharedStr>(args[0])->GetDataRead();

		ParseContext pc(resultHolder);

		for (auto stringRef: stringArray)
		{
			boost::property_tree::detail::rapidxml::xml_document<char> doc;

			SharedStr strCopy(stringRef.begin(), stringRef.end());

			doc.parse<boost::property_tree::detail::rapidxml::parse_trim_whitespace | boost::property_tree::detail::rapidxml::parse_no_string_terminators>(strCopy.begin() );

			pc.ProcessNode(&doc, nullptr, entity_id(UNDEFINED_VALUE(SizeT), UNDEFINED_VALUE(SizeT)));
		}
		// =========== store results
		AbstrUnit* entityTable = Unit<UInt32>::GetStaticClass()->CreateUnit(resultHolder, GetTokens().entityTableID);
		entityTable->SetCount(pc.m_EntityNames.size());

		InterestPtr<SharedPtr<AbstrDataItem>> entityNames = CreateDataItem(entityTable, GetTokens().valuesID, entityTable,  Unit<SharedStr>::GetStaticClass()->CreateDefault());
		StoreValues<SharedStr>(entityNames, pc.m_EntityNames);

		Entity defaultEntity;

		TreeItem* walker = nullptr; 
		while (walker = WalkNextElementOrContainer(resultHolder.GetNew(), walker))
		{
			if (walker->GetDynamicClass() == TreeItem::GetStaticClass())
			{
				walker->SetIsInstantiated();
				continue;
			}
			if (IsUnit(walker) && !walker->HasCalculator())
			{
				AbstrUnit* entityDomain = AsUnit(walker);
				dms_assert(entityDomain->GetID() != GetTokens().valuesTableID);
				dms_assert(entityDomain->GetID() != GetTokens().entityTableID);

//				dms_assert(!entityDomain->GetTreeParent() || entityDomain->GetTreeParent()->DataInMem());
				SharedStr relativeName = entityDomain->GetFullName();
				const Entity* entity = dynamic_cast<const Entity*>(pc.m_KnownEntities[TokenID(relativeName)]);
				if (!entity)
					entity = &defaultEntity;

				entityDomain->SetCount(entity->GetCount());
				InterestPtr<SharedPtr<AbstrDataItem>> parentEntityTableRelAdi;
				if (entityDomain->GetTreeParent() == resultHolder.GetNew())
				{
					parentEntityTableRelAdi = CreateDataItem(entityDomain, GetTokens().parentEntityTableRelID, entityDomain, entityTable);
					StoreValues<entity_index>(parentEntityTableRelAdi, entity->m_ParentEntityTableRel);
				}
				InterestPtr<SharedPtr<AbstrDataItem>> parentRelAdi = CreateDataItem(entityDomain, GetTokens().parentRelID, entityDomain, Unit<entity_index>::GetStaticClass()->CreateDefault());
				StoreValues<entity_index>(parentRelAdi, entity->m_ParentRel);

				AbstrUnit* valueSet = Unit<entity_index>::GetStaticClass()->CreateUnit(entityDomain, GetTokens().valuesTableID);
				valueSet->SetCount(entity ? entity->m_Values.size() : 0);

					InterestPtr<SharedPtr<AbstrDataItem>> valuesIdAdi = CreateDataItem(valueSet, GetTokens().valuesID, valueSet, Unit<SharedStr>::GetStaticClass()->CreateDefault());
					StoreValues<SharedStr>(valuesIdAdi, entity->m_Values.GetVec());

					InterestPtr<SharedPtr<AbstrDataItem>> valueRelAdi = CreateDataItem(entityDomain, GetTokens().valueRelID, entityDomain, valueSet);
					StoreValues<entity_index>(valueRelAdi, entity->m_ValueIndex);
			}
			else if (IsDataItem(walker) && !walker->HasCalculator())
			{
				SharedStr relativeName = walker->GetFullName();
				const Attribute* attr = dynamic_cast<const Attribute*>(pc.m_KnownEntities[TokenID(relativeName)]);
				StoreAttrValues(AsDataItem(walker), attr ? attr->m_Data : defaultEntity.m_Values.GetVec());
			}
		}

		return true;
	}
};


// *****************************************************************************
//											INSTANTIATION
// *****************************************************************************

	RapidXmlOperator oper;

} // namespace anonymous



