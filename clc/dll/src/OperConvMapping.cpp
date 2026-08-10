// Copyright (C) 1998-2024 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

// OperConvMapping.cpp - Mapping operator instantiations
// Split from OperConv.cpp for parallel compilation

#include "ClcPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif

#include "OperConv.h"
#include "SeparableMapping.h"

// *****************************************************************************
//			Mapping helper functions (local to this unit)
// *****************************************************************************

// The conversion functor is NOT built here any more: it lives in the shared MappingState that
// AbstrMappingOperator::CreateResult builds once per invocation. When that state carries a
// separable cross (issue #298), a tile is filled straight from the whole-domain x and y arrays
// and no coordinate is transformed at all.
template <typename TR, typename TA, typename TCF, typename RIT>
void do_mapping(const MappingState<TR, TA, TCF>& state, tile_id t, RIT dstIter, RIT dstEnd)
{
	auto tileRange = state.GetTileRange(t);
	SizeT n = Cardinality(tileRange);
	MG_CHECK(dstIter + n == dstEnd);

	if constexpr (MappingState<TR, TA, TCF>::has_cross_support)
		if (state.m_HasCross)
		{
			FillTileFromCross<TA>(state.m_Cross, dstIter, tileRange, state.m_DomainRange);
			return;
		}
	DispatchMapping(state.GetFunctor(), dstIter, tileRange, n);
}

template <typename Cardinal, typename TR, typename TA, typename TCF, typename RIT>
void do_mapping_count(const MappingCountState<TR, TA, TCF>& state, tile_id t, RIT dstIter, RIT dstEnd)
{
	auto srcTileRange = state.GetTileRange(t);
	auto dstRange = state.m_DstUnit->GetRange();
	SizeT n = Cardinality(dstRange);
	MG_CHECK(dstIter + n == dstEnd);

	if constexpr (MappingCountState<TR, TA, TCF>::has_cross_support)
		if (state.m_HasCross && state.m_CrossIndex.m_IsValid)
		{
			// The outer product skips the source cells altogether; it declines only when the
			// destination window is larger than the tile, and then the per-cell walk is cheaper.
			if (CountTileFromCrossProduct<TA, Cardinal>(state.m_CrossIndex, dstIter, srcTileRange, state.m_DomainRange, n))
				return;
			CountTileFromCrossIndex<TA>(state.m_CrossIndex, dstIter, srcTileRange, state.m_DomainRange, n);
			return;
		}
	DispatchMappingCount(state.GetFunctor(), dstIter, srcTileRange, dstRange, n);
}

// *****************************************************************************
//			Mapping operator classes (local to this unit)
// *****************************************************************************

template <typename TR, typename TA>
class MappingOperator : public AbstrMappingOperator
{
	typedef DataArray<TR> ResultType;
	typedef Unit<TA> Arg1Type;
	typedef Unit<TR> Arg2Type;

public:
	MappingOperator(CommonOperGroup* cog)
		: AbstrMappingOperator(cog
			, ResultType::GetStaticClass()
			, Arg1Type::GetStaticClass()
			, Arg2Type::GetStaticClass()
		)
	{}

	using StateType = MappingState<TR, TA, TypeConversionF<std::false_type>>;

	auto CreateMappingState(const AbstrUnit* argDomainUnit, const AbstrUnit* argValuesUnit) const -> std::shared_ptr<AbstrMappingState> override
	{
		return std::make_shared<StateType>(
			debug_cast<const Unit<TR>*>(argValuesUnit),
			debug_cast<const Unit<TA>*>(argDomainUnit)
		);
	}

	void Calculate(AbstrDataObject* borrowedDataHandle, const AbstrMappingState& state, tile_id t) const override
	{
		auto resultData = mutable_array_cast<TR>(borrowedDataHandle)->GetDataWrite(t, dms_rw_mode::write_only_all);

		do_mapping<TR, TA, TypeConversionF<std::false_type>>(
			*debug_cast<const StateType*>(&state), t,
			resultData.begin(), resultData.end()
		);
	}
};

template <typename TR, typename TA, typename Cardinal = UInt32>
class MappingCountOperator : public AbstrMappingCountOperator
{
	typedef DataArray<Cardinal> ResultType;
	typedef Unit<TA> Arg1Type;
	typedef Unit<TR> Arg2Type;
	typedef Unit<Cardinal> Arg3Type;

public:
	MappingCountOperator(CommonOperGroup* cog)
		: AbstrMappingCountOperator(cog
			, ResultType::GetStaticClass()
			, Arg1Type::GetStaticClass()
			, Arg2Type::GetStaticClass()
			, Arg3Type::GetStaticClass()
		)
	{}

	using StateType = MappingCountState<TR, TA, TypeConversionF<std::false_type>>;

	auto CreateMappingState(const AbstrUnit* argDomainUnit, const AbstrUnit* argValuesUnit) const -> std::shared_ptr<AbstrMappingState> override
	{
		return std::make_shared<StateType>(
			debug_cast<const Unit<TR>*>(argValuesUnit),
			debug_cast<const Unit<TA>*>(argDomainUnit)
		);
	}

	bool HasIndependentResultTiles(const AbstrMappingState& state) const override
	{
		if constexpr (StateType::has_cross_support)
		{
			auto& typedState = *debug_cast<const StateType*>(&state);
			// An irregular source tiling is fine here: m_CrossCounts then holds one histogram
			// pair per source tile and a destination tile aggregates the ones that overlap it.
			return typedState.m_HasCross && typedState.m_HasCountProduct;
		}
		else
			return false;
	}

	// One RESULT tile: one multiplication per destination cell, written straight into that tile.
	// Nothing is read from the source and nothing is accumulated, so this is safe to run for any
	// tile, in any order, on any thread -- which is what lets the result be a LazyTileFunctor.
	void Calculate(AbstrDataObject* borrowedDataHandle, const AbstrMappingState& state, tile_id t) const override
	{
		if constexpr (StateType::has_cross_support)
		{
			auto& typedState = *debug_cast<const StateType*>(&state);
			auto resultData = mutable_array_cast<Cardinal>(borrowedDataHandle)->GetWritableTile(t, dms_rw_mode::write_only_all);

			FillCountTileFromProduct<Cardinal, TR>(typedState.m_CrossCounts, resultData.begin()
				, typedState.GetDstTileRange(t), typedState.GetDstRange());
		}
		else
			throwIllegalAbstract(MG_POS, "MappingCountOperator::Calculate"); // gated by HasIndependentResultTiles
	}

	void AccumulateFromSourceTiles(DataWriteHandle& borrowedDataHandle, const AbstrMappingState& state, tile_id nrSrcTiles) const override
	{
		// One handle for the whole loop: see the comment in AbstrMappingCountOperator.
		auto resultData = mutable_array_cast<Cardinal>(borrowedDataHandle)->GetDataWrite(no_tile, dms_rw_mode::read_write);
		auto& typedState = *debug_cast<const StateType*>(&state);

		for (tile_id t = 0; t != nrSrcTiles; ++t)
			do_mapping_count<Cardinal, TR, TA, TypeConversionF<std::false_type>>(
				typedState, t, resultData.begin(), resultData.end()
			);
	}
};

namespace {

	// Operator groups - local to this translation unit
	CommonOperGroup cog_mapping("mapping");
	CommonOperGroup cog_mapping_count("mapping_count");

	// Mapping operator templates
	template <typename TRL>
	struct mappingOpers
	{
		template <typename TA>
		struct apply_TA
		{
			tl_oper::inst_tuple<TRL, tl::bind_placeholders<MappingOperator, ph::_1, TA> > m_MappingOpers{ &cog_mapping };
		};
	};

	template <typename TRL>
	struct mappingCountOpers
	{
		template <typename TA>
		struct apply_TA
		{
			tl_oper::inst_tuple<TRL, tl::bind_placeholders<MappingCountOperator, ph::_1, TA, UInt8 > > m_MappingCountOpers_08{ &cog_mapping_count };
			tl_oper::inst_tuple<TRL, tl::bind_placeholders<MappingCountOperator, ph::_1, TA, UInt16> > m_MappingCountOpers_16{ &cog_mapping_count };
			tl_oper::inst_tuple<TRL, tl::bind_placeholders<MappingCountOperator, ph::_1, TA, UInt32> > m_MappingCountOpers_32{ &cog_mapping_count };
		};
	};

	// Numeric mapping operators
	// typelists::ints × typelists::num_objects
	tl_oper::inst_tuple_templ<typelists::ints, mappingOpers<typelists::num_objects>::apply_TA > numericMappingOpers;

	// Point mapping operators
	// typelists::domain_points × typelists::points
	tl_oper::inst_tuple_templ<typelists::domain_points, mappingOpers<typelists::points>::apply_TA > pointMappingOpers;

	// Point mapping count operators
	// typelists::domain_points × typelists::domain_points × {UInt8, UInt16, UInt32}
	tl_oper::inst_tuple_templ<typelists::domain_points, mappingCountOpers<typelists::domain_points>::apply_TA > pointMappingCountOpers;

} // end anonymous namespace
