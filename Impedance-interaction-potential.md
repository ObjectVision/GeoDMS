*[[Network functions]] Impedance interaction potential*

## interaction potential

In order to get tij and Mij, the calculated interaction potential and trip flow per od-pair, one can recalculate them from the results available from od:impedance,OrgZone_rel,DstZone_rel

- When no alternative link impedance is given, define it as an extra sub-item of the resulting od-pair unit:

<pre>
attribute&lt;float32&gt; t_ij := impedance >= 1e+38f
? 0f
: distDecay == 0f
  ? 1f
  : impedance^-distDecay;
</pre>

Note that <B>tij \>=max_dist</B> only for od-pairs without available route, which only appears in results when no filtering option was used. If
filtering was used and it is known that distDecay is non-zero, the above can be simplified to 

<pre>
attribute&lt;float32&gt; t_ij := impedance^-distDecay;
</pre>

- When alternative(link_imp):alt_imp is used, use:

<pre>
attribute&lt;float32&gt; t_ij := !IsDefined(alt_imp)
   ? 0f
   : distDecay == 0f
      ? 1f
      : alt_imp^-distDecay;
</pre>

When OrgZone_min or DstZone_min were specified, replace the last impedance measure (impedance or alt_imp) by

1.  max_elem(impedance, min_imp[OrgZone_rel]) when min_imp is given per origin zone,
2.  max_elem(impedance, min_imp[DstZone_rel]) when min_imp is given per destination zone, or just
3.  max_elem(impedance, min_imp) when it is a single value parameter, applied for all od-pairs.

The Mij can then be calculated with

<pre>
attribute&lt;float32&gt; M_ij := D_i[OrgZone_rel] <= 0f
   ? 0f
   : t_ij * D_i[OrgZone_rel]^(demand_alpha - 1.0f);
</pre>

if the demand for each zone i is assumed to be inelastic, i.e. demand_alpha == 0f, this can be simplified to

<pre>
attribute&lt;float32&gt; M_ij := D_i[OrgZone_rel] <= 0f
  ? 0f
  : t_ij * (1f / D_i)[OrgZone_rel];
</pre>

## see also

- [[Impedance general (formerly known as Dijkstra)]]
- [[Impedance key entities]]
- [[Impedance functions]]
- [[Impedance options]]
- [[Impedance warning]]
- [[Impedance additional]]
- [[Impedance future]]
- [[Impedance links]]