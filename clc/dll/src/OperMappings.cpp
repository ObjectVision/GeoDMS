// Copyright (C) 1998-2024 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "ClcPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif

// Classification/mapping operator TUs, merged (2026-08): ClassBreak, ID,
// SeparableMapping.

// ==== ClassBreak ====

#include "RtcTypeLists.h"
#include "dbg/DebugContext.h"
#include "utl/TypeListOper.h"

#include "TreeItemClass.h"
#include "Unit.h"
#include "UnitClass.h"

#include "CalcClassBreaks.h"
#include "ValueGetter.h"
#include "ValuesTable.h"

#include "DataArray.h"
#include "DataItemClass.h"
#include "OperSignature.h"
#include "TileChannel.h"

struct ClassifyFixedOperator: public BinaryOperator
{
	ClassifyFixedOperator(AbstrOperGroup* og, ClassBreakFunc classBreakFunc, const DataItemClass* dic)
		:	BinaryOperator(og, dic, dic, AbstrUnit::GetStaticClass())
		,	m_ClassBreakFunc(classBreakFunc)
	{}

	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		assert(args.size() == 2);

		const AbstrDataItem* arg1A = debug_cast<const AbstrDataItem*>(args[0]);
		assert(arg1A);
		assert(arg1A->GetDynamicObjClass() == GetResultClass());

		const AbstrUnit* arg1Domain = arg1A->GetAbstrDomainUnit();
		assert(arg1Domain);
		const AbstrUnit* valuesUnit= arg1A->GetAbstrValuesUnit();
		assert(valuesUnit);

		const AbstrUnit* classUnit = AsUnit(args[1]);

		if (!resultHolder)
		{
			resultHolder = CreateCacheDataItem(classUnit, valuesUnit);
			resultHolder->m_StatusFlags.SetHasSortedValues();
		}

		if (mustCalc)
		{
			AbstrDataItem* res = AsDataItem(resultHolder.GetNew());
			DataReadLock  arg1Lock(arg1A);

			auto vcpc = GetCounts<Float64, typelists::num_objects, CountType>(arg1A);

			m_ClassBreakFunc(res, vcpc.first, vcpc.second.get());
		}
		return true;
	}
private:
	ClassBreakFunc m_ClassBreakFunc;
};

static StaticLateTokenID s_CB_Values("Values");
static StaticLateTokenID s_CB_Count("Count");

// *****************************************************************************
// weeded_counts(A: D->V) -> unit U { Values: U->V ; Count: U->UInt64 }
//
// The value-count table every classification starts from, as a result of its own, so that its two
// consumers -- the class count that sizes the palette domain and the classification that fills the
// break attribute -- name the same subexpression, share one DataController and therefore scan the
// argument once (#1248).
//
// The pair count is capped at MAX_PAIR_COUNT, the cap the map view has always applied; above it
// adjacent values are merged. That is a property of the operator, not an argument: no caller
// varies it, and making it settable would make the result depend on a number nobody sets.
// *****************************************************************************

class AbstrWeededCountsOperator : public UnaryOperator
{
public:
	AbstrWeededCountsOperator(AbstrOperGroup& og, ClassCPtr argCls)
		: UnaryOperator(&og, AbstrUnit::GetStaticClass(), argCls)
	{}

	// K6: weeded_counts(values: attribute<V>(D)) -> a FRESH unit U [new] of value class uint32.
	// The two members are a described, complete member set: both are attributes over U, Values
	// carrying the argument's value class and Count a plain uint64 tally.
	bool DescribeSignature(AbstrSignatureBuilder& sb) const override
	{
		auto argCls = dynamic_cast<const DataItemClass*>(GetArgClass(0));
		if (!argCls)
			return false;
		sig_var V = sb.UnitVar("V"), D = sb.UnitVar("D"), U = sb.GeneratedUnit("U"), C = sb.UnitVar("C");
		sb.MemberValueClass(V, argCls->GetValuesType());
		sb.MemberValueClass(U, Unit<UInt32>::GetStaticClass()->GetValueType());
		sb.MemberValueClass(C, Unit<UInt64>::GetStaticClass()->GetValueType());
		sb.ArgName(0, "values");
		sb.ArgAttr(0, V, D, ValueComposition::Single);
		sb.ResultUnit(U);
		sb.ResultContainerMember("Values", V, U, ValueComposition::Single);
		sb.ResultContainerMember("Count", C, U, ValueComposition::Single);
		sb.ResultMembersComplete();
		return true;
	}

	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		assert(args.size() == 1);

		const AbstrDataItem* arg1A = debug_cast<const AbstrDataItem*>(args[0]);
		assert(arg1A);
		const AbstrUnit* arg1Values = arg1A->GetAbstrValuesUnit();

		auto res_owner = Unit<UInt32>::GetStaticClass()->CreateResultUnit(resultHolder.GetNew()); AbstrUnit* res = res_owner.get();
		assert(res);
		resultHolder = res;

		AbstrDataItem* resValues = CreateDataItem(res, s_CB_Values, res, arg1Values).get(); // owned by res
		MG_PRECONDITION(resValues);
		resValues->m_StatusFlags.SetHasSortedValues(); // ascending by construction

		AbstrDataItem* resCount = CreateDataItem(res, s_CB_Count, res, Unit<UInt64>::GetStaticClass()->CreateDefault()).get(); // owned by res
		MG_PRECONDITION(resCount);

		if (mustCalc)
		{
			// GetWeededCounts takes its own DataReadLock and hands back the values-unit range data
			// that converts the Float64 pair values back on write: the same round trip
			// FillBreakAttrFromArray performs for a break attribute.
			auto wc = GetWeededCounts<ClassBreakValueType, typelists::num_objects, CountType>(arg1A, MAX_PAIR_COUNT);
			SizeT n = wc.first.size();

			res->SetCount(n);

			std::vector<Float64> values; values.reserve(n);
			std::vector<UInt64>  counts; counts.reserve(n);
			for (const auto& vcp : wc.first)
			{
				values.push_back(vcp.first);
				counts.push_back(vcp.second);
			}

			DataWriteLock valuesLock(resValues, dms_rw_mode::write_only_all, wc.second.get());
			valuesLock->SetValuesAsFloat64Array(tile_loc(no_tile, 0), n, begin_ptr(values));
			valuesLock.Commit();

			locked_tile_write_channel<UInt64> countWriter(resCount);
			countWriter.Write(counts.begin(), counts.end());
			assert(countWriter.IsEndOfChannel());
			countWriter.Commit();
		}
		return true;
	}
};

template <typename V>
class WeededCountsOperator : public AbstrWeededCountsOperator
{
public:
	WeededCountsOperator(AbstrOperGroup& og)
		: AbstrWeededCountsOperator(og, DataArray<V>::GetStaticClass())
	{}
};

// *****************************************************************************
// Classify*(values: U->W, count: U->UInt64, C: unit) -> (C->W)
//
// The ternary sibling of ClassifyFixedOperator: same groups, same ClassBreakFunc, but the
// value-count table arrives as two attributes instead of being scanned here. That is what lets a
// palette domain's size and its break attribute share one weeded_counts result (#1248).
// *****************************************************************************

struct ClassifyCountsOperator : TernaryOperator
{
	ClassifyCountsOperator(AbstrOperGroup* og, ClassBreakFunc classBreakFunc, const DataItemClass* dic)
		:	TernaryOperator(og, dic
			,	dic                                   // values: U->W, of the result's own class
			,	DataArray<UInt64>::GetStaticClass()   // count : U->UInt64
			,	AbstrUnit::GetStaticClass())          // C     : the class unit
		,	m_ClassBreakFunc(classBreakFunc)
	{}

	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		assert(args.size() == 3);

		const AbstrDataItem* valuesA = debug_cast<const AbstrDataItem*>(args[0]);
		const AbstrDataItem* countA = debug_cast<const AbstrDataItem*>(args[1]);
		assert(valuesA && countA);
		assert(valuesA->GetDynamicObjClass() == GetResultClass());

		const AbstrUnit* valuesUnit = valuesA->GetAbstrValuesUnit();
		assert(valuesUnit);
		const AbstrUnit* classUnit = AsUnit(args[2]);

		if (!resultHolder)
		{
			resultHolder = CreateCacheDataItem(classUnit, valuesUnit);
			resultHolder->m_StatusFlags.SetHasSortedValues();
		}

		if (mustCalc)
		{
			AbstrDataItem* res = AsDataItem(resultHolder.GetNew());

			valuesA->GetAbstrDomainUnit()->UnifyDomain(countA->GetAbstrDomainUnit()
			,	"values of a classification", "counts of a classification", UnifyMode(UM_Throw));

			DataReadLock valuesLock(valuesA);
			DataReadLock countLock(countA);

			SizeT n = valuesA->GetAbstrDomainUnit()->GetCount();

			std::vector<Float64> values(n);
			if (n)
				valuesLock->GetValuesAsFloat64Array(tile_loc(no_tile, 0), n, begin_ptr(values));
			auto countData = const_array_cast<UInt64>(countA)->GetDataRead();

			ValueCountPairContainer vcpc;
			vcpc.reserve(n MG_DEBUG_ALLOCATOR_SRC("ClassifyCountsOperator"));
			for (SizeT i = 0; i != n; ++i)
				vcpc.emplace_back(MG_DEBUG_ALLOCATOR_FIRST("ClassifyCountsOperator") values[i], countData[i]);

			m_ClassBreakFunc(res, vcpc, valuesA->GetCurrRefObj()->GetAbstrValuesRangeData().get());
		}
		return true;
	}

private:
	ClassBreakFunc m_ClassBreakFunc;
};

// *****************************************************************************
//                               INSTANTIATION
// *****************************************************************************

namespace 
{
	CommonOperGroup cog_EqualInterval("ClassifyEqualInterval", oper_policy::dynamic_result_class);
	CommonOperGroup cog_NZEqualInterval("ClassifyNonzeroEqualInterval", oper_policy::dynamic_result_class);
	CommonOperGroup cog_LogInterval("ClassifyLogInterval", oper_policy::dynamic_result_class);
	CommonOperGroup cog_EqualCount("ClassifyEqualCount", oper_policy::dynamic_result_class);
	CommonOperGroup cog_NZEqualCount("ClassifyNonzeroEqualCount", oper_policy::dynamic_result_class);
	CommonOperGroup cog_UniqueValues("ClassifyUniqueValues", oper_policy::dynamic_result_class);
	CommonOperGroup cog_CRJenksFisher("ClassifyJenksFisher", oper_policy::dynamic_result_class);
	CommonOperGroup cog_NZJenksFisher("ClassifyNonzeroJenksFisher", oper_policy::dynamic_result_class);

	// dynamic_result_class, as for cog_unique: the registered result class is the abstract
	// AbstrUnit, so MakeResult has to run before a consumer -- nrofrows(weeded_counts(x)), say --
	// can find a signature for the concrete uint32 unit this produces.
	CommonOperGroup cog_WeededCounts("weeded_counts", oper_policy::dynamic_result_class);

	template <typename V>
	struct ClassBreakOperators
	{

		ClassBreakOperators()
			:	cfoEI(&cog_EqualInterval, ClassifyEqualInterval, DataArray<V>::GetStaticClass())
			,	cfoNZEI(&cog_NZEqualInterval, ClassifyNZEqualInterval, DataArray<V>::GetStaticClass())
			,	cfoLI(&cog_LogInterval,   ClassifyLogInterval  , DataArray<V>::GetStaticClass())
			,	cfoEC(&cog_EqualCount,    ClassifyEqualCount   , DataArray<V>::GetStaticClass())
			,	cfoNZEC(&cog_NZEqualCount, ClassifyNZEqualCount, DataArray<V>::GetStaticClass())
			,	cfoUV(&cog_UniqueValues,  ClassifyUniqueValues , DataArray<V>::GetStaticClass())
			,	cfoNZJF(&cog_NZJenksFisher, ClassifyNZJenksFisher, DataArray<V>::GetStaticClass())
			,   cfoCRJF(&cog_CRJenksFisher, ClassifyCRJenksFisher, DataArray<V>::GetStaticClass())

			,	ccoEI(&cog_EqualInterval, ClassifyEqualInterval, DataArray<V>::GetStaticClass())
			,	ccoNZEI(&cog_NZEqualInterval, ClassifyNZEqualInterval, DataArray<V>::GetStaticClass())
			,	ccoLI(&cog_LogInterval,   ClassifyLogInterval  , DataArray<V>::GetStaticClass())
			,	ccoEC(&cog_EqualCount,    ClassifyEqualCount   , DataArray<V>::GetStaticClass())
			,	ccoNZEC(&cog_NZEqualCount, ClassifyNZEqualCount, DataArray<V>::GetStaticClass())
			,	ccoUV(&cog_UniqueValues,  ClassifyUniqueValues , DataArray<V>::GetStaticClass())
			,	ccoNZJF(&cog_NZJenksFisher, ClassifyNZJenksFisher, DataArray<V>::GetStaticClass())
			,   ccoCRJF(&cog_CRJenksFisher, ClassifyCRJenksFisher, DataArray<V>::GetStaticClass())

			,	wco(cog_WeededCounts)
		{}

		ClassifyFixedOperator cfoEI, cfoNZEI;
		ClassifyFixedOperator cfoLI;
		ClassifyFixedOperator cfoEC, cfoNZEC;
		ClassifyFixedOperator cfoUV;
		ClassifyFixedOperator cfoNZJF, cfoCRJF;

		ClassifyCountsOperator ccoEI, ccoNZEI;
		ClassifyCountsOperator ccoLI;
		ClassifyCountsOperator ccoEC, ccoNZEC;
		ClassifyCountsOperator ccoUV;
		ClassifyCountsOperator ccoNZJF, ccoCRJF;

		WeededCountsOperator<V> wco;
	};

	tl_oper::inst_tuple_templ<typelists::num_objects, ClassBreakOperators> classBreakInstances;
} // end anonymous namespace

/******************************************************************************/



// ==== ID ====

#include "vt/CheckedCalc.h"
#include "mci/CompositeCast.h"
#include "ptr/InterestHolders.h"

#include "DataArray.h"
#include "DataItemClass.h"
#include "LispTreeType.h"
#include "ParallelTiles.h"
#include "Unit.h"
#include "UnitClass.h"
#include "UnitProcessor.h"
#include "TileFunctorImpl.h"

// *****************************************************************************
//                         UNARY RELATIONAL FUNCTIONS
// *****************************************************************************
// Generate_key(E1->Bool): E1->E2 x (E2) NOT!
// ID(E1)          : E1->E1
// Subset(E1->Bool): E2 x (E2->E1)
// Unique
// Relate

// *****************************************************************************
//                         ID
// *****************************************************************************

CommonOperGroup cog_id(token::id);

class AbstrIDOperator : public UnaryOperator
{
public:
	// Override Operator
	AbstrIDOperator(const Class* resultClass, const UnitClass* arg1Class)
		: UnaryOperator(&cog_id, resultClass, arg1Class) 
	{}

	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		assert(args.size() == 1);

		const AbstrUnit* e1 = AsUnit(args[0]);
		assert(e1);

		if (!resultHolder)
		{
			resultHolder = CreateCacheDataItem(e1, e1);
			resultHolder.GetNew()->SetFreeDataState(true); // never cache
			resultHolder->SetTSF(TSF_Categorical);
		}
		if (mustCalc)
		{
			AbstrDataItem* res = AsDataItem(resultHolder.GetNew());

			const AbstrIDOperator* idOper = this;
			auto trd = e1->GetTiledRangeData();
			MG_CHECK(trd);
			resultHolder->m_StatusFlags.SetHasSortedValues(trd->HasSortedValues());

			auto lazyFunctorCreator = [idOper, res, trd]<typename V>(const Unit<V>*domainUnit) {
				auto lazyTileFunctor = make_unique_LazyTileFunctor<V>(make_shared_tree(res, existing_obj{}), trd.get(), domainUnit->m_RangeDataPtr
				,	[idOper, res, trd](AbstrDataObject* self, tile_id t) {
						idOper->Calculate(self, trd.get(), t); // write into the same tile.
					}
					MG_DEBUG_ALLOCATOR_SRC(res->md_FullName +  ": = id()")
				);
				res->m_DataObject = lazyTileFunctor.release();
			};

			e1 = AsUnit(e1->GetCurrRangeItem()).get();
			visit<typelists::domain_elements>(e1, std::move(lazyFunctorCreator));
		}
		return true;
	}
	virtual void Calculate(AbstrDataObject* borrowedDataHandle, const AbstrTileRangeData* anyRange, tile_id t) const =0;
};

template <class E1>
class IDOperator : public AbstrIDOperator
{
	typedef Unit<E1>       Arg1Type;
	typedef DataArray<E1>  ResultType;

public:
	// Override Operator
	IDOperator() : AbstrIDOperator(ResultType::GetStaticClass(), Arg1Type::GetStaticClass()) 
	{}

	void Calculate(AbstrDataObject* borrowedDataHandle, const AbstrTileRangeData* tileRanges, tile_id t) const override
	{
		ResultType* result = mutable_array_cast<E1>(borrowedDataHandle);
		assert(result);

		auto resData = result->GetWritableTile(t, dms_rw_mode::write_only_all);
		auto resRange = debug_cast<const typename Unit<E1>::range_data_t*>(tileRanges)->GetTileRange(t);
		CalcTile(resData, resRange MG_DEBUG_ALLOCATOR_SRC(borrowedDataHandle->md_SrcStr.c_str()));
	}

	void CalcTile(sequence_traits<E1>::seq_t resData, Unit<E1>::range_t resRange MG_DEBUG_ALLOCATOR_SRC_ARG) const
	{
		auto
			i = resData.begin(),
			e = resData.end();

		assert(SizeT(e-i) == Cardinality(resRange));

		for (row_id count = 0; i != e; ++i)
			*i = Range_GetValue_naked(resRange, count++);
	}
};


// *****************************************************************************
//                               INSTANTIATION
// *****************************************************************************

#include "RtcTypeLists.h"
#include "utl/TypeListOper.h"

namespace 
{
	tl_oper::inst_tuple_templ<typelists::domain_elements, IDOperator > 
		operInstances;
} // end anonymous namespace




// ==== SeparableMapping ====

#include "OperConv.h"

#include <string_view>

namespace {

// *****************************************************************************
//			PROJ.4 definition scanning
// *****************************************************************************

// Calls f(key, value) for each whitespace-separated "+key=value" token of a PROJ.4 definition
// string. A bare "+key" yields an empty value; anything not starting with '+' is skipped.
template <typename Func>
void ScanProjParams(std::string_view def, Func&& f)
{
	size_t i = 0, e = def.size();
	while (i != e)
	{
		while (i != e && std::isspace(static_cast<unsigned char>(def[i])))
			++i;
		size_t b = i;
		while (i != e && !std::isspace(static_cast<unsigned char>(def[i])))
			++i;
		if (b == i)
			break;

		auto token = def.substr(b, i - b);
		if (token.front() != '+')
			continue;
		token.remove_prefix(1);

		auto eqPos = token.find('=');
		if (eqPos == std::string_view::npos)
			f(token, std::string_view{});
		else
			f(token.substr(0, eqPos), token.substr(eqPos + 1));
	}
}

// *****************************************************************************
//			The whitelist
// *****************************************************************************

// Geographic and NORMAL-ASPECT CYLINDRICAL projections: their easting formula reads only the
// longitude and their northing formula only the latitude, so the image of a rectangular grid is
// the product of the images of its two coordinate ranges. That is precisely the property #298
// exploits.
//
// Deliberately a whitelist, not a blacklist: anything unlisted falls back to the generic
// per-point loop, which is correct but slow. Adding a method here is a correctness claim about
// its formulas -- do not add one without checking that neither ordinate reads the other.
//
// Notably NOT separable, and easy to mistake for separable:
//   tmerc / utm / etmerc  - transverse aspect: easting depends on the latitude too
//   omerc                 - oblique aspect
//   lcc / aea / laea      - conic / azimuthal: both ordinates read both inputs
//   stere / sterea        - azimuthal
//   moll / sinu / robin   - pseudo-cylindrical: the parallels' length varies, so x reads phi
bool IsSeparableProjMethod(std::string_view method)
{
	return method == "longlat"   // geographic, any axis order or angular unit
		|| method == "latlong"   // PROJ's alias for the same
		|| method == "merc"      // Mercator 1SP / 2SP, incl. EPSG:3395
		|| method == "webmerc"   // Pseudo-Mercator, EPSG:3857
		|| method == "eqc"       // Equirectangular / Plate Carree
		|| method == "mill"      // Miller cylindrical
		|| method == "cea";      // Cylindrical equal area
}

// Whether a single CRS, as PROJ.4 sees it, is one of the separable families.
// +lon_0 +lat_ts +lat_0 +k_0 +x_0 +y_0 +units= +a= +b= are per-axis constants and are fine.
bool ProjDefIsSeparable(std::string_view def)
{
	bool sawProj = false, methodIsSeparable = false, rest = true;

	ScanProjParams(def, [&](std::string_view key, std::string_view value)
		{
			if (key == "proj")
			{
				sawProj = true;
				methodIsSeparable = IsSeparableProjMethod(value);
			}
			else if (key == "alpha" || key == "gamma")
				rest = false; // an oblique parameterisation of an otherwise separable method
			else if (key == "nadgrids")
			{
				// EPSG:3857 exports with "+nadgrids=@null", the explicit no-op grid -- rejecting
				// every +nadgrids would kill the very case #298 targets. Any real grid file is a
				// per-point datum shift and is not separable.
				rest = rest && (value == "@null");
			}
			else if (key == "geoidgrids")
				rest = false; // a vertical component, i.e. not a plain 2D map
		}
	);

	return sawProj && methodIsSeparable && rest;
}

// Reads the PROJ.4 form of srs and classifies it. Returns false when GDAL cannot express the
// CRS that way, which is itself a reason to refuse the fast path.
bool SpatialRefIsSeparable(const OGRSpatialReference& srs)
{
	CplString buffer; // frees m_Text with CPLFree
	if (srs.exportToProj4(&buffer.m_Text) != OGRERR_NONE || !buffer.m_Text)
		return false;
	return ProjDefIsSeparable(std::string_view(buffer.m_Text));
}

bool SpatialRefsAreAxisSeparable(const OGRSpatialReference& src, const OGRSpatialReference& dst)
{
	// No datum change. This is the load-bearing condition, not a nicety: a datum shift is a
	// Helmert or grid-shift step that routes through geocentric XYZ, where every output ordinate
	// reads every input ordinate. It also rules out GDAL selecting among candidate operations
	// per point by area of use.
	if (!src.IsSameGeogCS(&dst))
		return false;

	return SpatialRefIsSeparable(src) && SpatialRefIsSeparable(dst);
}

} // anonymous namespace

// *****************************************************************************
//			SpatialRefBlock::IsAxisSeparableCrsPair
// *****************************************************************************

bool SpatialRefBlock::IsAxisSeparableCrsPair() const
{
	if (m_IsAxisSeparable.has_value())
		return *m_IsAxisSeparable;

	bool result = false;
	{
		// A CRS that GDAL cannot cleanly express in PROJ.4 form makes noise we do not want in the
		// event log: this is an optimization probe, and its only consequence is which of two
		// equivalent code paths runs. Swallow whatever came up and answer "not separable".
		GDAL_ErrorFrame frame;
		result = SpatialRefsAreAxisSeparable(m_Src, m_Dst);
		if (frame.HasError())
		{
			frame.ReleaseError();
			result = false;
		}
	}

	m_IsAxisSeparable = result;
	return result;
}
