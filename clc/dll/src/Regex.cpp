// Copyright (C) 1998-2024 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "ClcPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif

#include "Operator.h"

#include "ser/AsString.h"

#include <boost/regex.hpp>

#include "CheckedDomain.h"
#include "ParallelTiles.h"
#include "Unit.h"
#include "UnitClass.h"
#include "DataArrayValue.h"

// *****************************************************************************
//										regex_search
// *****************************************************************************

typedef UInt32 strlen_t;

struct RegexSearchOperator : CommonOperGroup, TernaryOperator
{
	RegexSearchOperator() 
		:	CommonOperGroup("regex_search")
		,	TernaryOperator(this,
				DataArray<SharedStr>::GetStaticClass(),
				DataArray<SharedStr>::GetStaticClass(),
				DataArray<SharedStr>::GetStaticClass(),
				DataArray<UInt32>::GetStaticClass()
			)
	{
		m_NrOptionalArgs = 1;
	}

	// Override Operator
	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		dms_assert(args.size() == 2 || args.size() == 3);

		const AbstrDataItem* arg1A = AsDataItem(args[0]);
		const AbstrUnit* e1 = arg1A->GetAbstrDomainUnit();

		checked_domain<Void>(args[1], "a2");
		if (args.size() >= 3)
			checked_domain<Void>(args[2], "a3");


		if (!resultHolder)
			resultHolder = CreateCacheDataItem(e1, arg1A->GetAbstrValuesUnit());
		
		if (mustCalc)
		{
			DataReadLock lock1(AsDataItem(args[1]));
			boost::regex rx( GetTheCurrValue<SharedStr>(args[1]).c_str() );

			boost::regex_constants::match_flag_type flags = boost::regex_constants::match_default;
			if (args.size() >= 3) // (str, regex, flags): the flags are the third argument, not the fourth of regex_replace
			{
				DataReadLock lock2(AsDataItem(args[2]));
				flags = (boost::regex_constants::match_flag_type)GetTheCurrValue<UInt32>(args[2]);
			}

			DataReadLock a1Lock(arg1A);
			const DataArray<SharedStr>* arg1 = const_array_cast<SharedStr>(a1Lock);

			AbstrDataItem* results = AsDataItem(resultHolder.GetNew());
			DataWriteLock resLock(results, dms_rw_mode::write_only_mustzero);
			DataArray<SharedStr>* res = mutable_array_cast<SharedStr>(resLock);

			parallel_tileloop(e1->GetNrTiles(), [res, arg1, &rx, flags] (tile_id t)->void 
				{
					auto data = arg1->GetLockedDataRead(t);

					// CLC-A30: the search runs once per element; its match is kept, as a range into the argument
					// tile that stays locked, for the write pass, which ran the search a second time.
					std::vector<std::pair<const char*, const char*>> matches(data.size(), { nullptr, nullptr });
					SizeT totalSize = 0;
					auto matchPtr = matches.begin();
					for (auto i=data.begin(), e=data.end(); i!=e; ++i, ++matchPtr)
					{
						boost::cmatch matchResult;

						if (i->IsDefined() && boost::regex_search(i->begin(), i->end(), matchResult, rx, flags))
						{
							*matchPtr = { matchResult[0].first, matchResult[0].second };
							totalSize += (matchResult[0].second - matchResult[0].first);
						}
					}

					DataArray<SharedStr>::locked_seq_t resData = res->GetLockedDataWrite(t, dms_rw_mode::write_only_mustzero);
					resData.get_sa().data_reserve(totalSize MG_DEBUG_ALLOCATOR_SRC("res->md_SrcStr"));
					auto resI = resData.begin();

					for (const auto& match: matches)
					{
						if (match.first)
							resI->assign(match.first, match.second MG_DEBUG_ALLOCATOR_SRC("RegexSearch"));
						else
							resI->assign(Undefined());
						++resI;
					}
					dms_assert(resData.get_sa().actual_data_size() == totalSize); // the count predicts the data; the capacity can be a larger store
				}
			);
			resLock.Commit();
		}
		return true;
	}
};

struct RegexMatchOperator : CommonOperGroup, TernaryOperator
{
	RegexMatchOperator() 
		:	CommonOperGroup("regex_match")
		,	TernaryOperator(this,
				DataArray<Bool>::GetStaticClass(),
				DataArray<SharedStr>::GetStaticClass(),
				DataArray<SharedStr>::GetStaticClass(),
				DataArray<UInt32>::GetStaticClass()
			)
	{
		m_NrOptionalArgs = 1;
	}

	// Override Operator
	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		dms_assert(args.size() == 2 || args.size() == 3);

		const AbstrDataItem* arg1A = AsDataItem(args[0]);
		const AbstrUnit* e1 = arg1A->GetAbstrDomainUnit();
		checked_domain<Void>(args[1], "a2");
		if (args.size() >= 3)
			checked_domain<Void>(args[2], "a3");

		if (!resultHolder)
			resultHolder = CreateCacheDataItem(e1, Unit<Bool>::GetStaticClass()->CreateDefault());
		
		if (mustCalc)
		{
			DataReadLock lock1( AsDataItem(args[1]) );

			boost::regex rx( GetTheCurrValue<SharedStr>(args[1]).c_str() );
			boost::regex_constants::match_flag_type flags = boost::regex_constants::match_default;
			if (args.size() >= 3) // (str, regex, flags): the flags are the third argument, not the fourth of regex_replace
			{
				DataReadLock lock2( AsDataItem(args[2]) );
				flags = (boost::regex_constants::match_flag_type)GetTheCurrValue<UInt32>( args[2] );
			}

			DataReadLock a1Lock(arg1A);
			const DataArray<SharedStr>* arg1 = const_array_cast<SharedStr>(a1Lock);

			AbstrDataItem* results = AsDataItem(resultHolder.GetNew());
			DataWriteLock resLock(results, dms_rw_mode::write_only_all);
			DataArray<Bool>* res = mutable_array_cast<Bool>(resLock);

			parallel_tileloop(e1->GetNrTiles(), [res, arg1, &rx, flags] (tile_id t)->void 
				{
					auto data = arg1->GetTile(t);
					auto resData = res->GetWritableTile(t, dms_rw_mode::write_only_all);

					auto resI = resData.begin();

					for (auto i=data.begin(), e=data.end(); i!=e; ++resI, ++i)
					{
						*resI = (i->IsDefined() && boost::regex_match(i->begin(), i->end(), rx, flags));
					}
				}
			);
			resLock.Commit();
		}
		return true;
	}
};


struct dev_null
{
	template<typename T>
	void operator =(const T& v)
	{}
};

struct count_iterator
{
	count_iterator(SizeT count = 0)
		:	m_Count(count)
	{}

	count_iterator& operator ++()   { ++m_Count; return *this; }
	count_iterator operator ++(int) { return m_Count++;  }

	dev_null operator *() const { return dev_null(); }
	operator SizeT() const { return m_Count; }

	SizeT m_Count;
};

struct RegexReplaceOperator : CommonOperGroup, QuaternaryOperator
{
	RegexReplaceOperator() 
		:	CommonOperGroup("regex_replace")
		,	QuaternaryOperator(this,
				DataArray<SharedStr>::GetStaticClass(),
				DataArray<SharedStr>::GetStaticClass(),
				DataArray<SharedStr>::GetStaticClass(),
				DataArray<SharedStr>::GetStaticClass(),
				DataArray<UInt32>::GetStaticClass()
			)
	{
		m_NrOptionalArgs = 1;
	}

	// Override Operator
	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		dms_assert(args.size() == 3 || args.size() == 4);

		const AbstrDataItem* arg1A = AsDataItem(args[0]);
		const AbstrUnit* e1 = arg1A->GetAbstrDomainUnit();

		checked_domain<Void>(args[1], "a2");
		checked_domain<Void>(args[2], "a3");
		if (args.size() >= 4)
			checked_domain<Void>(args[3], "a4");

		if (!resultHolder)
			resultHolder = CreateCacheDataItem(e1, arg1A->GetAbstrValuesUnit());
		
		if (mustCalc)
		{
			AbstrDataItem* results = AsDataItem(resultHolder.GetNew());

			DataReadLock lock1( AsDataItem(args[1]) ); boost::regex rx ( GetTheCurrValue<SharedStr>(args[1]).c_str() );
			DataReadLock lock2( AsDataItem(args[2]) ); std::string  fmt( GetTheCurrValue<SharedStr>(args[2]).c_str() );
			boost::regex_constants::match_flag_type flags = boost::regex_constants::match_default;
			if (args.size() >= 4)
			{
				DataReadLock lock3( AsDataItem(args[3]) );
				flags = (boost::regex_constants::match_flag_type)GetTheCurrValue<UInt32>(args[3]);
			}
			DataReadLock a1Lock(arg1A);
			const DataArray<SharedStr>* arg1 = const_array_cast<SharedStr>(arg1A);

			DataWriteLock resLock(results, dms_rw_mode::write_only_mustzero);
			DataArray<SharedStr>* res = mutable_array_cast<SharedStr>(resLock);
			parallel_tileloop(e1->GetNrTiles(), [res, arg1, &rx, &fmt, flags] (tile_id t)->void
				{
					auto data = arg1->GetTile(t);

					// CLC-A30: the replacement runs once per element, into one buffer for the tile; the write
					// pass copies each element's part, where it ran the replacement a second time. An element
					// that is not defined keeps the end offset of its predecessor and is written as undefined.
					std::string replaced;
					std::vector<SizeT> ends(data.size());
					auto endPtr = ends.begin();
					for (auto i=data.begin(), e=data.end(); i!=e; ++i, ++endPtr)
					{
						if (i->IsDefined())
							boost::regex_replace(std::back_inserter(replaced), i->begin(), i->end(), rx, fmt, flags);
						*endPtr = replaced.size();
					}
					SizeT totalSize = replaced.size();

					DataArray<SharedStr>::locked_seq_t resData = res->GetWritableTile(t, dms_rw_mode::write_only_mustzero);
					resData.get_sa().data_reserve(totalSize MG_DEBUG_ALLOCATOR_SRC("res->md_SrcStr + : RegexReplaceOperator.data_reserve()"));
					auto resI = resData.begin();

					SizeT begin = 0;
					endPtr = ends.begin();
					for (auto i=data.begin(), e=data.end(); i!=e; ++resI, ++i, ++endPtr)
					{
						if (i->IsDefined())
							resI->assign(replaced.data() + begin, replaced.data() + *endPtr MG_DEBUG_ALLOCATOR_SRC("RegexReplace"));
						else
							resI->assign(Undefined());
						begin = *endPtr;
					}
					dms_assert(resData.get_sa().actual_data_size() == totalSize); // the count predicts the data; the capacity can be a larger store
				}
			);
			resLock.Commit();
		}
		return true;
	}
};

namespace {
	RegexSearchOperator theRegexSearchOper;
	RegexMatchOperator theRegexMatchOper;
	RegexReplaceOperator theRegexReplaceOper;
}