// Copyright (C) 1998-2024 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "ClcPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif

#define BOOST_PROPERTY_TREE_RAPIDXML_STATIC_POOL_SIZE 8*1024

#include <boost/property_tree/detail/rapidxml.hpp>

#include <map>
#include <string>
#include <string_view>

#include "mci/ValueClass.h"
#include "set/IndexedStrings.h"
#include "utl/StrFormat.h"
#include "utl/splitPath.h"

#include "AbstrUnit.h"
#include "SessionData.h"
#include "TreeItemClass.h"
#include "Unit.h"
#include "UnitClass.h"

#include "MoreDataControllers.h"

// boost::property_tree::detail::rapidxml is listed as a boost sub-library by the boost VersionComponent in the geo DLL.

using IndexedStringValues = IndexedStrings<false, GenericEqual, GenericHasher>;

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
		// #1259: interned once here rather than on every GetNameID(). ProcessBase asks the context
		// element for its name id once per XML NODE, and resolving a name is an acquire of the one
		// process wide IndexedStrings section (GetCS()), so that was a lock per node for a value
		// that never changes.
		m_NameID = GetTokenID(CharPtrRange(m_DmsFullName.begin() + 1, m_DmsFullName.send()));
	}
	virtual ~Element() {}
	void Release() const { delete this;	}
	virtual void AddValue(entity_id parentID, CharPtr begin, CharPtr end) {}

	SharedStr GetNameStr() const { return m_DmsFullName.empty() ? SharedStr() : SharedStr(CharPtrRange(m_DmsFullName.begin()+1, m_DmsFullName.send())); }
	TokenID   GetNameID() const { return m_NameID; }

	Entity*      m_Parent = nullptr;
	entity_index m_EntityIndex = UNDEFINED_VALUE(entity_index);
	SharedStr    m_DmsFullName;
	TokenID      m_NameID = TokenID::GetEmptyID(); // empty while m_DmsFullName is, as GetNameID used to return
};

struct Entity : Element
{
	Entity() {}

	Entity(AbstrUnit* domain, Entity* parent, entity_index entityIndex)
		:	Element(parent, entityIndex, domain)
		,	m_Domain(make_shared_tree(domain, existing_obj{}))
	{}
	void AddValue(entity_id parentID, CharPtr begin, CharPtr end) override
	{
		if(!m_Parent)
			m_ParentEntityTableRel.push_back(parentID.first);
		m_ParentRel.push_back(parentID.second);
		// #1259: m_Values is this Entity's own table, reachable only from the ParseContext of the one
		// CalcResult that is filling it, and it holds parsed values rather than names. _mt put every
		// one of them through the process wide section that the token registry also uses, so for the
		// BAG pand schema, whose posList values are unique per object, that was an EXCLUSIVE acquire
		// per parsed object.
		m_ValueIndex.push_back( m_Values.GetOrCreateID_private(begin, end) );
	}

	SizeT GetCount() const { return m_ValueIndex.size(); }

	std::shared_ptr<AbstrUnit>  m_Domain;

	std::vector<entity_index> m_ParentEntityTableRel;
	std::vector<entity_index> m_ParentRel;
	std::vector<entity_index> m_ValueIndex;
	IndexedStringValues       m_Values;
};

struct Attribute : Element
{
	Attribute(AbstrDataItem* adi, Entity* parent, entity_index entityIndex)
		:	Element(parent, entityIndex, adi)
		,	m_Attr(make_shared_tree(adi, existing_obj{}))
	{
		MG_CHECK(parent);
	}

	void AddValue(entity_id parentID, CharPtr begin, CharPtr end) override
	{
		SizeT currCount = m_Data.size();
		SizeT entityCount = m_Parent->GetCount();

		assert(parentID.first == m_Parent->m_EntityIndex);
		assert(parentID.second == entityCount - 1);

		if (currCount >= entityCount)
			m_Attr->throwItemErrorF("Too many occurrences of attribute {} in entity {} #{}", GetNameStr(), m_Parent->GetNameStr(), UInt64(entityCount));
		for (SizeT i= entityCount - currCount - 1; i;--i)
			m_Data.push_back(Undefined() MG_DEBUG_ALLOCATOR_SRC("XML::Attribute.AddUndefined"));
		m_Data.push_back_seq(begin, end MG_DEBUG_ALLOCATOR_SRC("XML::Attribute.AddValue"));
		assert(m_Data.size() == entityCount);
	}

	std::shared_ptr<AbstrDataItem>  m_Attr;
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

	// #1259: one registry acquire per DISTINCT element name instead of one per node. ProcessBase
	// interns the node's name only to key m_Map with it, and a document repeats the same handful of
	// names once per object: a BAG pand fileset of 500 000 objects asks for the same dozen names a
	// few million times. Every one of those went through IndexedStrings::GetOrCreateID_mt, which
	// acquires the single process wide section that GetCS() hands to every instance, so this was the
	// bulk of what stopped two parses from running side by side.
	std::map<std::string, TokenID, std::less<> > m_NameIds;

	TokenID NameID(CharPtr first, CharPtr last)
	{
		std::string_view key(first, last - first);
		auto i = m_NameIds.find(key);
		if (i != m_NameIds.end())
			return i->second;
		auto id = GetTokenID_mt(first, last);
		m_NameIds.emplace(key, id);
		return id;
	}

	Element* CreateElement(TreeItemDualRef& resultHolder, Entity* parent, TokenID id, char* name, char* nameEnd, entity_index entityIndex)
	{
		std::vector<char> nameBuffer;
		nameBuffer.reserve(nameEnd - name);
		MakeItemName(name, nameEnd, std::back_inserter(nameBuffer));
		auto nameRange = CharPtrRange(begin_ptr(nameBuffer), end_ptr(nameBuffer));

		TreeItem* container = parent ? parent->m_Domain.get() : resultHolder.GetNew();
		TreeItem* item = container->GetItem(nameRange);
		if (!item && parent)
		{
			item = resultHolder.GetNew()->GetItem(nameRange);
			if (item && IsUnit(item))
			{
				entity_map::key_type key(TokenID::GetEmptyID(), id);
				auto iter = m_Map.lower_bound(key);
				if (iter != m_Map.end() && iter->first == key)
					return iter->second.get();
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
		TokenID ns = context ? context->GetNameID() : TokenID::GetEmptyID();
		TokenID id = NameID(name, nameEnd);

		entity_map::key_type key(ns, id);
		auto iter = m_Map.lower_bound(key);
		if (iter == m_Map.end() || iter->first != key)
		{
			iter = m_Map.insert(iter, entity_map::value_type(key, CreateElement(m_ResultHolder, context, id, name, nameEnd, m_EntityNames.size())));
			SharedStr elementName = mySSPrintF("{}.{}", ns, id);
			m_EntityNames.push_back_seq(elementName.cbegin(), elementName.csend() MG_DEBUG_ALLOCATOR_SRC("BoostXML::EntityNames"));
			if (!iter->second->m_DmsFullName.empty())
				m_KnownEntities.insert(std::make_pair(iter->second->m_DmsFullName, iter->second.get_ptr()));
		}
		Element* element = iter->second.get();
		assert(element);
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
	DataWriteLock lock(adi, dms_rw_mode::write_only_mustzero); // as in StoreAttrValues below; the mode must be given here, not at GetDataWrite
	auto data = mutable_array_cast<V>(lock)->GetDataWrite(no_tile, dms_rw_mode::write_only_mustzero);
	SizeT i=0;
	for (auto v: c)
		Assign(data[i++], Convert<V>(v));
	lock.Commit();
}

template <typename Container >
void StoreAttrValues(AbstrDataItem* adi, Container& c)
{
	DataWriteLock lock(adi, dms_rw_mode::write_only_mustzero); 
	auto aval = lock->GetValuesType()->CreateValue();
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
			walker->GetNameID() == GetTokens().valuesTableID || 
			walker->GetNameID() == GetTokens().entityTableID || 
			walker->GetNameID() == GetTokens().parentRelID   ||
			walker->GetNameID() == GetTokens().valueRelID   ||
			walker->GetNameID() == GetTokens().parentEntityTableRelID
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

// #1259: NOT calc_requires_metainfo, so that CanRunParallel holds and several parses run on the
// worker pool at once. That flag forced every parse onto the meta thread (a false CanRunParallel
// makes OperationContext_ScheduleThis run the task inline and getUniqueLicenseToRun refuse it), which
// made a run over N filesets strictly serial however much memory and however many cores were free.
//
// What makes it safe is that the parse never touches the SHAPE of its result. CreateResultCaller
// instantiates the schema and creates every member the calculation writes; ParseContext::CreateElement
// only LOOKS members up, and an element the schema does not name gets the base Element, whose AddValue
// is a no-op. So CalcResult only fills a tree that already exists, and two concurrent parses work on
// disjoint cache trees. The remaining shared state is thread-safe already: TokenID(WeakStr) interns
// through the mt path (Token.cpp:150), IndexedStrings uses GetOrCreateID_mt, the status flags are a
// std::atomic<UInt32>, and the per-UnitClass default units are singletons already warmed by
// CreateResultCaller on the meta thread. The CreateUnit/CreateDataItem calls in CalcResult resolve to
// lookups, since TreeItem_CreateItem returns an existing sub-item when there is one.
//
// SetCount from a worker is not new: unique(), a plain CommonOperGroup, sets its result unit's count in
// its own CalcResult, and storage_read_table produces a whole tree of members off the meta thread.
//
// The consequence to know about: a parallel operator takes the IsAllInterestedCalculatingOrDataReady
// branch of FuncDC's mustStartCalc instead of IsAllDataCurrStandby (MoreDataControllers.cpp), so the
// recalculation decision now looks at the members that carry interest rather than at every member.
SpecialOperGroup rapidXmlOg("parse_xml", 2, rapidXmlArgs, oper_policy::allow_extra_args|oper_policy::has_template_arg);

struct RapidXmlOperator : public BinaryOperator
{
	RapidXmlOperator()
		: BinaryOperator(&rapidXmlOg, TreeItem::GetStaticClass()
		, DataArray<SharedStr>::GetStaticClass()
			,	TreeItem::GetStaticClass()
			)
	{}

	void CreateResultCaller(TreeItemDualRef& resultHolder, const ArgRefs& argRefs, LispPtr metaCallArgs) const override
	{
		assert(!CanExplainValue()); // or this method should be overridden.
		auto args = GetItems(argRefs);
		assert(args.size() >= 2); // parse_xml(xmlData, schema, optional arguemts for schema...)

		if (!resultHolder)
			resultHolder = TreeItem::CreateCacheRoot();

		assert(metaCallArgs);

		if (!metaCallArgs.IsRealList())
			throwErrorD(GetGroup()->GetNameStr(), "arguments expected");
		if (!metaCallArgs.Right().IsRealList())
			throwErrorD(GetGroup()->GetNameStr(), "2nd argument expected");


		InstantiateTemplate(resultHolder.GetNew(), args[1], metaCallArgs.Right().Right()); // GetArgList()->m_Next->m_Next) is the remainder of the nul-terminated left-right list after taking the first two elements out
//		TemplDC::Instantiate(resultHolder, args[1], debug_cast<FuncDC*>(&resultHolder)->GetArgList()->m_Next->m_Next);

		AbstrUnit* entityTable = Unit<UInt32>::GetStaticClass()->CreateUnit(resultHolder.GetNew(), GetTokens().entityTableID).get();
		CreateDataItem(entityTable, GetTokens().valuesID, entityTable, Unit<SharedStr>::GetStaticClass()->CreateDefault());

		TreeItem* walkRoot = resultHolder.GetNew();
		TreeItem* walker = nullptr;
		while ((walker = WalkNextElementOrContainer(walkRoot, walker)))
		{
			if (walker != walkRoot && walker->HasCalculatorImpl())
			{
				GetGroup()->throwOperErrorF("illegal calculation rule in xml schema at {}", walker->GetFullName());
			}
			AbstrUnit* entityDomain = AsDynamicUnit(walker);
			if (entityDomain)
			{
				assert(entityDomain->GetNameID() != GetTokens().valuesTableID);
				assert(entityDomain->GetNameID() != GetTokens().entityTableID);
				if (entityDomain->GetTreeParent().get() == resultHolder.GetNew())
					CreateDataItem(entityDomain, GetTokens().parentEntityTableRelID, entityDomain, entityTable);
				CreateDataItem(entityDomain, GetTokens().parentRelID, entityDomain, Unit<entity_index>::GetStaticClass()->CreateDefault());

				AbstrUnit* valueSet = Unit<entity_index>::GetStaticClass()->CreateUnit(entityDomain, GetTokens().valuesTableID).get();
				CreateDataItem(valueSet, GetTokens().valuesID, valueSet, Unit<SharedStr>::GetStaticClass()->CreateDefault());
				CreateDataItem(entityDomain, GetTokens().valueRelID, entityDomain, valueSet);
			}
		}
		assert(resultHolder);
	}

	// #1259: without this the base estimator calls parse_xml a FREE operation. Its result is a
	// cache-root container, so Operator::EstimatePerformance takes the early-out for a non-DataItem
	// result -- regime 'meta', zero bytes, and confidence 'derived', i.e. high confidence in the
	// number 0 -- and MemoryLedger_Retain, guarded on the same IsDataItem test, books nothing after
	// the fact either. The gate therefore reads 0 for the hungriest operation in a BAG import and
	// admits the next fileset whatever the process already holds.
	//
	// The two terms are shaped differently and were measured separately (synthetic PND filesets of
	// 43 to 346 MB, GeoDms 20.19.3.m):
	//
	//  * The RESULT accumulates over the whole argument: one ParseContext lives across the file loop
	//    in CalcResult, and what lands in it is the element text the SCHEMA captures -- an element
	//    the schema does not name gets the base Element, whose AddValue is a no-op. Measured 0.60x
	//    the input bytes for the BAG pand schema, which captures the coordinate lists, and 0.02x for
	//    a schema of one attribute. The charge below is the input size itself: captured text is a
	//    subset of the document, so that is a sound ceiling that needs no per-schema calibration,
	//    and over-charging is the direction the charge policy prefers (a slower run, not a paging
	//    collapse). A sharper factor would have to come from the schema in arg 1.
	//
	//  * The WORKING set is per FILE, not per argument: the rapidxml document and the string copy it
	//    parses in place are declared inside the loop body and freed each iteration. Measured: the
	//    peak of a minimal-schema parse equals the peak of the read that feeds it, at every size, so
	//    charging the whole argument here would over-book by the file count -- fifty-fold on this
	//    configuration.
	//
	// The regime stays 'meta' because that is what the result IS; resultingMemory is what
	// LedgerChargeOf reads. Confidence is left as the base set it, for the reason
	// AbstrPolygonConnectivityOperator spells out: RefreshEstimateForAdmission installs nothing above
	// 'declared', so downgrading here would discard the figure this exists to supply.
	auto EstimatePerformance(TreeItemDualRef& resultHolder, const ArgRefs& args) const -> PerformanceEstimationData override
	{
		auto result = BinaryOperator::EstimatePerformance(resultHolder, args);

		if (args.empty())
			return result;
		auto argItem = GetItem(args[0]);
		if (!argItem || !IsDataItem(argItem))
			return result;
		auto argAdi = AsDataItem(argItem);

		AbstrUnit::CountEstimate argCount;
		try { argCount = argAdi->GetAbstrDomainUnit()->EstimateCount(); }
		catch (...) { return result; } // an unresolvable domain keeps the base's figures
		auto inputBytes = EstimateDataBytes(argAdi, argCount.expected);
		if (!inputBytes)
			return result;

		// rapidxml parses in place into a copy of the element, and its node pool holds a record per
		// node beside it. An order-of-magnitude shape factor, not a measurement of a particular run.
		static constexpr SizeT XML_DOM_BYTES_PER_ELEMENT_BYTE = 3;

		result.inputSize = inputBytes;
		result.inputSizePerChore = inputBytes;
		result.nrChores = 1;
		result.extraTasks = 1;

		auto bytesPerElement = argCount.expected ? (inputBytes / argCount.expected) : inputBytes;
		result.workingMemorySize = bytesPerElement * XML_DOM_BYTES_PER_ELEMENT_BYTE;
		result.workingMemorySizePerChore = result.workingMemorySize;

		result.resultingNrElements = argCount.expected;
		result.resultingMemory = inputBytes;
		result.resultingMemoryUpperBound = inputBytes;
		result.residentMemory = inputBytes;
		result.choreMemory = inputBytes;
		return result;
	}

	bool CalcResult(TreeItemDualRef& resultHolder, const ArgRefs& argRefs, std::vector<ItemReadLock> readLocks, Explain::Context* context = nullptr) const override
	{
		assert(resultHolder);
		assert(!CanExplainValue()); // or this method should be overridden.
		auto args = GetItems(argRefs);
		assert(args.size() >= 2);

		DataReadLock stringLock(AsDataItem(args[0]));
		auto stringArray = const_array_cast<SharedStr>(args[0])->GetDataRead();

		ParseContext pc(resultHolder);

		for (auto stringRef: stringArray)
		{
			boost::property_tree::detail::rapidxml::xml_document<char> doc;

			SharedStr strCopy(CharPtrRange(stringRef.begin(), stringRef.end()));

			doc.parse<boost::property_tree::detail::rapidxml::parse_trim_whitespace | boost::property_tree::detail::rapidxml::parse_no_string_terminators>(strCopy.begin() );

			pc.ProcessNode(&doc, nullptr, entity_id(UNDEFINED_VALUE(SizeT), UNDEFINED_VALUE(SizeT)));
		}
		// =========== store results
		AbstrUnit* entityTable = Unit<UInt32>::GetStaticClass()->CreateUnit(resultHolder.GetNew(), GetTokens().entityTableID).get();
		entityTable->SetCount(pc.m_EntityNames.size());

		// #1259: an OWNING share, not an InterestPtr. This body runs on a worker now, and starting
		// interest is a meta-thread operation: Actor::IncInterestCount asserts IsMetaThread on the
		// 0 -> 1 edge, which is what taking an InterestPtr on a freshly created member is. The
		// interest bought nothing anyway: these are cache items, and TreeItem::TryCleanupMem returns
		// early for a cache item that is not the cache root, so the data written below is never
		// reclaimed on the strength of their own count. What has to be kept is the item itself,
		// which the owning share does, and the cache root's interest covers the tree as a whole.
		auto entityNames = CreateDataItem(entityTable, GetTokens().valuesID, entityTable,  Unit<SharedStr>::GetStaticClass()->CreateDefault()); // owned by entityTable (parent)
		StoreValues<SharedStr>(entityNames.get(), pc.m_EntityNames);

		Entity defaultEntity;

		TreeItem* walker = nullptr; 
		while ((walker = WalkNextElementOrContainer(resultHolder.GetNew(), walker)))
		{
			if (walker->GetDynamicClass() == TreeItem::GetStaticClass())
			{
				walker->SetIsInstantiated();
				continue;
			}
			if (IsUnit(walker) && !walker->HasCalculator())
			{
				AbstrUnit* entityDomain = AsUnit(walker);
				assert(entityDomain->GetNameID() != GetTokens().valuesTableID);
				assert(entityDomain->GetNameID() != GetTokens().entityTableID);

				SharedStr relativeName = entityDomain->GetFullName();
				const Entity* entity = dynamic_cast<const Entity*>(pc.m_KnownEntities[TokenID(relativeName)]);
				if (!entity)
					entity = &defaultEntity;

				entityDomain->SetCount(entity->GetCount());
				if (entityDomain->GetTreeParent().get() == resultHolder.GetNew())
				{
					auto parentEntityTableRelAdi = CreateDataItem(entityDomain, GetTokens().parentEntityTableRelID, entityDomain, entityTable); // owned by entityDomain (parent)
					StoreValues<entity_index>(parentEntityTableRelAdi.get(), entity->m_ParentEntityTableRel);
				}
				auto parentRelAdi = CreateDataItem(entityDomain, GetTokens().parentRelID, entityDomain, Unit<entity_index>::GetStaticClass()->CreateDefault()); // owned by entityDomain (parent)
				StoreValues<entity_index>(parentRelAdi.get(), entity->m_ParentRel);

				AbstrUnit* valueSet = Unit<entity_index>::GetStaticClass()->CreateUnit(entityDomain, GetTokens().valuesTableID).get();
				valueSet->SetCount(entity ? entity->m_Values.size() : 0);

					auto valuesIdAdi = CreateDataItem(valueSet, GetTokens().valuesID, valueSet, Unit<SharedStr>::GetStaticClass()->CreateDefault()); // owned by valueSet (parent)
					StoreValues<SharedStr>(valuesIdAdi.get(), entity->m_Values.GetVec());

					auto valueRelAdi = CreateDataItem(entityDomain, GetTokens().valueRelID, entityDomain, valueSet); // owned by entityDomain (parent)
					StoreValues<entity_index>(valueRelAdi.get(), entity->m_ValueIndex);
			}
			else if (IsDataItem(walker) && !walker->HasCalculator())
			{
				SharedStr relativeName = walker->GetFullName();
				const Attribute* attr = dynamic_cast<const Attribute*>(pc.m_KnownEntities[TokenID(relativeName)]);
				StoreAttrValues(AsDataItem(walker), attr ? attr->m_Data : defaultEntity.m_Values.GetVec());
			}
		}

		defaultEntity.Abandon();

		return true;
	}
};


// *****************************************************************************
//											INSTANTIATION
// *****************************************************************************

	RapidXmlOperator oper;

} // namespace anonymous



