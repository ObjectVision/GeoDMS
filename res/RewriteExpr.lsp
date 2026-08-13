/* Rules in slisp notation for the rewriting of expr slisps of treeitems.

   Syntax: 

   rewrite-rule-list ::= ( rewrite-rules )
   rewrite-rules     ::= rewrite-rules rewrite-rule | rewrite-rule
   rewrite-rule      ::= [pattern resolvent]
   pattern           ::=  slisp-expr
   resolvent         ::=  slisp-expr
   slisp-expr        ::= SYMBOL | NUMBER | _VAR | ( slisp-exprs ) | [slisp-expr slisp-expr]
   slisp-exprs       ::= slisp-expr slisp-exprs | NIL

   Notational conventions:

   _X: Value Attribute
   _P: Partitioner
   _s: Partition restriction

*/
(

/*********** Const function    *********/

[(Value _P _VU)                  (convert _P _VU)]
/* the 3-arg const completion is RETIRED to a prelude function (arity-aware head
   dispatch: the registered const operator is binary): three(p,e,vu) =
   const(Value(p,vu), e) -- the Value rule above fires at body parse, so the stored
   body equals this rule's fixpoint. */

/*********** selection Functions *********/

[(collect_by_cond (select        _Cond) _Data) (collect_by_cond (select        _Cond) _Cond _Data)]
[(collect_by_cond (select_uint8  _Cond) _Data) (collect_by_cond (select_uint8  _Cond) _Cond _Data)]
[(collect_by_cond (select_uint16 _Cond) _Data) (collect_by_cond (select_uint16 _Cond) _Cond _Data)]
[(collect_by_cond (select_uint32 _Cond) _Data) (collect_by_cond (select_uint32 _Cond) _Cond _Data)]

[(collect_by_cond (select_with_org_rel        _Cond) _Data) (collect_by_cond (select_with_org_rel        _Cond) _Cond _Data)]
[(collect_by_cond (select_uint8_with_org_rel  _Cond) _Data) (collect_by_cond (select_uint8_with_org_rel  _Cond) _Cond _Data)]
[(collect_by_cond (select_uint16_with_org_rel _Cond) _Data) (collect_by_cond (select_uint16_with_org_rel _Cond) _Cond _Data)]
[(collect_by_cond (select_uint32_with_org_rel _Cond) _Data) (collect_by_cond (select_uint32_with_org_rel _Cond) _Cond _Data)]

[(collect_by_org_rel (select_with_org_rel        _Cond) _Data) (lookup (SubItem (select_with_org_rel        _Cond) "org_rel") _Data)]
[(collect_by_org_rel (select_uint8_with_org_rel  _Cond) _Data) (lookup (SubItem (select_uint8_with_org_rel  _Cond) "org_rel") _Data)]
[(collect_by_org_rel (select_uint16_with_org_rel _Cond) _Data) (lookup (SubItem (select_uint16_with_org_rel _Cond) "org_rel") _Data)]
[(collect_by_org_rel (select_uint32_with_org_rel _Cond) _Data) (lookup (SubItem (select_uint32_with_org_rel _Cond) "org_rel") _Data)]

/* The rules above destructure the SUBSET argument, so they only fire when its structure is
   visible. Since #1180 an item under an IntegrityCheck hands consumers its guarded form,
   (integrity_check <expr> <check>), which hides that structure: the binary spelling then
   found no rule and no binary operator either. Lift the guard to the call, where it means
   the same thing -- the consumer fails when the check fails -- and the subset argument is
   bare again. This peels one guard per iteration: the template's inner call is re-applied
   (AssocList_RepApplyTopEnv), so a second guard, from a second checked ancestor, hoists on
   the next pass, and the bare call finally meets the rules above. */
[(collect_by_cond    (IntegrityCheck _Subset _Check) _Data) (IntegrityCheck (collect_by_cond    _Subset _Data) _Check)]
[(collect_by_org_rel (IntegrityCheck _Subset _Check) _Data) (IntegrityCheck (collect_by_org_rel _Subset _Data) _Check)]

/*********** Elementary funcs  *********/

/* the 2-arg log rule is RETIRED to a prelude function (arity-aware head dispatch:
   the registered log operator is unary): two(x,b) = log(x) / log(b). */
/* plogp, sqr and abs retired to the typed prelude (res/prelude.dms, WP4.5); the
   pow resolvents below carry the sqr expansion inline so pow keys are unchanged. */
[(pow _X 2)                 (mul _X _X)]
[(pow _X 3)                 (mul _X (mul _X _X))]
[(pow _X 4)                 (mul (mul _X _X) (mul _X _X))]
[(pow _X 5)                 (mul _X (pow _X 4))]
[(pow _X 6)                 (mul (mul _X _X) (pow _X 4))]
/* These integer-literal fast paths keep unit-aware exact powers (e.g. length^2 -> length^2).
   All other a^b cases (float/runtime/fractional exponents) are handled by the first-class
   pow operator (issue #839, clc/OperAttrBin.cpp); the old catch-all rewrite
   [(pow _X _Y) (exp (mul (log _X) _Y))] is removed because it yielded Null for any
   non-positive base (e.g. 0f^2f). */
/*********** Associative Binary functions *********/

/* the add/mul/or/and unary collapses and N-ary left folds are RETIRED to prelude
   variadic functions, reachable via ARITY-AWARE HEAD DISPATCH: the binary operators
   keep arity 2; argument counts no operator member accepts (1 and >=3) route to the
   same-named prelude function, whose fold bodies spell these rules' exact resolvents.
   The min_elem/max_elem(_fast) unary collapses below STAY: their groups are
   allow_extra_args, so the operator claims every arity >= its minimum and dispatch
   can never fire for them.
   The union fold rule is REMOVED (2026-07-18, Maarten's ruling): the union operator
   itself is variadic (cog_union, allow_extra_args) and implements the same; the
   chained SubItem(union(a,b),'UnionData') spelling is not used by the release tests --
   the precise idiom is union_unit + union_data. Multi-argument union(...) now keys
   FLAT (the operator's own semantics) instead of as a nested chain.
   The MakeDefined unary/2-arg/fold rules are RETIRED to a prelude variadic function
   (see the Missing values section). */

[(min_elem _a1)               _a1]
[(max_elem _a1)               _a1]
[(min_elem_fast _a1)          _a1]
[(max_elem_fast _a1)          _a1]

/* concat is RETIRED to a prelude variadic function ('...rest' fold): the variant
   bodies spell the exact resolvents (MakeDefined wrap + right add-fold), so the
   reduced keys equal this rule chain's fixpoint. */

/*********** switch case   *********/

[[switch [(case (false) _V) _REST]]
							[switch _REST]]
[[switch [(case (true) _V) _REST]]
							_V]
[[switch [(case _c _V) _REST]]
							(iif _c _V [switch _REST])]
[(switch _OTHERWISE)         _OTHERWISE ]

/*********** simplicifaction rules for operations on booleand    *********/

[(eq _X (true) ) _X]
[(eq _X (false)) (not _X)]
[(eq (true)  _X) _X]
[(eq (false) _X) (not _X)]
[(ne _X (false)) _X]
[(ne _X (true) ) (not _X)]
[(ne (false) _X) _X]
[(ne (true)  _X) (not _X)]
[(and _X (true) ) _X]
[(or  _X (false)) _X]
[(and (true)  _X) _X]
[(or  (false) _X) _X]
[(iif _X _THEN (false))(and _X _THEN)]
[(iif _X _THEN (true)) (or  (not _X) _THEN)]
[(iif _X (false) _ELSE)(and (not _X) _ELSE)]
[(iif _X (true)  _ELSE)(or  _X _ELSE)]
[(not (not _X)) _X]
[(not (true)) (false)]
[(not (false)) (true)]

// For NlLater
[(BaseUnit _SI "Bool") (BaseUnit _SI Bool)]
[(BaseUnit "" Bool) Bool]
[(convert 1.0 Bool) (true)]
[(convert 0.0 Bool) (false)]

/*********** Predicate functions *********/

[(order _a _b) (interval (min_elem _a _b) (max_elem _a _b))]

/* resolvent carries the retired le_or_lhs_null expansion inline (WP4.5): keys unchanged */
[(isOverlapping (interval _a1 _a2)(interval _B1 _B2))
	(and
		(or (le _B1 _a2) (IsNull _B1))
		(or (le _a1 _B2) (IsNull _a1))
	)
]

/* neighbourhood, float_isNearby and point_isNearby retired to the typed prelude
   (res/prelude.dms, WP4.5 tranche 3): their bodies spell order/isOverlapping
   calls, on which the two retained rules above still fire at body parse, so the
   reduced keys equal the old rule chain. */

/* the 3-arg median entry rule is RETIRED to a prelude function (a pure rule head: no
   registered operator): three(a,b,c) = median(a, order(b, c)) -- the retained order
   rule and the interval-destructuring rule below fire at body parse, so the stored
   body equals this rule chain's fixpoint. The destructuring rule stays (retained). */

[(median _a (interval _b _c)) // use guarantee that _b <= _c
	(iif (le _a _b) 
		_b
		(min_elem _a _c)
	)
]
/* the *_or_*_null predicate family retired to the typed prelude (res/prelude.dms, WP4.5) */

/*********** Default arguments *********/

/* ReadValue is RETIRED to a prelude function (a pure rule head: no registered
   operator): ReadValue(s, vu, pos) = ReadArray(s, Void, vu, pos) -- the exact
   resolvent, the Void symbol passing through as the value-type reference. */

/*********** Rescale functions *********/

/* distribute, scalesum, rescale and normalize retired to the typed prelude
   (res/prelude.dms, WP4.5 tranche 2): multi-arity forms dispatch via variant sets */

/*********** Remove symbolic constants from rescale & normalize *********/

[(add _X 0)          _X]
[(sub _X 0)          _X]
[(mul _X (div 1 _Y)) (div _X _Y)]

/*********** Properties        *********/

/* The six property accessors (name/Descr/Expr/Label/STORAGE/EK -> PropValue) are
   RETIRED to prelude.dms functions with meta-reference ('item') parameters: the
   argument now binds as a raw item reference (the same sourceDescr form PropValue's
   calc_never argument gets in a direct call), so PropValue reads the CONFIG item's
   metadata for containers, units and attributes alike, and the reduced key equals
   the direct call's key.
   The '(result _T)' unwrap rule is retired (WP4.5): no config calls result(...) in
   expression position, and the head collided with nested 'result' functions (§5.10). */

/*********** Missing values    *********/

/* MakeDefined is RETIRED to a prelude variadic function (a pure rule head: no
   registered operator): one(a) = a, two(x,y) = iif(IsDefined(x), x, y),
   more(x,y,...rest) = MakeDefined(MakeDefined(x,y), rest). Only the structural
   idempotence collapse below stays: it destructures its ARGUMENT (a function
   cannot), fires at parse as a syntactic normalizer, and its 2-arg output then
   resolves to the prelude function -- rule and function compose, and the
   collapse-corner keys are preserved. It does not match the prelude fold body
   (the pattern requires the same _Y twice). */
[(MakeDefined (MakeDefined _X _Y) _Y) (MakeDefined _X _Y)]

/* replace_value is RETIRED to a prelude variadic function: base = iif(eq(x,v),w,x),
   fold = replace_value(replace_value(x,v,w), rest) -- the exact resolvents. */

/* the 5+-arg replace fold is RETIRED to a prelude variadic function (arity-aware
   head dispatch: the registered replace operator is ternary): more(x,v,w,...rest) =
   replace(replace(x,v,w), rest) -- the exact pairwise left-fold resolvent, with the
   3-arg operator as the recursion's base case. */

/*********** RuimteScanner specifics *********/

/* _Tj never gets negative, but it can get 0; causing _Mj to be zero in the next iteration */
[(claim_divF32  _ADj _Mj) (iif (isPositive _ADj) (div _ADj _Mj) (Float32 0 ))]
[(claim_corrF32 _ADj _Mj _Oper) 
                     (iif (eq _Oper (Value 0 '/Classifications/OperatorType')) (min_elem (claim_divF32 _ADj _Mj) (Float32 1 )) /* resulting _Aj such that: _Mj * _Aj <= _ADj */
                     (iif (eq _Oper (Value 1 '/Classifications/OperatorType'))           (claim_divF32 _ADj _Mj)               /* resulting _Aj such that: _Mj * _Aj == _ADj */
                     (iif (eq _Oper (Value 2 '/Classifications/OperatorType')) (max_elem (claim_divF32 _ADj _Mj) (Float32 1 )) /* resulting _Aj such that: _Mj * _Aj >= _ADj */
                            (Float32 1 ) )))]

/*********** Logit funcs       *********/

/* llpart and ll1 retired to the typed prelude (res/prelude.dms, WP4.5) */

/*********** Relational funcs ***********/

[(rjoin _a _b _c)            (lookup (rlookup _a _b) _c)]
[(lookup (rlookup _a _a) _c) _c]
/* sort_str, reversed_id and reverse retired to the typed prelude
   (res/prelude.dms, WP4.5 tranche 3) */
/* the multi-argument index and subindex fold rules are DELETED (2026-07-18): they
   were dead as written -- the index resolvent referenced (index_a) as ONE symbol
   (a typo for (index _a)) and both expanded into rank_sorted / sub_rank_sorted
   heads that no rule or operator defines, so any multi-argument use errored at
   reduction anyway. Multi-argument index/subindex now report a clean operator
   arity error (or reach a same-named function via arity-aware head dispatch,
   should one ever be defined). */

/* combine_data is RETIRED to a prelude variadic function: base = the Value(...)
   linearization formula, fold = combine_data(V, combine_data(combine_unit(...),
   a1, a2), rest) -- the exact resolvents; ValuesUnit/LowerBound/NrOfRows args are
   calc_as_result (subst_allowed), so body params substitute identically to the
   syntactic _a/_b of this rule. */


// [(invert _X)              (rlookup (id (ValuesUnit _X)) _X)]

/*********** Rewrites for pseudo-aggregations ***********/

[(UInt32 (div _V _V)) (UInt32 1)]                                   // rewrite gridverhouding for 100m
[(div (pointrow _GRIDIDS) (convert (UInt32 1) _PRU)) (pointrow _GRIDIDS)] // rewrite pseudo divide
[(div (pointcol _GRIDIDS) (convert (UInt32 1) _PRU)) (pointcol _GRIDIDS)] // rewrite pseudo divide

[(div _X (UInt32 1)) _X] // rewrite calculation of #cells of gridset

[(convert (id _E) _E) (id _E)]

// don't aggregate over an id relation; assume that domain is compatible
[(mean  _X (id _E)) _X]
[(modus _X (id _E)) _X]
[(sum   _X (id _E)) _X]

)