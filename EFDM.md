# intro

Model description of proposed
[<https://forestwiki.jrc.ec.europa.eu/efdm/index.php/Main_Page>
EFDM](https://forestwiki.jrc.ec.europa.eu/efdm/index.php/Main_Page_EFDM "wikilink")
extensions.

Included are:

-   Fitting activities to regional commodity demands
-   Allocating activities to a raster of discrete forest data and apply
    transition rules locally.

Not included are:

-   Allowing for non-even forests. We remain the assumption that forests
    belong to a specific age-group and don't have a mixed state.
    However, it might be possible within the proposed framework to model
    non-even age groups as specific age categories that participate in
    the transition rules (such as thinning an even aged forest might
    cause it to become oddly aged).

# indices

-   Time *t*

The following indices can be country specific:

-   Raster cell *i*
-   Region *r*
-   Species *s*, which may include several non-mutating characteristics
    such as Siteclass, but is initially limited to *conifer* and
    *broad-leaf*.
-   Volume Classes 1..11: *v*, *w*
-   Age groups 1..33: *g*, *h*
-   Activity 1..3: *a*, *b*, usually: no activity, thinning, and full
    felling.
-   Commodity 1..4: *c*, usually: Coniferous Pulp, Coniferous Timber,
    Broad-leaf Pulp, and Broad-leaf Timber.

<!-- -->

-   Forest type *f*: a combination of and substitute for *s*, *v*, and
    *g*.

A symbol $\\sideset{_{t}^{f}}{_{a}^{b}}X$ indicates a set of tables or
matrices
[<http://www.objectvision.nl/geodms/operators-a-functions/metascript/for_each>
for
each](http://www.objectvision.nl/geodms/operators-a-functions/metascript/for_each_for_each "wikilink")
*f* with *a* indicating the set of rows and *b* indicating the set of
columns or values indicating a (probabilistic) mapping from the set of
*a*'s to the set of *b*'s, defined in a script that is instantiated
[<http://www.objectvision.nl/geodms/operators-a-functions/metascript/for_each>
for
each](http://www.objectvision.nl/geodms/operators-a-functions/metascript/for_each_for_each "wikilink")
*t*. Although Region *r* is mostly used as upper-left index, in the
implementation it might become part of the lower-right indices, thus
part of the rowset definitions.

# regional model

Input:

-   Initial State vectors: $\\sideset{_0^{rs}}{_{vg}}X$ in \[ha\]
-   State Transition fraction matrices:
    $\\sideset{^{ras}}{_{wh}^{vg}}P$, given per region. This will be
    implemented as a sparse matrix (a table with only the nonzero
    elements) for each *r*, *a* and *s*.
-   Activity fraction Prior matrices: $\\sideset{^{rs}}{_{vg}^a}A$.
-   Activity Yield matrices: $\\sideset{^{rs}}{_{vg}^{ca}}H$ in
    \[q/ha\], given per region.
-   National Demand for Commodities covectors: $\\sideset{_t}{^c}D$ in
    \[q\]

For each *a* and *t* use [Iterative proportional
fitting](Iterative_proportional_fitting "wikilink") or [Continuous
Allocation](Continuous_Allocation "wikilink") to find balancing factors
$\\sideset{_t^{s}}{^a}\\lambda$ such that
$\\forall c: \\sum\\limits_{rasvg} \\sideset{^{rs}}{_{vg}^{ca}}H \\cdot \\sideset{_t^{rs}}{_{vg}^a}B \\cdot \\sideset{_t^{rs}}{_{vg}}X = \\sideset{_t}{^c}D$

with Activity fraction Posterior matrices
$\\sideset{_t^{rs}}{_{vg}^a}B := {{\\sideset{_t^{s}}{^a}\\lambda \\cdot \\sideset{^{rs}}{_{vg}^a}A} \\over {\\sum\\limits_{b}\\sideset{_t^{s}}{^b}\\lambda \\cdot \\sideset{^{rs}}{_{vg}^b}A}}$

This requires that each *c* is linked to a disjunct non-empty set of
combinations of *a* and *s* that contributes most to that commodity,
maybe determined for each *a* and *s* by
$\\underset{c}{\\mathbf{argmax}}\\left( \\sum\\limits_{rvg} \\sideset{^{rs}}{_{vg}^{ca}}H\\right)$

Then apply the State Transition and Posterior Activity fractions to
calculate State vectors for the next time step:
$\\sideset{_{t+1}^{rs}}{_{wh}}X := \\sum\\limits_{avg} \\sideset{^{ras}}{_{wh}^{vg}}P \\cdot \\sideset{_t^{rs}}{_{vg}^a}B \\cdot \\sideset{_{t}^{rs}}{_{vg}}X$

Note that this application seems equivalent to the current EFDM when all
balancing factors are assumed to be 1.

# raster model

Input:

-   Initial State incidence map: $\\sideset{_0}{_i^{svg}}Y$ \[ha\]
    with exactly one 1\[ha\] in each row.
-   Region incidence map: *R*<sub>*i*</sub><sup>*r*</sup> with exactly
    one 1 in each row.
-   Activity Suitability maps: $\\sideset{^a}{_i}S$ \[utility\]

Incidence maps will be implemented as index vectors
$\\sideset{_0}{_i}{svg}$ and *r*<sub>*i*</sub>, but for simplicity,
the matrix notation is used here.

It is a precondition that
$\\forall r, s, v, g: \\sum\\limits_{i} R_i^r \\cdot \\sideset{_0}{_i^{svg}}Y = \\sideset{_0^{rs}}{_{vg}}X$

For each t, r, s, v, and g use [Discrete
Allocation](Discrete_Allocation "wikilink") to disaggregate the
regional amount of activities resulting in an Activity incidence map
$\\sideset{_t}{_i^{a}}M$ that maximizes total Suitability
$\\sum\\limits_{ia} \\sideset{^a}{_i}S \\cdot \\sideset{_t}{_i^{a}}M$
subject to
$\\forall a: \\sum\\limits_{i} R_i^r \\cdot \\sideset{_t}{_i^{svg}}Y \\cdot \\sideset{_t}{_i^{a}}M = \\sideset{_t^{rs}}{_{vg}^a}B \\cdot \\sideset{_t^{rs}}{_{vg}}X$

Then apply the state transitions and allocated activities to calculate a
probability distrbution for the state of each cell in the next time step
and draw a sample from that:
$E\\left\[ \\sideset{_{t+1}}{_i^{swh}}Y \\right\] = Prob( \\sideset{_{t+1}}{_i^{swh}}Y = 1 ) := \\sum\\limits_{avg} \\sideset{^{as}}{_{wh}^{vg}}P \\cdot \\sideset{_t}{_i^{a}}M \\cdot \\sideset{_t}{_i^{svg}}Y$

Note that the expected occurrence of each new state balances with the
new regional totals since for each *t*, *r*, *s*, *w*, and *h*:
$E\\left\[ \\sum\\limits_{i} R_i^r \\cdot \\sideset{_{t+1}}{_i^{swh}}Y \\right\] = \\sum\\limits_{i} R_i^r \\cdot E\\left\[ \\sideset{_{t+1}}{_i^{swh}}Y \\right\] = \\sum\\limits_{i} \\left\[ R_i^r \\cdot \\sum\\limits_{avg} \\sideset{^{as}}{_{wh}^{vg}}P \\cdot \\sideset{_t}{_i^{a}}M \\cdot \\sideset{_t}{_i^{svg}}Y \\right\] = \\sum\\limits_{avg} \\left\[ \\sideset{^{as}}{_{wh}^{vg}}P \\cdot \\sum\\limits_{i} R_i^r \\cdot \\sideset{_t}{_i^{svg}}Y \\cdot \\sideset{_t}{_i^{a}}M \\right\] = \\sum\\limits_{avg} \\sideset{^{as}}{_{wh}^{vg}}P \\cdot \\sideset{_t^{rs}}{_{vg}^a}B \\cdot \\sideset{_t^{rs}}{_{vg}}X = \\sideset{_{t+1}^{rs}}{_{wh}}X$

Additional notes:

The raster model would use the discrete allocation operation to allocate
activities that maximize suitability within the bounds set by the
regional amount of activities (expressed as constraints after the
"subject to"). This means that decisions are taken locally based on a
suitability that is augmented (within the discrete allocation) by a
shadow price per region per activity to meet these constraints. A
positive shadow price reflects a 'subsidy' that might be required to get
sufficient locations for an activity whereas a negative shadow price
reflects a 'taxation' to prevent over-allocation.

For more details, see:

-   <http://wiki.objectvision.nl/index.php/Discrete_Allocation>
-   <http://wiki.objectvision.nl/index.php/Function:discrete_alloc>
-   <http://www.objectvision.nl/geodms/operators-a-functions/allocation>

Even in degenerated cases when too many different locations have the
same relative suitability for one activity, the discrete allocation will
only allocate the required amount of cells by taking arbitrary choices
based on <http://wiki.objectvision.nl/index.php/Virtual_perturbation> .

Suitabilities are expressed as 32 bits integers to prevent round-off
issues that arise when dealing with floating point values with varying
magnitudes. Usually a calculated bidding price in EUR / ha is multiplied
by a factor and then rounded off to make it integer, but one can also
define a limited set of suitability classes with fixed units of extra
utility as the resulting degeneration is not a big issue.

# suitability and uncertainty

The activity suitability maps may contain several factors which are
weighted according to a logit regression on observed choices. When
maximization is considered 'too optimal', the discrete allocation can
also be used to get a result of a Monte Carlo sample from a stochastic
activity choice based on the probabilities
$P_i(a) := {\\exp(\\sideset{^a}{_i}S + \\lambda_a) \\over { \\sum\\limits_b \\exp {\\sideset{^b}{_i}S + \\lambda_b} }}$
by adding to the augmented suitability a random term with exponential
distribution where the *β* parameter indicates the scale of suitability
increments, usally 1. The odds ratio of the probability of one option to
another then becomes equal to the exponent of the augmented suitability
difference where augmentation here includes the regional schadow prices
as well as raster price,
$p_i := \\ln \\sum\\limits_b \\exp \\left\[\\sideset{^b}{_i}S + \\lambda_a\\right\]$.
A random term with exponential distribution can be drawn for each
combination of *i* and *a* by calculating  − *β* ⋅ ln (*U*) with *U* iid
uniform random between 0 and 1.

See also:

-   [Exponential distribution in
    wikipedia](https://en.wikipedia.org/wiki/Exponential_distribution)

# proposed changes to the initial description during the implementation of the prototype

The following proposed changes are provisional and will be used to
update the above description when agreed upon and demonstrated in a
working prototype.

1.  the age groups and volume class breaks are specified per forest type
    per NUTS1 (and not per country).
2.  Forest type is set to {CF, BL} for all run countries (NUTS0) for the
    prototype, but will be adjustable per NUTS1, provided that forest
    type specific data is provided.
3.  For rules on transitions (source: e-mail dd 23-10-2015):
    -   Thinning: For now assume one drop in volume class for thinned
        areas for the following time step. This rule is a blanket rule
        that should be flexible from region to region, but we do not
        have data now, so this assumption holds for all regions and
        species.
    -   Final felling: assume the area drops to volume class0 and age
        class0 in the next time step because it is clear cut and no
        volume remains. The area felled then starts to grow again using
        the probabilities in the transition probabilities matrices for
        each species per region.
4.  How to assess the amount of m3 wood per volume class (see: e-mail dd
    09-10-2015):
    -   Final Felling: I'll assume the average of the class-break and
        the next class-break that is fine, except the last class, where
        the next class-break is assumed to be 25 % above the break of
        the beginning of the last class.
    -   Thinning: the m3 wood production is determined by the difference
        in average volumes of the decreased volume class.
5.  Sawlogs versus pulpwood: I don't see now how different production
    types contribute to the different type of wood production (except
    for the obvious different forest types). I assume for now that I
    rescale the different activity quantities towards balancing the
    total wood production to the sum of sawlogs + pulpwood. This assumes
    100% yield of the removed wood which can be varied later, but I
    expect this to give a more balanced regional model than by selecting
    different age groups and volume classes to use the relative
    differences between sawlog and pulpwood production to meet the
    demand.