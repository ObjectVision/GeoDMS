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

   _x: Value Attribute
   _p: Partitioner
   _s: Partition restriction

*/
(

/*********** Const function    *********/

[(value _P _VU)                  (convert _P _VU)]
[(const _P _E _VU)               (const (value _P _VU) _E)]
[(convert (convert _X _U1) _U2)  (convert _X _U2)]

/*********** Renamed Functions *********/

[(interpolate_linear _D _U) (interpolate_linear _D (SubItem _U "X") (SubItem _U "Y"))]

/*********** selection Functions *********/

[(select_data (select        _Cond) _Data) (select_data (select        _Cond) _Cond _Data)]
[(select_data (select_uint8  _Cond) _Data) (select_data (select_uint8  _Cond) _Cond _Data)]
[(select_data (select_uint16 _Cond) _Data) (select_data (select_uint16 _Cond) _Cond _Data)]
[(select_data (select_uint32 _Cond) _Data) (select_data (select_uint32 _Cond) _Cond _Data)]

[(select_data (select_org_rel        _Cond) _Data) (select_data (select_org_rel        _Cond) _Cond _Data)]
[(select_data (select_org_rel_uint8  _Cond) _Data) (select_data (select_org_rel_uint8  _Cond) _Cond _Data)]
[(select_data (select_org_rel_uint16 _Cond) _Data) (select_data (select_org_rel_uint16 _Cond) _Cond _Data)]
[(select_data (select_org_rel_uint32 _Cond) _Data) (select_data (select_org_rel_uint32 _Cond) _Cond _Data)]

[(select_data (select_unit        _Cond) _Data) (select_data (select_unit        _Cond) _Cond _Data)]
[(select_data (select_unit_uint8  _Cond) _Data) (select_data (select_unit_uint8  _Cond) _Cond _Data)]
[(select_data (select_unit_uint16 _Cond) _Data) (select_data (select_unit_uint16 _Cond) _Cond _Data)]
[(select_data (select_unit_uint32 _Cond) _Data) (select_data (select_unit_uint32 _Cond) _Cond _Data)]

[(select_data (select_orgrel        _Cond) _Data) (select_data (select_orgrel        _Cond) _Cond _Data)]
[(select_data (select_orgrel_uint8  _Cond) _Data) (select_data (select_orgrel_uint8  _Cond) _Cond _Data)]
[(select_data (select_orgrel_uint16 _Cond) _Data) (select_data (select_orgrel_uint16 _Cond) _Cond _Data)]
[(select_data (select_orgrel_uint32 _Cond) _Data) (select_data (select_orgrel_uint32 _Cond) _Cond _Data)]

[(collect_by_org_rel (select        _Cond) _Data) (collect_by_org_rel (select        _Cond) _Cond _Data)]
[(collect_by_org_rel (select_uint8  _Cond) _Data) (collect_by_org_rel (select_uint8  _Cond) _Cond _Data)]
[(collect_by_org_rel (select_uint16 _Cond) _Data) (collect_by_org_rel (select_uint16 _Cond) _Cond _Data)]
[(collect_by_org_rel (select_uint32 _Cond) _Data) (collect_by_org_rel (select_uint32 _Cond) _Cond _Data)]

[(collect_by_org_rel (select_org_rel        _Cond) _Data) (collect_by_org_rel (select_org_rel        _Cond) _Cond _Data)]
[(collect_by_org_rel (select_uint8_org_rel  _Cond) _Data) (collect_by_org_rel (select_uint8_org_rel  _Cond) _Cond _Data)]
[(collect_by_org_rel (select_uint16_org_rel _Cond) _Data) (collect_by_org_rel (select_uint16_org_rel _Cond) _Cond _Data)]
[(collect_by_org_rel (select_uint32_org_rel _Cond) _Data) (collect_by_org_rel (select_uint32_org_rel _Cond) _Cond _Data)]

/*********** Elementary funcs  *********/

[(log _x _y)                (div (log _x) (log _y))]
[(plogp _x)                 (MakeDefined (mul _x (log _x)) 0.0)]
[(sqr _x)                   (mul _x _x)]
[(abs _x)                   (iif (isNegative _x) (neg _x) _x)]
[(pow _x 2)                 (sqr _x)]
[(pow _x 3)                 (mul _x (sqr _x))]
[(pow _x 4)                 (sqr (sqr _x))]
[(pow _x 5)                 (mul _x (pow _x 4))]
[(pow _x 6)                 (mul (sqr _x) (pow _x 4))]
[(pow x (neg _y))           (div 1 (pow _x _y))]

[(pow _x _y)                (exp (mul (log _x) _y))]

/* right associative */
/* REMOVE
[(max_elem_r _a)             _a]
[(min_elem_r _a)             _a]
[[max_elem_r [_a _T]]        (iif (gt_or_rhs_null [max_elem_r _T] _a) [max_elem_r _T] _a)]
[[min_elem_r [_a _T]]        (iif (lt_or_rhs_null [min_elem_r _T] _a) [min_elem_r _T] _a)]

[(max_elem_fast_r _a)             _a]
[(min_elem_fast_r _a)             _a]
[[max_elem_fast_r [_a _T]]        (iif (gt [max_elem_fast_r _T] _a) [max_elem_fast_r _T] _a)]  // take left only if left is not less that right; take right is one of them is null
[[min_elem_fast_r [_a _T]]        (iif (lt [min_elem_fast_r _T] _a) [min_elem_fast_r _T] _a)]  // take left only if left is not greater that right; take right is one of them is null
*/

/*********** Helper functions for lists are OBSOLETE and DEPRECIATED  *********/

[[add_list _T] [add _T]]
[[mul_list _T] [mul _T]]
[[or_list  _T] [or  _T]]
[[and_list _T] [and _T]]

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
[[union [_a1 [_a2 [_a3 _T]]]]  [union [(subitem (union _a1 _a2) "UnionData") [_a3 _T]]] ]

[(concat _a1) (MakeDefined _a1 "")]
[[concat [_a1 _T]] (add (MakeDefined _a1 "") [concat _T] ) ]
[(concat) ""]


//[(add _a (add _b _c))        (add (add _a _b) _c) ]
//[(mul _a (mul _b _c))        (mul (mul _a _b) _c) ] geeft problemen bij claimHa2 = (10000 *Meter *Meter) * (10000 *Meter *Meter)
//[(and _a (and _b _c))        (and (and _a _b) _c) ]
//[(or  _a (or  _b _c))        (or  (or  _a _b) _c) ]


/*********** switch case   *********/

[[switch [(case (false) _V) _REST]]
							[switch _REST]]
[[switch [(case (true) _V) _REST]]
							_V]
[[switch [(case _C _V) _REST]]
							(iif _C _V [switch _REST])]
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
[(baseunit _SI "Bool") (baseunit _SI Bool)]
[(baseunit "" Bool) Bool]
[(convert 1.0 Bool) (true)]
[(convert 0.0 Bool) (false)]

/*********** Predicate functions *********/

// [(isPositive _x)      (gt _x (neg _x))]
// [(isNegative _x)      (lt _x (neg _x))]
// [(isZero     _x)      (eq _x (neg _x))]

[(order _A _B) (interval (min_elem _A _B) (max_elem _A _B))]

[(isOverlapping (interval _A1 _A2)(interval _B1 _B2))
	(and
		(le_or_lhs_null _B1 _A2)
		(le_or_lhs_null _A1 _B2)
	)
]

[(neighbourhood _X _Factor) (order _X (mul _Factor _X) )]

[(float_isNearby _A _B _Factor)
	(isOverlapping (neighbourhood _A _Factor) (neighbourhood _B _Factor))]

[(point_isNearby _A _B _Factor)
	(and
		(float_isNearby (pointRow _A) (pointRow _B) _Factor)
		(float_isNearby (pointCol _A) (pointCol _B) _Factor)
	)
]

[(median _A _B _C) (median _A (order _B _C)) ]

[(median _A (interval _B _C)) // use guarantee that _B <= _C
	(iif (le _A _B) 
		_B
		(min_elem _A _C)
	)
]
[(eq_or_both_null _A _B) (or (eq _A _B) (and (isNull _A) (isNull _B)))]
[(lt_or_lhs_null  _A _B) (or (lt _A _B) (and (isNull _A) (not (isNull _B))))]
[(gt_or_lhs_null  _A _B) (or (gt _A _B) (and (isNull _A) (not (isNull _B))))]
[(le_or_lhs_null  _A _B) (or (le _A _B) (isNull _A))]
[(ge_or_lhs_null  _A _B) (or (ge _A _B) (isNull _A))]
[(lt_or_rhs_null  _A _B) (gt_or_lhs_null _B _A)]
[(gt_or_rhs_null  _A _B) (lt_or_lhs_null _B _A)]
[(ge_or_rhs_null  _A _B) (le_or_lhs_null _B _A)]
[(le_or_rhs_null  _A _B) (ge_or_lhs_null _B _A)]
[(ne_or_one_null  _A _B) (not (eq_or_both_null _A _B))]

/*********** Default arguments *********/

[(ReadValue _STRING _ValuesUnit _ReadPos) (ReadArray _STRING void _ValuesUnit _ReadPos)]

/*********** Rescale functions *********/

[(distribute _s _x   )      (scalesum _x    _s)]
[(distribute _s _x _p)      (scalesum _x _p _s)]

[(scalesum _x    _s)        (iif (isZero     _s) _s (mul _x            (div _s (sum _x   )) ))]
[(scalesum _x _p _s)        (iif (isZero     (lookup _p _s)) (lookup _p _s) (mul _x (lookup _p (div _s (sum _x _p)))))]
//[(scalesum _x    _s)        (mul _x            (div _s (sum _x   )) )]
//[(scalesum _x _p _s)        (mul _x (lookup _p (div _s (sum _x _p))))]

[(rescale  _x)              (rescale _x 0 1)] /* is rewritten to (div (sub _x (min _x)) (sub (max _x)(min _x))) */
[(rescale  _x    _min _max) 
                     (add (mul 
                        (sub _x (min _x))
                        (div 
                           (sub _max _min) 
                           (sub (max _x)(min _x))
                        )
                     ) _min)]
[(rescale  _x _p _min _max) 
                     (add (mul 
                        (sub _x (lookup _p (min _x _p)))
                        (lookup _p 
                           (div 
                              (sub _max _min) 
                              (sub (max _x _p)(min _x _p))
                           )
                        )
                     ) _min)]

[(normalize _x )           (normalize _x  0 1 )] /* is rewritten to: (div (sub _x  (mean _x )) (sd   _x )) */
[(normalize _x _E )        (normalize _x _E 1 )] /* is rewritten to: (add (div (sub _x  (mean _x )) (sd   _x )) _E) */

[(normalize _x _E _SD)     (add (mul (sub _x  (mean _x )) (div _SD (sd   _x ))) _E)]
[(normalize _x _p _E _SD)  (add (mul (sub _x  (lookup _p (mean _x _p))) 
                               (lookup _p (div _SD (sd   _x _p)))) _E)]

/*********** Remove symbolic constants from rescale & normalize *********/

[(add _x 0)          _x]
[(sub _x 0)          _x]
[(mul _x (div 1 _y)) (div _x _y)]

/*********** Properties        *********/

[(NAME   _T)       (PropValue _T "name" )]
[(DESCR  _T)       (PropValue _T "Descr")]
[(EXPR   _T)       (PropValue _T "Expr" )]
[(LABEL  _T)       (PropValue _T "Label")]
[(STORAGE _T)      (PropValue _T "StorageName")]
[(EK _T)           (PropValue _T "ExternalKeyData")]	
[(RESULT _T)       (_T) ]

/*********** Missing values    *********/

[(MakeDefined (MakeDefined _X _Y) _Y) (MakeDefined _X _Y)]
[(MakeDefined _X _Y)            (iif (IsDefined _X) _X _Y)]

[(replace_value _X _V _W) (iif (eq _X _V) _W _X)]
[[replace_value [_X [_V1 [_W1 [_V2 _T]]]]]  [replace_value [(replace_value _X _V1 _W1) [_V2 _T]]] ]

[[replace [_X [_V1 [_W1 [_V2 _T]]]]]  [replace [(replace _X _V1 _W1) [_V2 _T]]] ]

/*********** EXECUTE External .exe and .dll functions *********/

/* TODO
// [(EXEC_ACCESS_PROC   _PROC)      (EXECDLL "AccessRun.dll" "RunProcAndWait"   ProcMdb _PROC)]
// [(EXEC_ACCESS_ACTION _ACT )      (EXECDLL "AccessRun.dll" "RunActionAndWait" ProcMdb _ACT)]
*/

[(EXEC_ACCESS_PROC_DB        _DB _PROC) (EXECDLL "AccessRun.dll" "RunProcAndWait"            _DB _PROC)]
[(EXEC_ACCESS_ACTION_DB      _DB _ACT ) (EXECDLL "AccessRun.dll" "RunActionAndWait"          _DB _ACT )]
[(EXEC_VIS_ACCESS_PROC_DB    _DB _ACT ) (EXECDLL "AccessRun.dll" "RunProcVisible"            _DB _ACT )]
[(EXEC_VIS_ACCESS_ACTION_DB  _DB _ACT ) (EXECDLL "AccessRun.dll" "RunActionVisible"          _DB _ACT )]
[(EXEC_WAIT_ACCESS_ACTION_DB _DB _ACT ) (EXECDLL "AccessRun.dll" "RunActionAndWaitUserClose" _DB _ACT )]

[(EXEC_ACCESS_SQL    _TBL _SQL)  (EXECDLL "AccessRun.dll" "RunSqlAndWait" "storage_name" _TBL _SQL)]

/*********** RuimteScanner specifics *********/

/* _Tj never gets negative, but it can get 0; causing _Mj to be zero in the next iteration */
[(claim_div     _ADj _Mj) (iif (isPositive _ADj) (div _ADj _Mj) (value 0 '/Units/DemandUnit'))]
[(claim_divF32  _ADj _Mj) (iif (isPositive _ADj) (div _ADj _Mj) (Float32 0 ))]
[(claim_divF64  _ADj _Mj) (iif (isPositive _ADj) (div _ADj _Mj) 0.0)]
[(claim_divF32D _ADj _Mj) 
					(iif (isPositive _ADj) 
						(div _ADj (max_elem _Mj (value 20 ggHa))) 
						(Float32 0 ))]

[(claim_corr _ADj _Mj _Oper) 
                     (iif (eq _Oper (value 0 '/Units/OperatorRange')) (min_elem (claim_div _ADj _Mj) (value 1 '/Units/DemandUnit')) /* resulting _Aj such that: _Mj * _Aj <= _ADj */
                     (iif (eq _Oper (value 1 '/Units/OperatorRange'))           (claim_div _ADj _Mj)                                /* resulting _Aj such that: _Mj * _Aj == _ADj */   
                     (iif (eq _Oper (value 2 '/Units/OperatorRange')) (max_elem (claim_div _ADj _Mj) (value 1 '/Units/DemandUnit')) /* resulting _Aj such that: _Mj * _Aj >= _ADj */
                            (value 1 '/Units/DemandUnit') )))]
[(claim_corrF32 _ADj _Mj _Oper) 
                     (iif (eq _Oper (value 0 '/Classifications/OperatorType')) (min_elem (claim_divF32 _ADj _Mj) (Float32 1 )) /* resulting _Aj such that: _Mj * _Aj <= _ADj */
                     (iif (eq _Oper (value 1 '/Classifications/OperatorType'))           (claim_divF32 _ADj _Mj)               /* resulting _Aj such that: _Mj * _Aj == _ADj */
                     (iif (eq _Oper (value 2 '/Classifications/OperatorType')) (max_elem (claim_divF32 _ADj _Mj) (Float32 1 )) /* resulting _Aj such that: _Mj * _Aj >= _ADj */
                            (Float32 1 ) )))]

[(claim_corrF32D _ADj _Mj _Oper) 
                     (iif (eq _Oper (value 0 '/Classifications/OperatorType')) (min_elem (claim_divF32D _ADj _Mj) (Float32 1 )) /* resulting _Aj such that: _Mj * _Aj <= _ADj */
                     (iif (eq _Oper (value 1 '/Classifications/OperatorType'))           (claim_divF32D _ADj _Mj)               /* resulting _Aj such that: _Mj * _Aj == _ADj */
                     (iif (eq _Oper (value 2 '/Classifications/OperatorType')) (max_elem (claim_divF32D _ADj _Mj) (Float32 1 )) /* resulting _Aj such that: _Mj * _Aj >= _ADj */
                            (Float32 1 ) )))]
[(claim_corrF32DL _ADj _Mj _Oper) 
                     (iif (eq _Oper (value 0 '/Classifications/OperatorType')) (min_elem (sub _ADj _Mj) (value 0 EUR_M2)) /* resulting _Aj such that: _Mj * _Aj <= _ADj */
                     (iif (eq _Oper (value 1 '/Classifications/OperatorType'))           (sub _ADj _Mj)                   /* resulting _Aj such that: _Mj * _Aj == _ADj */
                     (iif (eq _Oper (value 2 '/Classifications/OperatorType')) (max_elem (sub _ADj _Mj) (value 0 EUR_M2)) /* resulting _Aj such that: _Mj * _Aj >= _ADj */
                            (value 0 EUR_M2) )))]

[(claim_minmax_corrF32 _ADj_min _ADj_max _Mj) 
	(median 
		(Float32 1 ) 
		(interval
			(MakeDefined (claim_divF32 _ADj_min _Mj) (Float32 0))
			(MakeDefined (claim_divF32 _ADj_max _Mj) (Float32 1))
		)
	)
]

[(claim_minmax_corrF64 _ADj_min _ADj_max _Mj) 
	(median 
		1.0 
		(interval
			(MakeDefined (claim_divF64 _ADj_min _Mj) 0.0)
			(MakeDefined (claim_divF64 _ADj_max _Mj) 1.0)
		)
	)
]

[(claim_minmax_corrF32D _ADj_min _ADj_max _Mj) 
	(median 
		(Float32 1 ) 
		(interval
			(MakeDefined (claim_divF32D _ADj_min _Mj) (Float32 0))
			(MakeDefined (claim_divF32D _ADj_max _Mj) (Float32 1))
		)
	)
]

[(claim_minmax_corrF32L _logADj_min _logADj_max _logMj) 
	(median 
		(Float32 0 ) 
		(interval
			(MakeDefined (sum _logADj_min _logMj) (Float32 -80))
			(MakeDefined (sum _logADj_max _logMj) (Float32 0))
		)
	)
]

/*********** Logit funcs       *********/

[(llpart _lc _xc _oc)       (sub (mul _oc (log _xc)) (log _lc))]
[(ll1    _lc _xc _oc)       (add (llpart _lc _xc _oc) (llpart _lc (sub _lc _xc) (sub _lc _oc)))]

/*********** Relational funcs ***********/

[(rjoin _a _b _c)            (lookup (rlookup _a _b) _c)]
[(lookup (rlookup _a _a) _c) _c]
[(sort_str _a)            (lookup (index _a) _a)]
[(reversed_id _D)         (sub (sub  (add (UpperBound _D) (LowerBound _D) ) (convert 1 _D)) (ID _D))]
[(reverse _a)             (lookup (reversed_id (domainUnit _a)) _a)]
[[index [_a [_b _R]]]     [subindex [(index _a) [(rank_sorted (index_a) _a) [_b _R]]]] ]

[[subindex [_I [_O [_b [_c _R]]]]] 
    [subindex 
	[(subindex _I _O _b)
	[(sub_rank_sorted (subindex _I _O _b) _O _b)
	[_c _R]]]]]

[(combine_data _V _a _b) (value (add (mul (sub (int64 _a) (int64 (LowerBound (valuesUnit _a)))) (int64 (NrOfRows (valuesUnit _b)))) (sub (int64 _b)(int64 (LowerBound (valuesUnit _b))))) _V)]

// [(invert _X)              (rlookup (ID (ValuesUnit _X)) _X)]

/*********** Rewrites for pseudo-aggregations ***********/

[(UInt32 (div _V _V)) (UInt32 1)]                                   // rewrite gridverhouding for 100m
[(div (pointrow _GRIDIDS) (convert (UInt32 1) _PRU)) (pointrow _GRIDIDS)] // rewrite pseudo divide
[(div (pointcol _GRIDIDS) (convert (UInt32 1) _PRU)) (pointcol _GRIDIDS)] // rewrite pseudo divide

//[(point (Float32 (neg (convert _V1 _U))) (Float32 (convert _V2 _U)) _GU) (point (Float32 (neg _V1)) (Float32 _V2) _GU)]
//[(Float32 (neg (UInt32 _V))) (Float32 (neg _V))]
//[(Float32      (UInt32 _V) ) (Float32      _V )]

[(div _X (UInt32 1)) _X] // rewrite calculation of #cells of gridset

[(convert (ID _E) _E) (ID _E)]

// don't aggregate over an ID relation; assume that domain is compatible
[(mean  _X (ID _E)) _X]
[(modus _X (ID _E)) _X]
[(sum   _X (ID _E)) _X]

)