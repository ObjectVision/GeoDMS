// Copyright (C) 2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "ClcPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif

// pareto_optimal(partition_rel, crit1, ..., critN): per row of the domain E of the
// arguments, true iff the row is Pareto-optimal within its partition on the N criteria,
// all of which are minimised. A row is dominated when another row of the same partition
// is less than or equal on every criterion and either strictly less on one of them or,
// when equal on all of them, earlier (a lower row id): exact duplicates keep their
// first occurrence, the weak-dominance rule with the id tie-break that the
// join_equal_values formulation of NetworkModel_PBL used.
//
// Offline form of the label-setting rule of the pareto option of impedance_matrix
// (doc/bicriteria-impedance.md section 3): sort the rows lexicographically on
// (partition, crit1, ..., critN, id), the order in which the bi-criteria Dijkstra pops
// its labels, and sweep. Every earlier row of the group then has crit1 <= the row under
// test, so with two criteria the test collapses to one scalar per group, the minimum
// crit2 accepted so far (Hansen 1980), and with more criteria to a comparison against the
// rows accepted so far only: a row rejected earlier was dominated by an accepted row
// that, by transitivity, dominates whatever the rejected row would have dominated.
// n log n for the sort plus n times the front size for the sweep, instead of the k^2
// pairs per group of a self-join.
//
// The criteria may have different numeric value types; they are compared as Float64. The
// partition relation is any attribute to a domain unit. A row with an undefined partition
// or an undefined (or NaN) criterion is never optimal and dominates nothing.

#include <algorithm>
#include <deque>
#include <execution>
#include <memory>

#include "mci/CompositeCast.h"
#include "set/VectorFunc.h"
#include "mem/MyContainers.h"
#include "utl/StrFormat.h"

#include "DataArray.h"
#include "DataItemClass.h"
#include "IndexGetterCreator.h"
#include "TreeItemClass.h"
#include "Unit.h"
#include "UnitClass.h"
#include "UnitProcessor.h"
#include "OperRelUni.h"

CommonOperGroup cog_pareto_optimal("pareto_optimal");

struct ParetoOptimalOperator : VariadicOperator
{
	// The result class is the concrete DataArray<Bool>: the operator lookup of an expression that
	// uses the result (a != on it, a uint32() of it) matches on that class before anything is calculated.
	ParetoOptimalOperator(arg_index nrArgs)
		: VariadicOperator(&cog_pareto_optimal, DataArray<Bool>::GetStaticClass(), nrArgs)
	{
		fast_fill(m_ArgClasses.get(), m_ArgClasses.get() + nrArgs, AbstrDataItem::GetStaticClass());
	}

	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		assert(args.size() >= 2);
		const AbstrDataItem* partA = AsDataItem(args[0]);
		MG_USERCHECK2(partA, "pareto_optimal: the first argument must be the partition relation, an attribute to a domain unit");
		const AbstrUnit* e = partA->GetAbstrDomainUnit();
		MG_USERCHECK2(partA->GetAbstrValuesUnit()->CanBeDomain(), "pareto_optimal: the first argument must be a relation to a domain unit (unsigned integer values)");

		for (arg_index i = 1; i != args.size(); ++i)
		{
			const AbstrDataItem* critA = AsDataItem(args[i]);
			MG_USERCHECK2(critA, mySSPrintF("pareto_optimal: argument {} must be a numeric attribute (criterion {})", i + 1, i).c_str());
			e->UnifyDomain(critA->GetAbstrDomainUnit(), "e1", mySSPrintF("e{}", i + 1).c_str(), UM_Throw);
			MG_USERCHECK2(critA->GetAbstrValuesUnit()->GetValueType()->IsNumeric(), mySSPrintF("pareto_optimal: criterion {} must be numeric", i).c_str());
		}

		if (!resultHolder)
			resultHolder = CreateCacheDataItem(e, Unit<Bool>::GetStaticClass()->CreateDefault());

		if (mustCalc)
		{
			std::deque<DataReadLock> argLocks;
			for (arg_index i = 0; i != args.size(); ++i)
				argLocks.emplace_back(AsDataItem(args[i]));

			AbstrDataItem* res = AsDataItem(resultHolder.GetNew());
			DataWriteLock resLock(res, dms_rw_mode::write_only_all);

			Calculate(mutable_array_cast<Bool>(resLock), partA, args);

			resLock.Commit();
		}
		return true;
	}

	static void Calculate(DataArray<Bool>* res, const AbstrDataItem* partA, const ArgSeqType& args)
	{
		const AbstrUnit* e = partA->GetAbstrDomainUnit();
		const SizeT     n  = e->GetCount();
		const tile_id   nt = e->GetNrTiles();
		const arg_index d  = args.size() - 1;

		// 1. the partition per row, as SizeT; an undefined partition excludes the row
		my_vec_t<SizeT> part(n);
		{
			std::unique_ptr<IndexGetter> partGetter(IndexGetterCreator::Create(partA, no_tile));
			for (SizeT i = 0; i != n; ++i)
				part[i] = partGetter->Get(i);
		}

		// 2. the criteria as Float64 columns, whatever their numeric value types
		std::vector<my_vec_t<Float64>> crit(d);
		for (arg_index k = 0; k != d; ++k)
		{
			const AbstrDataItem* critA = AsDataItem(args[k + 1]);
			my_vec_t<Float64>& col = crit[k];
			col.resize(n);
			visit<typelists::num_objects>(critA->GetAbstrValuesUnit(), [critA, &col, e, nt] <typename V> (const Unit<V>*)
				{
					auto da = const_array_cast<V>(critA);
					for (tile_id t = 0; t != nt; ++t)
					{
						auto tileData = da->GetTile(t);
						SizeT first = e->GetTileFirstIndex(t);
						for (SizeT i = 0, m = tileData.size(); i != m; ++i)
						{
							V v = tileData[i];
							col[first + i] = IsDefined(v) ? Float64(v) : UNDEFINED_VALUE(Float64);
						}
					}
				}
			);
		}

		// 3. the rows that take part: a defined partition and defined criteria
		my_vec_t<SizeT> idx;
		idx.reserve(n);
		for (SizeT i = 0; i != n; ++i)
		{
			if (!IsDefined(part[i]))
				continue;
			bool ok = true;
			for (arg_index k = 0; k != d && ok; ++k)
			{
				Float64 v = crit[k][i];
				ok = IsDefined(v) && v == v; // v == v: not NaN, which has no place in a strict weak order
			}
			if (ok)
				idx.push_back(i);
		}

		// 4. lexicographic order on (partition, crit1, ..., critN, row id): the offline form of
		//    the order in which the bi-criteria Dijkstra pops its labels
		auto lexLess = [&part, &crit, d](SizeT a, SizeT b) -> bool
		{
			if (part[a] != part[b])
				return part[a] < part[b];
			for (arg_index k = 0; k != d; ++k)
				if (crit[k][a] != crit[k][b])
					return crit[k][a] < crit[k][b];
			return a < b;
		};
		if (idx.size() > 4096)
			std::sort(std::execution::par, idx.begin(), idx.end(), lexLess);
		else
			std::sort(idx.begin(), idx.end(), lexLess);

		// 5. the sweep per partition. Every earlier row of the group has crit1 <= the row under
		//    test (and, when equal on all criteria, a lower id), so dominance reduces to the
		//    remaining criteria: with one criterion only the first row of the group survives,
		//    with two the row survives iff its crit2 is strictly below the minimum crit2 accepted
		//    so far, with more it must escape every accepted row of the group.
		my_vec_t<UInt8> keep(n, UInt8(0));
		my_vec_t<SizeT> front;   // the accepted rows of the current group, d >= 3 only
		SizeT   currPart = UNDEFINED_VALUE(SizeT);
		bool    hasAccepted = false;
		Float64 minCrit2 = 0.0;
		for (SizeT r : idx)
		{
			if (part[r] != currPart)
			{
				currPart = part[r];
				hasAccepted = false;
				front.clear();
			}
			bool dominated;
			if (d == 1)
				dominated = hasAccepted;
			else if (d == 2)
				dominated = hasAccepted && crit[1][r] >= minCrit2;
			else
			{
				dominated = false;
				for (SizeT f : front)
				{
					bool dom = true;
					for (arg_index k = 1; k != d && dom; ++k)
						dom = crit[k][f] <= crit[k][r];
					if (dom)
					{
						dominated = true;
						break;
					}
				}
			}
			if (dominated)
				continue;
			keep[r] = 1;
			hasAccepted = true;
			if (d == 2)
				minCrit2 = crit[1][r];
			else if (d > 2)
				front.push_back(r);
		}

		// 6. the result, tile by tile
		for (tile_id t = 0; t != nt; ++t)
		{
			auto resData = res->GetWritableTile(t, dms_rw_mode::write_only_all);
			SizeT first = e->GetTileFirstIndex(t);
			auto resI = resData.begin();
			for (SizeT i = 0, m = resData.size(); i != m; ++i, ++resI)
				*resI = (keep[first + i] != 0);
		}
	}
};

namespace
{
	// one instance per arity: the partition relation plus one to eight criteria
	ParetoOptimalOperator po2(2), po3(3), po4(4), po5(5), po6(6), po7(7), po8(8), po9(9);
}
