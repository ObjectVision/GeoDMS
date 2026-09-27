# Jenks-Fisher: Monge gives monotone breaks, not unimodal rows

Status (2026-09-25): an O(k m) SMAWK row search was implemented in `JenksFisher::CalcCB`, verified in
GeoDMS and not adopted; `CalcRange` stays (§8). Its code is kept in the appendix.

Scope: the per-row break search of `struct JenksFisher` in `clc/dll/src/CalcClassBreaks.cpp`, which serves
`ClassifyJenksFisher` and `ClassifyNonzeroJenksFisher`, read against the wiki page
*Fisher's Natural Breaks Classification complexity proof*. The questions: can the no-crossing-paths
(Monge) property support a row-by-row, left-to-right search that stops at the first deterioration; and
what does replacing the divide and conquer by SMAWK buy?

## 0. Summary

- The no-crossing-paths property proven on the wiki makes every row of the dynamic program a Monge-type
  (totally monotone) matrix: the optimal break never moves left as the row index grows. It says nothing
  about the shape of a single row, and rows of Fisher's objective do rise, fall and rise again (§3).
- A search that starts at the previous row's break and stops at the first decrease is therefore **not
  valid** here. On 4000 small random instances it was suboptimal in 30%; on 1 000 values into 15 classes
  its SSD was 69% above the optimum.
- Without unimodality no forward pass over the rows can be linear: a forward pass only ever learns lower
  bounds (§4). Linear time needs upper bounds taken from rows further down, which is what SMAWK's
  recursion provides.
- **The current implementation is not such a forward search.** `CalcRange` is a divide and conquer over
  the rows and `FindMaxBreakIndex` scans each candidate interval completely. It relies on the monotone
  breaks alone, is exact, and costs O(m log m) per DP row where SMAWK costs O(m) (§5).
- **SMAWK was built and measured (§6, §7).** It gives identical breaks and cuts the evaluations by a factor
  (log2 m - 0.6) / 7: 2.7x at one million unique values, 3.2x at seven million. GeoDMS counted 249 185 283
  evaluations for `CalcRange` against 91 860 346 for SMAWK on the same million values into 15 classes. The
  time gain is only 0.96-1.20x, because each SMAWK evaluation costs about 2.7 times as much.
- **Decision: keep `CalcRange`** (§8). SMAWK adds +84/-33 lines of subtle code and 12 bytes per unique
  value for that gain, and the operator as a whole stays O(n log n) because the value-count table is built
  by sorting.

## 1. The row problem

The dynamic rule of the proof page, for j classes:

    SSM(i, j) = max over p in j..i of  SSM(p-1, j-1) + ssm(p..i)          ssm(S) = w(S) * mean(S)^2

with CB(i, j) the maximising p. For a fixed j this is a row-maxima problem on the matrix

    A(i, p) = SSM(p-1, j-1) + ssm(p..i)          defined for p <= i

`JenksFisher` stores it with offsets. While computing the row after `m_NrCompletedRows` = r completed
rows, buffer row `i` stands for the last value index `i + r`, buffer column `p` for the first value index
`p + r` of the last class, and the entry is `m_PrevSSM[p] + GetSSM(p + r, i + r)`. Rows and columns both
run over `B = m - k + 1` positions (`m_BufSize`), and the matrix is lower triangular: column `p` exists in
rows `i >= p`.

## 2. What Monge gives

The proof establishes, for p1 < p2 <= i1 < i2, strictly because the values are strictly increasing:

    ssm(p1..i1) + ssm(p2..i2) > ssm(p1..i2) + ssm(p2..i1)

The term `SSM(p-1, j-1)` depends on the column only and cancels, so the same holds for A:

    A(i2, p2) - A(i2, p1) > A(i1, p2) - A(i1, p1)

In words: if the right column p2 is at least as good as p1 in row i1, it is strictly better in every later
row i2. This is total monotonicity, and CB(i1, j) <= CB(i2, j), the no-crossing-paths property, follows.

Two features of this statement matter below:

- it relates two rows and two columns; it says nothing about three columns within one row;
- it is directional. An earlier row can only tell a later row that a *right* column wins, which is a
  lower bound on the later break. Its contrapositive, that a *left* column winning in a later row also
  wins in every earlier row, is the only source of upper bounds, and it comes from rows further down.

## 3. What Monge does not give: unimodal rows

Adding any function of the column alone to a Monge matrix keeps it Monge, because the term cancels in the
inequality of §2. A DP row is exactly such a sum, ssm(p..i) plus the column term SSM(p-1, j-1), so the
Monge property cannot constrain the shape of a row. Rows are indeed not unimodal. The smallest example has
unit weights, values 0, 1, 5, 6, 12 and two classes:

| last class starts at | classes | WSM (maximised) | SSD |
|---:|---|---:|---:|
| 1 | {0} {1, 5, 6, 12} | 144.000 | 62.000 |
| 5 | {0, 1} {5, 6, 12} | 176.833 | 29.167 |
| 6 | {0, 1, 5} {6, 12} | 174.000 | 32.000 |
| 12 | {0, 1, 5, 6} {12} | 180.000 | 26.000 |

The row rises, falls and rises again. A search that stops at the first fall returns {0, 1} {5, 6, 12}; the
optimum is {0, 1, 5, 6} {12}. Starting from the monotone lower bound does not rescue it: the best break
for the first four values is at 5, and the walk from there stops at 6.

A Python prototype of all variants, in exact rational arithmetic and checked against the exhaustive
O(k m^2) DP, gives the same picture on random data:

- 4000 instances with k = 2..6, up to 40 values, unit and random weights: the stopping walk was suboptimal
  in 1197 (30%); the current divide and conquer and SMAWK matched the exhaustive DP in all 4000, breaks
  included;
- 15 classes over unique squares of uniform values: at 1 000 values the walk's SSD is 69% above the
  optimum, at 10 000 0.6%, and at 100 000 within 0.01% but with 13 of the 14 breaks elsewhere.

## 4. Why a forward pass cannot be linear without unimodality

When a pass over the rows in order reaches row i+1, all it knows is CB(i+1) >= CB(i) (§2). Every column
right of CB(i) is still a candidate, and nothing seen so far can exclude one, so the pass has to evaluate
them all: up to the diagonal, the whole last class of every prefix, which is quadratic in B. With unimodal
rows the first fall would itself be the upper bound and the walk would cost O(B) per row. That is the
property Fisher's rows lack.

Linear time therefore needs upper bounds, and only later rows provide them. SMAWK obtains them by
recursion:

- It first solves every other row (recursively). Each remaining row then lies between the breaks of its
  two neighbours, and these brackets overlap only at their end points, so filling in the remaining rows
  costs O(rows + columns).
- REDUCE, a single pass over the columns without recursion, cuts each level's column count to at most its
  row count. It compares each new column with the top of a stack, at the row given by the stack depth
  rather than a current row, and uses both directions of §2. A win removes the stack top for good: the
  new column beats it below that row, and its predecessor on the stack beats it above. A loss confines the
  new column to later rows. Every comparison either pops a column or ends the new column's turn, so REDUCE
  is O(columns), whatever the number of rows. It only prunes; the maxima come from the recursion and the
  interpolation.

`CalcRange` passes column brackets down its recursion too (`bp`, `ep`: left half `[bp, mp]`, right half
`[mp, ep)`), but it never removes a column: at every depth the brackets of the rows solved there tile the
whole column range. SMAWK's brackets tile a list that REDUCE keeps no longer than the level's row count.
One real DP row (B = 4096, k = 15, textbook SMAWK), evaluations per recursion depth:

| depth | CalcRange: rows solved | evaluations | SMAWK: rows | columns after REDUCE | evaluations |
|---:|---:|---:|---:|---:|---:|
| 0 | 1 | 2 049 | 4 096 | 4 096 | 9 941 |
| 1 | 2 | 2 179 | 2 048 | 2 048 | 12 702 |
| 2 | 4 | 2 531 | 1 024 | 1 024 | 6 602 |
| 3 | 8 | 3 099 | 512 | 512 | 3 296 |
| 4 | 16 | 3 630 | 256 | 256 | 1 635 |
| ... | | | | | |
| 11 | 2 048 | 6 136 | 2 | 2 | 7 |
| total | 4 096 | 45 679 | | | 35 692 |

`CalcRange` pays about B per depth over log2 B depths (less at the top, where `FindMaxBreakIndex` stops at
the diagonal). SMAWK pays a constant times each level's row count, B + B/2 + B/4 + ... in all, but it
front-loads: its first two levels cost more than `CalcRange`'s first seven.

**What the log factor is worth.** O(m log m) against O(m) hides both constants. `CalcRange` needs about
log2 B - 0.6 evaluations per row entry, SMAWK about 7, so SMAWK saves a factor (log2 m - 0.6) / 7, not
log2 m. That factor does grow without bound, by about 0.14 per doubling of m, but a factor 10 would take
m near 10^21:

| unique values | log2 m | (log2 m - 0.6) / 7 | measured |
|---:|---:|---:|---:|
| 5 000 | 12.3 | 1.7 | 1.8-1.9 |
| 100 000 | 16.6 | 2.3 | 2.2 |
| 1 000 000 | 19.9 | 2.8 | 2.7-2.8 |
| 2 000 000 | 20.9 | 2.9 | 2.9 |
| 7 000 000 | 22.7 | 3.2 | 3.2-3.4 |

## 5. The current implementation

`CalcRange(bi, ei, bp, ep)` (`CalcClassBreaks.cpp:549`) solves the middle row `mi` of the row range
`[bi, ei)` with `FindMaxBreakIndex(mi, bp, min(ep, mi+1))` (`:520`), then recurses into the left half with
columns `[bp, mp]` and the right half with `[mp, ep)`. `FindMaxBreakIndex` evaluates every column of its
interval (`:534-545`), keeps the leftmost maximum (`currSSM > minSSM`) and has no early exit. The search
thus uses the no-crossing-paths property and nothing else, so it is exact for any row shape. Measured, a
row entry costs slightly less than log2 B evaluations (19.1-20.3 at 1-2 million values), so a row costs
O(B log B) and the classification O(k m log m), as the proof page states. The last DP row is only needed
at its last entry, which `GetBreaks` obtains with one full scan.

## 6. SMAWK on this matrix

The version that was implemented (appendix):

- **Lower-triangular shape.** Entries with p > i are absent; treating them as -inf keeps the matrix
  totally monotone, because a -inf in the upper row of a 2x2 never lets the right column win there, and
  REDUCE pops only on a strict win.
- **The top-level REDUCE is skipped.** On the full row it removes nothing: column c meets the stack at
  row c-1, where it is -inf, so it is always pushed. The row search starts with the odd rows against all B
  columns, which saves one evaluation per row entry.
- **One evaluation fewer per pop.** After a pop, the new column is placed at the depth it just won, whose
  row it was evaluated in; reusing that value saves about 9% of all evaluations.
- **Ties.** Leftmost maxima throughout, as `FindMaxBreakIndex` does. With strictly increasing values every
  maximiser of a row lies at or left of every maximiser of a later row, so each bracket contains all
  maximisers of its row, and both searches return the same breaks. They did in every run.
- **Memory.** The reduced column lists of all levels take fewer than B indices, and the REDUCE stack at
  most B/2 + 1 values: 12 bytes per unique value (84 MB at 7 million), next to the 8 (k - 2) bytes per value
  of the stored break matrix. `CalcRange` needs only its recursion stack.

## 7. Measurements

**In GeoDMS (Debug x64).** The SMAWK version replaced `CalcRange` in `CalcCB` and was checked against the
unchanged code on the same data:

- nine classifications gave byte-identical class sizes, and identical breaks where compared: 1 million
  squares of uniform values into 2, 3 and 15 classes, 100 000 log-uniform values into 40, 200 000 integers
  with repeats into 15, signed floats and signed integers through `ClassifyNonzeroJenksFisher`, and a case
  with 3 negative values, whose negative side becomes a 1x1 matrix;
- the Classify tests of the operator suite (`/Classify/{UnTiled2UnTiled,Tiled2UnTiled,EUntiled}/results/tests`)
  passed;
- a counter in `GetSSM` gave 249 185 283 evaluations for `CalcRange` and 91 860 346 for SMAWK on 999 899
  unique values into 15 classes: 19.1 against 7.0 per row entry;
- the operator classifies the full value-count table: `ClassifyUniqueValues`, which takes the same
  `GetCounts` path, returned 5 000 distinct breaks out of 5 000 on those values. The 4 096-pair cap
  (`MAX_PAIR_COUNT`) applies only to the map view's generated classification and `weeded_counts`.

**Timing.** A standalone benchmark holds a copy of `JenksFisher` with both row searches pasted in verbatim,
built with the Release flags of `DmsRelease.props` (MSVC 18, `/O2 /GL`, `/std:c++latest`) and run on an
Intel i5-1135G7, single-threaded, pinned to one core at high priority, best of 7 interleaved runs. Values
are unique, with unit weights. Evaluations per row entry:

| unique values | k | CalcRange | SMAWK |
|---:|---:|---:|---:|
| 5 000 | 3 | 12.1-12.3 | 6.3-7.0 |
| 100 000 | 40 | 15.4 | 7.1 |
| 1 000 000 | 15 | 19.1-19.3 | 6.9-7.1 |
| 2 000 000 | 15 | 20.1-20.3 | 6.9-7.1 |
| 7 000 000 | 5 | 22.3-22.6 | 6.6-7.1 |

Time:

| unique values, k | CalcRange | SMAWK | speed |
|---|---:|---:|---:|
| 1 M squares, 15 | 1.33 s | 1.20 s | x1.11 |
| 2 M squares, 15 | 2.63 s | 2.18 s | x1.20 |
| 1 M lognormal, 15 | 0.84 s | 0.87 s | x0.96 |
| 2 M lognormal, 15 | 1.78 s | 1.82 s | x0.98 |
| 7 M squares, 5 | 1.68 s | 1.46 s | x1.15 |
| 7 M lognormal, 5 | 1.69 s | 1.54 s | x1.10 |

Breaks were identical in every run. Why 3x fewer evaluations buy so little: `CalcRange` spends about 3 ns
per evaluation, SMAWK about 8 ns. The likely reason is that the iterations of `FindMaxBreakIndex` are
independent (sequential loads, and each candidate's division can start before the previous comparison is
resolved), whereas in REDUCE each fresh division decides a data-dependent branch before the next step's
operands are known. Two intermediate versions were slower: textbook SMAWK ran at 0.79-1.19x, and without
the evaluation saved per pop at 0.83-1.38x (median 1.00).

## 8. Decision and recommendations

- **`CalcRange` stays.** SMAWK replaces one recursive function with a single invariant by four functions
  that rest on a stack invariant linking depths to rows, stride arithmetic, -inf padding that is only safe
  with strict pops, a top-level special case and two scratch buffers whose bounds are proven, not checked.
  A slip in tie handling or index arithmetic gives plausible but different breaks. It buys 0.96-1.20x on the
  DP of millions of unique values; the map view's classification (at most 4 096 pairs) takes milliseconds
  either way; and the operator stays O(n log n) because the value-count table of unsorted input is built by
  sorting.
- **Never add an early exit to `FindMaxBreakIndex`**, nor any walk that stops at the first decrease (§3).
- The O(k m) bound through SMAWK remains a valid theoretical statement; the ClassifyJenksFisher wiki page
  presents it as such.
- If ClassifyJenksFisher ever needs to be faster, cheaper levers exist. The previous-row lower bound (§9)
  measured a median 1.07x at 15 classes and up to 1.23x at 40. Unmeasured but structurally more promising:
  once `mp` is known, the two recursive calls of `CalcRange` read only `m_PrevSSM` and write disjoint parts
  of `m_CurrSSM` and the break row, so they can run on separate threads. SMAWK's REDUCE is sequential.

## 9. The previous-row lower bound

Using CB(i, j-1) <= CB(i, j) as the start of a scan, `(m_CBPtr - m_BufSize)[i+1] - 1` in buffer terms,
saves about 12% of the evaluations at 15 classes and about 30% at 40. The assumption held in all 239 505
pairs checked in exact arithmetic and in every benchmark run, and a debug `assert` in `CalcRange` (`:573`)
checks it; the wiki page states it without proof. An implementation must raise `bp` *before* evaluating
the incumbent `minSSM`. The disabled `MG_ASSUME_CB_INC` block, since removed from `FindMaxBreakIndex`,
evaluated it at the original `bp` and then raised `bp`, so the loop compared against a column outside the
new range while `foundP` named a column whose value was never computed.

## References

- Wiki: *Fisher's Natural Breaks Classification complexity proof* (dynamic rule, no-crossing-paths property).
- A. Aggarwal, M. M. Klawe, S. Moran, P. Shor, R. Wilber (1987). Geometric applications of a
  matrix-searching algorithm. *Algorithmica* 2, 195-208. (SMAWK)
- A. Grønlund, K. G. Larsen, A. Mathiasen, J. S. Nielsen, S. Schneider, M. Song (2017). Fast exact
  k-means, k-medians and Bregman divergence clustering in 1D. arXiv:1701.07204. (O(k n) for this problem
  through SMAWK)

## Appendix: the SMAWK row search as implemented in `JenksFisher`

This is the code that was built into `CalcClassBreaks.cpp` and verified as described in §7. It needs
`#include <limits>`.

The new members of `JenksFisher`:

```cpp
	std::unique_ptr<SizeT[]>               m_ReducedCols;
	std::unique_ptr<ClassBreakValueType[]> m_ReduceValues;
```

In `CalcCB`, after `m_CBPtr = m_CB.get();`, and with `CalcRow()` in place of
`CalcRange(0, m_BufSize, 0, m_BufSize)` in the loop:

```cpp
			if (m_K > 2)
			{
				m_ReducedCols .reset(new SizeT[m_BufSize]);                          // the lists of all levels: fewer than m_BufSize
				m_ReduceValues.reset(new ClassBreakValueType[m_BufSize / 2 + 1]); // the largest REDUCE stack, over the odd rows
			}
```

The row search, replacing `CalcRange`:

```cpp
	// A DP row is a row-maxima problem: row i, column p holds m_PrevSSM[p] + GetSSM(p+r, i+r) for p <= i,
	// and columns p > i do not exist. The no-crossing-paths property makes this matrix totally monotone,
	// which is all SMAWK needs. A row itself need not be unimodal, so no scan may stop at a first decrease.
	ClassBreakValueType RowEntry(SizeT i, SizeT p)
	{
		if (p > i)
			return -std::numeric_limits<ClassBreakValueType>::infinity();
		return m_PrevSSM[p] + GetSSM(p + m_NrCompletedRows, i + m_NrCompletedRows);
	}

	// Leftmost maxima of the rows start + t*step, t < nrRows, over the candidate columns cols[0..nrCols),
	// which are 0..nrCols-1 when cols is null. REDUCE keeps at most nrRows of them in reducedCols; the
	// recursion on every other row appends its own list behind that one.
	void SmawkRows(SizeT start, SizeT step, SizeT nrRows, const SizeT* cols, SizeT nrCols, SizeT* reducedCols)
	{
		if (!nrRows)
			return;

		// REDUCE. Stack depth t stands for row start + t*step, and the column there can only be the maximum
		// from that row on. A new column that beats it in that row beats it in every later row too.
		ClassBreakValueType* stackValue = m_ReduceValues.get();
		SizeT nrReduced = 0;
		for (SizeT ci = 0; ci != nrCols; ++ci)
		{
			SizeT c = cols ? cols[ci] : ci;
			ClassBreakValueType cValue = 0;
			bool cValueKnown = false;
			while (nrReduced)
			{
				ClassBreakValueType v = RowEntry(start + (nrReduced - 1) * step, c);
				if (!(stackValue[nrReduced - 1] < v))
					break;
				--nrReduced;
				cValue = v; // c's value in the row of the depth it takes over
				cValueKnown = true;
			}
			if (nrReduced == nrRows)
				continue; // c loses in every row up to the last one
			stackValue[nrReduced] = cValueKnown ? cValue : RowEntry(start + nrReduced * step, c);
			reducedCols[nrReduced++] = c;
		}

		SmawkRows(start + step, 2 * step, nrRows / 2, reducedCols, nrReduced, reducedCols + nrReduced);
		InterpolateRows(start, step, nrRows, reducedCols, nrReduced);
	}

	// Each row at an even position lies between the maxima of its odd neighbours, which the recursion found.
	// Consecutive brackets share only an end column, so all scans together cost O(nrRows + nrCols).
	void InterpolateRows(SizeT start, SizeT step, SizeT nrRows, const SizeT* cols, SizeT nrCols)
	{
		auto col = [cols](SizeT ci) { return cols ? cols[ci] : ci; };
		SizeT ci = 0;
		for (SizeT t = 0; t < nrRows; t += 2)
		{
			SizeT row = start + t * step;
			SizeT lastCol = (t + 1 < nrRows) ? m_CBPtr[row + step] : col(nrCols - 1);
			SizeT foundP = col(ci);
			ClassBreakValueType maxSSM = RowEntry(row, foundP);
			while (col(ci) != lastCol)
			{
				SizeT p = col(++ci);
				ClassBreakValueType currSSM = RowEntry(row, p);
				if (currSSM > maxSSM)
				{
					maxSSM = currSSM;
					foundP = p;
				}
			}
			m_CBPtr[row] = foundP;
			m_CurrSSM[row] = maxSSM;
		}
	}

	// O(m_BufSize) per row. REDUCE would remove nothing from the full row: column c meets the stack in row c-1,
	// where it does not exist. So recurse on the odd rows against all columns right away.
	void CalcRow()
	{
		DBG_START("JenksFisher", "CalcRow", MG_DEBUG_CLASSBREAKS);
		SmawkRows(1, 2, m_BufSize / 2, nullptr, m_BufSize, m_ReducedCols.get());
		InterpolateRows(0, 1, m_BufSize, nullptr, m_BufSize);
	}
```
