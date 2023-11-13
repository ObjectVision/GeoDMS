Consider the following problem:

Known:

-   *G*<sub>*g*</sub><sup>*r*</sup>: number of objects of g-type *g* in
    zone *r*, aka row-totals.
-   *H*<sub>*h*</sub><sup>*r*</sup>: number of objects of h-type *h* in
    zone *r*, aka column-totals.

Requested:

-   *Q*<sub>*g**h*</sub><sup>*r*</sup>, an estimated number of objects
    with g-type *g* and h-type *h* in zone *r*.
-   estimated number of objects per h-type as a function (preferably a
    linear combination) of object numbers per h-type (for estimating a
    h-type distribution given a g-type distribution of new objects).

We assume that:

-   For each zone *r*, the *F* and *G* count all objects, thus:
    $\\forall r: \\sum\\limits_g G^r_g =  \\sum\\limits_h H^r_h$
-   *P*<sub>*h*</sub><sup>*i*</sup> is equal for all *i* in the same *r*
    and with the same *g*, thus depends only on *P*(*h*\|*r**g*).
-   *Q*<sub>*g**h*</sub><sup>*r*</sup> has a
    *G*<sub>*g*</sub><sup>*r*</sup> repeated [categorical
    distribution](http://en.wikipedia.org/wiki/Categorical_distribution)
    per row *g* and zone *r*; thus
    *E*\[*Q*<sub>*g**h*</sub><sup>*r*</sup>\] = *P*(*h*\|*r**g*) ⋅ *G*<sub>*g*</sub><sup>*r*</sup>
-   *Q*<sub>*g**h*</sub><sup>*r*</sup> := *f*<sub>*g*</sub> ⋅ *f*<sub>*h*</sub> ⋅ *P*<sub>*g**h*</sub>
    such that $\\sum\\limits_{h} Q^r_{gh} = G^r_g$ and
    $\\sum\\limits_{g} Q^r_{gh} = H^r_h$, to be determined by
    [Iterative proportional
    fitting](Iterative_proportional_fitting "wikilink")

Thus:

-   $\\sum\\limits_{h} P(h\|rg) = 1$,
    $f_g = G^r_g / \\sum\\limits_{h} f_h \\cdot P_{gh}$, and
    $f_h = H^r_h / \\sum\\limits_{g} f_g \\cdot P_{gh}$.
-   *P*<sub>*g**h*</sub> is to be determined by regression of
    *H*<sub>*h*</sub><sup>*r*</sup> by *G*<sub>*g*</sub><sup>*r*</sup>,
    thus, written in matrix notation: *H* = *G* × *P* + *ϵ* with *ϵ* a
    *r* × *h* matrix of independent stochasts with zero expectation.
-   it follows that:
    *P* := (*G*<sup>*T*</sup>×*G*)<sup>−1</sup> × (*G*<sup>*T*</sup>×*H*)

ToDo:

-   Consider Alternative: *Q*<sub>*b**j**h*</sub> is determined by
    discrete allocation with the given constraints and suitability
    *P*<sub>*g**h*</sub>
-   Consider Alternative:
    *P* := (*G*<sup>*T*</sup>×*H*) × (*H*<sup>*T*</sup>×*H*)<sup>−1</sup>
    which follows from regression of *G*<sub>*g*</sub><sup>*r*</sup> by
    *H*<sub>*h*</sub><sup>*r*</sup>.
-   Consider Alternative: *P* := (*G*<sup>*T*</sup>×*H*)
-   Consider Effect of possible
    [heteroscedasticity](http://en.wikipedia.org/wiki/Heteroscedasticity)
    of *ϵ* with *H*; i.e. assume *v**a**r*(*ϵ*) ∼ *H*, as each element
    of *H* is assumed to be the sum over *j* of the results of
    *G*<sub>*g*</sub><sup>*r*</sup> trials of [categorical
    distributions](http://en.wikipedia.org/wiki/Categorical_distribution)
    with conditional probabilities *P*<sub>*g*\|*h*</sub>.