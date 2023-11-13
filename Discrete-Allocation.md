Discrete Allocation is the [Allocation](Allocation "wikilink") of resources to a set of categories.

In the context of the GeoDMS and its applications it is defined as
finding the *X*<sub>*i**j*</sub> \>  = 0 for each land unit *i* and land
use type *j* that solve the following [Semi Assignment
Problem](Semi_Assignment_Problem "wikilink") for given suitabilities
*S*<sub>*i**j*</sub>:

$max \sum\limits\_{ij}{X\_{ij} S\_{ij}} \ subject to

for each claim *j*:
$ClaimMin_j \le \sum\limits\_{i}{X\_{ij}} \le ClaimMax_j$ and for each
land unit *i*: $\sum\limits\_{j}{X\_{ij}} = 1$

Thus *X*<sub>*ij*</sub> represents whether land unit i is allocated to land use type j and only one single allocation per land unit is allowed.

It is used to find the allocation of land use to land units that maximizes total suitability when endogenous interactions are disregarded.

Discrete Allocation can also be used to aggregate a discrete map ([aka](aka "wikilink") [Downsampling](http://en.wikipedia.org/wiki/Downsampling)) to a larger zones or raster-cells while keeping the total area's constant or within bounds by using the amount of each land use type in or near an aggregate unit as suitability for that type and the total areas as claims (rounded down as minimum claim and rounded up as maximum claim). A script called BalancedClassAgggr.dms will become available in our [code examples](http://www.objectvision.nl/geodms/configuration-examples).

When applied iteratively and by incorporation of dynamic neighbourhood enrichment, one can simulate land use change caused by natural processes while minimum demands and/or maximum land use restrictions (as specified by the claims) are maintained. When applied iteratively with a feedback from future (neigbourhood dependend) yields on the current suitability, one can model a time consistent market equilibrium.

In the GeoDms, discrete allocation can be done with the [[discrete_alloc function]].

In [[Luisa]], the suitabilities for discrete allocation are called [[Transition Potentials]] and there  are three [[ModelTraits]] for calculating them:

- [[Mnl100]] (the default and mostly used)
- [[Split100]]
- [[Linear100]]