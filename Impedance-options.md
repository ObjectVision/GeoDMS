*[[Network functions]] Impedance options*

## options specification

The options parameter indicates how to calculate routes, which additional [[arguments|argument]] are given and which results are to be produced. Each section is separated by a ';', starts with a label and indicates comma-separated extra arguments between '(' and ')'. Optional results are specified by name after a ':'. All sections, extra arguments and products must be specified in the order of the following description. All labels and names are case-sensitive and no intermediate spaces are allowed.

## link direction

The first section is obligatory and indicates the allowed traversal direction of each link ([directed](https://en.wikipedia.org/wiki/Directed_graph) versus [indirected graphs](https://en.wikipedia.org/wiki/Graph_(discrete_mathematics)#Undirected_graph)). 

It can be:
- **directed**: only routes from a start point to an endpoint that traverses each link forward from node f1 to node f2 are considered and not vice versa.
- **bidirectional**: also backward traversal is considered through each link, from node f2 to node f1.
- **bidirectional(link_flag)**: indicates an additional argument <B>Link → Bool</B>, a boolean [[attribute]] of <B>Link</B>, that indicates for each link if it is to be considered as bidirectional. False indicates forward traversal only. 

## startPoints

The optional **startPoint(Node_rel,impedance,OrgZone_rel):max_imp** specifies a set of origin zones.

<details><summary>options:</summary>

- the **Node_rel**, optional for **impedance_matrix_od**, indicates the presence of an additional argument: <B>startPoint → Node</B>, a Node attribute of startPoint in order to select a subset of nodes as startPoints. Required for the **impedance_table** function. Default: when omitted with the **impedance_matrix** function, all Nodes are considered as startPoints and **startPoint** := **Node**.
- the optional **impedance** indicates the presence of an additional argument: <B>startPoint → Impedance</B>, as departure impedance attribute of startPoint, useful mainly when origin zones contain multiple startPoints with specific departure impedances. Default value when omitted: zero.
- the **OrgZone_rel**, optional for **impedance_matrix** and not allowed for **impedance_table**, indicates the presence of an additional argument: startPoint → OrgZone, an origin zone attribute of startPoint, in order to define multiple startPoints per origin zone. The default for the **impedance_matrix** function when omitted: all startPoints are considered as separate origin zones. This option is not allowed with the **impedance_table**, where all startPoints are assumed to belong to the same origin zone.
- the optional **max_imp**, indicates the production of a [[subitem]] <B>OrgZone → Imp</B>, an Imp attribute of OrgZone, named **MaxImp** that contains the maximum impedance for each OrgZone for all connected DstZones. This is especially useful in combination with the **limit(OrgZone_max_mass,DstZone_mass)** option with which one can produce the distance to the n-th unit of a DstZone specific quantity. When all Nodes are considered as separate OrgZones, one can specify **startPoints:max_imp**. This option is available in GeoDMS 7.168 and later versions.
- when no additional arguments are indicated, this section should be omitted; **startPoint()** is a non-allowed syntax.
- when no startPoints are specified (not allowed with **impedance_matrix_s**), all Nodes are considered as separate origin zones. 

</details>

## endPoints

The optional **endPoint(Node_rel,impedance,DstZone_rel)** specifies a set of destination zones.

<details><summary>options:</summary>

- the optional **Node_rel** indicates the presence of an additional argument **endPoint → Node**, a Node attribute of endPoint in order to select a subset of nodes as endPoints. Default: all Nodes are considered as separate endPoints
- the optional **impedance** indicates the presence of an additional argument **endPoint → MeasureType**, an arrival impedance attribute of endPoint, useful mainly for when Destination Zones contain multiple endPoints.
- the optional **DstZone_rel** indicates the presence of an additional argument **endPoint → DstZone**, a destination zone per endPoint, in order to define multiple endPoints per DstZone. Default: each endPoint is considered as a separate destination.
- when no additional arguments are indicated, this section should be omitted; **endPoint()** is a non-allowed syntax.
- when no endPoints are specified, all Nodes are considered as separate destination zones.

</details>

## filters

Optional **filters** can be applied to limit the resulting set.

<details><summary>options:</summary>

- The optional **cut(OrgZone_max_imp)** specifies that a maximum route impedance will limit the route search, which requires an additional argument: OrgZone → Impedance, an impedance attribute of the origin zones or a single impedance limit parameter that is applied for all origin zones.

- The optional **limit(OrgZone_max_mass,DstZone_mass)** specifies that a maximum amount of destinations per origin zone will limit the route development, which requires two additional arguments:

    - **OrgZone → Mass or {∅} → Mass** (must have the same [[value type]] as **Impedance**), a Mass attribute of OrgZone or a single Mass parameter, that sets a maximum on the amount of Mass to be reached at the DstZones.
    - **DstZone → Mass or {∅} → Mass** (must have the same value type as **Impedance**), a Mass attribute of DstZone or a single Mass parameter, that indicates a Mass for each DstZone, which is accumulated until the limit is reached for each OrgZone.
</details>

## euclidean distance filter
The optional **euclid(maxSqrDist)** specifies the maximum Euclean search distance for destinations. The specified parameter is the search distance squared.

## alternative impedance

The optional **alternative(link_imp):alt_imp** specifies that an alternative link impedance is to be used to calculate an impedance per od-pair for further use in the interaction potential calculation.

<details><summary>options:</summary>

- This requires an additional argument: **Links → Impedance2**, an alternative impedance per link, where Impedance2 must have the same value type as the original impedance, but may have a different [[metric]].
- the optional product **alt_imp** indicates the production of an Impedance2 attribute of the resulting od-pair entity named **alt_imp** with the total alternative impedance of the found routes for each od-pair.
- when this section is omitted, the original Impedance is used in the interaction calculation and the unit Impedance2 is set to be equal to Impedance.
- when no filtering applies, some od-pairs may represent combinations without connecting routes. The impedance is MAX_VALUE&lt;ImpType&gt; there but the alt_imp is [[null]] there. Both values will not be taken into account in an interaction distribution.

</details>

## interaction

The optional **interaction(OrgZone_min,DstZone_min,v_i,w_j,dist_decay,dist_logit(alpha,beta,gamma),OrgZone_alpha): D_i,M_ix,C_j,M_xj,Link_flow** indicates the application of a free or origin constrained interaction model following the thoughts and notation of Alonso's General Theory of Movement (GTM) with a fixed supply elasticity per destination of 1 (thus *β*<sub>*j*</sub> = 1.0; no constraint on the destination side). Based on [De Vries, Nijkamp & Rietveld (2000)](https://econpapers.repec.org/scripts/redir.pf?u=https%3A%2F%2Fpapers.tinbergen.nl%2F00062.pdf;h=repec:tin:wpaper:20000062).

<details><summary>theory:</summary>

This describes the theory of a free or origin-constrained interaction model following the thoughts and notation of Alonso's General Theory of Movement (GTM) with a fixed supply elasticity per destination of 1 (thus $β_j = 1.0$; no constraint on the destination side).

- $M_{ij} = (v_i \\cdot w_j \\cdot t_{ij}) / D_i^{α-1}$

in which:

- $M_{ij}$ = potential interaction between i and j
- $v_i$ = trip generation weight per origin
- $w_j$ = trip distribution weight per destination
- $t_{ij}$ = facility of movement between i and j
  - $t_{ij} = d_{ij}^{-γ}$, in case of a normal distance decay specification.
  - $t_{ij} = (1+exp(alpha)⋅d_{ij}^{beta}⋅exp(d_{ij})^{gamma})^{−1}$, in case of a log-logistic distance decay specification.
- $d_{ij}$ = impedance (e.g. distance or travel time) from i to j
- $γ$ = distance decay factor, and in the case of the log-logistic specification there are the $alpha$, $beta$, and $gamma$ parameters.

Summations:

- $D_i = \\sum\\limits_{j} w_j \\cdot t_{ij}$ 
- $M_{ix} = v_i \\cdot\\ D_i^α = \\sum\\limits_{j} v_i \\cdot w_j \\cdot t_{ij} \\cdot D_i^{α-1}$ = summation of decayed impedances from origin $i$.
- $C_j = \\sum\\limits_{i} v_i \\cdot t_{ij} \\cdot D_i^{(α-1)}$
- $M_{xj} = w_j \\cdot C_j$ = summation of decayed impedances to destination $j$.
- $LF_l = \\sum\\limits_{l \\in i \\rightarrow j} v_i \\cdot w_j \\cdot t_{ij} \\cdot D_i^{α-1}$ = link flow over link $l$.

</details>

<details><summary>options:</summary>
- impedance within the interaction is alt_imp if defined. The first impedance is used for route decisions, while the alt impedance will be used to aggregate the interaction calculations. 
- the optional **OrgZone_min** indicates an additional argument: **OrgZone → Impedance2**, which can also be a single value ([[void]]), indicating a minimum (alternative) impedance to be used for each destination to avoid infinite auto interaction potential for each origin. The default value is 0.
- the optional **DstZone_min** indicates an additional argument: **DstZone → Impedance2**, which can also be a single value, indicating a minimum (alternative) impedance to be used for each origin to a void infinite auto interaction potential for each destination. The default value is 0.
- From this we define the distance measure to be $d_{ij} := max(impedance_{ij}, OrgZone min_i, DstZone min_j)$.
- the obligatory **v_i** indicates an additional argument: **OrgZone → Mass** which can also be a single value, indicating a trip generation weight per origin, aka *v*<sub>*i*</sub>. The value type should match the value type of the impedance.
- the obligatory $w_j$ indicates an additional argument: **DstZone → Mass** which can also be a single value, indicating a trip distribution weight per destination, aka $w_j$. The value type should match the value type of the impedance.
- the optional **dist_decay** indicates an additional argument *gamma* : Float64 that indicates the rate of distance decay. The used interaction potential $t_{ij}$ is calculated as $t_{ij} := d_{ij}^{−gamma}$.
    - $gamma = 1.0$ thus indicates $t_{ij} = 1/d_{ij}$
    - If no route exists from *i* to *j* (which can be visible in the results when no filtering was applied),  $t_{ij}$ is set to 0.
    - $gamma = 0.0$ indicates $t_{ij} = 1$, even when $d_{ij} ≤ 0$, except when no route exists. 
    - $gamma = − 1.0$ indicates $t_{ij} = d_{ij}$
    - when $d_{ij} ≤ 0$ and gamma ≠ 0.0, $t_{ij}$ is set to zero, to avoid incorporating infinite potentials when no minimum impedances nor departure or arrival impedances were specified. Auto-interaction is then excluded but included when gamma = 0.0.

- the optional **dist_logit(alpha,beta,gamma)** indicates three additional arguments that can be used to alternatively define $t_{ij}$ as a log-logistic distance decay function of $d_{ij}$:

  - $t_{ij} := (1+exp(alpha+beta⋅ln(d_{ij})+gamma⋅d_{ij}))^{−1} = (1+exp(alpha)⋅d_{ij}^{beta}⋅exp(d_{ij})^{gamma})^{−1}$
  - The value type of all parameters should be float64.
  - note that either **dist_decay** or **dist_logit(alpha,beta,gamma)** must be specified in an interaction section.
  - note that we name the parameters by Latin characters instead of Greek characters. Since this alternative definition of $t_{ij}$ uses the same parameter names as in the other interaction definitions. So to avoid confusion, we use the Latin characters for the distance decay parameters.

- the optional **OrgZone_alpha** indicates an additional argument: **OrgZone −  > Float64** which can also be a single value, indicating the elasticity of the origin's supply for the number of demand alternatives. $α = 1.0$ indicates an elastic model; $α = 0.0$ indicates a push model with fixed supply $v_i$ per origin. The default value is 0.

Calculated products (at least one should be specified):

- the optional **D_i**, indicates the production of a subitem **OrgZone → Mass**, a Mass attribute of OrgZone, named D_i, defined as $D_i:= \\sum\\limits_{j} w_j \\cdot t_{ij}$.
- the optional **M_ix**, indicates the production of a subitem **OrgZone → Mass**, a Mass attribute of OrgZone, named M_ix, defined as $M_{ix} := v_i \\cdot\\ D_i^α = \\sum\\limits_{j} v_i \\cdot w_j \\cdot t_{ij} \\cdot D_i^{(α-1)}$
  - Note that when $D_i = 0$, $D_i^α$ and $M_{ix}$ are assumed to be zero, even if $α = 0$, in order to respect the fact that if there is no demand potential at all, it cannot be raised to a set demand whatever the elasticity.
- the optional **C_j**, indicates the production of a subitem <B>DstZone → Mass</B>, a Mass attribute of DstZone, named C_j, defined as $C_j := \\sum\\limits_{i} v_i \\cdot t_{ij} \\cdot D_i^{(α-1)}$.
- the optional **M_xj**, indicates the production of a subitem <B>DstZone → Mass</B>, a Mass attribute of DstZone, named M_xj, defined as $M_{xj}:= w_j \\cdot C_j = \\sum\\limits_{i} v_i \\cdot w_j \\cdot t_{ij} \\cdot D_i^{(α-1)}$.
- the optional **Link_flow**, indicates the production of a traffic flow network assignment, a subitem <B>Link → Mass</B>, a Mass attribute of Link, named **Link_flow**, defined as **$LF_l := \\sum\\limits_{l \\in i \\rightarrow j} v_i \\cdot w_j \\cdot t_{ij} \\cdot D_i^{(α-1)}$**.

</details>

## traceback production

<details><summary>options:</summary>

- The optional **node:TraceBack** (only available for **impedance_matrix_s**) produces a subitem <B>TraceBack : Node → Link</B>, a Node attribute of Node, indicating for each node which Link of the grown tree(s) traces back to the origin of startPoint Node(s).
For the **impedance_matrix_od** function, producing this info is complicated by the fact that a traceback would be available for each origin.

</details>

## od-pair specification and attribute production

the optional **od:impedance,OrgZone_rel,DstZone_rel,LinkSet**, indicates the production of a specific kind of od-paid entity and related attributes.

<details><summary>options:</summary>

- the optional **impedance** indicates the production of a subitem **impedance : OD → Impedance**, an attribute with the lowest route impedance per found od-pair. This impedance includes startPoint(impedance) and endPoint(impedance) but not OrgZone_min nor DstZone_min.
- the optional **OrgZone_rel** indicates the production of a subitem **OrgZone_rel : OD → OrgZone**, an attribute with the OrgZone of each od-pair.
- the optional **DstZone_rel** indicates the production of a subitem **DstZone_rel : OD → DstZone**, an attribute with the DstZone of each od-pair.
- the optional **LinkSet** indicates the production of a subitem **LinkSet : OD → Link-sequence**, an attribute with the sequence of Links of each od-pair route.

When no od-pair-related products are requested (including no **alt_imp**), the memory footprint of the operation will be greatly reduced as intermediate potentials and route flows per od-pair are produced per OrgZone and directly aggregated to the OrgZone, DstZone, and/or Link level and no storage is required per od-pair simultaneously for different OrgZones.

</details>

## production of t_ij and M_ij

In order to get $t_{ij}$ and $M_{ij}$, the calculated interaction potential and trip flow per od-pair, one can recalculate them from the results available from **od:impedance,OrgZone_rel,DstZone_rel**

- When no alternative link impedance is given, defined as an extra subitem of the resulting od-pair unit:

```
attribute<float32> t_ij := impedance >= 1e+38f ? 0.0f : distDecay == 0.0f ? 1.0f : impedance^-distDecay;
```

Note that $t_{ij} >= max_dist$ only for od-pairs without available route, which only appears in results when no filtering option was used. If filtering was used and it is known that distDecay is non-zero, the above can be simplified to

```
attribute<float32> t_ij := impedance^-distDecay;
```

- When alternative(link_imp):alt_imp is used, use:

```
attribute<float32> t_ij := !IsDefined(alt_imp) ? 0.0f : distDecay == 0.0f ? 1.0f : alt_imp^-distDecay;
```

- When **OrgZone_min** or **DstZone_min** were specified , replace the last impedance measure (**impedance** or **alt_imp**) by
    - **max_elem(impedance, min_imp[OrgZone_rel])** when **min_imp** is given per origin zone,
    - **max_elem(impedance, min_imp[DstZone_rel])** when **min_imp** is given per destination zone, or just
    - **max_elem(impedance, min_imp)** when it is a single value parameter, applied for all od-pairs.

- The </B>*M*<sub>*ij*</sub></B> can then be calculated with

```
attribute<float32> M_ij := D_i[OrgZone_rel] <= 0.0f ? 0.0f : t_ij*D_i[OrgZone_rel]^(demand_alpha-1.0f);
```

- if the demand for each zone i is assumed to be inelastic, i.e. demand_alpha == 0f, this can be simplified to

```
attribute<float32> M_ij := D_i[OrgZone_rel] <= 0.0f ? 0.0f : t_ij*(1.0f / D_i)[OrgZone_rel];
```

An example of this can be found in the operator test configuration at **/Network/UnTiled/dijkstra_all_interaction**.

## see also

- [[Impedance general (formerly known as Dijkstra)]]
- [[Impedance key entities]]
- [[Impedance functions]]
- [[Impedance warning]]
- [[Impedance interaction potential]]
- [[Impedance additional]]
- [[Impedance future]]
- [[Impedance links]]