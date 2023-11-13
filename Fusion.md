---
title: FUSION
permalink: /FUSION/
---

FUSION is a set of spatially explicit model components to integrate
[<https://en.wikipedia.org/wiki/CBM-CFS3>
CBM](https://en.wikipedia.org/wiki/CBM-CFS3_CBM "wikilink"),
[<https://ec.europa.eu/jrc/en/publication/global-forest-trade-model-gftm>
GFTM](https://ec.europa.eu/jrc/en/publication/global-forest-trade-model-gftm_GFTM "wikilink"),
(projected) Land Use, and a harvest cost calculations for the purpose of
integrated modelling to assess forestry biomass feedstock availability.
This page describes the components of Fusion as mappings of input
datasets to resulting datasets. Each dataset is described as a mapping
from a set of indices to a set of attributes, which can also be indices.
Fusion is implemented in the [GeoDMS](GeoDMS "wikilink").

# indices

-   _1A: StatusA ? {'Ar', 'For', 'Un'}
-   _1B: StatusB ? {CC: Clear Cutting allowed aka FAWS (IUCN_CAT =
    Other), Th: Only Thinning allowed aka FAWSp(IUCN_CAT = 2 or 4), Un:
    Unavailable aka FnAWS(IUCN_CAT =0 or 1)}
-   _2: Forest Type, aka FT
-   _3: NutsRegion, aka NR
-   _4: Management Type, aka MT
-   _5: Management Strategy, aka MS
-   _6: Climatic Unit, aka CU
-   _7: BLCF, functionally dependent on _2 and further ignored, except
    when allocating GFTM demand
-   AC: AgeClass

<!-- -->

-   DT: Disturbance Type
-   HWP: Harvested Wood Products ? { IRW: Industrial Round-Wood, FW:
    Fuel Wood }

<!-- -->

-   t: time { 2015, 2020, 2025, 2030}
-   y: {1..5} Year (of a disturbance within a time period)
-   C: raster cell, (100*m*)<sup>2</sup>.

# combined indices

-   6A: combination of _1A, _2 .. _6.
-   6B: combination of _1B, _2 .. _6.
-   row: generic term for a set of keys for a CBM record
-   rowA: combination of 6A and AC.
-   rowB: combination of 6B and AC.

# exogenous datasets

-   BACK_INVENTORY: rowA->(Area, _7, OtherAttrs)

produced by CBM GUI

# endogenous datasets

## BACK_INVENTORY_AWS


rowB -> (Area, _7, OtherAttrs)

Produced by FUSION_AWS, see issue
[942](http://www.mantis.objectvision.nl/view.php?id=942)

Using

-   BACK_INVENTORY
-   AWS maps,
-   species probability maps

Disaggregation to c by fencing on _3 and _6, discrete allocation on
_2, _4, _5 using probability maps per FT related species, and MT and
MS related factor maps when available, and then assign AC based on a
volume map for \< 2015. resulting in C -> row

## INVENTORY(2015)

rowB -> (Area, _7, Biomass_carbon_ha, Other attributes) produced by CBM
2015 using BACK_INVENTORY_AWS: , and a fixed set of disturbances.

## INV_MAX(t)

-   (_7)->(???)

Produced by CBM MWS(t) with t= 2015..2020, or later using INVENTORY(t)

## HC


rowB -> (Costs \[EURO per m3\], AreaCheck)

Produced by FUSION_HC

using

-   INVENTORY(2015),
-   CostFactors of Gulia: C->Costs\[EURO per m3\]

Disaggregation to c by fencing on _3 and _6, discrete allocation on
_2, _4, _5 using probability maps per FT related species, and MT and
MS related factor maps when available, and then assign AC based on a
volume map for 2015. resulting in C -> row, used to aggregate
C->Costs\[EURO per m3\] to HC. QUESTION: HOW to aggregate? Average costs
might take very expensive locations into account which will not be
harvested. Better: an average of the cheapest half.

## DEMAND(t)

-   (_7,HWP->q) \[m^3\]

Produced by GFTM(t)

Using

-   INV_MAX(t) as constraint.

## disturbances(t)


(rowB, AC)->(_7, DT, Removed Carbon)

Produced by FUSION_DT

Using

-   INVENTORY(t): rowB->(Area, _7, Biomass_carbon_ha), aka
    *A*<sub>*r**o**w**B*</sub> and *C*<sub>*r**o**w**B*</sub>
-   DEMAND(T): (_7)->Demand\[m^3 / yr\], aka *D*(*T*)<sub>_7</sub>
-   HC: rowB -> (Costs \[EURO per m3\]), aka *R*<sub>*r**o**w**B*</sub>
-   silviculture: (_1B, _2, _4, _5, {MAN, NAT}, AG) -> (DT,
    Frac_Merch_Biom_rem), aka *D**T*<sub>*s**c*</sub> and
    *?*<sub>*s**c*</sub>

Steps:

-   Each INVENTORY row, relates to one row in HC, one row in
    silviculture if the NAT rows are ignored, and one row in Demand(t).
-   For each INVENTORY row, the total volume \[in kg\] is determined by
    multiplying Area\[ha\] and Biomass_carbon_ha \[kg/ha\].
-   For each INVENTORY row, the maximum yield is determined by
    multiplying the total volume with the related
    silviculture->Frac_Merch_Biom_rem:
    *Y*<sub>*r**o**w**B*</sub> := *A*<sub>*r**o**w**B*</sub> · *C*<sub>*r**o**w**B*</sub>.
-   Then per related demand category, all Inventory rows are ordered on
    HC->Costs in ascending order and the first rows are selected up to
    the cumulative total of the maximum yield exceeds the demand.
-   For the selected INVENTORY rows, the DT of the related silviculture
    are applied, for the unselected rows, DT becomes 0.
-   Removed_Carbon becomes maximum_yield for the selected rows and 0 for
    the unselected rows.

Alternatively, one could allocate an area fraction
*x*<sub>*r**o**w**B*</sub> to each INVENTORY row, based on a descending
function of Harvest costs, for example, proportional to
exp (-*ß*·*R*<sub>*r**o**w**B*</sub>), such that the
$\\sum\\limits_{rowB \\in \\_7} x_{rowB} \\cdot Y_{rowB} = D(t)_{\\_7}$

These steps are taken for IRW.

Then, a supply curve for FW and OWC provision is generated, listing
harvesting options for the 5 year period in ascending order of costs per
m3. The supply curve lists quantity and costs, and for which row and why
it is available: {OWC of selected IRW, FW with OWC, FW, OWC of cheaper
FW}.

## INVENTORY(t>2015)

rowB -> (Area, _7) produced by CBM(t) using INV(t-1), disturbances(t)

## remaining work

-   divide IRW demand over the different disturbance types, proportional
    to maximum potential yield.
-   for SI: MS prob maps should only be used of MT=1 => combine MS and
    MT in selecting a probability map.
-   allocate disturbances per year and report only active disturbances
-   allow fractional disturbances (area intensity), keep a record of
    remaining area per row during the years
-   provide a FuelWood supply curve over the 5 years, including OWC that
    may arise from IRW and FW harvesting
-   clean-up unique_region and unique_type relations; replace overlay by
    key, xx_ref, and rlookup.