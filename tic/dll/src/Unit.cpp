#include "TicPCH.h"
#pragma hdrstop

#include "Unit.h"

#include "act/UpdateMark.h"
#include "dbg/DmsCatch.h"
#include "geo/Conversions.h"
#include "geo/RangeIndex.h"
#include "geo/StringBounds.h"
#include "mci/ValueWrap.h"
#include "ptr/InterestHolders.h"
#include "ser/AsString.h"
#include "ser/MoreStreamBuff.h"
#include "ser/PointStream.h"
#include "ser/RangeStream.h"
#include "ser/SequenceArrayStream.h"
#include "utl/IncrementalLock.h"
#include "utl/MySPrintF.h" 

#include "AbstrCalculator.h"
#include "AbstrDataItem.h"
#include "DataStoreManagerCaller.h"
#include "DataController.h"
#include "DataLocks.h"
#include "Metric.h"
#include "Projection.h"
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

namespace {

	template <typename U> 
	inline UInt32 Unit_GetDimSize(const IndexableUnitAdapter<U>* self, DimType dimNr, const ord_type_tag*)
	{
		dms_assert(dimNr == 0);
		dms_assert(self->GetNrDimensions() == 1);
		return self->GetCount();
	}

	template <typename U> 
	inline UInt32 Unit_GetDimSize(const IndexableUnitAdapter<U>* self, DimType dimNr, const crd_point_type_tag*)
	{
		dms_assert(self->GetNrDimensions() == 2);
		dms_assert(dimNr < 2);
		if (!dimNr)
			return Height(self->GetRange());// case 0
		else 
			return Width(self->GetRange()); // case 1
	}

} // end anonymous namespace

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

auto GetRangeDataAsLispRef(Void rd, bool asCategorical, LispPtr base) -> LispRef
{
	return base;
}

//----------------------------------------------------------------------
// UnitBase member funcs implementations
//----------------------------------------------------------------------

template <class V>
LispRef UnitBase<V>::GetKeyExprImpl() const
{
	assert(!IsCacheItem());

	LispRef result;
	if (!IsDefaultUnit()) // || IsCacheRoot())
	{
		result = AbstrUnit::GetKeyExprImpl();
	}

#if defined(MG_DEBUG)
	auto resultStr = AsString(result);
#endif

	if (IsCacheItem()) // || result != ExprList(GetValueType()->GetID()))
		return result;

	// present as metric-less BaseUnit with a unique keyExpr.
	if (result.EndP() && (!IsLoadable() || GetTSF(USF_HasConfigRange)))
	{
		result = ExprList(GetValueType()->GetID());
		//	if constexpr (is_integral_v<V> && has_var_range_v<V>) // could be domain or projection base; enforce [expr(x) == expr(y)] => [fullname(x) == fullname(y)];
		if constexpr (has_var_range_v<V>) // could be domain or projection base; enforce [expr(x) == expr(y)] => [fullname(x) == fullname(y)];
		{
			if (!IsDefaultUnit())
			{
				LispRef baseUnitMetricExpr;
				auto sr = GetSpatialReference();
				if (sr)
				{
					// BaseUnit( 'sr', result ) provides a format specific identity
					auto srStr = sr.AsStrRange();
					baseUnitMetricExpr = LispRef(srStr.m_CharPtrRange.first, srStr.m_CharPtrRange.second);
				}
				else
				{
					// BaseUnit( Left('%FullName%', 0), result ) provides a unique domain and projection identity with compatible metric
					auto fullName = this->GetFullName();
					baseUnitMetricExpr = ExprList(token::left
						, LispRef(fullName.begin(), fullName.send())
						, ExprList(token::UInt32, LispRef(Number(0.0)))
					);
				}
				result = ExprList(token::BaseUnit
					, baseUnitMetricExpr
					, result
				);

#if defined(MG_DEBUG)
				auto resultStr2 = AsString(result);
#endif
			}
		}
	}

#if defined(MG_DEBUG_LISP_TREE)
	reportF(SeverityTypeID::ST_MinorTrace, "AbstrUnit::GetAsLispRef -> %s", AsString(result).c_str());
	dms_assert(IsExpr(result));
#endif
	// add range or tile spec to keyExpr
	if (GetTSF(USF_HasConfigRange))
		result = GetRangeDataAsLispRef(m_RangeDataPtr, GetTSF(TSF_Categorical), result); // enforce [expr(x) == expr(y)] => [range(x) == range(y)];

#if defined(MG_DEBUG_LISP_TREE)
	reportF(SeverityTypeID::ST_MinorTrace, "-> %s", AsString(result).c_str());
	dms_assert(IsExpr(result));
#endif

	return result;
}

//----------------------------------------------------------------------
// RangedUnit member funcs implementations
//----------------------------------------------------------------------

template <class V>
void  RangedUnit<V>::ClearData(garbage_t& g) const
{ 
	g |= std::move(const_cast<RangedUnit<V>*>(this)->m_RangeDataPtr); 
	dms_assert(!this->HasTiledRangeData());
}

template <class V>
void RangedUnit<V>::ValidateRange (const range_t& range) const
{
	auto currRange = GetRange();
	if (range != currRange)
	{
		const ValueClass* cls = this->GetValueType();
		dms_assert(cls);
		throwItemErr("ValidateRange(%s) failed because current range is %s"
		,	AsString(range).c_str()
		,	AsString(currRange).c_str()
		);
	}
}

//	Override TreeItem virtuals
template <typename V> 
void RangedUnit<V>::CopyProps(TreeItem* result, const CopyTreeContext& copyContext) const
{
	AbstrUnit::CopyProps(result, copyContext);
	debug_cast<RangedUnit*>(result)->m_RangeDataPtr = this->m_RangeDataPtr;
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

template <typename V>
std::enable_if_t<is_numeric_v<V>, V>
TileStart(const Range<V>& range, tile_offset size, tile_offset nr_tiles, tile_id t)
{
	return range.first + row_id(size) * t;
}

template <typename V>
Point<V> TileStart(const Range<Point<V>>& range, tile_extent_t<Point<V>> tileExtent, tile_extent_t<Point<V>> tilingExtent, tile_id t)
{
	return range.first + Point<V>(Range_GetValue_naked_zbase(tilingExtent, t)) * Point<V>(tileExtent);
}

template <typename Base>
auto RegularAdapter<Base>::GetTileRange(tile_id t) const -> Range<value_type>
{ 
	dms_assert(t != no_tile);
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
}

template <typename Base>
tile_loc RegularAdapter<Base>::GetTiledLocationForValue(value_type v) const
{
	dms_assert(IsIncluding(this->m_Range, v));

	tile_id t = Range_GetIndex_naked_zbase(this->tiling_extent(), tile_extent_t<value_type>((v - this->m_Range.first) / this->tile_extent()));
	return tile_loc(t, Range_GetIndex_naked(GetTileRange(t), v));
}

template <typename Base>
tile_loc RegularAdapter<Base>::GetTiledLocation(row_id index) const
{
	assert(index < Cardinality(this->m_Range));
//	if (index >= Cardinality(this->m_Range))
//		return tile_loc{UNDEFINED_VALUE(tile_id), UNDEFINED_VALUE(tile_offset)};

	auto v = Range_GetValue_naked(this->m_Range, index);
	return GetTiledLocationForValue(v);
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
void RangedUnit<V>::LoadBlobStream(const InpStreamBuff* is)
{
	BinaryInpStream bis(is);
	LoadRangeImpl(bis);

//	this->SetDataInMem();
}
template <typename V>
void RangedUnit<V>::LoadRangeImpl(BinaryInpStream& pis)
{
	if constexpr (has_simple_range_v<V>)
	{
		Range<V> range;
		tile_id tn;
		pis >> range >> tn;
		if (range.empty() || tn == 0)
			this->m_RangeDataPtr.reset();
		else
			this->m_RangeDataPtr.reset(new SimpleRangeData<V>());
	}
/*
		std::unique_ptr<SimpleRangeData<V>> newRangeDataPtr;
		tile_type_id tt; pis >> reinterpret_cast<UInt32&>(tt);
		switch (tt) {
		case tile_type_id::none: this->m_RangeDataPtr.reset(); return;
		case tile_type_id::simple: newRangeDataPtr.reset(new SimpleRangeData<V>()); break;
		default: throwErrorF("Stream", "Unexpected tile_type_id in BinaryInpStream");
		}
		dms_assert(newRangeDataPtr);
		newRangeDataPtr->Load(pis);
		this->m_RangeDataPtr = newRangeDataPtr.release();
*/
}

template <typename V>
void CountableUnitBase<V>::LoadRangeImpl(BinaryInpStream& pis)
{
	if constexpr (has_var_range_v<V>)
	{
		Range<V> range;
		tile_id tn;
		pis >> range >> tn;

		if constexpr (has_small_range_v<V>)
		{
			MG_CHECK(tn == 0);
			if (range.empty())
				this->m_RangeDataPtr.reset();
			else
				this->m_RangeDataPtr.reset(new SmallRangeData<V>(range));
		}
		else
		{
			if (tn == 0)
				this->m_RangeDataPtr.reset(new DefaultTileRangeData<V>(range));
			else if (tn == no_tile) // assume one big old tile (as in 7.xxx)
				this->m_RangeDataPtr.reset(new RegularTileRangeData<V>(range, Size(range)));
			else
			{
				std::vector<Range<V>> tileRanges(tn);
				tile_id ti = tn; while (ti) // WARNING: REVERSE ORDER
					pis >> tileRanges[--ti];
				if (tn == 1)
					this->m_RangeDataPtr.reset(new RegularTileRangeData<V>(tileRanges[0], Size(tileRanges[0])));
				// TODO G8: recognize Default and Regular tilings and construct them if possible.
				this->m_RangeDataPtr.reset(new IrregularTileRangeData<V>(std::move(tileRanges)));
				for (tile_id t = 0; t != tn; ++t)
				{
					MG_CHECK(IsIncluding(range, this->m_RangeDataPtr->GetTileRange(t)));
				}
			}
/* REMOVE
			std::unique_ptr<TiledRangeData<V>> newRangeDataPtr;
			tile_type_id tt; pis >> reinterpret_cast<UInt32&>(tt);
			switch (tt) {
			case tile_type_id::none: this->m_RangeDataPtr.reset(); return;
			case tile_type_id::default_: newRangeDataPtr.reset(new DefaultTileRangeData<V>()); break;
			case tile_type_id::regular: newRangeDataPtr.reset(new RegularTileRangeData<V>()); break;
			case tile_type_id::irregular: newRangeDataPtr.reset(new IrregularTileRangeData<V>()); break;
			default: throwErrorF("Stream", "Unexpected tile_type_id in BinaryInpStream");
			}
			dms_assert(newRangeDataPtr);
			newRangeDataPtr->Load(pis);
			newRangeDataPtr->CalcTilingExtent();

			this->m_RangeDataPtr = newRangeDataPtr.release();
*/
		}
	}
}

template <typename V> 
void RangedUnit<V>::StoreBlobStream(OutStreamBuff* os) const
{
	BinaryOutStream bos(os);
	this->StoreRangeImpl(bos);
}

template <typename V> 
void RangedUnit<V>::StoreRangeImpl(BinaryOutStream& pos) const
{
	if constexpr (has_var_range_v<V>)
	{
		if (this->m_RangeDataPtr)
			pos << this->m_RangeDataPtr->GetRange();
		else
			pos << Range<V>(V(), V());
	}
}

template <typename V>
void CountableUnitBase<V>::StoreRangeImpl(BinaryOutStream& pos) const
{
	if constexpr (has_var_range_v<V>)
	{
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
bool CountableUnitBase<V>::ContainsUndefined(tile_id t) const
{
	dms_assert(this->m_RangeDataPtr); // PRECONDITION: IsCurrTiled()
	return :: ContainsUndefined(this->m_RangeDataPtr->GetTileRange(t));
}

template <typename V>
bool RangedUnit<V>::PrepareRange() const
{
	if (!this->PrepareDataUsage(DrlType::Suspendible))
		return false;

	return WaitForReadyOrSuspendTrigger(this->GetCurrRangeItem());
}

template <class V>
typename Range<V>
RangedUnit<V>::GetPreparedRange() const
{
	dbg_assert(this->CheckMetaInfoReadyOrPassor() || this->HasConfigData());

	dms_assert(!this->InTemplate() || this->GetTSF(TSF_HasConfigData)); // PRECONDITION?

	MG_CHECK(IsMainThread()); // DEBUG

	dms_check_not_debugonly; 
	dms_assert(!this->m_State.IsInTrans() || (this->m_State.GetTransState() > actor_flag_set::AF_CalculatingData) );

	bool result = this->PrepareDataUsage(DrlType::CertainOrThrow);
	dms_assert(result);
	return GetRange();
}

template <class V>
auto RangedUnit<V>::GetRange() const -> range_t
{
	auto sm = this->GetCurrSegmInfo();
	MG_CHECK(sm || this->IsPassor());
	if (!sm)
		return range_t();
	return sm->GetRange();
/* REMOVE
	dbg_assert(CheckMetaInfoReadyOrPassor() || HasConfigData());
	dms_assert(!InTemplate() || GetTSF(TSF_HasConfigData)); // PRECONDITION?

	const RangedUnit<V>* refItem = this;
	while (true) {

 		dms_assert(refItem->PartOfInterest() || refItem->DataInMem() || refItem->HasConfigData() || GetCurrRangeItem()->IsSdKnown());

		if (refItem->GetTSF(TSF_HasConfigData))
			return refItem->m_Range; // no interest needed, no wait.

		const RangedUnit<V>* refRefItem = debug_cast<const RangedUnit<V>*>( refItem->GetCurrRefItem() );
		if (!refRefItem)
			break;
		refItem = refRefItem;
	}

	dms_assert(refItem == GetCurrRangeItem());
	if (!WaitReady(refItem)) // may wait for the completion of ItemWriteLock from a generating operation that was started by PrepareDataUsage.
	{
		if (WasFailed(FR_Data))
			ThrowFail(refItem);
		ThrowFail("Cannot derive value range", FR_Data);
	}

	dms_assert(refItem->DataInMem());
	return refItem->m_Range;
	*/
}

template <typename V>
const UnitMetric*
RangedUnit<V>::GetMetric() const
{
	const RangedUnit<V>* refItem = debug_cast<const RangedUnit<V>*>(this->GetReferredItem());
	if (refItem)
		return refItem->GetMetric();

	return m_Metric;
}

template <typename V>
const UnitMetric*
RangedUnit<V>::GetCurrMetric() const
{
	dbg_assert(this->CheckMetaInfoReadyOrPassor());
	const RangedUnit<V>* refItem = debug_cast<const RangedUnit<V>*>(this->GetCurrRefItem());
	if (refItem)
		return refItem->GetCurrMetric();

	return m_Metric;
}

template <typename V>
void RangedUnit<V>::SetMetric(SharedPtr<const UnitMetric> m)
{ 
	m_Metric = m;
}

void MarkUnitChange(AbstrUnit* au) {
	auto ts = UpdateMarker::GetActiveTS(MG_DEBUG_TS_SOURCE_CODE("SetRange"));
	au->MarkTS(ts);
	au->SetDC(nullptr);


/*
	TimeStamp ts = 0;
	if (!TreeItem::s_ConfigReadLockCount)
	if (au->HasConfigData() && !au->IsPassor())
		ts = DataStoreManager::GetCachedConfigSourceTS(au);
	if (ts == 0)
		ts = UpdateMarker::GetActiveTS(MG_DEBUG_TS_SOURCE_CODE(mySSPrintF("SetRange(%s)", au->GetFullName().c_str()).c_str()));
	au->MarkTS(ts);
	au->SetDataInMem();

	dms_assert(au->DataInMem());

	if (TreeItem::s_ConfigReadLockCount)
		return;
	if (!au->IsAutoDeleteDisabled())
		return;

	dms_assert(au->IsDcKnown() || !au->IsSdKnown());
	dms_assert(!au->IsDcKnown() || !au->mc_RefItem && (au->IsCacheItem() || au->HasConfigData() || au->IsDataReadable()));

	dms_assert(au->HasVarRange());
	if (!au->IsSdKnown())
		DataStoreManager::StoreBlob(au);

#if defined(MG_DEBUG_DATA)
	dms_assert_impl(!au->IsSdKnown() || DataStoreManager::Curr()->CheckBlob(au));
#endif
*/
}


//----------------------------------------------------------------------
// OrderedUnit member funcs implementations
//----------------------------------------------------------------------

template <class V> typename std::enable_if_t<!std::is_floating_point_v< scalar_of_t<V> > >
NotifyRangeDataChange(RangedUnit<V>* self, const typename RangedUnit<V>::range_data_t* oldRangeData, const typename RangedUnit<V>::range_data_t* newRangeData)
{
//	dms_assert(self->DataInMem());
	auto oldSize = oldRangeData->GetDataSize();
	auto newSize = newRangeData->GetDataSize();

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
void OrderedUnit<V>::Split(SizeT pos, SizeT len)
{
	if (!len)
		return;

	auto currContext = domain_change_context{ pos };

	this->SetRange(typename OrderedUnit::range_t(0, ThrowingConvert< V>(this->GetCount() + len)));
}

template <class V>
void OrderedUnit<V>::Merge(SizeT pos, SizeT len)
{
	if (!len)
		return;

	auto currContext = domain_change_context{ pos };

	this->SetRange(typename OrderedUnit::range_t(0, ThrowingConvert< V>(this->GetCount() - len)));
}

template <class V>
void CountableUnitBase<V>::SetRange(const range_t& range)
{
	auto oldRangeDataPtr = this->m_RangeDataPtr;
	static_assert(!has_simple_range_v<V>);

	if constexpr (has_small_range_v<V>)
		this->m_RangeDataPtr.assign(std::make_unique<SmallRangeData<V>>(range).release());
	else
		this->m_RangeDataPtr.assign(std::make_unique<DefaultTileRangeData<V>>(range).release());

	if (this->IsCacheItem())
		return;

//	if (!UpdateMetaInfoDetectionLock::IsLocked())
//		this->DoInvalidate();
//	this->SetDC(nullptr);
//	this->SetReferredItem(nullptr);

	if (oldRangeDataPtr)
	{
		dms_assert(!UpdateMarker::IsLoadingConfig());
		NotifyRangeDataChange(this, oldRangeDataPtr.get_ptr(), this->m_RangeDataPtr.get_ptr());
	}
	MarkUnitChange(this);
}

template <class V>
void CountableUnitBase<V>::SetMaxRange()
{
	dms_assert(!this->m_RangeDataPtr);
	if constexpr (has_small_range_v<V>)
		SetRange(range_t(MinValue<V>(), MaxValue<V>()));
	else
		this->m_RangeDataPtr.assign(std::make_unique <MaxRangeData<V>>().release()); // not suitable as domain
}

template <class V>
void FloatUnit<V>::SetRange (const range_t& range)
{
	this->m_RangeDataPtr.assign(std::make_unique<SimpleRangeData<V>>(range).release());

	if (this->IsCacheItem())
		return;
	MarkUnitChange(this);
}

template <class V>
void FloatUnit<V>::SetMaxRange()
{
	this->SetRange(range_t(MinValue<V>(), MaxValue<V>()));
}

template <typename Base>
void TileAdapter<Base>::SetIrregularTileRange(std::vector<range_t> optionalTileRanges)
{
	static_assert(!has_simple_range_v<value_t>);
	std::unique_ptr< TiledRangeData<value_t> > newRangeData;
	if (optionalTileRanges.empty())
		newRangeData.reset(new DefaultTileRangeData<value_t>(UNDEFINED_VALUE(range_t)));
	else
		newRangeData.reset(new IrregularTileRangeData<value_t>(std::move(optionalTileRanges)));

	if (this->m_RangeDataPtr)
		NotifyRangeDataChange(this, this->m_RangeDataPtr.get_ptr(), newRangeData.get());
	this->m_RangeDataPtr.assign(newRangeData.release());
	MarkUnitChange(this);
}

template <typename Base>
void TileAdapter<Base>::SetRegularTileRange(const range_t& range, extent_t tileExtent)
{
	static_assert(!has_simple_range_v<value_t>);

	std::unique_ptr< TiledRangeData<value_t> > newRangeData;
	auto orgRangeSize = Size(range);
	MakeLowerBound(orgRangeSize, ThrowingConvert<unsigned_type_t<value_t>>(MaxValue<extent_t>()));
	auto rangeSize = ThrowingConvert<extent_t>(orgRangeSize);
	MakeLowerBound(tileExtent, rangeSize);

	if (IsLowerBound(tileExtent, default_tile_size<value_t>()))
		newRangeData = std::make_unique < DefaultTileRangeData<value_t>>(range);
	else
		newRangeData = std::make_unique < RegularTileRangeData<value_t>>(range, tileExtent);
//	newRangeData->CalcTilingExtent();

	if (this->m_RangeDataPtr)
		NotifyRangeDataChange(this, this->m_RangeDataPtr.get_ptr(), newRangeData.get());
	this->m_RangeDataPtr.assign(newRangeData.release());
	MarkUnitChange(this);
}
//----------------------------------------------------------------------
// NumericUnitAdapter member funcs implementations
//----------------------------------------------------------------------

template <typename U>
Range<Float64> NumRangeUnitAdapterBase<U>::GetRangeAsFloat64() const
{
	return Convert<Range<Float64>>(this->GetRange());
}

template <class U>
void VarNumRangeUnitAdapter<U>::SetRangeAsFloat64(Float64 begin, Float64 end)
{
	this->SetRange(
		typename U::range_t(
			Convert<typename U::value_t>(begin)
		,	Convert<typename U::value_t>(end)
		)
	);
}

template <class U>
void VarNumRangeUnitAdapter<U>::SetRangeAsUInt64(UInt64 begin, UInt64 end)
{
	this->SetRange(
		typename U::range_t(
			Convert<typename U::value_t>(begin)
			, Convert<typename U::value_t>(end)
		)
	);
}

//----------------------------------------------------------------------
// GeoUnitAdapter member funcs implementations
//----------------------------------------------------------------------

template <class U>
GeoUnitAdapter<U>::~GeoUnitAdapter() // hide dtor of SharedPtr<const UnitProjection>
{}

// Support for Geometrics
template <class U>
IRect GeoUnitAdapter<U>::GetRangeAsIRect() const
{
	typename U::range_t range = this->GetRange();
	return Convert<IRect>(range);
}

template <typename U>
I64Rect GeoUnitAdapter<U>::GetTileSizeAsI64Rect(tile_id t) const
{
	dbg_assert(this->CheckMetaInfoReadyOrPassor());
	MG_CHECK(this->m_RangeDataPtr);
	typename U::range_t result;
	if constexpr (has_simple_range_v<typename U::value_t>)
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

template <class U>
IRect GeoUnitAdapterI<U>::GetTileRangeAsIRect(tile_id t) const
{
	dbg_assert(this->CheckMetaInfoReadyOrPassor());
	return ThrowingConvert<IRect>(this->GetTileRange(t));
}


template <class U>
void GeoUnitAdapter<U>::SetRangeAsIPoint(Int32 rowBegin, Int32  colBegin, Int32  rowEnd, Int32  colEnd )
{
	auto topLeft = shp2dms_order<Int32>(colBegin, rowBegin);
	auto bottomRight = shp2dms_order<Int32>(colEnd, rowEnd);
	auto iRange = IRect(topLeft, bottomRight);
	auto range = ThrowingConvert<typename U::range_t>(iRange);
	this->SetRange(range);
}

template <class U>
DRect GeoUnitAdapter<U>::GetRangeAsDRect() const
{
	return Convert<DRect>(this->GetRange());
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

template <class U>
void GeoUnitAdapter<U>::SetRangeAsDPoint(Float64  rowBegin, Float64  colBegin, Float64  rowEnd, Float64  colEnd )
{
	this->SetRange(
		ConvertRange<typename U::value_t>(
			DRect(
				shp2dms_order<Float64>(colBegin, rowBegin)
			,	shp2dms_order<Float64>(colEnd,   rowEnd  )
			)
		)
	);
}

template <class U>
const UnitProjection* GeoUnitAdapter<U>::GetProjection() const
{
	dms_assert(this->GetNrDimensions() == 2);

	const GeoUnitAdapter<U>* refItem = debug_cast<const GeoUnitAdapter<U>*>(this->GetReferredItem());
	if (refItem)
		return refItem->GetProjection();

//	dbg_assert(IsMetaInfoReadyOrPassor()); // caused by call to GetReferredItem

	return m_Projection;
}

template <class U>
const UnitProjection* GeoUnitAdapter<U>::GetCurrProjection() const
{
	dms_assert(this->GetNrDimensions() == 2);
	dbg_assert(this->CheckMetaInfoReadyOrPassor()); // caused by call to GetReferredItem

	const GeoUnitAdapter<U>* refItem = debug_cast<const GeoUnitAdapter<U>*>(this->GetCurrRefItem());
	if (refItem)
		return refItem->GetCurrProjection();

	return m_Projection;
}

template <class U>
void GeoUnitAdapter<U>::SetProjection(SharedPtr <const UnitProjection> p)
{ 
	assert(!p || p->GetBaseUnit() != this);
	m_Projection = std::move(p);
}

template <typename U>
void
GeoUnitAdapter<U>::CopyProps(TreeItem* result, const CopyTreeContext& copyContext) const
{
	U::CopyProps(result, copyContext);

	GeoUnitAdapter<U>* resultUnit = debug_cast<GeoUnitAdapter<U>*>(result);
	resultUnit->m_Projection = m_Projection;
}

//----------------------------------------------------------------------
// CountableUnitBase member funcs implementations
//----------------------------------------------------------------------

template <typename Range> 
UInt32 CheckedCardinality(const TreeItem* context, const Range& range, bool throwOnUndefined)
{
	if (!IsDefined(range))
		if (throwOnUndefined)
			context->throwItemError("Cardinality is undefined");
		else
			return UNDEFINED_VALUE(UInt32);
	return Cardinality(range);
}

template <typename V> 
auto RangedUnit<V>::GetSegmInfo() const -> const range_data_t *
{
	this->PrepareDataUsage(DrlType::Certain);
	if (this->IsFailed(FR_Data))
		return nullptr;

	return this->GetCurrSegmInfo();
}

template <typename V> 
auto RangedUnit<V>::GetCurrSegmInfo() const -> const range_data_t*
{
	dbg_assert(this->CheckMetaInfoReadyOrPassor());

	const RangedUnit<V>* ultimateCU = debug_cast<const RangedUnit<V>*>(this->GetCurrRangeItem());
	dbg_assert(ultimateCU->CheckMetaInfoReadyOrPassor());
	dbg_assert(CheckCalculatingOrReady(ultimateCU) || ultimateCU->WasFailed(FR_Data));

//	dms_assert(this->PartOfInterestOrKeep() || ultimateCU->DataInMem());
	WaitReady(ultimateCU);
	if (ultimateCU->WasFailed(FR_Data))
		ultimateCU->ThrowFail();
	dbg_assert(CheckDataReady(ultimateCU)); // calculation must have been finished
	return ultimateCU->m_RangeDataPtr.get_ptr();
}


template <typename V> 
bool CountableUnitBase<V>::IsTiled() const
{
	auto si = this->GetSegmInfo();
	if (!si)
	{
		MG_CHECK(this->IsPassor());
		return false;
	}
	return si->GetNrTiles() != 1;
}

template <typename V> 
bool CountableUnitBase<V>::IsCurrTiled() const
{
	auto si = this->GetCurrSegmInfo();
	if (!si)
	{
		MG_CHECK(this->IsPassor());
		return false;
	}
	return si->GetNrTiles() != 1;
}

template <typename V>
auto CountableUnitBase<V>::GetTiledRangeData() const -> SharedPtr <const AbstrTileRangeData>
{ 
	return this->m_RangeDataPtr.get();
}

template <typename V>
V OrderedUnit<V>::GetTileFirstValue (tile_id t) const
{
	dms_assert(t != no_tile);
	auto si = this->GetCurrSegmInfo();
	MG_CHECK(si);
	return si->GetTileRange(t).first;
}

template <typename V>
V OrderedUnit<V>::GetTileValue (tile_id t, tile_offset localIndex) const
{
	dms_assert(t != no_tile);
	auto si = this->GetCurrSegmInfo();
	MG_CHECK(si);
	return Range_GetValue_checked(si->GetTileRange(t), localIndex);
}

template <typename V> typename
auto CountableUnitBase<V>::GetTileRange(tile_id t) const -> range_t
{
	dms_assert(t != no_tile);
	auto si = this->GetCurrSegmInfo();
	MG_CHECK(si);
	return si->GetTileRange(t);
}

template <typename V>
SizeT CountableUnitBase<V>::GetPreparedCount(bool throwOnUndefined) const
{
	return CheckedCardinality(this, this->GetPreparedRange(), throwOnUndefined );
}

template <typename V> 
SizeT CountableUnitBase<V>::GetCount() const
{
	return Cardinality(this->GetRange());
}


template <typename V>
tile_offset CountableUnitBase<V>::GetPreparedTileCount(tile_id t) const
{
	return CheckedCardinality(this, this->GetSegmInfo()->GetTileRange(t), false);
}

template <typename V>
tile_offset CountableUnitBase<V>::GetTileCount(tile_id t) const
{
	return CheckedCardinality(this, this->GetCurrSegmInfo()->GetTileRange(t), false);
}


template <typename V> 
typename CountableUnitBase<V>::value_t 
CountableUnitBase<V>::GetValueAtIndex(row_id i) const
{
	return Range_GetValue_checked(this->GetRange(), i);
}

template <typename V> 
row_id CountableUnitBase<V>::GetIndexForValue(const value_t& v) const
{
	return Range_GetIndex_checked(this->GetRange(), v);
}

//----------------------------------------------------------------------
// IndexableUnitAdapter member funcs implementations
//----------------------------------------------------------------------

template <typename U> 
row_id IndexableUnitAdapter<U>::GetDimSize(DimType dimNr) const
{
	dms_assert(dimNr < this->GetNrDimensions());
	return Unit_GetDimSize(this, dimNr, TYPEID(elem_traits<typename U::value_t>));
}

template <typename U> 
AbstrValue* IndexableUnitAdapter<U>::CreateAbstrValueAtIndex(SizeT i) const
{
	return new ValueWrap<typename U::value_t>(IsDefined(i) ? this->GetValueAtIndex(i) : UNDEFINED_OR_ZERO(typename U::value_t));
}


template <typename U> 
SizeT IndexableUnitAdapter<U>::GetIndexForAbstrValue(const AbstrValue& av) const
{
	return this->GetIndexForValue(debug_cast<const ValueWrap<typename U::value_t>*>(&av)->Get());
}


//----------------------------------------------------------------------
// OrdinalUnit member funcs implementations
//----------------------------------------------------------------------

template <class V>
void OrdinalUnit<V>::SetCount(SizeT count)
{
	this->SetRange(typename OrdinalUnit::range_t(0, ThrowingConvert<V>(count)));
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
//	visitor.Visit(this);
	const UnitVisitor<Unit<V> >& castedVisitor = visitor;
	castedVisitor.Visit(this);
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
	static TokenID result = GetTokenID_st(myArrayPrintF<100>("Unit<%s>", ValueWrap<T>::GetStaticClass()->GetID().c_str_st()));
	return result;
}

template <typename T> 
const UnitClass* Unit<T>::GetStaticClass()
{
	static UnitClass s_Cls(
		CreateFunc<Unit<T> >
	, 	GetUnitClassID<T>()
//	, 	GetTokenID("Unit<" #T ">")
	,	ValueWrap<T>::GetStaticClass());
	return &s_Cls;\
}


template <typename T>
const Class* Unit<T>::GetDynamicClass() const
{
	return GetStaticClass();
}

namespace {
	TypeListClassReg<
		tl::transform<
			typelists::all_unit_types
		,	Unit<_>
		>
	> s_x;

	template <typename V>
	struct TiledUnitInstantiator
	{
		TiledUnitInstantiator()
		{
			&Unit<V>::SetRegularTileRange;
			&Unit<V>::SetIrregularTileRange;
		}

		DefaultTileRangeData<V>* dtr = nullptr;
		RegularTileRangeData<V>* rtr = nullptr;
		IrregularTileRangeData<V>* itr = nullptr;
	};

	tl_oper::inst_tuple_templ<typelists::ranged_unit_objects, RangeProp, bool > unitRangeProps(false);
	tl_oper::inst_tuple_templ<typelists::ranged_unit_objects, RangeProp, bool > unitCatRangeProps(true);

	tl_oper::inst_tuple_templ<typelists::tiled_domain_elements, TiledUnitInstantiator > tui;
}

// Explicit Template Instantiation; TODO G8: Why?
template auto RangedUnit<UInt8>::GetCurrSegmInfo() const -> const range_data_t*;
template auto RangedUnit<Int8>::GetCurrSegmInfo() const -> const range_data_t*;
template auto RangedUnit<UInt16>::GetCurrSegmInfo() const -> const range_data_t*;
template auto RangedUnit<Int16>::GetCurrSegmInfo() const -> const range_data_t*;

template auto CountableUnitBase<Int32>::GetTileRange(tile_id t) const->range_t;
template auto CountableUnitBase<Int64>::GetTileRange(tile_id t) const->range_t;
template auto CountableUnitBase<Int16>::GetTileRange(tile_id t) const->range_t;
template auto CountableUnitBase<Int8>::GetTileRange(tile_id t) const->range_t;
template auto CountableUnitBase<UInt32>::GetTileRange(tile_id t) const->range_t;
template auto CountableUnitBase<UInt64>::GetTileRange(tile_id t) const->range_t;
template auto CountableUnitBase<UInt16>::GetTileRange(tile_id t) const->range_t;
template auto CountableUnitBase<UInt8>::GetTileRange(tile_id t) const->range_t;

//----------------------------------------------------------------------
// C style Interface functions for StaticClass retrieval
//----------------------------------------------------------------------

typedef SharedStr String;

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

			return uc->CreateUnit(parent, GetTokenID_mt(name));

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
		Int32 rowBegin, Int32 colBegin, Int32 rowEnd, Int32 colEnd)
	{
		DMS_CALL_BEGIN

			TreeItemContextHandle checkPtr(self, AbstrUnit::GetStaticClass(), "DMS_GeometricUnit_SetRangeAsIPoint");

			self->SetRangeAsIPoint(rowBegin, colBegin, rowEnd, colEnd);

		DMS_CALL_END
	}

	TIC_CALL void DMS_CONV DMS_GeometricUnit_GetRangeAsIPoint(const AbstrUnit* self,
		Int32* rowBegin, Int32* colBegin, Int32* rowEnd, Int32* colEnd)
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