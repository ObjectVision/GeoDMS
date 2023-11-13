# virtual perturbations of suitability maps

In order to make exact allocation possible, equal suitabilities are
virtually perturbated. It should never be the case that for any two
cells i,l and types j,k, the distance from points S_i and S_l to the
facet (j,k) is equal

or formally:

` (R1):`
` (S_ij + C_j ) - (S_ik + C_k) <> (S_lj + C_j ) - (S_lk + C_k) unless i==l OR j==k which is equivalent to`
` (S_ij - S_ik) + (C_j  - C_k) <> (S_lj - S_lk) + ( C_j - C_k) unless i==l OR j==k which is equivalent to`
` (S_ij - S_ik)                <> (S_lj - S_lk) unless i==l OR j==k.`

This is achieved by applying symbolic pertubations to the cost values
and require some administration which points are compared.

S_ij(epsilon) := S_ij + epsilon\*i\*j

Thus S_ij(epsilon) \<\> S_kl(epsilon) for i\<\>k XOR j\<\>l since S_ij
== S+kl implies S_ij(epsilon) - S_kl(epsilon) == epsilon\*(ij - kl)

and (S_ij(epsilon) - S_ik(epsilon)) - (S_lj(epsilon) - S_lk(epsilon))

## epsilon \* \[(ij - ik) - (lj - lk)\]

epsilon \* \[(i-l)(j-k)\], which fullfills requirement (R1).

The sufficiency of (R1) and thus the fact that degeneracies such as S_ij
== S_lk for i\<\>l AND j\<\>k doesn't matter follow from close analysis
of the used operators:

-   We take and count maxima per cell i of (S_ij + C_j) over j.
-   We keep a queue of cells i for each communicating (j,k) facet,
    strictly ordered by (S_ij(epsilon) - S_ik(epsilon)), small values
    have priority
-   We update C_j(epsilon) := C_k(epsilon) - (S_ij(epsilon) -
    S_ik(epsilon)) using facet (j,k)

thus:

` C_j.first  := C_k.first    - heap(j,k).top.first // reduce coordinate-j of the splitter to enable the reallocation of one unit from j to k`
` C_j.second := C_k.second   - i*(j-k)             // update the epsilon component of the shadow price`

Note that the C_j on this page is [aka](aka "wikilink") lambda_j.

# links

-   [Edelsbrunner, H. And Mcke, E. Simulation of simplicity: a technique
    to cope with degenerate cases in geometric algorithms, 4th Annual
    ACM Symposium on Computational Geometry (1988)
    118-133.](http://arxiv.org/PS_cache/math/pdf/9410/9410209v1.pdf)