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
// join_equal_values formulation of NetworkModel_PBL used. Issue #1281.
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
// pareto_optimal_eps(partition_rel, crit1, eps1, ..., critN, epsN): the same with
// epsilon-dominance (#1282, the offline form of pareto(imp2_epsilon)). Every criterion is
// bucketed to floor(crit / eps) (eps 0: exact), the sort is on (partition, bucket1, ...,
// bucketN, crit1, ..., critN, id) and the sweep compares buckets. Per box the row with the
// smallest raw values in criterion order survives: eps1 decides which rows count as equal
// on the first criterion, and among those the lower bucket of the second criterion wins,
// then the smaller raw first criterion. The result is a subset of the exact result, and
// every exact-optimal row that drops out has a surviving row of its partition that is
// less than eps better or equal on every criterion (its bucket is <= on all of them).
//
// The criteria may have different numeric value types; they are compared as Float64. The
// partition relation is any attribute to a domain unit. A row with an undefined partition
// or an undefined (or NaN) criterion is never optimal and dominates nothing.

#include <algorithm>
#include <cmath>
#include <deque>
#include <execution>
#include <memory>

#include "mci/CompositeCast.h"
#include "set/VectorFunc.h"
#include "mem/MyContainers.h"
#include "utl/StrFormat.h"

#include "CheckedDomain.h"
#include "DataArray.h"
#include "DataItemClass.h"
#include "IndexGetterCreator.h"
#include "TreeItemClass.h"
#include "Unit.h"
#include "UnitClass.h"
#include "UnitProcessor.h"
#include "OperRelUni.h"

CommonOperGroup cog_pareto_optimal("pareto_optimal");
CommonOperGroup cog_pareto_optimal_eps("pareto_optimal_eps");

struct ParetoOptimalOperator : VariadicOperator
{
	// The result class is the concrete DataArray<Bool>: the operator lookup of an expression that
	// uses the result (a != on it, a uint32() of it) matches on that class before anything is calculated.
	// withEps: the arguments after the partition come in (criterion, epsilon) pairs.
	ParetoOptimalOperator(arg_index nrCriteria, bool withEps)
		: VariadicOperator(withEps ? &cog_pareto_optimal_eps : &cog_pareto_optimal, DataArray<Bool>::GetStaticClass(), 1 + nrCriteria * (withEps ? 2 : 1))
		, m_NrCriteria(nrCriteria), m_WithEps(withEps)
	{
		fast_fill(m_ArgClasses.get(), m_ArgClasses.get() + (1 + nrCriteria * (withEps ? 2 : 1)), AbstrDataItem::GetStaticClass());
	}

	CharPtr Name() const { return m_WithEps ? "pareto_optimal_eps" : "pareto_optimal"; }
	arg_index CritArg(arg_index k) const { return m_WithEps ? 1 + 2 * k : 1 + k; } // argument index of criterion k (0-based)
	arg_index EpsArg (arg_index k) const { assert(m_WithEps); return 2 + 2 * k; }

	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		assert(args.size() == 1 + m_NrCriteria * (m_WithEps ? 2 : 1));
		const AbstrDataItem* partA = AsDataItem(args[0]);
		MG_USERCHECK2(partA, mySSPrintF("{}: the first argument must be the partition relation, an attribute to a domain unit", Name()).c_str());
		const AbstrUnit* e = partA->GetAbstrDomainUnit();
		MG_USERCHECK2(partA->GetAbstrValuesUnit()->CanBeDomain(), mySSPrintF("{}: the first argument must be a relation to a domain unit (unsigned integer values)", Name()).c_str());

		for (arg_index k = 0; k != m_NrCriteria; ++k)
		{
			const AbstrDataItem* critA = AsDataItem(args[CritArg(k)]);
			MG_USERCHECK2(critA, mySSPrintF("{}: argument {} must be a numeric attribute (criterion {})", Name(), CritArg(k) + 1, k + 1).c_str());
			e->UnifyDomain(critA->GetAbstrDomainUnit(), "e1", mySSPrintF("e{}", CritArg(k) + 1).c_str(), UM_Throw);
			MG_USERCHECK2(critA->GetAbstrValuesUnit()->GetValueType()->IsNumeric(), mySSPrintF("{}: criterion {} must be numeric", Name(), k + 1).c_str());
			if (m_WithEps)
			{
				const AbstrDataItem* epsA = AsDataItem(args[EpsArg(k)]);
				MG_USERCHECK2(epsA, mySSPrintF("{}: argument {} must be a numeric parameter (epsilon of criterion {})", Name(), EpsArg(k) + 1, k + 1).c_str());
				checked_domain<Void>(epsA, mySSPrintF("epsilon of criterion {}", k + 1).c_str());
				MG_USERCHECK2(epsA->GetAbstrValuesUnit()->GetValueType()->IsNumeric(), mySSPrintF("{}: epsilon of criterion {} must be numeric", Name(), k + 1).c_str());
			}
		}

		if (!resultHolder)
			resultHolder = CreateCacheDataItem(e, Unit<Bool>::GetStaticClass()->CreateDefault());

		if (mustCalc)
		{
			std::deque<DataReadLock> argLocks;
			for (arg_index i = 0; i != args.size(); ++i)
				argLocks.emplace_back(AsDataItem(args[i]));

			std::vector<const AbstrDataItem*> critItems(m_NrCriteria);
			std::vector<Float64> eps(m_NrCriteria, 0.0);
			for (arg_index k = 0; k != m_NrCriteria; ++k)
			{
				critItems[k] = AsDataItem(args[CritArg(k)]);
				if (m_WithEps)
				{
					eps[k] = AsDataItem(args[EpsArg(k)])->GetRefObj()->GetValueAsFloat64(0);
					MG_USERCHECK2(IsDefined(eps[k]) && eps[k] >= 0.0, mySSPrintF("{}: the epsilon of criterion {} must be a defined, nonnegative value", Name(), k + 1).c_str());
				}
			}

			AbstrDataItem* res = AsDataItem(resultHolder.GetNew());
			DataWriteLock resLock(res, dms_rw_mode::write_only_all);

			Calculate(mutable_array_cast<Bool>(resLock), partA, critItems, eps);

			resLock.Commit();
		}
		return true;
	}

	static void Calculate(DataArray<Bool>* res, const AbstrDataItem* partA, const std::vector<const AbstrDataItem*>& critItems, const std::vector<Float64>& eps)
	{
		const AbstrUnit* e = partA->GetAbstrDomainUnit();
		const SizeT     n  = e->GetCount();
		const tile_id   nt = e->GetNrTiles();
		const arg_index d  = critItems.size();
		assert(eps.size() == d);

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
			const AbstrDataItem* critA = critItems[k];
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

		// 4. epsilon-dominance (#1282): the dominance keys are the buckets floor(crit / eps) where
		//    eps > 0 and the raw values elsewhere. A quotient within a millionth of a bucket width
		//    below an edge counts as the higher bucket, as in the engine's Imp2Bucket.
		bool anyEps = false;
		std::vector<my_vec_t<Float64>> bucket(d);
		std::vector<const my_vec_t<Float64>*> key(d);
		for (arg_index k = 0; k != d; ++k)
		{
			if (eps[k] > 0.0)
			{
				anyEps = true;
				bucket[k].resize(n);
				const Float64 epsK = eps[k];
				for (SizeT i : idx)
					bucket[k][i] = std::floor(crit[k][i] / epsK + 1e-6);
				key[k] = &bucket[k];
			}
			else
				key[k] = &crit[k];
		}

		// 5. lexicographic order on (partition, keys, raw criteria, row id): the offline form of the
		//    order in which the bi-criteria Dijkstra pops its labels; the raw criteria only order
		//    within equal buckets, so that the best row of a box comes first
		auto lexLess = [&part, &key, &crit, d, anyEps](SizeT a, SizeT b) -> bool
		{
			if (part[a] != part[b])
				return part[a] < part[b];
			for (arg_index k = 0; k != d; ++k)
				if ((*key[k])[a] != (*key[k])[b])
					return (*key[k])[a] < (*key[k])[b];
			if (anyEps)
				for (arg_index k = 0; k != d; ++k)
					if (crit[k][a] != crit[k][b])
						return crit[k][a] < crit[k][b];
			return a < b;
		};
		if (idx.size() > 4096)
			std::sort(std::execution::par, idx.begin(), idx.end(), lexLess);
		else
			std::sort(idx.begin(), idx.end(), lexLess);

		// 6. the sweep per partition. Every earlier row of the group has key1 <= the row under
		//    test (and, when equal on all keys, a lower raw value or id), so dominance reduces to
		//    the remaining keys: with one criterion only the first row of the group survives,
		//    with two the row survives iff its key2 is strictly below the minimum key2 accepted
		//    so far, with more it must escape every accepted row of the group.
		my_vec_t<UInt8> keep(n, UInt8(0));
		my_vec_t<SizeT> front;   // the accepted rows of the current group, d >= 3 only
		SizeT   currPart = UNDEFINED_VALUE(SizeT);
		bool    hasAccepted = false;
		Float64 minKey2 = 0.0;
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
				dominated = hasAccepted && (*key[1])[r] >= minKey2;
			else
			{
				dominated = false;
				for (SizeT f : front)
				{
					bool dom = true;
					for (arg_index k = 1; k != d && dom; ++k)
						dom = (*key[k])[f] <= (*key[k])[r];
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
				minKey2 = (*key[1])[r];
			else if (d > 2)
				front.push_back(r);
		}

		// 7. the result, tile by tile
		for (tile_id t = 0; t != nt; ++t)
		{
			auto resData = res->GetWritableTile(t, dms_rw_mode::write_only_all);
			SizeT first = e->GetTileFirstIndex(t);
			auto resI = resData.begin();
			for (SizeT i = 0, m = resData.size(); i != m; ++i, ++resI)
				*resI = (keep[first + i] != 0);
		}
	}

	arg_index m_NrCriteria;
	bool      m_WithEps;
};

namespace
{
	// one instance per arity: the partition relation plus one to eight criteria, without and with epsilons
	ParetoOptimalOperator po1(1, false), po2(2, false), po3(3, false), po4(4, false), po5(5, false), po6(6, false), po7(7, false), po8(8, false);
	ParetoOptimalOperator pe1(1, true ), pe2(2, true ), pe3(3, true ), pe4(4, true ), pe5(5, true ), pe6(6, true ), pe7(7, true ), pe8(8, true );
}
