// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////

#include "ClcPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif

// subset/select operators: building selection domains and their org_rel
// attributes from condition attributes.

#include "mci/CompositeCast.h"
#include "mci/ValueClass.h"
#include "utl/StrFormat.h"

#include "DataArray.h"
#include "DataArrayValue.h"
#include "DataItemClass.h"
#include "LispTreeType.h"
#include "ParallelTiles.h"
#include "TileChannel.h"
#include "TreeItemClass.h"
#include "Unit.h"
#include "UnitClass.h"
#include "UnitProcessor.h"

#include "OperSignature.h"

// *****************************************************************************
//                         count_selected
// *****************************************************************************

template<typename Block>
SizeT count_bitvalues(bit_iterator<1, Block> sb, bit_iterator<1, Block> se)
{
	SizeT count = 0;
	for (; sb.m_NrElems && sb != se; ++sb)
		if (bit_value<1>(*sb))
			++count;
	while (sb.m_BlockData != se.m_BlockData)
	{
		if ((*sb.m_BlockData & bit_info<1, Block>::used_bits_mask) == 0)
			++sb.m_BlockData;
		else if ((*sb.m_BlockData & bit_info<1, Block>::used_bits_mask) == bit_info<1, Block>::used_bits_mask)
		{
			count += bit_info<1, Block>::nr_elem_per_block;
			++sb.m_BlockData;
		}
		else
			do {
				if (bit_value<1>(*sb))
					++count;
				++sb;
			} while (sb.m_NrElems);
	}
	for (; sb != se; ++sb)
		if (bit_value<1>(*sb))
			++count;
	return count;
}

// *****************************************************************************
//                               select, subset, select_with_org_rel
// *****************************************************************************

template <typename ResContainer>
void make_subset_container(ResContainer* resultSub, const DataArray<Bool>* boolArray)
{
	dms_assert(resultSub);

	using ArgType = DataArray<Bool>;
	using org_value_t = typename ResContainer::value_type;
	using ArgDomain = Unit< org_value_t>;
	using ArgTileType = range_or_void_data<org_value_t>;
	auto orgAbstrTiling = boolArray->GetTiledRangeData();
	auto orgTiling = checked_cast<const ArgTileType*>(orgAbstrTiling.get());

	tile_write_channel<org_value_t> resDataChannel(resultSub);
	auto tn = orgTiling->GetNrTiles();
	for (tile_id t = 0; t!=tn; ++t)
	{
		auto resValuesRange = orgTiling->GetTileRange(t);
		auto boolData = boolArray->GetTile(t);

		dms_assert(!boolData.begin().m_NrElems);

		ArgType::const_iterator::block_type* di = boolData.begin().m_BlockData;
		ArgType::const_iterator::block_type* de = boolData.end  ().m_BlockData;

		SizeT count = 0;
		for (; di != de; ++di)
		{
			if (*di) // process the bit-block
			{
				for (ArgType::const_iterator i(di, SizeT(0)), e(di, SizeT(ArgType::const_iterator::nr_elem_per_block)); i!=e; ++count, ++i)
					if (Bool(*i))
						resDataChannel.Write(Range_GetValue_naked(resValuesRange, count) );
			}
			else
				count += ArgType::const_iterator::nr_elem_per_block; // jump over the bit-block
		}
		// process the remaining bits
		for (ArgType::const_iterator i(di, SizeT(0)), e=boolData.end(); i!=e; ++count, ++i)
			if (Bool(*i))
				resDataChannel.Write(Range_GetValue_naked(resValuesRange, count) );
	}
	dms_assert(resDataChannel.IsEndOfChannel());
}

enum class OrgRelCreationMode { none, org_rel, nr_OrgEntity, org_rel_and_use_it };

struct SubsetOperator: public UnaryOperator
{
	using ArgType = DataArray<Bool>;

	OrgRelCreationMode m_ORCM;

	SubsetOperator(CommonOperGroup& cog, const Class* resDomainClass, OrgRelCreationMode orcm)
		: UnaryOperator(&cog, resDomainClass, DataArray<Bool>::GetStaticClass())
		, m_ORCM(orcm)
	{}

	// K6: select(condition: attribute<bool>(D)) -> a FRESH subset unit U [new].
	// The condition's Bool class is the checkable argument constraint (member
	// elimination already rejects a non-bool concrete condition); the result is
	// an existential unit whose value class is crd(D) -- fixed for the typed
	// select_uintN groups, unconstrained for the dynamic-result-class select.
	// §12.7 slSubItemCall tranche: the org_rel / nr_OrgEntity sub-item --
	// attribute<D>(U); D is in BOTH roles, so the member's values IDENTITY
	// rides the condition's domain (the batch-B K2 rule). The member set is
	// complete per ORCM variant -- the plain select_* groups create NO
	// sub-items, so a member reference on them reports at definition
	bool DescribeSignature(AbstrSignatureBuilder& sb) const override
	{
		auto argCls = dynamic_cast<const DataItemClass*>(GetArgClass(0));
		if (!argCls)
			return false;
		sig_var B = sb.UnitVar("B"), D = sb.UnitVar("D"), U = sb.GeneratedUnit("U");
		sb.MemberValueClass(B, argCls->GetValuesType());
		if (auto resCls = dynamic_cast<const UnitClass*>(GetResultClass()))
			if (auto rvt = resCls->GetValueType())
				sb.MemberValueClass(U, rvt);
		sb.ArgName(0, "condition");
		sb.ArgAttr(0, B, D, ValueComposition::Single);
		sb.ResultUnit(U);
		if (m_ORCM != OrgRelCreationMode::none)
			sb.ResultContainerMember(
				m_ORCM == OrgRelCreationMode::nr_OrgEntity ? "nr_OrgEntity" : "org_rel"
				, D, U, ValueComposition::Single);
		sb.ResultMembersComplete();
		return true;
	}

	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		dms_assert(args.size() == 1);

		const AbstrDataItem* arg1A = debug_cast<const AbstrDataItem*>(args[0]);
		dms_assert(arg1A);

		const AbstrUnit* arg1Domain = arg1A->GetAbstrDomainUnit();
		dms_assert(arg1Domain);

		const ValueClass* vc           = arg1Domain->GetValueType();
		const UnitClass*  resDomainCls = dynamic_cast<const UnitClass*>(m_ResultClass);
		if (!resDomainCls)
			resDomainCls = UnitClass::Find(vc->GetCrdClass());

		auto res_owner = resDomainCls->CreateResultUnit(resultHolder.GetNew()); AbstrUnit* res = res_owner.get();
		assert(res);
		res->SetTSF(TSF_Categorical);

		resultHolder = res;

		AbstrDataItem* resSub = nullptr;
		if (m_ORCM != OrgRelCreationMode::none)
		{
			TokenID resSubName = ((m_ORCM == OrgRelCreationMode::org_rel) || (m_ORCM == OrgRelCreationMode::org_rel_and_use_it)) ? token::org_rel : token::nr_OrgEntity; // NOT auto: that deduces StaticTokenID and copies its TokenComponent
			resSub = CreateDataItem(res, resSubName, res, arg1Domain).get(); // owned by res
			resSub->SetTSF(TSF_Categorical);

			MG_PRECONDITION(resSub);
		}

		if (mustCalc)
		{
			DataReadLock arg1Lock(arg1A);

			Calculate(res, resSub, arg1A, arg1Domain);
		}
		return true;
	}

	void Calculate(AbstrUnit* res, 
		      AbstrDataItem* resSub, 
		const AbstrDataItem* arg1A,
		const AbstrUnit*     arg1Domain) const
	{
		const ArgType* arg1Obj  = const_array_cast<Bool>(arg1A);
		dms_assert(arg1Obj);

		tile_id tn = arg1Domain->GetNrTiles();

		ArgType::locked_cseq_t arg1RecycledLock; // use to avoid multiple locking in case of 1 tile only

		std::atomic<SizeT> count = 0;
		parallel_tileloop(tn, [=, &count, &arg1RecycledLock](tile_id t)
			{
				auto boolData = arg1Obj->GetTile(t);
				count += count_bitvalues(boolData.begin(), boolData.end());
				if (!t)
					arg1RecycledLock = std::move(boolData);
			}
		);

		res->SetCount(count);

		if (resSub)
		{
			DataWriteLock resSubLock(resSub, dms_rw_mode::write_only_all, arg1Domain->GetTiledRangeData().get());

			assert(resSub->GetAbstrValuesUnit()->UnifyDomain(arg1A->GetAbstrDomainUnit(), "values of resSub", "e1"));

			if (auto trd = AsUnit(arg1Domain->GetCurrRangeItem())->GetTiledRangeData())
				resSub->m_StatusFlags.SetHasSortedValues(trd->HasSortedValues());

			visit<typelists::domain_elements>(arg1Domain, 
				[&resSubLock, arg1Obj] <typename a_type> (const Unit<a_type>* arg1Domain)
				{
					
					make_subset_container(
						mutable_array_cast<a_type>(resSubLock)
						, arg1Obj
					);
				}
			);

			resSubLock.Commit();
		}
	}
};

// *****************************************************************************
//                          attribute collection scope
// *****************************************************************************

// Which sub-items of the container argument the _with_attr_ and collect_attr_
// operators collect.
//
// ref: also walk the referred-item chain. A template case parameter is bound by
//      an ArgCalc and deliberately gets no sub-items of its own (TreeItem::Copy:
//      "don't copy subItems from this to result (take them from arg)"), so the
//      attributes of the unit it is bound to are only reachable through
//      mc_RefItem. Name lookup already follows that chain in
//      GetConstSubTreeItemByID, which is why the condition argument resolves
//      while the collection came up empty; ref makes enumeration agree with it.
// sub: also descend into sub-containers, mirroring their structure in the result.
enum class attr_scope { own = 0, ref = 1, sub = 2, ref_sub = 3 };

inline bool FollowsRef(attr_scope s) { return UInt32(s) & UInt32(attr_scope::ref); }
inline bool Recurses  (attr_scope s) { return UInt32(s) & UInt32(attr_scope::sub); }

struct collect_target
{
	SharedStr            relPath; // "name", or "geo/point" for a mirrored member
	const AbstrDataItem* src;
	UInt32               depth;   // 0 for a direct member of the result
};

// GeoDMS spells "N levels up" as N+1 DOTS: "." is the context itself, ".." its
// parent, "..." its grandparent -- the same encoding GetFindableName builds with
// RepeatedDots. A slash-joined "../.." is not a path at all: the expression
// parser reads the slash as division and reports div(.., ..).
SharedStr UpDots(UInt32 levels)
{
	std::vector<char> dots(levels + 1, '.');
	return SharedStr(CharPtrRange(dots.data(), dots.data() + dots.size()));
}

// Enumerate the attributes a select_with_attr / collect_attr call considers.
// Candidates only: the caller applies the domain filter, as select and collect
// differ in what they report about a domain mismatch.
//
// Within one container level the first name found wins, so an own sub-item
// shadows a same-named one further down the referred-item chain -- the order
// GetConstSubTreeItemByID resolves in. A name is claimed before the caller's
// domain filter runs, so a shadowing item that is not itself collectable still
// hides the one it shadows, which is what {container}/{name} would resolve to.
void EnumCollectCandidates(const TreeItem* container, attr_scope scope
,	SharedStr prefix, UInt32 depth
,	std::vector<collect_target>& out, std::set<const TreeItem*>& visitedScopes)
{
	std::set<TokenID> seenNames;
	for (const TreeItem* link = container; link; )
	{
		if (!visitedScopes.insert(link).second)
			break; // a cycle in the referred-item chain, or a scope already scanned
		link->UpdateMetaInfo();
		for (auto subItem = link->GetFirstSubItem(); subItem; subItem = subItem->GetNextItem())
		{
			auto subID = subItem->GetID();
			if (subID == token::org_rel || subID == token::nr_OrgEntity)
				continue;
			if (!seenNames.insert(subID).second)
				continue; // shadowed by a nearer sub-item of the same name
			if (IsDataItem(subItem))
			{
				auto subDataItem = AsDataItem(subItem);
				subDataItem->UpdateMetaInfo();
				out.push_back({ mySSPrintF("{}{}", prefix, subID), subDataItem, depth });
			}
			else if (Recurses(scope) && !IsUnit(subItem)
				&& !subItem->IsTemplate() && !subItem->IsFunctionItem() && !subItem->IsCacheItem())
				EnumCollectCandidates(subItem, scope
				,	mySSPrintF("{}{}/", prefix, subID), depth + 1
				,	out, visitedScopes);
		}
		if (!FollowsRef(scope))
			break;
		link->UpdateDC(); // mc_RefItem of a bound case parameter is only set here
		link = link->GetCurrRefItem().get();
	}
}

// *****************************************************************************
//                          the select spec
// *****************************************************************************

// The select family varies along four independent axes: the value type of the
// result domain, whether an org_rel is produced, whether attributes are collected
// (and through that org_rel or through the condition), and -- since #337 -- how
// far that collection reaches. Each named operator pins all four at registration.
// select_spec reads them from a ';'-separated first argument instead, so a
// combination that has no name of its own stays reachable and the #337 scopes
// cost no new operator names.
//
// Same split as ForEach.cpp: for_each_<fs> fixes its field_spec at registration,
// for_each_ind takes it from the first argument's VALUE, which is what
// oper_policy::dynamic_argument_policies arranges.

struct select_config
{
	const Class* resDomainClass = nullptr; // null: derive from the condition's domain
	TokenID      selectOper;               // Tier-1 group that produces the domain
	bool         useOrgRelForCollect = false;
	bool         collectAttrs = false;
	attr_scope   scope = attr_scope::own;
};

struct select_vt_row { CharPtr name; TokenID plain, withOrgRel; const Class* cls; };

// A ';'-separated word list; order is free and an empty spec is allowed. An
// unknown word throws, naming what is accepted: a spec typo is a configuration
// error and belongs at definition time, not halfway through a calculation.
select_config ParseSelectSpec(const AbstrOperGroup* og, CharPtr specPtr)
{
	const select_vt_row vtRows[] =
	{ { ""      , token::select       , token::select_with_org_rel       , nullptr }
	, { "uint8" , token::select_uint8 , token::select_uint8_with_org_rel , Unit<UInt8 >::GetStaticClass() }
	, { "uint16", token::select_uint16, token::select_uint16_with_org_rel, Unit<UInt16>::GetStaticClass() }
	, { "uint32", token::select_uint32, token::select_uint32_with_org_rel, Unit<UInt32>::GetStaticClass() }
	, { "uint64", token::select_uint64, token::select_uint64_with_org_rel, Unit<UInt64>::GetStaticClass() }
	};
	const UInt32 nrVt = sizeof(vtRows) / sizeof(vtRows[0]);

	select_config cfg;
	UInt32 vtIndex = 0;
	bool wantOrgRel = false, followRef = false, recurse = false;

	for (CharPtr b = specPtr; b && *b; )
	{
		CharPtr e = b;
		while (*e && *e != ';')
			++e;
		CharPtr tb = b, te = e;
		while (tb != te && (*tb == ' ' || *tb == '\t')) ++tb;
		while (tb != te && (te[-1] == ' ' || te[-1] == '\t')) --te;
		b = *e ? e + 1 : e;
		if (tb == te)
			continue;

		auto is = [tb, te](CharPtr lit) -> bool
		{
			SizeT n = strlen(lit);
			return SizeT(te - tb) == n && !strncmp(tb, lit, n);
		};

		UInt32 i = 1;
		while (i != nrVt && !is(vtRows[i].name))
			++i;
		if (i != nrVt)         { vtIndex = i;                                      continue; }
		if (is("org_rel"))     { wantOrgRel = true;                                continue; }
		if (is("use_org_rel")) { wantOrgRel = true; cfg.useOrgRelForCollect = true; continue; }
		if (is("attr"))        { cfg.collectAttrs = true;                          continue; }
		if (is("ref"))         { followRef = true;                                 continue; }
		if (is("sub"))         { recurse = true;                                   continue; }

		og->throwOperErrorF(
			"unknown word '{}' in the specification; accepted: uint8, uint16, uint32, uint64, org_rel, use_org_rel, attr, ref, sub"
		,	SharedStr(CharPtrRange(tb, te))
		);
	}

	if (cfg.useOrgRelForCollect && !cfg.collectAttrs)
		og->throwOperError("'use_org_rel' says how attributes are collected, so it requires 'attr'");
	if (!cfg.collectAttrs && (followRef || recurse))
		og->throwOperError("'ref' and 'sub' say how far attributes are collected, so they require 'attr'");

	cfg.resDomainClass = vtRows[vtIndex].cls;
	cfg.selectOper     = wantOrgRel ? vtRows[vtIndex].withOrgRel : vtRows[vtIndex].plain;
	cfg.scope = attr_scope(UInt32(followRef ? attr_scope::ref : attr_scope::own)
	                     | UInt32(recurse   ? attr_scope::sub : attr_scope::own));
	return cfg;
}

// select_spec(spec, [container,] condition): the container argument is there
// exactly when the spec says 'attr' -- which is what makes the policies dynamic.
struct SelectSpecOperGroup : AbstrOperGroup
{
	SelectSpecOperGroup()
		:	AbstrOperGroup(token::select_spec
			,	oper_policy::dont_cache_result
			|	oper_policy::dynamic_result_class
			|	oper_policy::dynamic_argument_policies
			)
		// Deliberately NOT allow_extra_args: FindOperByArgs returns the first member
		// whose declared args all match, and with extra args allowed that is the
		// one-argument form even for a three-argument call. Arity acceptance does not
		// need it either -- a dont_cache_result group accepts any arity anyway.
	{}

	oper_arg_policy GetArgPolicy(arg_index argNr, CharPtr firstArgValue) const override
	{
		if (!argNr)
			return oper_arg_policy::calc_always; // the spec itself
		// FindOper strips trailing calc_as_result args while probing with a null spec
		// value (OperGroups.cpp): with no spec to read, the trailing condition is the
		// only answer that matters there, so do not insist on having one.
		if (firstArgValue && argNr == 1 && ParseSelectSpec(this, firstArgValue).collectAttrs)
			return oper_arg_policy::calc_never;  // the attribute container
		return oper_arg_policy::calc_as_result;  // the condition
	}
};

// *****************************************************************************
//                               selet_with_attr_xxx
// *****************************************************************************

struct SelectMetaOperator : public BinaryOperator
{
	// Fixed-spec form: each named operator pins its configuration here, the way
	// ForEachOperGroup pins its field_spec.
	SelectMetaOperator(AbstrOperGroup& cog, const Class* resDomainClass, OrgRelCreationMode orcm, TokenID selectOper)
		: BinaryOperator(&cog, resDomainClass, TreeItem::GetStaticClass(), DataArray<Bool>::GetStaticClass())
		, m_FromSpec(false)
	{
		m_Cfg.resDomainClass       = resDomainClass;
		m_Cfg.selectOper           = selectOper;
		m_Cfg.useOrgRelForCollect  = (orcm == OrgRelCreationMode::org_rel_and_use_it);
		m_Cfg.collectAttrs         = true;
	}

	// Spec-driven form, with a container argument ('attr' in the spec).
	SelectMetaOperator(SelectSpecOperGroup& cog, bool withContainer)
		: BinaryOperator(&cog, AbstrUnit::GetStaticClass(), DataArray<SharedStr>::GetStaticClass(), TreeItem::GetStaticClass())
		, m_FromSpec(true)
	{
		MG_CHECK(withContainer);
	}

	using ArgType = DataArray<Bool>;

	select_config m_Cfg;
	bool          m_FromSpec;

	void CreateResultCaller(TreeItemDualRef& resultHolder, const ArgRefs& args, LispPtr metaCallArgs) const override
	{
		select_config cfg = m_Cfg;
		LispPtr callArgs = metaCallArgs;

		if (!callArgs.IsRealList())
			throwErrorD(GetGroup()->GetNameStr(), "arguments expected");

		SizeT containerArg = 0;
		if (m_FromSpec)
		{
			auto specItem = GetItem(args[0]);
			MG_USERCHECK2(specItem && IsDataItem(specItem), "select_spec: a string specification is expected as 1st argument");
			cfg = ParseSelectSpec(GetGroup(), GetValue<SharedStr>(AsDataItem(specItem), 0).c_str());
			callArgs = callArgs.Right();
			containerArg = 1;
			if (!callArgs.IsRealList())
				throwErrorD(GetGroup()->GetNameStr(), "2nd argument expected");
		}

		const TreeItem* attrContainer = nullptr;
		LispPtr containerExpr = callArgs.Left();
		if (cfg.collectAttrs)
		{
			attrContainer = GetItem(args[containerArg]);
			callArgs = callArgs.Right();
			if (!callArgs.IsRealList())
				throwErrorD(GetGroup()->GetNameStr(), "condition argument expected");
		}
		auto conditionExpr = callArgs.Left();

		auto conditionExprStr = AsFLispSharedStr(conditionExpr, FormattingFlags::NoLimitInLispExpr);
		auto conditionCalc = AbstrCalculator::ConstructFromLispRef(resultHolder.GetOld(), conditionExpr, CalcRole::Other);
		MG_CHECK(conditionCalc);
		auto conditionDC = GetDC(conditionCalc.get());
		LispRef conditionKeyExpr;
		const AbstrDataItem* conditionA = nullptr;
		if (conditionDC)
		{
			conditionKeyExpr = conditionDC->GetLispRef();

			auto conditionItem = conditionDC->MakeResult();
			if (conditionDC->WasFailed(FailType::MetaInfo))
				conditionDC->ThrowFail();
			MG_CHECK(conditionItem);

			conditionA = AsDynamicDataItem(conditionItem.get());
		}
		if (!conditionA)
			throwErrorD(GetGroup()->GetNameStr(), "condition expected as last argument");

		const AbstrUnit* domain = conditionA->GetAbstrDomainUnit();
		assert(domain);

		const ValueClass* vc = domain->GetValueType();
		const UnitClass* resDomainCls = dynamic_cast<const UnitClass*>(cfg.resDomainClass);
		if (!resDomainCls)
			resDomainCls = UnitClass::Find(vc->GetCrdClass());

		auto res_owner = resDomainCls->CreateResultUnit(resultHolder.GetNew()); AbstrUnit* res = res_owner.get();
		assert(res);
		auto resExpr = ExprList(cfg.selectOper, conditionKeyExpr);
		assert(!resExpr.EndP());
		auto resDC = GetOrCreateDataController(resExpr);
		assert(resDC);
		res->SetDC(resDC);
		resultHolder = res;

		if (cfg.collectAttrs)
		{
			SizeT foundSubItems = 0;
			std::vector<collect_target> candidates;
			std::set<const TreeItem*> visitedScopes;
			EnumCollectCandidates(attrContainer, cfg.scope, SharedStr(), 0, candidates, visitedScopes);

			for (const auto& target : candidates)
			{
				if (!domain->UnifyDomain(target.src->GetAbstrDomainUnit()))
					continue;

				auto resSub = CreateDataItemFromPath(res, target.relPath.c_str(), res, target.src->GetAbstrValuesUnit(), target.src->GetValueComposition());

				// A mirrored member sits deeper than the result root, and its rule is
				// resolved from its own parent, so every relative path deepens with it.
				// At depth 0 these are ".", "org_rel" and "..", i.e. unchanged.
				auto subsetPath = UpDots(target.depth);                 // the result root: "." at depth 0
				auto orgRelPath = target.depth ? mySSPrintF("{}/org_rel", UpDots(target.depth)) : SharedStr("org_rel");
				auto srcScope   = UpDots(target.depth + 1);              // one above it: ".." at depth 0

				SharedStr selectExpr;
				if (cfg.useOrgRelForCollect)
					selectExpr = mySSPrintF("collect_by_org_rel({}, scope({}, {}/{}))"
					,	orgRelPath
					,	srcScope
					,	containerExpr.GetSymbID()
					,	target.relPath
					);
				else
					selectExpr = mySSPrintF("collect_by_cond({}, scope({}, {}), scope({}, {}/{}))"
					,	subsetPath
					,	srcScope
					,	conditionExprStr
					,	srcScope
					,	containerExpr.GetSymbID()
					,	target.relPath
					);
				auto oldExpr = resSub->GetExprMember();
				if (!oldExpr.empty() && oldExpr != selectExpr)
				{
					auto msg = mySSPrintF("Cannot set calculation rule '{}' to selected attribute '{}' as it is already defined as '{}'", selectExpr, target.relPath, oldExpr);
					throwErrorD(GetGroup()->GetNameID(), msg.c_str());
				}
				resSub->SetExpr(selectExpr);
				++foundSubItems;
			}
			if (!foundSubItems)
				reportF(SeverityTypeID::ST_Warning, "{}: no sub-items found with a domain that is compatible with the domain of the given condition", GetGroup()->GetNameStr());
		}
		res->SetIsInstantiated();
	}
};

// select_spec without 'attr': no container argument, so the result is just the
// selection domain that the named Tier-1 select operators produce.
struct SelectSpecNoAttrOperator : public UnaryOperator
{
	SelectSpecNoAttrOperator(SelectSpecOperGroup& cog)
		: UnaryOperator(&cog, AbstrUnit::GetStaticClass(), DataArray<SharedStr>::GetStaticClass())
	{}

	void CreateResultCaller(TreeItemDualRef& resultHolder, const ArgRefs& args, LispPtr metaCallArgs) const override
	{
		auto specItem = GetItem(args[0]);
		MG_USERCHECK2(specItem && IsDataItem(specItem), "select_spec: a string specification is expected as 1st argument");
		auto cfg = ParseSelectSpec(GetGroup(), GetValue<SharedStr>(AsDataItem(specItem), 0).c_str());
		MG_USERCHECK2(!cfg.collectAttrs, "select_spec: 'attr' needs a container argument between the specification and the condition");

		if (!metaCallArgs.IsRealList() || !metaCallArgs.Right().IsRealList())
			throwErrorD(GetGroup()->GetNameStr(), "condition argument expected");
		auto conditionExpr = metaCallArgs.Right().Left();

		auto conditionCalc = AbstrCalculator::ConstructFromLispRef(resultHolder.GetOld(), conditionExpr, CalcRole::Other);
		MG_CHECK(conditionCalc);
		auto conditionDC = GetDC(conditionCalc.get());
		MG_USERCHECK2(conditionDC, "select_spec: condition expected as last argument");
		auto conditionKeyExpr = conditionDC->GetLispRef();
		auto conditionItem = conditionDC->MakeResult();
		if (conditionDC->WasFailed(FailType::MetaInfo))
			conditionDC->ThrowFail();
		auto conditionA = AsDynamicDataItem(conditionItem.get());
		MG_USERCHECK2(conditionA, "select_spec: condition expected as last argument");

		const AbstrUnit* domain = conditionA->GetAbstrDomainUnit();
		const UnitClass* resDomainCls = dynamic_cast<const UnitClass*>(cfg.resDomainClass);
		if (!resDomainCls)
			resDomainCls = UnitClass::Find(domain->GetValueType()->GetCrdClass());

		auto res_owner = resDomainCls->CreateResultUnit(resultHolder.GetNew()); AbstrUnit* res = res_owner.get();
		res->SetDC(GetOrCreateDataController(ExprList(cfg.selectOper, conditionKeyExpr)));
		resultHolder = res;
		res->SetIsInstantiated();
	}
};

// *****************************************************************************
//                               collect_by_cond, collect_by_org_rel
// *****************************************************************************

struct AbstrCollectByCondOperator : TernaryOperator
{
	AbstrCollectByCondOperator(AbstrOperGroup& aog, ClassCPtr dataClass)
		: TernaryOperator(&aog, dataClass, AbstrUnit::GetStaticClass(), DataArray<Bool>::GetStaticClass(), dataClass)
	{}

	// collect_by_cond(subset: S; condition: attribute<bool>(D); data: attribute<V>(D))
	// -> attribute<V>(S). K1: the condition and data domains are unified
	// (UnifyDomain below) -- the single variable D catches a domain mismatch at
	// the definition's first reference (the batch-D headline). The result ranges
	// over the passed subset unit S (its arg0 identity) and borrows the data's
	// value class V (values-only, so class-level per the batch-B identity rule).
	// This was deferred from batch B (the fresh subset domain travels in arg0)
	bool DescribeSignature(AbstrSignatureBuilder& sb) const override
	{
		auto condCls = dynamic_cast<const DataItemClass*>(GetArgClass(1));
		auto dataCls = dynamic_cast<const DataItemClass*>(GetArgClass(2));
		if (!condCls || !dataCls)
			return false;
		sig_var S = sb.UnitVar("S"), D = sb.UnitVar("D"), B = sb.UnitVar("B"), V = sb.UnitVar("V");
		sb.MemberValueClass(B, condCls->GetValuesType());
		sb.MemberValueClass(V, dataCls->GetValuesType());
		auto vc = dataCls->GetValuesType()->GetValueComposition();
		sb.ArgName(0, "subset");    sb.ArgUnit(0, S);
		sb.ArgName(1, "condition"); sb.ArgAttr(1, B, D, ValueComposition::Single);
		sb.ArgName(2, "data");      sb.ArgAttr(2, V, D, vc);
		sb.ResultAttr(V, S, vc);
		return true;
	}

	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		dms_assert(args.size() == 3);

		const AbstrUnit* subset = AsUnit(args[0]);
		const AbstrDataItem* condA = AsDataItem(args[1]);
		const AbstrDataItem* dataA = AsDataItem(args[2]);
		condA->GetAbstrDomainUnit()->UnifyDomain(dataA->GetAbstrDomainUnit(), "e1", "e2", UM_Throw);

		if (!resultHolder)
		{
			resultHolder = CreateCacheDataItem(subset, dataA->GetAbstrValuesUnit(), dataA->GetValueComposition());
			if (dataA->GetTSF(TSF_Categorical))
				resultHolder->SetTSF(TSF_Categorical);
		}

		if (mustCalc)
		{
			AbstrDataItem* res = debug_cast<AbstrDataItem*>(resultHolder.GetNew());

			DataReadLock arg1Lock(condA);
			DataReadLock arg2Lock(dataA);
			DataWriteLock resLock(res);

			resultHolder->m_StatusFlags.SetHasSortedValues(dataA->m_StatusFlags.HasSortedValues() && AsUnit(dataA->GetAbstrDomainUnit()->GetCurrRangeItem())->GetTiledRangeData()->HasSortedValues());

			Calculate(resLock, subset, condA, dataA);
			resLock.Commit();
		}
		return true;
	}
	virtual void Calculate(DataWriteHandle& res, const AbstrUnit* subset, const AbstrDataItem* condA, const AbstrDataItem* dataA) const = 0;
};

template <typename V>
struct CollectByCondOperator : AbstrCollectByCondOperator
{
	CollectByCondOperator(AbstrOperGroup& aog)
		: AbstrCollectByCondOperator(aog, DataArray<V>::GetStaticClass())
	{}

	void Calculate(DataWriteHandle& res, const AbstrUnit* subset, const AbstrDataItem* condA, const AbstrDataItem* dataA) const override
	{
		tile_write_channel<V> resDataChannel(mutable_array_cast<V>(res));

		const DataArray<Bool>* cond = const_array_cast<Bool>(condA);
		const DataArray<V   >* data = const_array_cast<V>   (dataA);

		tile_id tn = condA->GetAbstrDomainUnit()->GetNrTiles();

		for (tile_id t = 0; t != tn; ++t)
		{
//			ReadableTileLock condLock(cond, t), dataLock(data, t);

			auto boolData = cond->GetLockedDataRead(t);

			auto di = boolData.begin().m_BlockData;
			auto de = boolData.end().m_BlockData;

			for (; di != de; ++di)
			{
				if (*di)
					goto processDataData;
			}
			for (DataArray<Bool>::const_iterator i(di, SizeT(0)), e = boolData.end(); i != e; ++i)
				if (Bool(*i))
					goto processDataData;
			continue; // go to next time

		processDataData:
			auto dataData = data->GetLockedDataRead(t);
			SizeT count = (di - boolData.begin().m_BlockData) * DataArray<Bool>::const_iterator::nr_elem_per_block;
			for (; di != de; ++di)
			{
				if (*di)
				{
					for (int i=0, e=DataArray<Bool>::const_iterator::nr_elem_per_block; i != e; ++count, ++i)
						if (Bool(*DataArray<Bool>::const_iterator(di, SizeT(i))))
							resDataChannel.Write(dataData[count]);
				}
				else
					count += DataArray<Bool>::const_iterator::nr_elem_per_block;
			}

			for (DataArray<Bool>::const_iterator i(di, SizeT(0)), e = boolData.end(); i != e; ++count, ++i)
				if (Bool(*i))
					resDataChannel.Write(dataData[count]);
		}
		MG_CHECK(resDataChannel.IsEndOfChannel());
	}
};

// *****************************************************************************
//                               collect_with_attr_by_cond, collect_with_attr_by_org_rel
// *****************************************************************************

enum class collect_mode { org_rel, condition };

struct collect_config
{
	collect_mode mode = collect_mode::org_rel;
	attr_scope   scope = attr_scope::own;
};

// A ';'-separated word list, like the select spec: e.g. 'by_cond;ref'. The mode
// word is required: the last argument is read as an org_rel or as a condition,
// and guessing which would silently produce a different result.
collect_config ParseCollectSpec(const AbstrOperGroup* og, CharPtr specPtr)
{
	collect_config cfg;
	bool modeSeen = false, followRef = false, recurse = false;

	for (CharPtr b = specPtr; b && *b; )
	{
		CharPtr e = b;
		while (*e && *e != ';')
			++e;
		CharPtr tb = b, te = e;
		while (tb != te && (*tb == ' ' || *tb == '\t')) ++tb;
		while (tb != te && (te[-1] == ' ' || te[-1] == '\t')) --te;
		b = *e ? e + 1 : e;
		if (tb == te)
			continue;

		auto is = [tb, te](CharPtr lit) -> bool
		{
			SizeT n = strlen(lit);
			return SizeT(te - tb) == n && !strncmp(tb, lit, n);
		};

		if (is("by_org_rel")) { cfg.mode = collect_mode::org_rel;   modeSeen = true; continue; }
		if (is("by_cond"))    { cfg.mode = collect_mode::condition; modeSeen = true; continue; }
		if (is("ref"))        { followRef = true;                                    continue; }
		if (is("sub"))        { recurse = true;                                      continue; }

		og->throwOperErrorF("unknown word '{}' in the specification; accepted: by_org_rel, by_cond, ref, sub"
		,	SharedStr(CharPtrRange(tb, te)));
	}
	if (!modeSeen)
		og->throwOperError("the specification must say by_org_rel or by_cond, so the last argument is read as the one that was meant");

	cfg.scope = attr_scope(UInt32(followRef ? attr_scope::ref : attr_scope::own)
	                     | UInt32(recurse   ? attr_scope::sub : attr_scope::own));
	return cfg;
}

struct CollectWithAttrOperator : public BinaryOperator
{
	collect_mode m_CollectMode;
	attr_scope   m_Scope;
	CollectWithAttrOperator(AbstrOperGroup& cog, collect_mode collectMode, attr_scope scope = attr_scope::own)
		: BinaryOperator(&cog, TreeItem::GetStaticClass(), TreeItem::GetStaticClass(), AbstrUnit::GetStaticClass()) //, AbstrDataItem::GetStaticClass())
		, m_CollectMode(collectMode)
		, m_Scope(scope)
	{}

	// Shared by the named collect_attr_by_xxx operators and by collect_spec:
	// only where the configuration comes from differs.
	static void Run(const AbstrOperGroup* og, TreeItemDualRef& resultHolder
	,	collect_mode mode, attr_scope scope, const ArgRefs& args, LispPtr callArgs)
	{
		assert(args.size() == 2);

		const TreeItem* attrContainer = GetItem(args[0]);

		auto subsetDomainItem = GetItem(args[1]);
		const AbstrUnit* domainA = AsDynamicUnit(subsetDomainItem);
		MG_USERCHECK2(domainA, "domain unit expected as 2nd argument");

		if (!callArgs.IsRealList())
			throwErrorD(og->GetNameStr(), "arguments expected");
		auto containerExpr = callArgs.Left();
		if (!callArgs.Right().IsRealList())
			throwErrorD(og->GetNameStr(), "2nd argument expected");
		auto subsetDomainExpr = callArgs.Right().Left();
		auto subsetDomainExprStr = AsFLispSharedStr(subsetDomainExpr, FormattingFlags::NoLimitInLispExpr);

		if (!callArgs.Right().Right().IsRealList())
			throwErrorD(og->GetNameStr(), mode == collect_mode::org_rel
			? "org_rel attribute expected as 3rd argument"
			: "attribute expected as 3rd argument"
		);
		const AbstrDataItem* condOrOrgRelA = nullptr;
		SharedStr condOrOrgRelExprStr;
		DataControllerRef condOrOrgRelDC;
		if (callArgs.Right().Right().IsRealList())
		{
			auto condOrOrgRelExpr = callArgs.Right().Right().Left();
			condOrOrgRelExprStr = AsFLispSharedStr(condOrOrgRelExpr, FormattingFlags::NoLimitInLispExpr);

			auto condOrOrgRelCalc = AbstrCalculator::ConstructFromLispRef(resultHolder.GetOld(), condOrOrgRelExpr, CalcRole::Other);
			condOrOrgRelDC = GetDC(condOrOrgRelCalc.get());
			MG_CHECK(condOrOrgRelDC);
			//		condOrOrgRelExpr = condOrOrgRelDC->GetLispRef();
			auto condOrOrgRelItem = condOrOrgRelDC->MakeResult();
			MG_CHECK(condOrOrgRelItem);

			condOrOrgRelA = AsDynamicDataItem(condOrOrgRelItem.get());
		}
		MG_USERCHECK2(condOrOrgRelA,
			mode == collect_mode::org_rel
			? "collect_with_attr_by_org_rel: org_rel data-item expected as 3rd argument"
			: "collect_with_attr_cond: condition data-item expected as 3rd argument"
		);

		const AbstrUnit* sourceDomain = (mode == collect_mode::org_rel) ? condOrOrgRelA->GetAbstrValuesUnit() : condOrOrgRelA->GetAbstrDomainUnit();
		assert(sourceDomain);
		assert(resultHolder);
		if (mode == collect_mode::org_rel)
			MG_USERCHECK2(domainA->UnifyDomain(condOrOrgRelA->GetAbstrDomainUnit()), "collect_with_attr_by_org_rel(attr_container, subset_domain, org_rel): target_domain doesn't match the domain of org_rel");

		std::vector<collect_target> candidates;
		std::set<const TreeItem*> visitedScopes;
		EnumCollectCandidates(attrContainer, scope, SharedStr(), 0, candidates, visitedScopes);

		for (const auto& target : candidates)
		{
			auto subDataItem = target.src;
			if (!sourceDomain->UnifyDomain(subDataItem->GetAbstrDomainUnit()))
			{
				if (mode == collect_mode::org_rel)
					reportF(SeverityTypeID::ST_Warning, "{}: image of org_rel is {}, which is incompatible with the domain of attribute {}, which is {}"
					,	og->GetNameStr()
					,	sourceDomain->GetFullCfgName().c_str()
					,	subDataItem->GetFullName().c_str()
					,	subDataItem->GetAbstrDomainUnit()->GetFullCfgName().c_str()
					);
				else
					reportF(SeverityTypeID::ST_Warning, "{}: domain of condition is {}, which is incompatible with the domain of attribute {}, which is {}"
					,	og->GetNameStr()
					,	sourceDomain->GetFullCfgName().c_str()
					,	subDataItem->GetFullName().c_str()
					,	subDataItem->GetAbstrDomainUnit()->GetFullCfgName().c_str()
					);
				continue;
			}
			auto resSub = CreateDataItemFromPath(resultHolder.GetNew(), target.relPath.c_str(), domainA, subDataItem->GetAbstrValuesUnit(), subDataItem->GetValueComposition());

			// see the note in SelectMetaOperator: at depth 0 this is "..", unchanged.
			auto srcScope = UpDots(target.depth + 1);

			SharedStr collectExpr;
			if (mode == collect_mode::org_rel)
				collectExpr = mySSPrintF("scope({}, lookup({}, {}/{}))"
					, srcScope
					, condOrOrgRelExprStr
					, containerExpr.GetSymbID()
					, target.relPath
				);
			else
				collectExpr = mySSPrintF("scope({}, collect_by_cond({}, {}, {}/{}))"
					, srcScope
					, subsetDomainExprStr
					, condOrOrgRelExprStr
					, containerExpr.GetSymbID()
					, target.relPath
				);

			auto oldExpr = resSub->GetExprMember();
			if (!oldExpr.empty() && oldExpr != collectExpr)
			{
				auto msg = mySSPrintF("Cannot set calculation rule '{}' to collected attribute '{}' as it is already defined as '{}'", collectExpr, target.relPath, oldExpr);
				throwErrorD(og->GetNameID(), msg.c_str());
			}
			resSub->SetExpr(collectExpr);
		}
		resultHolder->SetIsInstantiated();
	}

	void CreateResultCaller(TreeItemDualRef& resultHolder, const ArgRefs& args, LispPtr metaCallArgs) const override
	{
		Run(GetGroup(), resultHolder, m_CollectMode, m_Scope, args, metaCallArgs);
	}
};

// collect_spec(spec, container, subset_domain, org_rel_or_condition): the same
// four axes as the named collect_attr_by_xxx pair, taken from the first argument.
struct CollectSpecOperator : public TernaryOperator
{
	CollectSpecOperator(AbstrOperGroup& cog)
		: TernaryOperator(&cog, TreeItem::GetStaticClass()
		,	DataArray<SharedStr>::GetStaticClass(), TreeItem::GetStaticClass(), AbstrUnit::GetStaticClass())
	{}

	void CreateResultCaller(TreeItemDualRef& resultHolder, const ArgRefs& args, LispPtr metaCallArgs) const override
	{
		auto specItem = GetItem(args[0]);
		MG_USERCHECK2(specItem && IsDataItem(specItem), "collect_spec: a string specification is expected as 1st argument");
		auto cfg = ParseCollectSpec(GetGroup(), GetValue<SharedStr>(AsDataItem(specItem), 0).c_str());
		MG_USERCHECK2(metaCallArgs.IsRealList(), "collect_spec: arguments expected");

		ArgRefs tail(args.begin() + 1, args.end());
		CollectWithAttrOperator::Run(GetGroup(), resultHolder, cfg.mode, cfg.scope, tail, metaCallArgs.Right());
	}
};

// *****************************************************************************
// recollect_by_cond   (cond:    D->B, subset_attr: S->V, fillerValue: ->V) -> (D -> V)
// recollect_by_org_rel(org_rel: S->D, subset_attr: S->V, fillerValue: ->V) -> (D -> V)
// *****************************************************************************

struct AbstrRecollectByCondOperator : BinaryOperator
{
	AbstrRecollectByCondOperator(AbstrOperGroup& aog, ClassCPtr valuesClass)
		: BinaryOperator(&aog, valuesClass, DataArray<Bool>::GetStaticClass(), valuesClass)
	{}

	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		MG_USERCHECK2(args.size() == 2 || args.size() == 3, "recollect_by_cond: 2 or 3 arguments expected");

		const AbstrDataItem* condA = AsDataItem(args[0]);
		const AbstrDataItem* dataA = AsDataItem(args[1]);
		const AbstrDataItem* fillA = nullptr;
		if (args.size() == 3)
		{
			fillA = AsDynamicDataItem(args[2]);
			MG_USERCHECK2(fillA, "recollect_by_cond: third argument is expected to be an attribute or parameter");
			if (dataA->GetValueComposition() != fillA->GetValueComposition())
				reportF(SeverityTypeID::ST_Warning, "{}: value composition {} of the 2nd argument differs from value composition {} of the 3rd argument; the result takes the value composition of the 2nd argument"
					, GetGroup()->GetNameStr()
					, SharedStr(GetValueCompositionID(dataA->GetValueComposition()))
					, SharedStr(GetValueCompositionID(fillA->GetValueComposition())));
			dataA->GetAbstrValuesUnit()->UnifyValues(fillA->GetAbstrValuesUnit(), "v2", "v3", UnifyMode::UM_Throw | UnifyMode::UM_AllowDefaultRight);
			condA->GetAbstrDomainUnit()->UnifyDomain(fillA->GetAbstrDomainUnit(), "e1", "e3", UnifyMode::UM_Throw | UnifyMode::UM_AllowDefaultRight | UnifyMode::UM_AllowVoidRight);
		}

		if (!resultHolder)
			resultHolder = CreateCacheDataItem(condA->GetAbstrDomainUnit(), dataA->GetAbstrValuesUnit(), dataA->GetValueComposition());

		if (dataA->GetTSF(TSF_Categorical) || (fillA && fillA->GetTSF(TSF_Categorical)))
		{
			if (fillA)
				dataA->GetAbstrValuesUnit()->UnifyDomain(fillA->GetAbstrValuesUnit(), "v2", "v3", UnifyMode(UM_AllowDefaultRight | UM_Throw));
			resultHolder->SetTSF(TSF_Categorical);
		}
		if (mustCalc)
		{
			AbstrDataItem* res = debug_cast<AbstrDataItem*>(resultHolder.GetNew());

			DataReadLock arg1Lock(condA);
			DataReadLock arg2Lock(dataA);
			DataReadLock arg3Lock(fillA);
			DataWriteLock resLock(res);

			Calculate(resLock, condA, dataA, fillA);
			resLock.Commit();
		}
		return true;
	}
	virtual void Calculate(DataWriteHandle& res, const AbstrDataItem* condA, const AbstrDataItem* dataA, const AbstrDataItem* fillA) const = 0;
};


template <typename V>
struct RecollectByCondOperator : AbstrRecollectByCondOperator
{
	RecollectByCondOperator(AbstrOperGroup& aog)
		: AbstrRecollectByCondOperator(aog, DataArray<V>::GetStaticClass())
	{}

	void Calculate(DataWriteHandle& resH, const AbstrDataItem* condA, const AbstrDataItem* dataA, const AbstrDataItem* fillA) const override
	{
		const DataArray<Bool>* cond = const_array_cast<Bool>(condA);
		const DataArray<V   >* data = const_array_cast<V>   (dataA);
		const DataArray<V   >* fill = fillA ? const_array_cast<V>(fillA) : nullptr;

		V fillValue = (fillA && fillA->HasVoidDomainGuarantee())
			? fill->GetIndexedValue(0)
			: UNDEFINED_OR_ZERO(V);

		auto res = mutable_array_cast<V>(resH);

		auto valueReader = tile_read_channel<V>(data);

		tile_id tn = condA->GetAbstrDomainUnit()->GetNrTiles();

		for (tile_id t = 0; t != tn; ++t)
		{
			//			ReadableTileLock condLock(cond, t), dataLock(data, t);
			auto boolData = cond->GetTile(t);
			auto resData = res->GetWritableTile(t);
			auto resPtr = resData.begin();

			if (!fillA || fillA->HasVoidDomainGuarantee())
			{
				for (auto boolPtr = boolData.begin(), boolEnd = boolData.end(); boolPtr != boolEnd; ++resPtr, ++boolPtr)
				{
					if (Bool(*boolPtr))
					{
						MG_USERCHECK2(!valueReader.AtEnd(), "recollect_by_cond: number of trues in cond is greater than the number of values in the 2nd arguement. Attributues on select_by_cond with this condition are expected to match the number of elements.");
						*resPtr = *valueReader;
						++valueReader;
					}
					else
						*resPtr = fillValue;
				}
			}
			else
			{
				auto fillData = fill->GetTile(t);
				auto fillPtr = fillData.begin();
				for (auto boolPtr = boolData.begin(), boolEnd = boolData.end(); boolPtr != boolEnd; ++fillPtr, ++resPtr, ++boolPtr)
				{
					if (Bool(*boolPtr))
					{
						MG_USERCHECK2(!valueReader.AtEnd(), "recollect_by_cond: number of trues in cond is greater than the number of values in the 2nd arguement. Attributues on select_by_cond with this condition are expected to match the number of elements.");
						*resPtr = *valueReader;
						++valueReader;
					}
					else
						*resPtr = *fillPtr;
				}

			}
		}
		MG_USERCHECK2(valueReader.AtEnd(), "recollect_by_cond: number of trues in cond is less than the number of values in the 2nd arguement. Attributues on select_by_cond with this condition are expected to match the number of elements.");
	}
};

// *****************************************************************************
//                               INSTANTIATION
// *****************************************************************************

#include "RtcInterface.h"
#include "RtcVersionNumbers.h" // DMS_VERSION_MAJOR, for the v21 removal tripwire on the obsolete `subset` stub
namespace {

	CommonOperGroup cog_select(token::select, oper_policy::dynamic_result_class);
	CommonOperGroup cog_select_08(token::select_uint8);
	CommonOperGroup cog_select_16(token::select_uint16);
	CommonOperGroup cog_select_32(token::select_uint32);
	CommonOperGroup cog_select_64(token::select_uint64);

	CommonOperGroup cog_select_with_org_rel(token::select_with_org_rel, oper_policy::dynamic_result_class);
	CommonOperGroup cog_select_08_with_org_rel(token::select_uint8_with_org_rel);
	CommonOperGroup cog_select_16_with_org_rel(token::select_uint16_with_org_rel);
	CommonOperGroup cog_select_32_with_org_rel(token::select_uint32_with_org_rel);
	CommonOperGroup cog_select_64_with_org_rel(token::select_uint64_with_org_rel);

	// Partly DEPRECATED VARIANTS of select BEGIN
	// The obsolete `subset` stub below is still registered and MUST BE REMOVED IN v21 (issue #1177). The static_assert is the
	// primary guarantee: it fails the BUILD when the major version is bumped, whereas the
	// throw below runs from a STATIC INITIALIZER and would only surface as an opaque
	// STATUS_DLL_INIT_FAILED (0xC0000142) with no message.
	static_assert(DMS_VERSION_MAJOR <= 20,
		"v21: REMOVE the obsolete `subset` operator stub below (GeoDMS issue #1177), "
		"plus token::nrOrgEntity and OrgRelCreationMode::nr_OrgEntity, "
		"rather than bumping the major version with them still registered.");

	oper_policy ObsoleteOperatorsFlag()
	{
		if (DMS_GetMajorVersionNumber() < 20)
			return oper_policy::depreciated;
		if (DMS_GetMajorVersionNumber() <= 20)
			return oper_policy::obsolete;

		throwDmsErrD("This code should be removed in v21"); // also remove token::nrOrgEntity and  OrgRelCreationMode::nr_OrgEntity
	}

	Obsolete<CommonOperGroup> cog_subset_xx("use select_with_org_rel or select and use collect_by_cond for collecting selected attribute values", "subset", oper_policy::dynamic_result_class| ObsoleteOperatorsFlag());
	// Partly DEPRECATED VARIANTS of select END


	oper_arg_policy oap_select_with_attr[2] = { oper_arg_policy::calc_never , oper_arg_policy::calc_as_result };

	SpecialOperGroup cog_select_with_attr_by_cond   (token::select_with_attr_by_cond       , 2, oap_select_with_attr, oper_policy::dynamic_result_class | oper_policy::dont_cache_result);
	SpecialOperGroup cog_select_08_with_attr_by_cond(token::select_uint8_with_attr_by_cond , 2, oap_select_with_attr, oper_policy::dont_cache_result);
	SpecialOperGroup cog_select_16_with_attr_by_cond(token::select_uint16_with_attr_by_cond, 2, oap_select_with_attr, oper_policy::dont_cache_result);
	SpecialOperGroup cog_select_32_with_attr_by_cond(token::select_uint32_with_attr_by_cond, 2, oap_select_with_attr, oper_policy::dont_cache_result);
	SpecialOperGroup cog_select_64_with_attr_by_cond(token::select_uint64_with_attr_by_cond, 2, oap_select_with_attr, oper_policy::dont_cache_result);

	SpecialOperGroup cog_select_with_org_rel_with_attr_by_cond   (token::select_with_org_rel_with_attr_by_cond       , 2, oap_select_with_attr, oper_policy::dynamic_result_class | oper_policy::dont_cache_result);
	SpecialOperGroup cog_select_08_with_org_rel_with_attr_by_cond(token::select_uint8_with_org_rel_with_attr_by_cond , 2, oap_select_with_attr, oper_policy::dont_cache_result);
	SpecialOperGroup cog_select_16_with_org_rel_with_attr_by_cond(token::select_uint16_with_org_rel_with_attr_by_cond, 2, oap_select_with_attr, oper_policy::dont_cache_result);
	SpecialOperGroup cog_select_32_with_org_rel_with_attr_by_cond(token::select_uint32_with_org_rel_with_attr_by_cond, 2, oap_select_with_attr, oper_policy::dont_cache_result);
	SpecialOperGroup cog_select_64_with_org_rel_with_attr_by_cond(token::select_uint64_with_org_rel_with_attr_by_cond, 2, oap_select_with_attr, oper_policy::dont_cache_result);

	SpecialOperGroup cog_select_with_attr_by_org_rel   (token::select_with_attr_by_org_rel       , 2, oap_select_with_attr, oper_policy::dynamic_result_class | oper_policy::dont_cache_result);
	SpecialOperGroup cog_select_08_with_attr_by_org_rel(token::select_uint8_with_attr_by_org_rel , 2, oap_select_with_attr, oper_policy::dont_cache_result);
	SpecialOperGroup cog_select_16_with_attr_by_org_rel(token::select_uint16_with_attr_by_org_rel, 2, oap_select_with_attr, oper_policy::dont_cache_result);
	SpecialOperGroup cog_select_32_with_attr_by_org_rel(token::select_uint32_with_attr_by_org_rel, 2, oap_select_with_attr, oper_policy::dont_cache_result);
	SpecialOperGroup cog_select_64_with_attr_by_org_rel(token::select_uint64_with_attr_by_org_rel, 2, oap_select_with_attr, oper_policy::dont_cache_result);

	SubsetOperator operSXX(cog_select, AbstrUnit::GetStaticClass(), OrgRelCreationMode::none);
	SubsetOperator operS08(cog_select_08, Unit<UInt8>::GetStaticClass(), OrgRelCreationMode::none);
	SubsetOperator operS16(cog_select_16, Unit<UInt16>::GetStaticClass(), OrgRelCreationMode::none);
	SubsetOperator operS32(cog_select_32, Unit<UInt32>::GetStaticClass(), OrgRelCreationMode::none);
	SubsetOperator operS64(cog_select_64, Unit<UInt64>::GetStaticClass(), OrgRelCreationMode::none);

	SubsetOperator operSORXX(cog_select_with_org_rel, AbstrUnit::GetStaticClass(), OrgRelCreationMode::org_rel);
	SubsetOperator operSOR08(cog_select_08_with_org_rel, Unit<UInt8>::GetStaticClass(), OrgRelCreationMode::org_rel);
	SubsetOperator operSOR16(cog_select_16_with_org_rel, Unit<UInt16>::GetStaticClass(), OrgRelCreationMode::org_rel);
	SubsetOperator operSOR32(cog_select_32_with_org_rel, Unit<UInt32>::GetStaticClass(), OrgRelCreationMode::org_rel);
	SubsetOperator operSOR64(cog_select_64_with_org_rel, Unit<UInt64>::GetStaticClass(), OrgRelCreationMode::org_rel);

	// old subset BEGIN
	SubsetOperator operXX(cog_subset_xx, AbstrUnit::GetStaticClass(), OrgRelCreationMode::nr_OrgEntity);
	// old subset END


	SelectMetaOperator operMetaSMxx(cog_select_with_attr_by_cond, AbstrUnit::GetStaticClass(), OrgRelCreationMode::none, token::select);
	SelectMetaOperator operMetaSM08(cog_select_08_with_attr_by_cond, Unit<UInt8>::GetStaticClass(), OrgRelCreationMode::none, token::select_uint8);
	SelectMetaOperator operMetaSM16(cog_select_16_with_attr_by_cond, Unit<UInt16>::GetStaticClass(), OrgRelCreationMode::none, token::select_uint16);
	SelectMetaOperator operMetaSM32(cog_select_32_with_attr_by_cond, Unit<UInt32>::GetStaticClass(), OrgRelCreationMode::none, token::select_uint32);
	SelectMetaOperator operMetaSM64(cog_select_64_with_attr_by_cond, Unit<UInt64>::GetStaticClass(), OrgRelCreationMode::none, token::select_uint64);

	SelectMetaOperator operMetaCAxx(cog_select_with_org_rel_with_attr_by_cond, AbstrUnit::GetStaticClass(), OrgRelCreationMode::org_rel, token::select_with_org_rel);
	SelectMetaOperator operMetaCA08(cog_select_08_with_org_rel_with_attr_by_cond, Unit<UInt8>::GetStaticClass(), OrgRelCreationMode::org_rel, token::select_uint8_with_org_rel);
	SelectMetaOperator operMetaCA16(cog_select_16_with_org_rel_with_attr_by_cond, Unit<UInt16>::GetStaticClass(), OrgRelCreationMode::org_rel, token::select_uint16_with_org_rel);
	SelectMetaOperator operMetaCA32(cog_select_32_with_org_rel_with_attr_by_cond, Unit<UInt32>::GetStaticClass(), OrgRelCreationMode::org_rel, token::select_uint32_with_org_rel);
	SelectMetaOperator operMetaCA64(cog_select_64_with_org_rel_with_attr_by_cond, Unit<UInt64>::GetStaticClass(), OrgRelCreationMode::org_rel, token::select_uint64_with_org_rel);

	SelectMetaOperator operMetaSAxx(cog_select_with_attr_by_org_rel   , AbstrUnit::GetStaticClass(), OrgRelCreationMode::org_rel_and_use_it, token::select_with_org_rel);
	SelectMetaOperator operMetaSA08(cog_select_08_with_attr_by_org_rel, Unit<UInt8>::GetStaticClass(), OrgRelCreationMode::org_rel_and_use_it, token::select_uint8_with_org_rel);
	SelectMetaOperator operMetaSA16(cog_select_16_with_attr_by_org_rel, Unit<UInt16>::GetStaticClass(), OrgRelCreationMode::org_rel_and_use_it, token::select_uint16_with_org_rel);
	SelectMetaOperator operMetaSA32(cog_select_32_with_attr_by_org_rel, Unit<UInt32>::GetStaticClass(), OrgRelCreationMode::org_rel_and_use_it, token::select_uint32_with_org_rel);
	SelectMetaOperator operMetaSA64(cog_select_64_with_attr_by_org_rel, Unit<UInt64>::GetStaticClass(), OrgRelCreationMode::org_rel_and_use_it, token::select_uint64_with_org_rel);

	// #337: the spec form. No new names per scope -- the spec carries the value
	// type, the org_rel choice, whether attributes are collected, and how far.
	SelectSpecOperGroup sog_select_spec;
	SelectMetaOperator      operSelectSpecAttr(sog_select_spec, true);
	SelectSpecNoAttrOperator operSelectSpecPlain(sog_select_spec);


	oper_arg_policy oap_Relate[3] = { oper_arg_policy::calc_never , oper_arg_policy::calc_never, oper_arg_policy::calc_at_subitem };

	SpecialOperGroup sog_collect_attr_by_org_rel(token::collect_attr_by_org_rel, 3, oap_Relate, oper_policy::dont_cache_result);
	SpecialOperGroup sog_collect_attr_by_cond   (token::collect_attr_by_cond   , 3, oap_Relate, oper_policy::dont_cache_result);
	CollectWithAttrOperator operCF(sog_collect_attr_by_org_rel, collect_mode::org_rel);
	CollectWithAttrOperator operCM(sog_collect_attr_by_cond, collect_mode::condition);

	// #337: the spec form of the collect family; see select_spec.
	oper_arg_policy oap_CollectSpec[4] = { oper_arg_policy::calc_always, oper_arg_policy::calc_never, oper_arg_policy::calc_never, oper_arg_policy::calc_at_subitem };
	SpecialOperGroup sog_collect_spec(token::collect_spec, 4, oap_CollectSpec, oper_policy::dont_cache_result);
	CollectSpecOperator operCollectSpec(sog_collect_spec);

	CommonOperGroup cog_collect_by_cond(token::collect_by_cond);
	CommonOperGroup cog_recollect_by_cond(token::recollect_by_cond, oper_policy::allow_extra_args);

	tl_oper::inst_tuple_templ<typelists::value_elements, CollectByCondOperator> collectByCondOperInstances(cog_collect_by_cond);
	tl_oper::inst_tuple_templ<typelists::value_elements, RecollectByCondOperator> recollectByCondOperInstances(cog_recollect_by_cond);
}

