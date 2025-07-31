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
[(plogp _X)                 (MakeDefined (mul _X (log _X)) 0.0)]
[(sqr _X)                   (mul _X _X)]
[(abs _X)                   (iif (isNegative _X) (neg _X) _X)]
[(pow _X 2)                 (sqr _X)]
[(pow _X 3)                 (mul _X (sqr _X))]
[(pow _X 4)                 (sqr (sqr _X))]
[(pow _X 5)                 (mul _X (pow _X 4))]
[(pow _X 6)                 (mul (sqr _X) (pow _X 4))]
[(pow x (neg _Y))           (div 1 (pow _X _Y))]

[(pow _X _Y)                (exp (mul (log _X) _Y))]
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

[(concat _a1) (MakeDefined _a1 "")]
[[concat [_a1 _T]] (add (MakeDefined _a1 "") [concat _T] ) ]
[(concat) ""]

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

[(isOverlapping (interval _a1 _a2)(interval _B1 _B2))
	(and
		(le_or_lhs_null _B1 _a2)
		(le_or_lhs_null _a1 _B2)
	)
]

[(neighbourhood _X _Factor) (order _X (mul _Factor _X) )]

[(float_isNearby _a _b _Factor)
	(isOverlapping (neighbourhood _a _Factor) (neighbourhood _b _Factor))]
   
[(point_isNearby _a _b _Factor)
	(and
		(float_isNearby (pointrow _a) (pointrow _b) _Factor)
		(float_isNearby (pointcol _a) (pointcol _b) _Factor)
	)
]

[(median _a _b _c) (median _a (order _b _c)) ]

[(median _a (interval _b _c)) // use guarantee that _b <= _c
	(iif (le _a _b) 
		_b
		(min_elem _a _c)
	)
]
[(eq_or_both_null _a _b) (or (eq _a _b) (and (IsNull _a) (IsNull _b)))]
[(lt_or_lhs_null  _a _b) (or (lt _a _b) (and (IsNull _a) (not (IsNull _b))))]
[(gt_or_lhs_null  _a _b) (or (gt _a _b) (and (IsNull _a) (not (IsNull _b))))]
[(le_or_lhs_null  _a _b) (or (le _a _b) (IsNull _a))]
[(ge_or_lhs_null  _a _b) (or (ge _a _b) (IsNull _a))]
[(lt_or_rhs_null  _a _b) (gt_or_lhs_null _b _a)]
[(gt_or_rhs_null  _a _b) (lt_or_lhs_null _b _a)]
[(ge_or_rhs_null  _a _b) (le_or_lhs_null _b _a)]
[(le_or_rhs_null  _a _b) (ge_or_lhs_null _b _a)]
[(ne_or_one_null  _a _b) (not (eq_or_both_null _a _b))]

/*********** Default arguments *********/

[(ReadValue _STRING _ValuesUnit _ReadPos) (ReadArray _STRING Void _ValuesUnit _ReadPos)]

/*********** Rescale functions *********/

[(distribute _s _X   )      (scalesum _X    _s)]
[(distribute _s _X _P)      (scalesum _X _P _s)]

[(scalesum _X    _s)        (iif (isZero     _s) _s (mul _X            (div _s (sum _X   )) ))]
[(scalesum _X _P _s)        (iif (isZero     (lookup _P _s)) (lookup _P _s) (mul _X (lookup _P (div _s (sum _X _P)))))]
//[(scalesum _X    _s)        (mul _X            (div _s (sum _X   )) )]
//[(scalesum _X _P _s)        (mul _X (lookup _P (div _s (sum _X _P))))]

[(rescale  _X)              (rescale _X 0 1)] /* is rewritten to (div (sub _X (min _X)) (sub (max _X)(min _X))) */
[(rescale  _X    _min _max) 
                     (add (mul 
                        (sub _X (min _X))
                        (div 
                           (sub _max _min) 
                           (sub (max _X)(min _X))
                        )
                     ) _min)]
[(rescale  _X _P _min _max) 
                     (add (mul 
                        (sub _X (lookup _P (min _X _P)))
                        (lookup _P 
                           (div 
                              (sub _max _min) 
                              (sub (max _X _P)(min _X _P))
                           )
                        )
                     ) _min)]

[(normalize _X )           (normalize _X  0 1 )] /* is rewritten to: (div (sub _X  (mean _X )) (sd   _X )) */
[(normalize _X _E )        (normalize _X _E 1 )] /* is rewritten to: (add (div (sub _X  (mean _X )) (sd   _X )) _E) */
[(normalize _X _E _SD)     (add (mul (sub _X  (mean _X )) (div _SD (sd   _X ))) _E)]

/*********** Remove symbolic constants from rescale & normalize *********/

[(add _X 0)          _X]
[(sub _X 0)          _X]
[(mul _X (div 1 _Y)) (div _X _Y)]

/*********** Properties        *********/

[(name   _T)       (PropValue _T "name" )]
[(Descr  _T)       (PropValue _T "Descr")]
[(Expr   _T)       (PropValue _T "Expr" )]
[(Label  _T)       (PropValue _T "Label")]
[(STORAGE _T)      (PropValue _T "StorageName")]
[(EK _T)           (PropValue _T "ExternalKeyData")]	
[(result _T)       (_T) ]

/*********** Missing values    *********/

[(MakeDefined (MakeDefined _X _Y) _Y) (MakeDefined _X _Y)]
[(MakeDefined _X _Y)            (iif (IsDefined _X) _X _Y)]

[(replace_value _X _V _W) (iif (eq _X _V) _W _X)]
[[replace_value [_X [_V1 [_W1 [_V2 _T]]]]]  [replace_value [(replace_value _X _V1 _W1) [_V2 _T]]] ]

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

[(llpart _lc _xc _oc)       (sub (mul _oc (log _xc)) (log _lc))]
[(ll1    _lc _xc _oc)       (add (llpart _lc _xc _oc) (llpart _lc (sub _lc _xc) (sub _lc _oc)))]

/*********** Relational funcs ***********/

[(rjoin _a _b _c)            (lookup (rlookup _a _b) _c)]
[(lookup (rlookup _a _a) _c) _c]
[(sort_str _a)            (lookup (index _a) _a)]
[(reversed_id _D)         (sub (sub  (add (UpperBound _D) (LowerBound _D) ) (id _D)) (convert 1 _D))]
[(reverse _a)             (lookup (reversed_id (DomainUnit _a)) _a)]
[[index [_a [_b _R]]]     [subindex [(index _a) [(rank_sorted (index_a) _a) [_b _R]]]] ]

[[subindex [_I [_O [_b [_c _R]]]]] 
    [subindex 
	[(subindex _I _O _b)
	[(sub_rank_sorted (subindex _I _O _b) _O _b)
	[_c _R]]]]]

[(combine_data _V _a _b) (Value (add (mul (sub (Int64 _a) (Int64 (LowerBound (ValuesUnit _a)))) (Int64 (NrOfRows (ValuesUnit _b)))) (sub (Int64 _b)(Int64 (LowerBound (ValuesUnit _b))))) _V)]

[[combine_data [_V [_a1 [_a2 [_a3 _T]]]]]  [combine_data [_V [(combine_data (combine_unit (ValuesUnit _a1) (ValuesUnit _a2) ) _a1 _a2) [_a3 _T]]]] ]


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