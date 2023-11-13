If one assumes that the probability *P*(*i*→*j*) that actor *i* = 1..*n*
chooses alternative *j* = 1..*k* is proportional (within the set of
alternative choices) to the exponent of a linear combination of
*p* = 1..\#*p* data values *X*<sub>*i**j**p*</sub> related to i and j,
one arrives at the logit model, or more formally:

Assume $$ \\begin{align} P(i \\to j) &\\sim w_{ij} \\\\ w_{ij}
&:=exp(v_{ij}) \\\\ v_{ij} &:= \\sum\\limits_{p} \\beta_p X_{ijp}
\\end{align} $$

Thus *L*(*i*→*j*) := *l**o**g*(*P*(*i*→*j*)) ∼ *v*<sub>*i**j*</sub>.

Consequently, *w*<sub>*i**j*</sub> \> 0 and
$P(i \\to j) := { w_{ij} \\over \\sum\\limits_{j'}w_{ij'}}$, since
$\\sum\\limits_{j}P_{ij}$ must be 1.

Note that:

-   *v*<sub>*i**j*</sub> is a linear combination of
    *X*<sub>*i**j**p*</sub> with weights *β*<sub>*p*</sub> as logit
    model parameters.
-   the odds ratio $P(i \\to j) \\over  P(i \\to j')$ of choice *j*
    against alternative *j*′ is equal to
    ${w_{ij} \\over w_{ij'}} = exp( v_{ij} - v_{ij'} ) = exp \\sum\\limits_{p} \\beta_p \\left( X_{ijp}- X_{ij'p} \\right)$
-   this formulation does not require a separate beta index (aka
    parameter space dimension) per alternative choice j for each
    exogenous variable.

# observed data

Observed choices *Y*<sub>*i**j*</sub> are assumed to be drawn from a
[repreated Bernoulli
experiment](http://en.wikipedia.org/wiki/Bernoulli_trial) with
probabilites *P*(*i*→*j*).

Thus
$P(Y) = \\prod\\limits_{ij} {N_i ! \\times P(i \\to j)^{Y_{ij}} \\over Y_{ij}! }$
with $N_i := \\sum\\limits_{j} Y_{ij}$.

Thus *L*(*Y*) := *l**o**g*(*P*(*Y*)) $$\\begin{align} &= log
\\prod\\limits_{ij} {N_i ! \\times P(i \\to j)^{Y_{ij}} \\over
Y_{ij}! } \\\\ &= C + \\sum\\limits_{ij} (Y_{ij} \\times
log(P_{ij})) \\\\ &= C + \\sum\\limits_{i} \\left\[{
\\sum\\limits_{j}Y_{ij} \\times L(i \\to j)}\\right\] \\\\ &= C +
\\sum\\limits_{i} \\left\[{ \\sum\\limits_{j}Y_{ij} \\times \\left(
v_{ij} - log \\sum\\limits_{j'}w_{ij'}\\right)}\\right\] \\\\ &= C +
\\sum\\limits_{i} \\left\[{ \\left( \\sum\\limits_{j}Y_{ij} \\times
v_{ij} \\right) - N_i \\times log \\sum\\limits_{j}w_{ij}}\\right\]
\\end{align} $$ with $C = \\sum\\limits_{i} C_i$ and
$C_i := \[log (N_i!) - \\sum\\limits_{j} log (Y_{ij}!)\]$, which is
independent of *P*<sub>*i**j*</sub> and *β*<sub>*j*</sub>. Note that:
*N*<sub>*i*</sub> = 1 ⟹ *C*<sub>*i*</sub> = 0

# specification

The presented form
*v*<sub>*i**j*</sub> := *β*<sub>*p*</sub> \* *X*<sub>*i**j*</sub><sup>*p*</sup>
(using [Einstein
notation](http://en.wikipedia.org/wiki/Einstein_notation) from here) is
more generic than known implementations of logistic regression (such as
in SPSS and R), where *X*<sub>*i*</sub><sup>*q*</sup>, a set of
*q* = 1..\#*q* data values given for each *i*
(*X*<sub>*i*</sub><sup>0</sup> is set t 1 to represent the incident for
each j) and (*k*−1) \* (\#*q*+1) parameters are to be estimated, thus
*v*<sub>*i**j*</sub> := *β*<sub>*j**q*</sub> \* *X*<sub>*i*</sub><sup>*q*</sup>
for *j* = 2..*k* which requires a different beta for each alternative
choice and data set, causing unnecessary large parameter space.

The latter specification can be reduced to the more generic form by:

-   assigning a unique p to each jq combination, represented by
    *A*<sub>*j**q*</sub><sup>*p*</sup>.
-   defining
    *X*<sub>*i**j*</sub><sup>*p*</sup> := *A*<sub>*j**q*</sub><sup>*p*</sup> \* *X*<sub>*i*</sub><sup>*q*</sup>
    for *j* = 2..*k*, thus creating redundant and zero data values.

However, a generical model cannot be reduced to a specification with
different *β*'s for each alternative choice unless the latter parameter
space can be restricted to contain no more dimensions than a generic
form.

With large n and k, the set of data values X_{ijk} can be huge. To
mitigate the data size, the following tricks can be applied:

-   limit the set of combinations of *i* and *j* to the most probable or
    near *j*'s for each *i* and/or cluster the other *j*'s.
-   use only a sample from the set of possible *i*'s.
-   support specific forms of data:

| \#  | form                                                                               | reduction                                                                                               | description                                             |
|-----|------------------------------------------------------------------------------------|---------------------------------------------------------------------------------------------------------|---------------------------------------------------------|
| 0   | *β*<sub>*p*</sub>*X*<sub>*i**j*</sub><sup>*p*</sup>                                |                                                                                                         | general form of p factors specific for each *i* and *j* |
| 1   | *β*<sub>*p*</sub>*A*<sub>*j**q*</sub><sup>*p*</sup>*X*<sub>*i*</sub><sup>*q*</sup> | *X*<sub>*i**j*</sub><sup>*p*</sup> := *A*<sub>*j**q*</sub><sup>*p*</sup>*X*<sub>*i*</sub><sup>*q*</sup> | q factors that vary with *i* but not with *j*.          |
| 2   | *β*<sub>*p*</sub>*X*<sub>*i*</sub><sup>*p*</sup>*X*<sub>*j*</sub><sup>*p*</sup>    | *X*<sub>*i**j*</sub><sup>*p*</sup> := *X*<sub>*j*</sub><sup>*p*</sup>*X*<sub>*i*</sub><sup>*p*</sup>    | p specific factors in simple multiplicative form        |
| 3   | *β*<sub>*j**q*</sub>*X*<sub>*i*</sub><sup>*q*</sup>                                |                                                                                                         | q factors that vary with *j* but not with *i*.          |
| 4   | *β*<sub>*p*</sub>*X*<sub>*j*</sub><sup>*p*</sup>                                   | *X*<sub>*i**j*</sub><sup>*p*</sup> := *X*<sub>*j*</sub><sup>*p*</sup>                                   | state constants *D*<sub>*j*</sub>                       |
| 5   | *β*<sub>*j*</sub>                                                                  |                                                                                                         | state dependent intercept                               |
| 6   | *β*<sub>*p*</sub>(*J*<sub>*i*</sub><sup>*p*</sup>==*j*)                            |                                                                                                         | usage of a recorded preference                          |

# regression

The *β*<sub>*p*</sub>'s are found by maximizing the likelihood
*L*(*Y*\|*β*) which is equivalent to finding the maximum of
$\\sum\\limits_{i} \\left\[{ \\sum\\limits_{j}Y_{ij} \\times v_{ij} - N_i \\times log \\sum\\limits_{j}w_{ij}}\\right\]$

First order conditions, for each *p*:
$0 = { \\partial L \\over \\partial\\beta_p } =  \\sum\\limits_{i} \\left\[{ \\sum\\limits_{j}Y_{ij} \\times { \\partial v_{ij} \\over \\partial \\beta_p } - N_i \\times { \\partial log \\sum\\limits_{j}w_{ij} \\over \\partial \\beta_p }} \\right\]$

Thus, for each *p*:
$\\sum\\limits_{ij} Y_{ij} \\times X_{ijp} = \\sum\\limits_{ij} N_i \\times P_{ij} \\times X_{ijp}$
as ${ \\partial v_{ij} \\over \\partial \\beta_p } = X^p_{ij}$ and \\(
{ \\partial log \\sum\\limits_{j}w_{ij} \\over \\partial \\beta_p }

# { \\sum\\limits_{j} {\\partial w_{ij} / \\partial \\beta_p } \\over \\sum\\limits_{j}w_{ij} }

{ \\sum\\limits_{j} {w_{ij} \\times \\partial v_{ij} / \\partial
\\beta_p } \\over \\sum\\limits_{j}w_{ij} }

# { \\sum\\limits_{j} {w_{ij} \\times X_{ijp} } \\over \\sum\\limits_{j}w_{ij} }

\\sum\\limits_{j} P_{ij} \\times X_{ijp} \\)

# example

[logit regression of
rehousing](logit_regression_of_rehousing "wikilink").