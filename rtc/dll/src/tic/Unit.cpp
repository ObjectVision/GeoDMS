#include "TicPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

#include "Unit.h"

#include "act/UpdateMark.h"
#include "dbg/DmsCatch.h"
#include "vt/Conversions.h"
#include "vt/RangeIndex.h"
#include "vt/StringBounds.h"
#include "mci/ValueWrap.h"
#include "ptr/InterestHolders.h"
#include "ser/AsString.h"
#include "ser/MoreStreamBuff.h"
#include "ser/PointStream.h"
#include "ser/RangeStream.h"
#include "ser/SequenceArrayStream.h"
#include "utl/IncrementalLock.h"
#include "utl/FixedBufferFormat.h"

#include "AbstrCalculator.h"
#include "AbstrDataItem.h"
#include "DataController.h"
#include "DataLocks.h"
#include "Crs.h"
#include "Metric.h"
#include "Projection.h"
#include "PropFuncs.h"
#include "TiledRangeData.h"
#include "TicInterface.h"
#include "TiledRangeDataImpl.h"
#include "TiledUnit.h"
#include "TreeItemClass.h"
#include "TreeItemContextHandle.h"
#include "TreeItemProps.h"
#include "UnitProcessor.h"

//----------------------------------------------------------------------
// Compile time polymorphic helper functions
//----------------------------------------------------------------------
std::mutex sc_RangeDataPtrAccess;

//----------------------------------------------------------------------
// Domain Change Context
//----------------------------------------------------------------------

thread_local domain_change_context* s_CurrDomainChangeContext = nullptr;
domain_change_context::domain_change_context(row_id changePos_)
	: prevContext(s_CurrDomainChangeContext)
	, changePos(changePos_)
{
	s_CurrDomainChangeContext = this;
}

domain_change_context::~domain_change_context()
{
	s_CurrDomainChangeContext = prevContext;
}

auto domain_change_context::GetCurrContext()->domain_change_context*
{
	return s_CurrDomainChangeContext;
}

template <typename RD>
auto GetRangeDataAsLispRef(const RD& rd, bool asCategorical, LispPtr base) -> LispRef
{
	if (!rd)
		return base;
	return rd->GetAsLispRef(base, asCategorical);
}

static auto GetRangeDataAsLispRef(Void rd, bool asCategorical, LispPtr base) -> LispRef
{
	return base;
}

//----------------------------------------------------------------------
// Unit<V> member funcs: key expression
//----------------------------------------------------------------------

template <class V>
LispRef Unit<V>::GetKeyExprImpl() const
{
	LispRef result;
	if (!IsDefaultUnit()) // || IsCacheRoot())
	{
		result = AbstrUnit::GetKeyExprImpl();
	}
	bool hasCalcRule = !result.EndP();

#if defined(MG_DEBUG)
	auto resultStr = AsString(result.AsLispPtr());
#endif

	// present as metric-less BaseUnit with a unique keyExpr.
	if (result.EndP() && (!IsLoadable() || GetTSF(USF_HasConfigRange)))
	{
		result = ExprList(GetValueType()->GetID());
		//	if constexpr (is_integral_v<V> && has_var_range_v<V>) // could be domain or projection base; enforce [expr(x) == expr(y)] => [fullname(x) == fullname(y)];
		if constexpr (has_var_range_v<V>) // could be domain or projection base; enforce [expr(x) == expr(y)] => [fullname(x) == fullname(y)];
		{
			if (!IsDefaultUnit())
			{
				auto sr = GetSpatialReference();
				if (sr)
				{
					// A unit WITH a spatial reference gets its identity from that reference:
					//     (CrsUnit "EPSG:28992" (fpoint))
					// Two units declaring the same CRS are the same type, which is what makes
					// them unify -- and, unlike the encoding this replaces, their background
					// layer plays no part in it.
					//
					// Until Stage 7 this wrapped an inner
					//     (BaseUnit "EPSG:28992\xFF<DialogData>" (fpoint))
					// whose base-unit SYMBOL carried the CRS and the background hint joined by
					// a 0xFF byte, and whose "metric" was therefore a CRS identity tag rather
					// than a dimension. That is gone: the CRS is a first-class unit property
					// (tic/Crs.h) and CrsUnit derives a real linear metric for projected
					// systems, so area(geom, m2) no longer squares an identity tag. It also
					// makes GeoDMS logs valid UTF-8, since 0xFF is not a legal UTF-8 byte and
					// this symbol leaked into every rendered metric.
					// See doc/development/crs-metric-decoupling.md.

					// The background layer is remembered OUT OF BAND, under the CRS: a cache
					// unit has no DialogData of its own, and the hint deliberately does not
					// belong to the key above because it is presentation, not type.
					RegisterCrsBackgroundRef(sr, TreeItem_GetDialogData(this));

					// Materialize the token BEFORE building the LispRef: a TokenID str-range
					// temporary keeps the token-registry lock held for its lifetime, and
					// spanning a parse-capable call with it self-deadlocks at ~0 CPU.
					SharedStr srStr(sr);
					result = ExprList(token::CrsUnit
						, LispRef(srStr.begin(), srStr.send())
						, result
					);
				}
				else
				{
					// BaseUnit( Left('%FullName%', 0), result ) provides a unique domain and projection identity with compatible metric
					auto fullName = this->GetFullName();
					result = ExprList(token::BaseUnit
						, ExprList(token::left
							, LispRef(fullName.begin(), fullName.send())
							, ExprList(token::UInt32, LispRef(Number(0.0)))
						)
						, result
					);
				}

#if defined(MG_DEBUG)
				auto resultStr2 = AsString(result.AsLispPtr());
#endif
			}
		}
	}

#if defined(MG_DEBUG_LISP_TREE)
	reportF(SeverityTypeID::ST_MinorTrace, "AbstrUnit::GetAsLispRef -> {}", AsString(result.AsLispPtr()).c_str());
	dms_assert(IsExpr(result));
#endif
	// A unit that declares a SpatialReference AND has a calculation rule must produce a unit that
	// carries that CRS. Its rule (baseunit('m', fpoint)) yields a metric only, and asking the result
	// for its CRS then reaches that CRS-less unit, which is how a written shapefile lost its .prj.
	// Wrapping the rule the way the no-rule case above wraps the value type puts the CRS in the
	// identity, so the DataController result is a unit<V> with it; see the CrsUnit operator.
	// GetLocalCrs, not GetSpatialReference: only a CRS declared HERE belongs in this expression; one
	// reached through the rule is already part of the term that rule contributed.
	if constexpr (has_var_range_v<V>)
	{
		if (hasCalcRule && !IsEmpty(GetLocalCrs()))
		{
			// Materialize the token BEFORE building the LispRef, as the CRS branch above explains.
			SharedStr srStr(GetLocalCrs()->m_SpatialRef);
			result = ExprList(token::CrsUnit
				, LispRef(srStr.begin(), srStr.send())
				, result
			);
		}
	}

	// add range or tile spec to keyExpr
	if (GetTSF(USF_HasConfigRange))
		result = GetRangeDataAsLispRef(m_RangeDataPtr, GetTSF(TSF_Categorical), result); // enforce [expr(x) == expr(y)] => [range(x) == range(y)];

#if defined(MG_DEBUG_LISP_TREE)
	reportF(SeverityTypeID::ST_MinorTrace, "-> {}", AsString(result.AsLispPtr()).c_str());
	dms_assert(IsExpr(result));
#endif

	return result;
}

//----------------------------------------------------------------------
// Unit<V> member funcs: range data upkeep
//----------------------------------------------------------------------

template <class V>
void  Unit<V>::ClearDataObject(garbage_can& g) const
{
	if constexpr (ranged_unit_v<V>)
	{
		g |= std::move(const_cast<Unit<V>*>(this)->m_RangeDataPtr);
		dms_assert(!this->HasTiledRangeData());
	}
	else
		AbstrUnit::ClearDataObject(g);
}

template <class V>
void Unit<V>::ValidateRange (const range_t& range) const requires ranged_unit_v<V>
{
	auto currRange = GetRange();
	if (range != currRange)
	{
		const ValueClass* cls = this->GetValueType();
		dms_assert(cls);
		throwItemErrorF("ValidateRange({}) failed because current range is {}"
		,	AsString(range).c_str()
		,	AsString(currRange).c_str()
		);
	}
}

//	Override TreeItem virtuals
template <typename V>
void Unit<V>::CopyProps(TreeItem* result, const CopyTreeContext& copyContext) const
{
	AbstrUnit::CopyProps(result, copyContext);
	if constexpr (ranged_unit_v<V>)
	{
		auto resultUnit = debug_cast<Unit*>(result);
		resultUnit->m_Metric       = this->m_Metric;
		resultUnit->m_RangeDataPtr = this->m_RangeDataPtr;
		if constexpr (geo_unit_v<V>)
			resultUnit->m_Projection = this->m_Projection;
	}
}

struct hash_buffer: OutStreamBuff
{
	hash_code currValue = 0;
	void WriteBytes(const Byte* data, streamsize_t size) override
	{
		while (size >= sizeof(hash_code))
		{
			(currValue <<= 3) ^= *reinterpret_cast<const hash_code*>(data);
			data += sizeof(hash_code);
			size -= sizeof(hash_code);
		}
		while (size)
		{
			(currValue <<= 3) ^= *data++;
			--size;
		}
	}
	bool AtEnd() const override { return false; }
	streamsize_t CurrPos() const override { return 0; }
};

//-------------------------------

hash_code AbstrTileRangeData::GetHashCode() const
{
	hash_buffer hashBuff;
	BinaryOutStream hasher(&hashBuff);

	Save(hasher);
	return hashBuff.currValue;
}

row_id AbstrTileRangeData::GetElemCount() const
{
	if (IsCovered())
		return GetRangeSize();
	row_id result = 0;
	for (tile_id t = 0, tn = GetNrTiles(); t != tn; ++t)
		result += GetTileSize(t);
	return result;
}

//-------------------------------

template <NumericValue V>
auto TileStart(const Range<V>& range, tile_offset size, tile_offset nr_tiles, tile_id t) -> V
{
	return range.first + row_id(size) * t;
}

template <NumericValue V>
auto TileStart(const Range<Point<V>>& range, tile_extent_t<Point<V>> tileExtent, tile_extent_t<Point<V>> tilingExtent, tile_id t) -> Point<V>
{
	return range.first + Point<V>(Range_GetValue_naked_zbase(tilingExtent, t)) * Point<V>(tileExtent);
}

template <typename Base>
auto RegularAdapter<Base>::GetTileRange(tile_id t) const -> Range<value_type>
{
	MG_CHECK(t < this->GetNrTiles());

	value_type tileTL = TileStart(this->m_Range, this->tile_extent(), this->tiling_extent(), t);
	value_type tile_extent = value_type(this->tile_extent());
	value_type rangeEnd = this->m_Range.second;
	value_type tileBR =
		UpperBound(
			  LowerBound(tileTL, rangeEnd - tile_extent) + tile_extent
			, LowerBound(tileTL + tile_extent, rangeEnd)
		);
	MG_CHECK(IsStrictlyLower(tileTL, tileBR)); // follows from above calculations and asserts

	auto r = Range<value_type>(tileTL, tileBR);
	MG_CHECK(!r.empty());
	r &= this->m_Range;
	MG_CHECK(!r.empty());
	return r;
}

template <typename Base>
void RegularAdapter<Base>::CalcTilingExtent()
{
	if (this->m_Range.empty())
		m_TilingExtent = value_type();
	else
		m_TilingExtent = CeilDivide(unsigned_type_t<value_type>(this->m_Range.second - this->m_Range.first), this->tile_extent());
	// NB: this runs under sc_RangeDataPtrAccess (RangeData ctor) -> do NOT reportF here (lock-order deadlock with the log mutex)
}

template <typename Base>
tile_loc RegularAdapter<Base>::GetTiledLocationForValue(value_type v) const
{
	assert(IsIncluding(this->m_Range, v));

	tile_id t = Range_GetIndex_naked_zbase(this->tiling_extent(), tile_extent_t<value_type>((v - this->m_Range.first) / this->tile_extent()));
	return tile_loc(t, Range_GetIndex_naked(GetTileRange(t), v));
}

template <typename Base>
tile_loc RegularAdapter<Base>::GetTiledLocation(row_id index) const
{
	if (index >= Cardinality(this->m_Range))
		return tile_loc{UNDEFINED_VALUE(tile_id), UNDEFINED_VALUE(tile_offset)};

	auto v = Range_GetValue_naked(this->m_Range, index);
	return GetTiledLocationForValue(v);
}

template <typename Base>
tile_loc RegularAdapter<Base>::GetTileDataLocation(row_id dataIndex) const
{
	if constexpr (is_numeric_v<value_type>)
		return GetTiledLocation(dataIndex);
	else
	{
		assert(dataIndex < Cardinality(this->m_Range));

		auto tileSize = this->GetTileSize(0);
		auto tilingExtent = this->GetTilingExtent();

		assert(tilingExtent.Row() >= 1);
		assert(tilingExtent.Col() >= 1);
		auto lastCol = tilingExtent.Col() - 1;
		auto stripSize = tileSize * lastCol + this->GetTileSize(lastCol);
		auto tileRow = dataIndex / stripSize;
		auto tileCol = dataIndex % stripSize;
		assert(tileRow < tilingExtent.Row());
		if (tileRow + 1 == tilingExtent.Row())
			tileSize = this->GetTileSize(tileRow * tilingExtent.Col());
		return tile_loc(tileRow * tilingExtent.Col() + tileCol / tileSize, tileCol % tileSize);
	}
}

template <typename Base>
datarow_id RegularAdapter<Base>::GetTileDataRow(tile_loc tileLoc) const
{
	if constexpr (is_numeric_v<value_type>)
		return this->GetRowIndex(tileLoc.first, tileLoc.second);
	else
	{
		auto tileSize = this->GetTileSize(0);
		auto tilingExtent = this->GetTilingExtent();

		assert(tilingExtent.Row() >= 1);
		assert(tilingExtent.Col() >= 1);
		auto lastCol = tilingExtent.Col() - 1;
		auto stripSize = tileSize * lastCol + this->GetTileSize(lastCol);

		auto tileRow = tileLoc.first / tilingExtent.Col();
		auto tileCol = tileLoc.first % tilingExtent.Col();
		assert(tileRow < tilingExtent.Row());
		if (tileRow + 1 == tilingExtent.Row())
			tileSize = this->GetTileSize(tileRow * tilingExtent.Col());
		return stripSize * tileRow + tileSize * tileCol + tileLoc.second;
	}
}

template <typename Base>
tile_id RegularAdapter<Base>::GetNrTiles() const
{
	return Cardinality(this->tiling_extent());
}

//-------------------------------LispRef	LispRef GetAsLispRef(LispRef base) const

template <typename V>
auto SimpleRangeData<V>::GetAsLispRef(LispPtr base, bool asCategorical) const -> LispRef
{
	return AsLispRef(m_Range, base, asCategorical);
}

template <typename V>
auto SmallRangeData<V>::GetAsLispRef(LispPtr base, bool asCategorical) const -> LispRef
{
	return AsLispRef(m_Range, base, asCategorical);
}

//-------------------------------

template <typename V>
void SimpleRangeData<V>::Load(BinaryInpStream& pis)
{
	pis >> m_Range;
}

template <typename V>
void SmallRangeData<V>::Load(BinaryInpStream& pis)
{
	pis >> m_Range;
}

template <typename V>
void TiledRangeData<V>::Load(BinaryInpStream& pis)
{
	pis >> m_Range;
}

template <typename V>
void RegularTileRangeDataBase<V>::Load(BinaryInpStream& pis)
{
	TiledRangeData<V>::Load(pis);
	pis >> m_TileExtent;
}

template <typename V>
void IrregularTileRangeData<V>::Load(BinaryInpStream& pis)
{
	TiledRangeData<V>::Load(pis);
	pis >> m_Ranges;
/* REMOVE
//	tile_id nrTiles; pis >> nrTiles;
//	m_Ranges.resize(nrTiles);
//	while (nrTiles--)
//		pis >> m_SegmPtr->m_Ranges[nrTiles];
*/
}

template <typename V>
void SimpleRangeData<V>::Save(BinaryOutStream& pos) const
{
	pos << m_Range;
}

template <typename V>
void SmallRangeData<V>::Save(BinaryOutStream& pos) const
{
	pos << m_Range;
}

template <typename V>
void TiledRangeData<V>::Save(BinaryOutStream& pos) const
{
	pos << m_Range;
}

template <typename V>
void RegularTileRangeDataBase<V>::Save(BinaryOutStream& pos) const
{
	TiledRangeData<V>::Save(pos);
	pos << m_TileExtent;
}

template <typename V>
void IrregularTileRangeData<V>::Save(BinaryOutStream& pos) const
{
	TiledRangeData<V>::Save(pos);
	pos << m_Ranges;
}

//-------------------------------

template <typename V>
void Unit<V>::LoadBlobStream(const InpStreamBuff* is)
{
	if constexpr (ranged_unit_v<V>)
	{
		BinaryInpStream bis(is);
		LoadRangeImpl(bis);

	//	this->SetDataInMem();
	}
	else
		AbstrUnit::LoadBlobStream(is);
}

// Recognize Default and Regular tilings and construct them if possible.

template <typename V>
bool IsRegular(const auto& range, const auto& tileRanges)
{
	if (tileRanges.size() < 2)
		return false; // caught by earlier checks or range != tileRanges[0]

	auto firstTileExtents = Size(tileRanges[0]);
	RegularTileRangeData<V> exemplar(range, firstTileExtents); exemplar.Abandon();
	if (exemplar.GetNrTiles() != tileRanges.size())
		return false;

	tile_id t = 0;
	for (const auto& tr : tileRanges)
		if (tr != exemplar.GetTileRange(t++))
			return false;

	return true;
}

template<typename V>
auto CreateRegularTileRangeData(const auto& range, const auto& tileRange) -> SharedPtr<TiledRangeData<V>>
{
	auto tileRangeAsWPoint = Convert<tile_extent_t<V>>(tileRange);
	if (tileRangeAsWPoint == default_tile_size<V>())
		return new DefaultTileRangeData<V>(range);
	return new RegularTileRangeData<V>(range, tileRangeAsWPoint);
}

template <typename V>
void Unit<V>::LoadRangeImpl(BinaryInpStream& pis)
{
	if constexpr (simple_range_unit_v<V>)
	{
		Range<V> range;
		tile_id tn;
		pis >> range >> tn;
		auto lock = std::lock_guard(sc_RangeDataPtrAccess);
		if (range.empty() || tn == 0)
			this->m_RangeDataPtr.reset();
		else
			this->m_RangeDataPtr.reset(new SimpleRangeData<V>());
	}
	else if constexpr (countable_unit_v<V>)
	{
		Range<V> range;
		tile_id tn;
		pis >> range >> tn;

		auto lock = std::lock_guard(sc_RangeDataPtrAccess);
		if constexpr (has_small_range_v<V>)
		{
			MG_CHECK(tn == no_tile);
			if (range.empty())
				this->m_RangeDataPtr = nullptr;
			else
				this->m_RangeDataPtr = new SmallRangeData<V>(range);
		}
		else
		{
			if (tn == 0)
				this->m_RangeDataPtr = new DefaultTileRangeData<V>(range);
			else if (tn == no_tile) // assume one big old tile (as in 7.xxx)
			{
				auto tileExtents = Size(range);
				this->m_RangeDataPtr = CreateRegularTileRangeData<V>(range, tileExtents);
			}
			else
			{
				std::vector<Range<V>> tileRanges(tn);
				tile_id ti = tn; while (ti) // WARNING: REVERSE ORDER
					pis >> tileRanges[--ti];
				if ((tn == 1 && range == tileRanges[0]) || IsRegular<V>(range, tileRanges))
				{
					auto firstTileExtents = Size(tileRanges[0]);
					this->m_RangeDataPtr = CreateRegularTileRangeData<V>(range, firstTileExtents);
				}
				else
				{
					this->m_RangeDataPtr = new IrregularTileRangeData<V>(std::move(tileRanges));
					for (tile_id t = 0; t != tn; ++t)
					{
						MG_CHECK(IsIncluding(range, this->m_RangeDataPtr->GetTileRange(t)));
					}
				}
			}
		}
	}
}

template <typename V>
void Unit<V>::StoreBlobStream(OutStreamBuff* os) const
{
	if constexpr (ranged_unit_v<V>)
	{
		BinaryOutStream bos(os);
		this->StoreRangeImpl(bos);
	}
	else
		AbstrUnit::StoreBlobStream(os);
}

template <typename V>
void Unit<V>::StoreRangeImpl(BinaryOutStream& pos) const
{
	if constexpr (simple_range_unit_v<V>)
	{
		auto lock = std::lock_guard(sc_RangeDataPtrAccess);
		if (this->m_RangeDataPtr)
			pos << this->m_RangeDataPtr->GetRange();
		else
			pos << Range<V>(V(), V());
	}
	else if constexpr (countable_unit_v<V>)
	{
		auto lock = std::lock_guard(sc_RangeDataPtrAccess);
		MG_CHECK(this->m_RangeDataPtr);
		tile_id tn = this->m_RangeDataPtr->GetNrTiles();
		if (tn == 1)
		{
			pos << this->m_RangeDataPtr->GetTileRange(0) << tile_id(no_tile);
			return;
		}
		pos << this->m_RangeDataPtr->GetRange() << tn;
		while (tn)
			pos << this->m_RangeDataPtr->GetTileRange(--tn);
	}
}

template <typename V>
bool Unit<V>::ContainsUndefined(tile_id t) const
{
	if constexpr (countable_unit_v<V>)
	{
		auto lock = std::lock_guard(sc_RangeDataPtrAccess);
		assert(this->m_RangeDataPtr); // PRECONDITION: IsCurrTiled()
		return :: ContainsUndefined(this->m_RangeDataPtr->GetTileRange(t));
	}
	else
		return AbstrUnit::ContainsUndefined(t); // throws
}

template <class V>
auto Unit<V>::GetPreparedRange() const -> range_t requires ranged_unit_v<V>
{
	dbg_assert(this->CheckMetaInfoReadyOrPassor() || this->HasConfigData());

	dms_assert(!this->InTemplate() || this->GetTSF(TSF_HasConfigData)); // PRECONDITION?

	MG_CHECK(IsMetaThread()); // DEBUG

	dms_check_not_debugonly;
	dms_assert(!this->m_State.IsInTrans() || (this->m_State.GetTransState() > actor_flag_set::AF_CalculatingData) );

	bool result = this->PrepareDataUsage(DrlType::CertainOrThrow);
	dms_assert(result);
	return GetRange();
}

template <class V>
auto Unit<V>::GetRange() const -> range_t requires (ranged_unit_v<V> || fixed_range_unit_v<V>)
{
	if constexpr (fixed_range_unit_v<V>)
		return range_t(0, UInt32(1) << nrbits_of_v<V>); // [0, 2^N) for bit values, [0, 1) + 1 == [0, 1] for Void
	else
	{
		auto sm = this->GetCurrSegmInfo();
		MG_CHECK(sm || this->IsPassor());
		if (!sm)
			return range_t();
		return sm->GetRange();
	}
}

template <typename V>
const UnitMetric*
Unit<V>::GetMetric() const
{
	if constexpr (ranged_unit_v<V>)
	{
		const Unit<V>* refItem = debug_cast<const Unit<V>*>(this->GetReferredItem().get());
		if (refItem)
			return refItem->GetMetric();

		return m_Metric.get();
	}
	else
		return AbstrUnit::GetMetric(); // nullptr
}

template <typename V>
const UnitMetric*
Unit<V>::GetCurrMetric() const
{
	if constexpr (ranged_unit_v<V>)
	{
		dbg_assert(this->CheckMetaInfoReadyOrPassor());
		const Unit<V>* refItem = debug_cast<const Unit<V>*>(this->GetCurrRefItem().get());
		if (refItem)
			return refItem->GetCurrMetric();

		return m_Metric.get();
	}
	else
		return AbstrUnit::GetCurrMetric(); // nullptr
}

template <typename V>
void Unit<V>::SetMetric(SharedPtr<const UnitMetric> m)
{
	if constexpr (ranged_unit_v<V>)
		m_Metric = m;
	else
		AbstrUnit::SetMetric(m); // no-op
}

static void MarkUnitChange(AbstrUnit* au) {
	auto ts = UpdateMarker::GetActiveTS(MG_DEBUG_TS_SOURCE_CODE("SetRange"));
	au->MarkTS(ts);
	au->SetDC({});
}


//----------------------------------------------------------------------
// Unit<V> member funcs: setting ranges and tilings
//----------------------------------------------------------------------

template <class V> typename std::enable_if_t<!std::is_floating_point_v< scalar_of_t<V> > >
NotifyRangeDataChange(Unit<V>* self, const typename Unit<V>::range_data_t* oldRangeData, const typename Unit<V>::range_data_t* newRangeData)
{
	auto oldSize = oldRangeData->GetElemCount();
	auto newSize = newRangeData->GetElemCount();

	DomainChangeInfo info{
		oldRangeData, newRangeData
	,	oldSize, newSize, std::min(newSize, oldSize)
	,	domain_change_context::GetCurrContext()
	};

	if (info.domainChangeContext)
		info.changePos = info.domainChangeContext->changePos;

	if (self->GetNrDataItemsOut()) // avoid constructing ChangeSourceLock when no DataItems are to be changed
		self->OnDomainChange(&info);
}

template <class V>
void Unit<V>::SetRange(const range_t& range) requires ranged_unit_v<V>
{
	if constexpr (has_simple_range_v<V>)
	{
		{
			auto lock = std::lock_guard(sc_RangeDataPtrAccess);
			this->m_RangeDataPtr.reset(std::make_unique<SimpleRangeData<V>>(range).release());
		}

		if (this->IsCacheItem())
			return;
		MarkUnitChange(this);
	}
	else
	{
		decltype(this->m_RangeDataPtr) oldRangeDataPtr, newRangeDataPtr;
		{
			auto lock = std::lock_guard(sc_RangeDataPtrAccess);
			oldRangeDataPtr = this->m_RangeDataPtr;
			if constexpr (has_small_range_v<V>)
				this->m_RangeDataPtr.reset(std::make_unique<SmallRangeData<V>>(range).release());
			else
				this->m_RangeDataPtr.reset(std::make_unique<DefaultTileRangeData<V>>(range).release());
			newRangeDataPtr = this->m_RangeDataPtr;
		}
		if (this->IsCacheItem())
			return;

	//	if (!UpdateMetaInfoDetectionLock::IsLocked())
	//		this->DoInvalidate();
	//	this->SetDC(nullptr);
	//	this->SetReferredItem(nullptr);

		if (oldRangeDataPtr)
		{
			MG_CHECK(IsMetaThread());
			dms_assert(!UpdateMarker::IsLoadingConfig());
			NotifyRangeDataChange(this, oldRangeDataPtr.get_ptr(), newRangeDataPtr.get_ptr());
		}
		MarkUnitChange(this);
	}
}

template <class V>
void Unit<V>::SetRange(const range_t& range, extent_t blockSize) requires ranged_unit_v<V>
{
	if constexpr (has_simple_range_v<V>)
		SetRange(range); // ignore blockSize since tiling doesn't make sense for floating point types
	else
	{
		decltype(this->m_RangeDataPtr) oldRangeDataPtr, newRangeDataPtr;
		{
			auto lock = std::lock_guard(sc_RangeDataPtrAccess);
			oldRangeDataPtr = this->m_RangeDataPtr;
			if constexpr (has_small_range_v<V>)
				this->m_RangeDataPtr.reset(std::make_unique<SmallRangeData<V>>(range).release());
			else
			{
				if (blockSize == tile_extent_t<V>() || blockSize == default_tile_size<V>())
					this->m_RangeDataPtr.reset(std::make_unique<DefaultTileRangeData<V>>(range).release());
				else
					this->m_RangeDataPtr.reset(std::make_unique<RegularTileRangeData<V>>(range, blockSize).release());
			}
			newRangeDataPtr = this->m_RangeDataPtr;
		}
		if (this->IsCacheItem())
			return;

		//	if (!UpdateMetaInfoDetectionLock::IsLocked())
		//		this->DoInvalidate();
		//	this->SetDC(nullptr);
		//	this->SetReferredItem(nullptr);

		if (oldRangeDataPtr)
		{
			MG_CHECK(IsMetaThread());
			dms_assert(!UpdateMarker::IsLoadingConfig());
			NotifyRangeDataChange(this, oldRangeDataPtr.get_ptr(), newRangeDataPtr.get_ptr());
		}
		MarkUnitChange(this);
	}
}

template <class V>
void Unit<V>::SetMaxRange()
{
	if constexpr (simple_range_unit_v<V>)
		this->SetRange(range_t(MinValue<V>(), MaxValue<V>()));
	else if constexpr (countable_unit_v<V>)
	{
		dms_assert(!this->m_RangeDataPtr);
		if constexpr (has_small_range_v<V>)
			SetRange(range_t(MinValue<V>(), MaxValue<V>()));
		else
		{
			auto lock = std::lock_guard(sc_RangeDataPtrAccess);
			this->m_RangeDataPtr.reset(std::make_unique <MaxRangeData<V>>().release()); // not suitable as domain
		}
	}
	else
		AbstrUnit::SetMaxRange(); // no-op
}

template <class V>
void Unit<V>::SetIrregularTileRange(std::vector<range_t> optionalTileRanges) requires tileable_unit_v<V>
{
	static_assert(!has_simple_range_v<value_t>);
	std::unique_ptr< TiledRangeData<value_t> > newRangeData;
	SharedPtr<const TiledRangeData<value_t> >  oldRangeData;
	if (optionalTileRanges.empty())
		newRangeData.reset(new DefaultTileRangeData<value_t>(UNDEFINED_VALUE(range_t)));
	else
		newRangeData.reset(new IrregularTileRangeData<value_t>(std::move(optionalTileRanges)));

	{
		auto lock = std::lock_guard(sc_RangeDataPtrAccess);
		oldRangeData = std::move(this->m_RangeDataPtr);
		this->m_RangeDataPtr.reset(newRangeData.release());
	}

	if (oldRangeData)
	{
		MG_CHECK(IsMetaThread());
		NotifyRangeDataChange(this, oldRangeData.get_ptr(), newRangeData.get());
	}

	MarkUnitChange(this);
}

template <class V>
void Unit<V>::SetRegularTileRange(const range_t& range, extent_t tileExtent) requires tileable_unit_v<V>
{
	static_assert(!has_simple_range_v<value_t>);

	std::unique_ptr< TiledRangeData<value_t> > newRangeData;
	auto orgRangeSize = Size(range);
	MakeLowerBound(orgRangeSize, ThrowingConvert<unsigned_type_t<value_t>>(MaxValue<extent_t>()));
	auto rangeSize = ThrowingConvert<extent_t>(orgRangeSize);
	MakeLowerBound(tileExtent, rangeSize);

	if (tileExtent == default_tile_size<value_t>())
		newRangeData = std::make_unique < DefaultTileRangeData<value_t>>(range);
	else
		newRangeData = std::make_unique < RegularTileRangeData<value_t>>(range, tileExtent);

	if (this->m_RangeDataPtr)
		NotifyRangeDataChange(this, this->m_RangeDataPtr.get_ptr(), newRangeData.get());
	this->m_RangeDataPtr.reset(newRangeData.release());
	MarkUnitChange(this);
}

//----------------------------------------------------------------------
// Unit<V> member funcs: support for Numerics (the 1-dimensional ranges)
//----------------------------------------------------------------------

template <class V>
Range<Float64> Unit<V>::GetRangeAsFloat64() const
{
	if constexpr (num_range_unit_v<V>)
		return Convert<Range<Float64>>(this->GetRange());
	else
		return AbstrUnit::GetRangeAsFloat64(); // throws
}

template <class V>
void Unit<V>::SetRangeAsFloat64(Float64 begin, Float64 end)
{
	if constexpr (num_range_unit_v<V> && ranged_unit_v<V>) // bit values and Void keep AbstrUnit's throw
		this->SetRange(
			range_t(
				Convert<V>(begin)
			,	Convert<V>(end)
			)
		);
	else
		AbstrUnit::SetRangeAsFloat64(begin, end); // throws
}

template <class V>
void Unit<V>::SetRangeAsUInt64(UInt64 begin, UInt64 end)
{
	if constexpr (num_range_unit_v<V> && ranged_unit_v<V>) // bit values and Void keep AbstrUnit's throw
		this->SetRange(
			range_t(
				Convert<V>(begin)
				, Convert<V>(end)
			)
		);
	else
		AbstrUnit::SetRangeAsUInt64(begin, end); // throws
}

//----------------------------------------------------------------------
// Unit<V> member funcs: support for Geometrics (the point types)
//----------------------------------------------------------------------

template <class V>
Unit<V>::~Unit() // out of line: hides the dtors of SharedPtr<const UnitMetric> / <const UnitProjection>
{}

template <class V>
IRect Unit<V>::GetRangeAsIRect() const
{
	if constexpr (geo_unit_v<V>)
	{
		range_t range = this->GetRange();
		return Convert<IRect>(range);
	}
	else
		return AbstrUnit::GetRangeAsIRect(); // throws
}

template <class V>
I64Rect Unit<V>::GetTileSizeAsI64Rect(tile_id t) const
{
	if constexpr (geo_unit_v<V>)
	{
		dbg_assert(this->CheckMetaInfoReadyOrPassor());
		MG_CHECK(this->m_RangeDataPtr);
		range_t result;
		if constexpr (has_simple_range_v<V>)
		{
			result = this->GetRange();
			dms_assert(t == no_tile);
		}
		else if (t == no_tile)
			result = this->GetRange();
		else
			result = this->m_RangeDataPtr->GetTileRange(t);
		return ThrowingConvert<I64Rect>(result);
	}
	else
		return AbstrUnit::GetTileSizeAsI64Rect(t); // the 1-D implementation
}

template <class V>
IRect Unit<V>::GetTileRangeAsIRect(tile_id t) const
{
	if constexpr (geo_unit_v<V> && !has_simple_range_v<V>) // integral point types; float point types keep AbstrUnit's throw
	{
		dbg_assert(this->CheckMetaInfoReadyOrPassor());
		return ThrowingConvert<IRect>(this->GetTileRange(t));
	}
	else
		return AbstrUnit::GetTileRangeAsIRect(t);
}


template <class V>
void Unit<V>::SetRangeAsIPoint(Int32 rowBegin, Int32  colBegin, Int32  rowEnd, Int32  colEnd, UInt16 blockSizeY, UInt16 blockSizeX)
{
	if constexpr (geo_unit_v<V>)
	{
		auto topLeft = shp2dms_order<Int32>(colBegin, rowBegin);
		auto bottomRight = shp2dms_order<Int32>(colEnd, rowEnd);
		auto iRange = IRect(topLeft, bottomRight);
		auto range = ThrowingConvert<range_t>(iRange);
		// Cap per-tile rows at MAX_STRIP_SIZE so a single-strip native layout (a full-height strip on a
		// large grid) doesn't yield one over-tall tile. No cap on blockSizeX: it is already a UInt16
		// (<= 0xFFFF) and the per-tile cell count (<= 1024 * 65535 = 2^26) stays within tile_offset (UInt32).
		blockSizeY = Min<UInt16>(blockSizeY, UInt16(1024));
		this->SetRange(range, shp2dms_order(blockSizeX, blockSizeY));
	}
	else
		AbstrUnit::SetRangeAsIPoint(rowBegin, colBegin, rowEnd, colEnd, blockSizeY, blockSizeX); // throws
}

template<typename T>
T BoxValue(const T& lb, const T&x, const T& ub)
{
	dms_assert(IsStrictlyLower(lb, ub));
	return LowerBound(UpperBound(lb, x), ub);
}

template<typename T, typename U>
Range<T>
ConvertRange(const Range<U>& src)
{
	static Range<U> boxRange(
		ThrowingConvert<U>(MIN_VALUE(T))
	,	ThrowingConvert<U>(MAX_VALUE(T))
	);
	return Convert<Range<T> > (
		Range<U>(
			BoxValue(boxRange.first, src.first,  boxRange.second)
		,	BoxValue(boxRange.first, src.second, boxRange.second)
		)
	);
}

template <class V>
DRect Unit<V>::GetRangeAsDRect() const
{
	if constexpr (geo_unit_v<V>)
		return Convert<DRect>(this->GetRange());
	else
		return AbstrUnit::GetRangeAsDRect(); // throws
}

template <class V>
void Unit<V>::SetRangeAsDPoint(Float64  rowBegin, Float64  colBegin, Float64  rowEnd, Float64  colEnd )
{
	if constexpr (geo_unit_v<V>)
		this->SetRange(
			ConvertRange<V>(
				DRect(
					shp2dms_order<Float64>(colBegin, rowBegin)
				,	shp2dms_order<Float64>(colEnd,   rowEnd  )
				)
			)
		);
	else
		AbstrUnit::SetRangeAsDPoint(rowBegin, colBegin, rowEnd, colEnd); // throws
}

template <class V>
const UnitProjection* Unit<V>::GetProjection() const
{
	if constexpr (geo_unit_v<V>)
	{
		dms_assert(this->GetNrDimensions() == 2);

		const Unit<V>* refItem = debug_cast<const Unit<V>*>(this->GetReferredItem().get());
		if (refItem)
			return refItem->GetProjection();

	//	dbg_assert(IsMetaInfoReadyOrPassor()); // caused by call to GetReferredItem

		return m_Projection.get();
	}
	else
		return AbstrUnit::GetProjection(); // nullptr
}

template <class V>
const UnitProjection* Unit<V>::GetCurrProjection() const
{
	if constexpr (geo_unit_v<V>)
	{
		dms_assert(this->GetNrDimensions() == 2);
		dbg_assert(this->CheckMetaInfoReadyOrPassor()); // caused by call to GetReferredItem

		const Unit<V>* refItem = debug_cast<const Unit<V>*>(this->GetCurrRefItem().get());
		if (refItem)
			return refItem->GetCurrProjection();

		return m_Projection.get();
	}
	else
		return AbstrUnit::GetCurrProjection(); // nullptr
}

template <class V>
void Unit<V>::SetProjection(SharedPtr <const UnitProjection> p)
{
	if constexpr (geo_unit_v<V>)
	{
		assert(!p || p->GetBaseUnit() != this);
		m_Projection = std::move(p);
	}
	else
		AbstrUnit::SetProjection(std::move(p)); // no-op
}

//----------------------------------------------------------------------
// Unit<V> member funcs: segment info and counts
//----------------------------------------------------------------------

template <typename Range>
SizeT CheckedCardinality(const TreeItem* context, const Range& range, bool throwOnUndefined)
{
	if (!IsDefined(range))
	{
		if (throwOnUndefined)
			context->throwItemError("Cardinality is undefined");
		else
			return UNDEFINED_VALUE(SizeT);
	}
	return Cardinality(range);
}

template <typename V>
auto Unit<V>::GetSegmInfo() const -> const range_data_t* requires (ranged_unit_v<V> || fixed_range_unit_v<V>)
{
	if constexpr (fixed_range_unit_v<V>)
		return GetCurrSegmInfo();
	else
	{
		this->PrepareDataUsage(DrlType::Certain);
		if (this->IsFailed(FailType::Data))
			return nullptr;

		return this->GetCurrSegmInfo();
	}
}

template <typename V>
auto Unit<V>::GetCurrSegmInfo() const -> const range_data_t* requires (ranged_unit_v<V> || fixed_range_unit_v<V>)
{
	if constexpr (fixed_range_unit_v<V>)
	{
		// everlasting function-local singleton (FixedRange<N>); handing out the raw pointer
		// keeps the lifetime contract of the ranged branch, and GetTiledRangeData re-wraps
		// it in a SharedPtr (an AddRef of an everlasting object).
		static SharedPtr<const range_data_t> s_RangeData = new range_data_t;
		return s_RangeData.get_ptr();
	}
	else
	{
		dbg_assert(this->CheckMetaInfoReadyOrPassor());

		if (this->WasFailed(FailType::Data))
			this->ThrowFail();

		const Unit<V>* ultimateCU = debug_cast<const Unit<V>*>(this->GetCurrRangeItem().get());
		dbg_assert(ultimateCU->CheckMetaInfoReadyOrPassor());
		dbg_assert(CheckCalculatingOrReady(ultimateCU) || ultimateCU->WasFailed(FailType::Data));

	//	dms_assert(this->PartOfInterestOrKeep() || ultimateCU->DataInMem());
		WaitReady(ultimateCU);
		if (ultimateCU->WasFailed(FailType::Data))
			ultimateCU->ThrowFail();
		dbg_assert(CheckDataReady(ultimateCU)); // calculation must have been finished
		return ultimateCU->m_RangeDataPtr.get_ptr();
	}
}


template <typename V>
bool Unit<V>::IsTiled() const
{
	if constexpr (countable_unit_v<V>)
	{
		auto si = this->GetSegmInfo();
		if (!si)
		{
			MG_CHECK(this->IsPassor());
			return false;
		}
		return si->GetNrTiles() != 1;
	}
	else
		return AbstrUnit::IsTiled(); // false
}

template <typename V>
bool Unit<V>::IsCurrTiled() const
{
	if constexpr (countable_unit_v<V>)
	{
		auto si = this->GetCurrSegmInfo();
		if (!si)
		{
			MG_CHECK(this->IsPassor());
			return false;
		}
		return si->GetNrTiles() != 1;
	}
	else
		return AbstrUnit::IsCurrTiled(); // false
}

template <typename V>
auto Unit<V>::GetTiledRangeData() const -> SharedPtr <const AbstrTileRangeData>
{
	if constexpr (countable_unit_v<V>)
	{
		auto lock = std::lock_guard(sc_RangeDataPtrAccess);
		return this->m_RangeDataPtr.get();
	}
	else if constexpr (fixed_range_unit_v<V>)
		return GetCurrSegmInfo(); // re-wrap of the everlasting singleton
	else
		return AbstrUnit::GetTiledRangeData(); // {} for floats, float points and SharedStr
}

template <typename V>
V Unit<V>::GetTileFirstValue (tile_id t) const requires ordinal_unit_v<V>
{
	assert(t != no_tile);
	auto si = this->GetCurrSegmInfo();
	MG_CHECK(si);
	return si->GetTileRange(t).first;
}

template <typename V>
V Unit<V>::GetTileValue (tile_id t, tile_offset localIndex) const requires ordinal_unit_v<V>
{
	assert(t != no_tile);
	auto si = this->GetCurrSegmInfo();
	MG_CHECK(si);
	return Range_GetValue_checked(si->GetTileRange(t), localIndex);
}

template <typename V>
auto Unit<V>::GetTileRange(tile_id t) const -> range_t requires indexable_unit_v<V>
{
	if constexpr (fixed_range_unit_v<V>)
	{
		assert(t == 0);
		return GetRange();
	}
	else
	{
		assert(t != no_tile);
		auto si = this->GetCurrSegmInfo();
		MG_CHECK(si);
		return si->GetTileRange(t);
	}
}

template <typename V>
SizeT Unit<V>::GetPreparedCount(bool throwOnUndefined) const
{
	if constexpr (countable_unit_v<V>)
		return CheckedCardinality(this, this->GetPreparedRange(), throwOnUndefined );
	else
		return AbstrUnit::GetPreparedCount(throwOnUndefined); // delegates to the virtual GetCount
}

template <typename V>
SizeT Unit<V>::GetCount() const
{
	if constexpr (indexable_unit_v<V>) // CountableUnitBase and FixedNumRangeUnitAdapter had identical bodies
		return Cardinality(this->GetRange());
	else
		return AbstrUnit::GetCount(); // 0 for floats, float points and SharedStr
}

template <typename V>
SizeT Unit<V>::GetDataCount() const
{
	if constexpr (countable_unit_v<V>)
	{
		auto sm = this->GetCurrSegmInfo();
		MG_CHECK(sm || this->IsPassor());
		if (!sm)
			return GetCount();
		return sm->GetElemCount();
	}
	else
		return AbstrUnit::GetDataCount(); // delegates to the virtual GetCount
}


template <typename V>
tile_offset Unit<V>::GetPreparedTileCount(tile_id t) const
{
	if constexpr (countable_unit_v<V>)
		return CheckedCardinality(this, this->GetSegmInfo()->GetTileRange(t), false);
	else
		return AbstrUnit::GetPreparedTileCount(t);
}

template <typename V>
tile_offset Unit<V>::GetTileCount(tile_id t) const
{
	if constexpr (countable_unit_v<V>)
		return CheckedCardinality(this, this->GetCurrSegmInfo()->GetTileRange(t), false);
	else
		return AbstrUnit::GetTileCount(t);
}


template <typename V>
auto Unit<V>::GetValueAtIndex(row_id i) const -> value_t requires indexable_unit_v<V>
{
	if constexpr (is_void_v<V>)
	{
		assert(!i);
		return Void();
	}
	else if constexpr (is_bitvalue_v<V>)
		return i;
	else
		return Range_GetValue_checked(this->GetRange(), i);
}

template <typename V>
row_id Unit<V>::GetIndexForValue(const value_t& v) const requires indexable_unit_v<V>
{
	if constexpr (is_void_v<V>)
		return 0;
	else if constexpr (is_bitvalue_v<V>)
		return v;
	else
		return Range_GetIndex_checked(this->GetRange(), v);
}

//----------------------------------------------------------------------
// Unit<V> member funcs: support for indexable units
//----------------------------------------------------------------------

template <class V>
row_id Unit<V>::GetDimSize(DimType dimNr) const
{
	if constexpr (indexable_unit_v<V>)
	{
		dms_assert(dimNr < this->GetNrDimensions());
		if constexpr (dimension_of_v<V> == 2)
		{
			if (!dimNr)
				return Height(this->GetRange());// case 0
			else
				return Width(this->GetRange()); // case 1
		}
		else
		{
			dms_assert(dimNr == 0);
			return this->GetCount();
		}
	}
	else
		return AbstrUnit::GetDimSize(dimNr); // throws
}

template <class V>
auto Unit<V>::CreateAbstrValueAtIndex(SizeT i) const -> std::unique_ptr<AbstrValue>
{
	if constexpr (indexable_unit_v<V>)
		return std::make_unique<ValueWrap<V>>(IsDefined(i) ? this->GetValueAtIndex(i) : UNDEFINED_OR_ZERO(V));
	else
		return AbstrUnit::CreateAbstrValueAtIndex(i); // throws
}


template <class V>
SizeT Unit<V>::GetIndexForAbstrValue(const AbstrValue& av) const
{
	if constexpr (indexable_unit_v<V>)
		return this->GetIndexForValue(debug_cast<const ValueWrap<V>*>(&av)->Get());
	else
		return AbstrUnit::GetIndexForAbstrValue(av); // throws
}

//----------------------------------------------------------------------
// Unit<V> member funcs: support for ordinals
//----------------------------------------------------------------------

template <class V>
void Unit<V>::SetCount(SizeT count)
{
	if constexpr (ordinal_unit_v<V> && std::is_unsigned_v<V>) // Support for Ordinals; all other V keep AbstrUnit's throw
		this->SetRange(range_t(0, ThrowingConvert<V>(count)));
	else
		AbstrUnit::SetCount(count);
}

template <class V>
row_id Unit<V>::GetBase() const
{
	if constexpr (ordinal_unit_v<V>) // was OrderedUnit's
		return this->GetRange().first;
	else if constexpr (fixed_range_unit_v<V>) // was FixedNumRangeUnitAdapter's
		return 0;
	else
		return AbstrUnit::GetBase(); // throws; point types included, as before
}

//----------------------------------------------------------------------
// Unit<V> member funcs: formatting
//----------------------------------------------------------------------

template <class V>
SharedStr Unit<V>::GetRangeAsStr(FormattingFlags ff) const
{
	if constexpr (ranged_unit_v<V> || is_bitvalue_v<V>) // Void and SharedStr keep AbstrUnit's throw
		return AsString(this->GetRange(), ff);
	else
		return AbstrUnit::GetRangeAsStr(ff);
}

//----------------------------------------------------------------------
// Unit member funcs implementations
//----------------------------------------------------------------------

template <class V>
Unit<V>::Unit()
{
}

template <class V>
DimType  Unit<V>::GetNrDimensions() const
{
	return dimension_of<V>::value;
}

//----------------------------------------------------------------------
// Visitor support
//----------------------------------------------------------------------

template <class V>
void Unit<V>::InviteUnitProcessor(const UnitProcessor& visitor) const
{
	visitor.Visit(this);
}

//----------------------------------------------------------------------
// instantiation and registration
//----------------------------------------------------------------------

#include "ser/StringStream.h"
#include "UnitClass.h"
#include "UnitClassReg.h"
#include "RtcTypeLists.h"

template <typename T>
TokenID GetUnitClassID()
{
	static TokenID result = GetTokenID_st(myArrayPrintF<100>("Unit<{}>", ValueWrap<T>::GetStaticClass()->GetID().c_str_st()));
	return result;
}

template <typename T>
const UnitClass* Unit<T>::GetStaticClass()
{
	static UnitClass s_Cls(
		CreateFunc<Unit<T> >
	, 	GetUnitClassID<T>()
//	, 	GetTokenID("Unit<" #T ">")
	,	ValueWrap<T>::GetStaticClass()
	,	SharedCreateFunc<Unit<T> >);
	return &s_Cls;\
}


template <typename T>
const Class* Unit<T>::GetDynamicClass() const
{
	return GetStaticClass();
}

namespace {
	TypeListClassReg<
		tl::transform_templ<typelists::all_unit_types, Unit>
	> s_x;

	tl_oper::inst_tuple_templ<typelists::ranged_unit_objects, RangeProp> unitRangeProps(false);
	tl_oper::inst_tuple_templ<typelists::ranged_unit_objects, RangeProp> unitCatRangeProps(true);
}

// Explicit class instantiation, on both compilers. Now that every member lives on Unit<V> itself
// there are no base-class template members left for GCC to leave un-emitted, which is what the
// per-member lists here used to compensate for; and members whose requires-clause a given V does
// not satisfy are simply skipped ([temp.explicit]/10 -- the U4 DataArrayBase<Bool> canary). This
// also subsumes the former TiledUnitInstantiator: the tile-range members are emitted here for
// exactly the tileable types. TODO: confirm the Linux link on OVSRV10 before merge.
#include "utl/Instantiate.h"
using String = SharedStr;
#define INSTANTIATE(T) template class Unit<T>;
INSTANTIATE_FLD_ELEM
INSTANTIATE_VOID
#undef INSTANTIATE

//----------------------------------------------------------------------
// C style Interface functions for StaticClass retrieval
//----------------------------------------------------------------------

extern "C" {

	// TODO: Delphi code only uses UInt8, UInt32, String, Abstr, Void
	TIC_CALL const UnitClass* DMS_CONV DMS_UInt8Unit_GetStaticClass() { return Unit<UInt8> ::GetStaticClass(); }
	TIC_CALL const UnitClass* DMS_CONV DMS_UInt32Unit_GetStaticClass() { return Unit<UInt32> ::GetStaticClass(); }
	TIC_CALL const UnitClass* DMS_CONV DMS_StringUnit_GetStaticClass() { return Unit<String> ::GetStaticClass(); }
	TIC_CALL const UnitClass* DMS_CONV DMS_VoidUnit_GetStaticClass() { return Unit<Void> ::GetStaticClass(); }

	//----------------------------------------------------------------------
	// C style Interface functions for construction
	//----------------------------------------------------------------------

	TIC_CALL AbstrUnit* DMS_CreateUnit(TreeItem* parent, CharPtr name, const UnitClass* uc)
	{
		DMS_CALL_BEGIN

			CheckPtr(parent, TreeItem::GetStaticClass(), "DMS_CreateUnit");
			CheckPtr(uc, UnitClass::GetStaticClass(), "DMS_CreateUnit");
			assert(!parent->IsCacheItem());

			return uc->CreateUnit(parent, GetTokenID_mt(name)).get(); // unit owned by parent; raw stays valid

		DMS_CALL_END
		return nullptr;
	}

	//----------------------------------------------------------------------
	// C style Interface functions for GetRange / SetRange
	//----------------------------------------------------------------------

	TIC_CALL void DMS_CONV DMS_NumericUnit_SetRangeAsFloat64(AbstrUnit* self, Float64 begin, Float64 end)
	{
		DMS_CALL_BEGIN

			TreeItemContextHandle checkPtr(self, AbstrUnit::GetStaticClass(), "DMS_NumericUnit_SetRangeAsFloat64");
			assert(self->GetValueType()->IsNumeric());
			self->SetRangeAsFloat64(begin, end);

		DMS_CALL_END
	}

	TIC_CALL void DMS_CONV DMS_NumericUnit_GetRangeAsFloat64(const AbstrUnit* self, Float64* begin, Float64* end)
	{
		DMS_CALL_BEGIN

			TreeItemContextHandle checkPtr(self, AbstrUnit::GetStaticClass(), "DMS_NumericUnit_GetRangeAsFloat64");
			assert(self->GetValueType()->IsNumeric());

			auto result = self->GetRangeAsFloat64();
			if (begin) *begin = result.first;
			if (end)   *end = result.second;

		DMS_CALL_END
	}

	TIC_CALL void DMS_CONV DMS_GeometricUnit_SetRangeAsDPoint(AbstrUnit* self,
		Float64 rowBegin, Float64 colBegin, Float64 rowEnd, Float64 colEnd)
	{
		DMS_CALL_BEGIN

			TreeItemContextHandle checkPtr(self, AbstrUnit::GetStaticClass(), "DMS_GeometricUnit_SetRangeAsDPoint");

			self->SetRangeAsDPoint(rowBegin, colBegin, rowEnd, colEnd);

		DMS_CALL_END
	}

	TIC_CALL void DMS_CONV DMS_GeometricUnit_GetRangeAsDPoint(const AbstrUnit* self, Float64* rowBegin, Float64* colBegin, Float64* rowEnd, Float64* colEnd)
	{
		DMS_CALL_BEGIN

			TreeItemContextHandle checkPtr(self, AbstrUnit::GetStaticClass(), "DMS_GeometricUnit_GetRangeAsDPoint");

			auto [from, to_] = self->GetRangeAsDRect();
			if (rowBegin) *rowBegin = from.Row();
			if (colBegin) *colBegin = from.Col();
			if (rowEnd) *rowEnd= to_.Row();
			if (colEnd) *colEnd= to_.Col();

		DMS_CALL_END
	}

	TIC_CALL void DMS_CONV DMS_GeometricUnit_SetRangeAsIPoint(AbstrUnit* self,
		Int32 rowBegin, Int32 colBegin, Int32 rowEnd, Int32 colEnd, UInt16 blockSizeY, UInt16 blockSizeX)
	{
		DMS_CALL_BEGIN

			TreeItemContextHandle checkPtr(self, AbstrUnit::GetStaticClass(), "DMS_GeometricUnit_SetRangeAsIPoint");

			self->SetRangeAsIPoint(rowBegin, colBegin, rowEnd, colEnd, blockSizeY, blockSizeX);

		DMS_CALL_END
	}

	TIC_CALL void DMS_CONV DMS_GeometricUnit_GetRangeAsIPoint(const AbstrUnit* self, Int32* rowBegin, Int32* colBegin, Int32* rowEnd, Int32* colEnd)
	{
		DMS_CALL_BEGIN

			TreeItemContextHandle checkPtr(self, AbstrUnit::GetStaticClass(), "DMS_GeometricUnit_GetRangeAsIPoint");

			InterestPtr<const TreeItem*> lockPtr(self);
			auto rect = self->GetRangeAsIRect();
			if (rowBegin) *rowBegin = rect.first.Row();
			if (colBegin) *colBegin = rect.first.Col();
			if (rowEnd)   *rowEnd   = rect.second.Row();
			if (colEnd)   *colEnd   = rect.second.Col();

		DMS_CALL_END
	}

} // end of extern "C"
