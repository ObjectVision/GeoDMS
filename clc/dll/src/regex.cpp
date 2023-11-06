// Copyright (C) 2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "ClcPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

#include "Operator.h"

#include "ser/AsString.h"

#include <boost/regex.hpp>

#include "CheckedDomain.h"
#include "ParallelTiles.h"
#include "Unit.h"
#include "UnitClass.h"

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
			if (args.size() >= 4)
			{
				DataReadLock lock3(AsDataItem(args[3]));
				flags = (boost::regex_constants::match_flag_type)GetTheCurrValue<UInt32>(args[3]);
			}

			DataReadLock a1Lock(arg1A);
			const DataArray<SharedStr>* arg1 = const_array_cast<SharedStr>(a1Lock);

			AbstrDataItem* results = AsDataItem(resultHolder.GetNew());
			DataWriteLock resLock(results, dms_rw_mode::write_only_mustzero);
			DataArray<SharedStr>* res = mutable_array_cast<SharedStr>(resLock);

			parallel_tileloop(e1->GetNrTiles(), [res, arg1, &rx, flags] (tile_id t)->void 
				{
					auto data = arg1->GetLockedDataRead(t);

					SizeT totalSize = 0;
					for (auto i=data.begin(), e=data.end(); i!=e; ++i)
					{
						boost::cmatch matchResult;

						if (i->IsDefined() && boost::regex_search(i->begin(), i->end(), matchResult, rx, flags))
							totalSize += (matchResult[0].second - matchResult[0].first);
					}

					DataArray<SharedStr>::locked_seq_t resData = res->GetLockedDataWrite(t);
					resData.get_sa().data_reserve(totalSize MG_DEBUG_ALLOCATOR_SRC("res->md_SrcStr"));
					auto resI = resData.begin();

					for (auto i=data.begin(), e=data.end(); i!=e; ++resI, ++i)
					{
						boost::cmatch matchResult;

						if (i->IsDefined() && boost::regex_search(i->begin(), i->end(), matchResult, rx, flags))
							resI->assign(matchResult[0].first, matchResult[0].second);
						else
							resI->assign(Undefined());
					}
					dms_assert(resData.get_sa().actual_data_size() == resData.get_sa().data_capacity());
				}
			);
			resLock.Commit();
		}
		return true;
	}
};

#include "VersionComponent.h"
#include <windows.h>


struct RegexVersionComponent : AbstrVersionComponent {
	void Visit(ClientHandle clientHandle, VersionComponentCallbackFunc callBack, UInt32 componentLevel) const override {

		LCID lc = ::GetUserDefaultLCID();
		WCHAR localeName_utf16[LOCALE_NAME_MAX_LENGTH];
		int sz = LCIDToLocaleName(lc, localeName_utf16, LOCALE_NAME_MAX_LENGTH, 0);
		char localeName_utf8[LOCALE_NAME_MAX_LENGTH * 3];
		WideCharToMultiByte(utf8CP, 0, localeName_utf16, sz, localeName_utf8, LOCALE_NAME_MAX_LENGTH*3, nullptr, nullptr);

		callBack(clientHandle, componentLevel, mgFormat2string("Regex using Locale Identifier %1%(%2%)", lc, localeName_utf8).c_str());
	}
};

static RegexVersionComponent thisComponentWithItsUsedCodePage;

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
			if (args.size() >= 4)
			{
				DataReadLock lock3( AsDataItem(args[3]) );
				flags = (boost::regex_constants::match_flag_type)GetTheCurrValue<UInt32>( args[3] );
			}

			DataReadLock a1Lock(arg1A);
			const DataArray<SharedStr>* arg1 = const_array_cast<SharedStr>(a1Lock);

			AbstrDataItem* results = AsDataItem(resultHolder.GetNew());
			DataWriteLock resLock(results, dms_rw_mode::write_only_mustzero);
			DataArray<Bool>* res = mutable_array_cast<Bool>(resLock);

			parallel_tileloop(e1->GetNrTiles(), [res, arg1, &rx, flags] (tile_id t)->void 
				{
					auto data = arg1->GetLockedDataRead(t);
					auto resData = res->GetLockedDataWrite(t);

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

					count_iterator totalSize = 0;
					for (auto i=data.begin(), e=data.end(); i!=e; ++i)
					{
						if (i->IsDefined())
						{
							totalSize = 
								boost::regex_replace(
									totalSize, 
									i->begin(), i->end(), 
									rx, fmt, flags
								);
						}
					}

					DataArray<SharedStr>::locked_seq_t resData = res->GetLockedDataWrite(t);
					resData.get_sa().data_reserve(totalSize MG_DEBUG_ALLOCATOR_SRC("res->md_SrcStr + : RegexReplaceOperator.data_reserve()"));
					auto resI = resData.begin();

					for (auto i=data.begin(), e=data.end(); i!=e; ++resI, ++i)
					{
						if (i->IsDefined())
						{
							resI->clear();
							boost::regex_replace(
								std::insert_iterator<DataArray<SharedStr>::reference>(*resI, (*resI).end()), 
								i->begin(), i->end(), 
								rx, fmt, flags
							);
						}
						else
							resI->assign(Undefined());
					}
					dms_assert(resData.get_sa().actual_data_size() == resData.get_sa().data_capacity());
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