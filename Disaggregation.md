Disaggegation is the process of estimating a quantity *z*<sub>*i*</sub> for a finer grained domain *i*, given quantity *Z*<sub>*r*</sub> for a
coarser domain *r* and an incidence relation *q*<sub>*i*</sub><sup>*r*</sup> that indicates the fraction of unit *i* that belongs to region *r* such that *q*<sub>*i*</sub><sup>*r*</sup> ≥ 0 and $\\forall i: \\sum\\limits_{r}  q_i^r = 1$. In most cases, *q*<sub>*i*</sub><sup>*r*</sup> is discrete, thus either 0 or 1 and one can define *r*(*i*) such that *q*<sub>*i*</sub><sup>*r*(*i*)</sup> = 1.

The reverse of disaggregation is [[aggregation|aggregation functions]].

Quantitative modelling of attribute values can often be considered as some sort of (combination of ) disaggregation of known aggregates, restrictions and other proxy values.

*z*<sub>*i*</sub> can be an extensive (additive) quantity of i or an intensive quantity (such as discrete class values or density measures).

# extensive quantities

-   should adhere to the pycnophylactic principle (further: pp), i.e.
    $\\forall r: \\sum\\limits_{i} z_i \* q_i^r = Z_r$

<!-- -->

-   can be done using *s*<sub>*i*</sub> as proxy values. Then $z_i := \sum_{\lim_{r}} Z_r \* {s_i \* q_i^r} \\over {\sum_{\lim_{j}}  s_j \* q_j^r}$;
    which distributes *Z*<sub>*r*</sub> proportional to
    *s*<sub>*i*</sub>. The pp is guaranteed to match if:
    -   all *q*<sub>*i*</sub><sup>*r*</sup> are discrete (thus each *i* relates to a single aggregate) and 
    -   for each r:
        ${{\\sum\\limits_{j}  s_j \* q_j^r} > 0} \\vee {Z_r = 0}$ (thus each nonzero aggregate relates to at least one *i* ).

<!-- -->

-   When *q*<sub>*i*</sub><sup>*r*</sup> is discrete, the former can be reformualated to
    $z_i := Z_{r(i)} \* {{s_i} \\over {\\sum\\limits_{j: r(j) = r(i)} s_j}}$ which can be done with the GeoDMS function [[scalesum]](s, r, Z).

<!-- -->

-   can be smoothed out by [[convolution]] when disaggregating to proxies with approximate locations, such as point-related data, by using the [[potential]].

<!-- -->

-   can be made subject to minimum (zero?) and maximum values for *z*<sub>*i*</sub>, by transforming and capping the result of scalesum. To comply to the pp, an iterative fitting factor *f*<sub>*r*</sub>, initially set to 1, can be used. Capping in GeoDMS: [[min_elem]](z, z_max), [[max_elem]]
(z, z_min), [[median]](z, interval)

<!-- -->

-   can be combined with disaggregation of other quantities such that each unit i is allocated once ([[Iterative proportional fitting]], [[Continuous Allocation]], or [[Discrete Allocation]]).

<!-- -->

-   can be done by maximizing smoothness of the *z*<sub>*i*</sub> to adhere to [Tobler's first law of geography](http://en.wikipedia.org/wiki/Tobler's_first_law_of_geography), aka [[Smooth Pycnophylactic Interpolation]].

# intensive quantities

-   can be done using homogeneous distribution ([choropleth mapping](http://en.wikipedia.org/wiki/Choropleth_map)), which can be
    done in with the GeoDMS function [[[lookup]](r, Z)].
-   can be done using a incidence proxy *c*<sub>*i*</sub>, aka [dasymmetric mapping](http://en.wikipedia.org/wiki/Dasymetric_map),
    in GeoDMS: "c ? Z\[r\] : 0\[[ValuesUnit]](Z)\".