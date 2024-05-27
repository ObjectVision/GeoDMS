// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "ClcPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

#include "geo/Conversions.h"
#include "ser/StringStream.h"

#include "CheckedDomain.h"
#include "LispTreeType.h"
#include "OperGroups.h"
#include "ParallelTiles.h"
#include "TreeItemClass.h"
#include "UnitClass.h"

static TokenID readPosToken = GetTokenID_st("ReadPos");

// *****************************************************************************
//										ReadNumbersOperator
// *****************************************************************************

#include "MoreDataControllers.h"
#include "UnitProcessor.h"

struct NumberReaderBase : UnitProcessor
{
	// Closure
	NumberReaderBase() {}

	void SkipSeparator() const
	{
		char ch = m_FIS->NextChar();
		if (m_FIS->IsFieldSeparator(ch))
			m_FIS->ReadChar();
	}

	template <typename E>
	void VisitImpl(const Unit<E>* inviter) const
	{
		assert(m_ResPtr);
		assert(m_FIS);
		assert(m_Offset!= -1);
		assert(m_Count != -1);

		auto seq = mutable_array_cast<E>(*m_ResPtr)->GetDataWrite(m_TileID, dms_rw_mode::read_write);
		auto elem = E();

		assert(m_Offset           <= seq.size());
		assert(m_Count            <= seq.size());
		assert(m_Offset + m_Count <= seq.size());

		for (auto p = seq.begin()+m_Offset, e = p + m_Count; p!=e; ++p)
		{
			*m_FIS >> elem;
			*p = elem;
			SkipSeparator();
		}
	}
public:
	DataWriteLock*      m_ResPtr = nullptr;
	FormattedInpStream* m_FIS = nullptr;
	tile_id             m_TileID = no_tile;
	SizeT               m_Offset = 0, m_Count = 0;
};

struct NumberReader :  boost::mpl::fold<typelists::scalars, NumberReaderBase, VisitorImpl<Unit<_2>, _1> >::type
{};


//==================================================================================

struct ReadArrayOperator: public QuinaryOperator
{
	typedef DataArray<SharedStr> Arg1Type;
	typedef AbstrUnit            Arg2Type;
	typedef AbstrUnit            Arg3Type;
	typedef DataArray<UInt32>    Arg4Type;
	typedef DataArray<UInt32>    Arg5Type;

	ReadArrayOperator(AbstrOperGroup* og)
		:	QuinaryOperator(og
			,	AbstrDataItem::GetStaticClass()
			,	Arg1Type::GetStaticClass()
			,	Arg2Type::GetStaticClass()
			,	Arg3Type::GetStaticClass()
			,	Arg4Type::GetStaticClass()
			,	Arg5Type::GetStaticClass()
			)
	{
		m_NrOptionalArgs = 1;
	}

	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		dms_assert(args.size() == 4 || args.size() == 5 );
		const TreeItem* arg1 = args[0];
		dms_assert(arg1);

		checked_domain<Void>(args[0], "a1");
		checked_domain<Void>(args[3], "a4");
		if (args.size() >= 5)
			checked_domain<Void>(args[4], "a5");

		const AbstrUnit* adu = AsUnit(args[1]);
		const AbstrUnit* avu = AsUnit(args[2]);

		if (!resultHolder)
			resultHolder = CreateCacheDataItem(adu, avu);
		AbstrDataItem* res = AsDataItem(resultHolder.GetNew());

		AbstrDataItem* resReadPos = CreateDataItem(res, readPosToken
		,	Unit<Void  >::GetStaticClass()->CreateDefault()
		,	Unit<UInt32>::GetStaticClass()->CreateDefault()
		);

		if (!mustCalc)
			return true;

		FormattedInpStream::reader_flags flags = FormattedInpStream::reader_flags();
		if (args.size() >= 5)
		{
			DataReadLock lock4(AsDataItem(args[4]));
			flags = (FormattedInpStream::reader_flags)GetTheCurrValue<UInt32>(args[4]);
		}

		streamsize_t readPos;
		{
			DataReadLock lock0(AsDataItem(args[3]));
			readPos = GetTheCurrValue<UInt32>(args[3]);
		}

		DataReadLock drl0(AsDataItem(args[0]));
		auto readArrayData = const_array_cast<SharedStr>(args[0])->GetDataRead();
		Arg1Type::const_reference dataValues = readArrayData[0];

		if (readPos > dataValues.size())
			throwErrorF("ReadArray", "readPos %d is larger than dataArraySize %d"
			,	readPos
			,	dataValues.size()
			);
		MemoInpStreamBuff inpBuff(dataValues.begin()+readPos, dataValues.end());
		FormattedInpStream dataValuesStream(&inpBuff, flags);

		DataWriteLock dwl(res); // NYI: Deal with this

		NumberReader dr;
		dr.m_FIS = &dataValuesStream;
		dr.m_ResPtr = &dwl;
		dr.m_Offset = 0;
		dr.m_Count  = adu->GetCount();

		avu->InviteUnitProcessor(dr);
		dwl.Commit();

		SetTheValue<UInt32>(resReadPos, ThrowingConvert<UInt32>(readPos + dataValuesStream.CurrPos()));

		return true;
	}
};

//==================================================================================


struct ReadElemsOperator: public QuaternaryOperator
{
	typedef DataArray<SharedStr> Arg1Type; // String(s)
	typedef AbstrUnit            Arg2Type; // Values unit of result
	typedef DataArray<UInt32>    Arg3Type; // ReadPos(ses)
	typedef DataArray<UInt32>    Arg4Type; // Optional Flags, see below

	ReadElemsOperator(AbstrOperGroup* og)
		:	QuaternaryOperator(og
			,	AbstrDataItem::GetStaticClass()
			,	Arg1Type::GetStaticClass()
			,	Arg2Type::GetStaticClass()
			,	Arg3Type::GetStaticClass()
			,	Arg4Type::GetStaticClass()
			)
	{
		m_NrOptionalArgs = 1;
	}

	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		dms_assert(args.size() == 3 || args.size() == 4);
		const TreeItem* arg1 = args[0];
		dms_assert(arg1);

		const AbstrUnit* adu = AsDataItem(arg1)->GetAbstrDomainUnit();
		const AbstrUnit* avu = AsUnit(args[1]);

		adu->UnifyDomain(AsDataItem(args[2])->GetAbstrDomainUnit(), "e1", "e2", UM_Throw);

		if (args.size() >= 4)
			checked_domain<Void>(args[3], "a4");

		if (!resultHolder)
			resultHolder = CreateCacheDataItem(adu, avu);
		AbstrDataItem* res = debug_cast<AbstrDataItem*>(resultHolder.GetNew());

		AbstrDataItem* resReadPos = CreateDataItem(res, readPosToken
		,	adu
		,	Unit<UInt32>::GetStaticClass()->CreateDefault()
		);

		if (!mustCalc)
			return true;


		DataReadLock drl0(AsDataItem(args[0]));
		DataReadLock drl2(AsDataItem(args[2]));

		DataWriteLock dwl        (res, dms_rw_mode::write_only_mustzero);
		DataWriteLock resReadPosHandle(resReadPos);

		FormattedInpStream::reader_flags flags = FormattedInpStream::reader_flags();
		if (args.size() >= 4)
		{
			DataReadLock lock3(AsDataItem(args[3]));
			flags = (FormattedInpStream::reader_flags)GetTheCurrValue<UInt32>(args[3]);
		}

		parallel_tileloop(adu->GetNrTiles(), [&args, &dwl, &resReadPosHandle, avu, flags](tile_id t)->void
		{
			auto dataValueArray = const_array_cast<SharedStr>(args[0])->GetTile(t);
			auto readPosArray   = const_array_cast<UInt32   >(args[2])->GetTile(t);
			tile_offset n = dataValueArray.size();
			dms_assert(readPosArray.size() == n);

			auto resReadPosArray = mutable_array_cast<UInt32>(resReadPosHandle)->GetWritableTile(t);

			NumberReader dr;
			dr.m_ResPtr = &dwl;
			dr.m_TileID = t;
			dr.m_Count  = 1;

			for (tile_offset i=0; i!=n; ++i)
			{
				ReadElemsOperator::Arg1Type::const_reference dataValueStr = dataValueArray[i];
				UInt32 readPos = readPosArray[i];
				if (readPos > dataValueStr.size())
					throwErrorF("ReadElems", "Elem %d: readPos %d is larger than dataArraySize %d",
						i,
						readPos,
						dataValueStr.size()
					);
				MemoInpStreamBuff inpBuff(dataValueStr.begin()+readPos, dataValueStr.end());
				FormattedInpStream dataValuesStream(&inpBuff, flags);

				dr.m_FIS = &dataValuesStream;
				dr.m_Offset = i;
				avu->InviteUnitProcessor(dr);
				resReadPosArray[i] = ThrowingConvert<UInt32>( readPos + dataValuesStream.CurrPos() );
			}
		}
		);

		dwl.Commit();
		resReadPosHandle.Commit();

		return true;
	}
};

//==================================================================================

inline bool eofch(char ch)
{
	return !ch;
}

inline bool eolch(char ch)
{
	return ch ==char(10) || ch == char(13) || ch == char(14) || eofch(ch);
}

struct ReadLinesOperator: public TernaryOperator
{

	typedef DataArray<SharedStr> ResultType;
	typedef DataArray<SharedStr> Arg1Type; // String to read from
	typedef AbstrUnit            Arg2Type; // domain of resulting array of lines
	typedef DataArray<UInt32>    Arg3Type; // readPos to start reading 

	ReadLinesOperator(AbstrOperGroup* og)
		:	TernaryOperator(og
			,	ResultType::GetStaticClass()
			,	Arg1Type::GetStaticClass()
			,	Arg2Type::GetStaticClass()
			,	Arg3Type::GetStaticClass()
			)
	{}

	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		dms_assert(args.size() == 3);
		const TreeItem* arg1 = args[0];
		dms_assert(arg1);

		checked_domain<Void>(args[0], "a1");
		checked_domain<Void>(args[2], "a3");

		const AbstrUnit* adu = AsUnit(args[1]);
		const AbstrUnit* avu = Unit<SharedStr>::GetStaticClass()->CreateDefault();

		if (!resultHolder)
			resultHolder = CreateCacheDataItem(adu, avu);
		AbstrDataItem* res = debug_cast<AbstrDataItem*>(resultHolder.GetNew());
		dms_assert(res->IsPassor());

		AbstrDataItem* resReadPos = CreateDataItem(res, readPosToken
		,	Unit<Void  >::GetStaticClass()->CreateDefault()
		,	Unit<UInt32>::GetStaticClass()->CreateDefault()
		);
		dms_assert(resReadPos->IsPassor());

		if (!mustCalc)
			return true;

		UInt32 readPos;
		{
			DataReadLock lock2(AsDataItem(args[2]));
			readPos = GetTheCurrValue<UInt32>(args[2]);
		}

		DataReadLock drl0(AsDataItem(args[0]));
		Arg1Type::const_reference dataValues = const_array_cast<SharedStr>(args[0])->GetDataRead()[0];

		if (readPos > dataValues.size())
			throwErrorF("ReadLines", "readPos %d is larger than dataArraySize %d",
				readPos,
				dataValues.size()
			);
		MemoInpStreamBuff inpBuff(dataValues.begin()+readPos, dataValues.end());
		FormattedInpStream dataValuesStream(&inpBuff);

		DataWriteLock dwl(res);

		auto seq = mutable_array_cast<SharedStr>(dwl)->GetDataWrite();
		for (auto ref: seq)
		{
			// read line into ref
			char ch;
			while (ch = dataValuesStream.NextChar(), !eolch(ch))
			{
				ref.push_back(ch);
				dataValuesStream.ReadChar();
			}
			if (eofch(ch))
				break;
			while (ch = dataValuesStream.ReadChar(), eolch(ch) && !eofch(ch))
				;
		}

		dwl.Commit();

		DataWriteLock readPosLock(resReadPos);
		readPosLock->SetValue<UInt32>(0, ThrowingConvert<UInt32>( readPos + dataValuesStream.CurrPos() ));
		readPosLock.Commit();

		return true;
	}
};

template<typename SequenceValueType, typename ElementPredicate, typename ResultDomainType = UInt32>
struct SplitSequenceOperator : public UnaryOperator
{
	using ResultType = Unit<ResultDomainType>;
	using ResultSubType = DataArray<SequenceValueType>;

	using Arg1Type = DataArray<SequenceValueType> ; // Sequence array to read from

	ElementPredicate m_ElementPredicate;

	SplitSequenceOperator(AbstrOperGroup* og, ElementPredicate&& pred)
		: UnaryOperator(og, ResultType::GetStaticClass() , Arg1Type::GetStaticClass())
		, m_ElementPredicate(std::move(pred))
	{}

	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		assert(args.size() == 1);
		auto arg1 = AsDataItem(args[0]);
		assert(arg1);

		if (!resultHolder)
			resultHolder = ResultType::GetStaticClass()->CreateResultUnit(resultHolder);

		auto res = AsUnit(resultHolder.GetNew());
		assert(res->IsPassor());

		auto adu = arg1->GetAbstrDomainUnit();
		auto avu = arg1->GetAbstrValuesUnit();
		auto resSequenceItem = CreateDataItem(res, token::geometry, res, avu, composition_of_v< SequenceValueType>);

		AbstrDataItem* resOrgRelItem = nullptr;
		if (!arg1->HasVoidDomainGuarantee())
			resOrgRelItem = CreateDataItem(res, token::org_rel, res, adu);

		if (!mustCalc)
			return true;

		DataReadLock lock(arg1);
		auto sequences = const_array_cast<SequenceValueType>(args[0])->GetDataRead();

		SizeT nrBreaks = 0;
		for (auto seqRef : sequences)
		{
			for (const auto& elem : seqRef)
				if (m_ElementPredicate(elem))
					++nrBreaks;
		}
		
		SizeT nrSubSequences = sequences.size() + nrBreaks;
		res->SetCount(nrSubSequences);

		DataWriteLock dwlSequences(resSequenceItem, dms_rw_mode::write_only_all);
		DataWriteLock dwlOrgRel   (resOrgRelItem  , dms_rw_mode::write_only_all);

		auto resSequences = mutable_array_cast<SequenceValueType>(dwlSequences)->GetDataWrite(no_tile, dms_rw_mode::write_only_all);
		
		resSequences.get_sa().data_reserve(sequences.get_sa().data_size() - nrBreaks MG_DEBUG_ALLOCATOR_SRC("SplitSequenceOperator: resSequences.data_reserve"));
		auto resSequenceIter = resSequences.begin();
		SizeT orgRelIndex = 0;

		auto updateOrgRelIndexOperation = [&]()
			{
				visit<typelists::domain_elements>(adu,[&]<typename D>(const Unit<D>*du)
				{
					auto resOrgRelItem = mutable_array_cast<D>(dwlOrgRel);
					auto resIndex = resSequenceIter - resSequences.begin();
					auto orgIndex = du->GetValueAtIndex(orgRelIndex);
					resOrgRelItem->SetIndexedValue(resIndex, orgIndex);
				});
			};

		for (auto seqRef : sequences)
		{
			auto seqStartPtr = seqRef.begin(), seqEndPtr = seqRef.end();
			auto seqPtr = seqStartPtr;
			while (seqPtr != seqEndPtr)
			{
				if (m_ElementPredicate(*seqPtr))
				{
					if (dwlOrgRel)
						updateOrgRelIndexOperation();
					resSequenceIter->assign(seqStartPtr, seqPtr);
					++resSequenceIter;
					seqStartPtr = ++seqPtr;
				}
				else
					++seqPtr;
			}
			if (dwlOrgRel)
				updateOrgRelIndexOperation();
			resSequenceIter->assign(seqStartPtr, seqPtr);
			++resSequenceIter;
			++orgRelIndex;
		}
		MG_CHECK(resSequenceIter == resSequences.end());

		dwlSequences.Commit();
		dwlOrgRel.Commit();
		return true;
	}
};


//==================================================================================

namespace {
	CommonOperGroup cog_ReadArray("ReadArray", oper_policy::dynamic_result_class); // REMOVE, Make result class static DataArry with values based on Arg3Type
	CommonOperGroup cog_ReadElems("ReadElems", oper_policy::dynamic_result_class); // REMOVE, Make result class static DataArry with values based on Arg2Type
	CommonOperGroup cog_ReadLines("ReadLines");
	CommonOperGroup cog_SplitPipedString("split_piped_string", oper_policy::dynamic_result_class);
	CommonOperGroup cog_SplitMultiLineString("split_multi_linestring", oper_policy::dynamic_result_class);
	CommonOperGroup cog_SplitMultiNumberString("split_multi_numberstring", oper_policy::dynamic_result_class);


	//==================================================================================
	ReadArrayOperator readArray(&cog_ReadArray);
	ReadElemsOperator readElems(&cog_ReadElems);
	ReadLinesOperator readLinesOperator(&cog_ReadLines);

	auto isEOL = [](char ch) { return eolch(ch); };
	SplitSequenceOperator<SharedStr, decltype(isEOL)> splitLines(&cog_ReadLines, std::move(isEOL));

	auto isPipe = [](char ch) { return ch == '|'; };
	SplitSequenceOperator<SharedStr, decltype(isPipe)> splitPipedString(&cog_SplitPipedString, std::move(isPipe));

	auto isSeparator = [](const auto& p) { return !IsDefined(p); };

	template <typename Sequence>
	struct SPSO {
		SplitSequenceOperator<Sequence, decltype(isSeparator)> splitLinestring = { &cog_SplitMultiLineString, std::move(isSeparator) };
	};
	template <typename Sequence>
	struct SNSO {
		SplitSequenceOperator<Sequence, decltype(isSeparator)> splitLinestring = { &cog_SplitMultiNumberString, std::move(isSeparator) };
	};

	tl_oper::inst_tuple_templ<typelists::point_sequences, SPSO> splitMultiLineStrings;
	tl_oper::inst_tuple_templ<typelists::numeric_sequences, SNSO> splitMultiNumberStrings;
}
