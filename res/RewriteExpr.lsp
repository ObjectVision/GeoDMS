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
[(const _P _E _VU)               (const (Value _P _VU) _E)]

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

/*********** Elementary funcs  *********/

[(log _X _Y)                (div (log _X) (log _Y))]
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

[(add _a1)                    _a1]
[(mul _a1)                    _a1]
[(or  _a1)                    _a1]
[(and _a1)                    _a1]
[(MakeDefined _a1)            _a1]
[(min_elem _a1)               _a1]
[(max_elem _a1)               _a1]
[(min_elem_fast _a1)          _a1]
[(max_elem_fast _a1)          _a1]

[[add [_a1 [_a2 [_a3 _T]]]]  [add [(add _a1 _a2) [_a3 _T]]] ]
[[mul [_a1 [_a2 [_a3 _T]]]]  [mul [(mul _a1 _a2) [_a3 _T]]] ]
[[or  [_a1 [_a2 [_a3 _T]]]]  [or  [(or  _a1 _a2) [_a3 _T]]] ]
[[and [_a1 [_a2 [_a3 _T]]]]  [and [(and _a1 _a2) [_a3 _T]]] ]
[[MakeDefined [_a1 [_a2 [_a3 _T]]]]  [MakeDefined [(MakeDefined _a1 _a2) [_a3 _T]]] ]
[[union [_a1 [_a2 [_a3 _T]]]]  [union [(SubItem (union _a1 _a2) "UnionData") [_a3 _T]]] ]

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

[(median _a _b _c) (median _a (order _b _c)) ]

[(median _a (interval _b _c)) // use guarantee that _b <= _c
	(iif (le _a _b) 
		_b
		(min_elem _a _c)
	)
]
/* the *_or_*_null predicate family retired to the typed prelude (res/prelude.dms, WP4.5) */

/*********** Default arguments *********/

[(ReadValue _STRING _ValuesUnit _ReadPos) (ReadArray _STRING Void _ValuesUnit _ReadPos)]

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

[(MakeDefined (MakeDefined _X _Y) _Y) (MakeDefined _X _Y)]
[(MakeDefined _X _Y)            (iif (IsDefined _X) _X _Y)]

/* replace_value is RETIRED to a prelude variadic function: base = iif(eq(x,v),w,x),
   fold = replace_value(replace_value(x,v,w), rest) -- the exact resolvents. */

[[replace [_X [_V1 [_W1 [_V2 _T]]]]]  [replace [(replace _X _V1 _W1) [_V2 _T]]] ]

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
[[index [_a [_b _R]]]     [subindex [(index _a) [(rank_sorted (index_a) _a) [_b _R]]]] ]

[[subindex [_I [_O [_b [_c _R]]]]] 
    [subindex 
	[(subindex _I _O _b)
	[(sub_rank_sorted (subindex _I _O _b) _O _b)
	[_c _R]]]]]

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