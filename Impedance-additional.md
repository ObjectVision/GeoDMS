*[[Network functions]] Impedance additional*

## additional notes

1. Filtering with cut or limit is an essential tool for limiting the computation effort when modelling network flow due to local traffic with many origins and destinations. The implementation only processes localized data per OrgZone when such filtering is applied, making the computation time per OrgZone independent of the network size after initial initialization. For example, the processing of 4000 PC4 zones (both Origin and Destination) on the OpenStreetMap network of the Netherlands with boundary extensions took 10 minutes when cut was set to 1800[sec], resulting in an OD set of 750.000     pairs, thus approximately an average of 175 destinations per origin (of the 4000 potential destinations), whereas processing all od-pairs took several hours. The generation of Link_flow (Network Assignment) from the 4000 PC4 origins to all 5.000.000 nodes as destinations with a cut of 5 minutes took approximately 1 minute to produce. See: [[images/AllPC4_AllNodes_NetworkFlow.png]]
2. one can model constrained supply by iteratively adjusting wj with Cjßj-1 with ßj as the supply elasticity parameter.
3. one can model a congested network equilibrium by iterative application of the network assignment by implementing a feedback loop (http://en.wikipedia.org/wiki/Transportation_forecasting) on the link impedances.
4. by assigning a heterogeneous set of transport users (mode, income, trip purpose, etc.) in slices of, say 10% per slice, one can also obtain a balanced load of the network and the choice of startPoints.
5. by using shortcuts for interactions between many small zones and larger cuts for interactions between a smaller number of larger zones, one can obtain a balanced load of the network within a reasonable amount of computation time.
6. one can generate route counts per link with interaction(v_i,w_j,dist_decay,OrgZone_alpha):D_i,C_j,Link_flow by setting vi = wj = ai = 1.0 and γ= 0.0. The resulting Di and Cj are total route counts per origin and destination.
7. one can generate link counts per od-pair with alternative(link_impedance):alt_imp by providing 1.0 as alternative link impedance

## Alonso's General Theory of Movements (GTM)

see: [Alonso's General Theory of Movement: Advances in Spatial Interaction Modeling, de Vries, Nijkamp and Rietveld](https://research.vu.nl/en/publications/alonsos-theory-of-movements-developments-in-spatial-interaction-m).

The changed notation can be reverted by applying the following rewrite rules:<BR> **Tij -> Mij; Fij->tij; Oi -> Mix; Dj -> Mxj; Ai -> Di-1; Bj ->
Cj-1;Vi -> vi; Wj -> wj;cij -> dij; γ1 -> γ**

## see also

- [[Impedance general (formerly known as Dijkstra)]]
- [[Impedance key entities]]
- [[Impedance functions]]
- [[Impedance options]]
- [[Impedance warning]]
- [[Impedance interaction potential]]
- [[Impedance future]]
- [[Impedance links]]