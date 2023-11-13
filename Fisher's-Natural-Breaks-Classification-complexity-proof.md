An ***O(k × n × log(n))*** algorithm is presented here, with proof of its validity and complexity, for the [[classification]] of an array of *n* numeric
values into *k* classes such that the sum of the squared deviations from the class means is minimal, known as Fisher's Natural Breaks Classification. This algorithm is an improvement of [Jenks' Natural Breaks Classification Method](http://en.wikipedia.org/wiki/Jenks_natural_breaks_optimization),
which is a (re)implementation of the algorithm described by Fisher within the context of [choropleth maps](http://en.wikipedia.org/wiki/Choropleth_map), which has time complexity ***O(k × n <sup>2</sup>)***.

Finding breaks for 15 classes for a data array of 7 million unique values now takes 20 seconds on a desktop PC.

# definitions

Given a sequence of strictly increasing values ***v<sub>i</sub>*** and positive weights ***w<sub>i</sub>*** for ***i ? {1..n}***, and a given number of classes ***k*** with ***k = n***,

choose classification function ***I(x)*** : {1..***n***} ? {1..***k***}, such that ***SSD<sub>n, k***</sub>, the sum of the squares of the deviations from the class means, is minimal, where:

$$ \\begin{align} SSD_{n,k} &:= \\min\\limits_{I: \\{1..n\\} \\to \\{1..k\\}} \\sum\\limits^k_{j=1} ssd(S_j) & & \\text{minimal sum of sum of squared deviations } \\\\ S_j &:= \\{i\\in \\{1..n\\}\|I(i) = j\\} & & \\text{set of indices that map to class}\\,j\\\\ ssd(S) &:= { \\sum\\limits_{i \\in S} {w_i (v_i - m(S))^2} } & & \\text{the sum of squared deviations of the values of any index set}\\,S\\\\ m(S) &:= { s(S) \\over w(S) } & & \\text{the weighted mean of the values of any index set}\\,S\\\\ s(S) &:= { \\sum\\limits_{i \\in S} {w_i v_i} } & & \\text{the weighted sum of the values of any index set}\\,S\\\\ w(S) &:= \\sum\\limits_{i \\in S} {w_i} & & \\text{the sum of the value-weights of any index set}\\,S \\end{align} $$

Note that any array of n unsorted and not necessarily unique values can be sorted and made into unique ***v<sub>i***</sub> by removing duplicates with ***w<sub>i***</sub> representing the occurrence frequency of each value in ***O(nlog(n))*** time.

# characteristics

One can derive the following:

- ***ssd(S) = sqr(S) - wsm(S)*** with

$$ \\begin{align} sqr(S) &:= \\sum\\limits_{i \\in S} {w_i {v_i}^2} \\\\ wsm(S) &:= {w(S) {m(S)}^2} = s(S) m(S) & &\\text{the weighted square of the mean of S} \\end{align} $$

- ***SSD*** = ***SSV*** - ***WSM*** with

$$ \\begin{align} SSV &:= \\sum\\limits_{j} {sqr(S_j)} = sqr(\\{1..n\\}) \\\\ WSM &:= \\sum\\limits_{j} {wsm(S_j)} = \\sum\\limits_{j} {w(S_j) {m(S_j)}^2} = \\sum\\limits_{j} { {s(S_j)}^2 \\over w(S_j)} \\end{align} $$

- Since ***SSV*** is independent of the chosen partitioning ***S<sub>j</sub>***, minimizing ***SSD*** is equivalent to maximizing:

$$ WSM = \\sum\\limits_{j} { {\[\\sum\\limits_{i \\in S_j} {w_i v_i}\]^2} \\over {\\sum\\limits_{i \\in S_j} {w_i}} } $$

- Since the sequence ***v*** is sorted, all ***v<sub>i***</sub>'s that belong to one class ***S<sub>j***</sub> must be consecutive and if equal values would be allowed, these would end up in the same class provided that at least ***k*** unique values are given (for proof, see: Fisher, On Grouping for Maximum Homogeneity, 1958). Consequently, the ***S<sub>j***</sub>'s can be 1-to-1 related to a strictly increasing sequence of class-break-indices *i*<sub>***j***</sub> := min ***S<sub>j***</sub> for ***j*** = 1..***k*** with ***i***<sub>1</sub> = 1 and also to a strictly increasing sequence of class-break-values ***v<sub>i<sub>j</sub></sub>***.

<!-- -->

- With ***n*** values and ***k*** classes, one can choose $\\binom{n-1}{k-1} = \\frac{(n-1)!}{(k-1)!  (n-k)!}$ elements as first value of a class (the minimum value must be the minimum of the lowest class).

<!-- -->

- given indices ***i***<sub>1</sub> = ***i***<sub>2</sub> and ***i***<sub>3</sub> = ***i***<sub>4</sub>: ***i***<sub>1</sub> = ***i***<sub>3</sub> ? ***i***<sub>2</sub> = ***i***<sub>4</sub> ? ***m(i<sub>1</sub>..i***<sub>***2***</sub>) = ***m(i<sub>3</sub>..i***<sub>***4***</sub>)

# dynamic programming approach

Take ***SSM<sub>i,j***</sub> as the maximum sum of squared deviations for the first i values classified to j classes and ***CB<sub>i,j***</sub> as the value index of the last class-break for such classification. Since a maximal sum is a sum, it is easy to see that ***SSM<sub>i,j***</sub> is equal to the maximum value for ***p*** ? {***j***..***i***} of ***SSM<sub>p*** - 1, ***j*** - 1</sub> + ***ssm***({***p***..***i***}) . Thus, the ***SSM***<sub>***p*** - 1, ***j*** - 1</sub> provide [Overlapping Sub Problems](http://en.wikipedia.org/wiki/Overlapping_subproblem) with an [Optimal Sub Structure](http://en.wikipedia.org/wiki/Optimal_substructure) and the following dynamic program can be used to find a solution.

Dynamic rule for ***i*** = ***j*** > 1:

$$ \\begin{align} SSM_{i,j} &:= \\max\\limits_{p \\in \\{j..i\\}} SSM_{p-1, j-1}+ssm(\\{p..i\\}) \\\\ CB_{i,j} &:= arg\\max\\limits_{p \\in \\{j..i\\}} SSM_{p-1, j-1}+ssm(\\{p..i\\}) \\end{align} $$

Start conditions: 

$$ \\begin{align} SSM_{i,1} &:=  ssm(1..i) CB_{i,1} &:= 1 \\end{align}$$

Thus 

$$ \\begin{align} SSM_{i,i} &:= sqr(\\{1..i\\}) \\\\ CB_{i,i} &:= i \\\\ \\end{align} $$ 

Note that ***CB***<sub>***i,j***</sub> and ***SSM***<sub>***i,j***</sub> are only defined for ***j*** = ***i*** thus ***i*** - ***j*** = 0. Furthermore, for finding values with indices (***n***,***k***), only elements with ***i*** - ***j*** = ***n*** - ***k*** are relevant.

- If ***i*** = ***CB***<sub>***n***, ***k***</sub> then the corresponding previous class-break index has been stored as ***CB***<sub>***i*** - 1, ***k*** - 1</sub>.

<!-- -->

- Fisher described an algorithm that goes through all i and for each i finding and storing all ***SSD***<sub>***i,j***</sub> and ***CB***<sub>***i,j***</sub> for each subsequent j, which is the approach taken in the found implementations (see links). This requires ***O(k × n)*** working memory and ***O(k × n <sup>2</sup>)*** time.

<!-- -->

- Dynamically going through all j and for each j finding all ***SSM<sub>i,j***</sub> for each i only requires ***O(n-k)*** working memory and ***O(k × n × log(n))*** time by exploiting the no-crossing-paths characteristic, as proven below, but only provides the last ClassBreak for each i.

<!-- -->

- no crossing paths property: ***CB***<sub>***i***, ***j***</sub>is monotonous increasing with i, thus ***i1*** = ***i2*** ? ***CB***<sub>***i***1, ***j***</sub> = ***CB***<sub>***i2, j***</sub> (see proof below).

<!-- -->

- To obtain a chain of ***CB***<sub>***i,j***</sub> back from ***CB***<sub>***n***, ***k***</sub>, one needs to maintain (***n - k + 1***) × (***k - 2***) intermediate CB's, unless the ***O(k × nlog(n))*** algorithm is reapplied k times.

# complexity

- Each ***ssm***(***i<sub>1</sub>..i<sub>2***</sub>) can be calculated in constant time as ${{\\left(WV\[i_2\] - WV\[i_1-1\]\\right)^2} \\over {W\[i_2\] - W\[i_1-1\]}}$ after a single initialization of ***O(n)*** time and keeping a buffer of ***O(n)*** size containing series of cumulative weights, ***W***, and     cumulative weighted values, ***WV***.

<!-- -->

- During the calculation of the ***n - k + 1*** relevant values of SSM and CB for a row ***j ? {2..k - 1}*** using the ***n - k + 1*** values that were calculated for row ***j - 1***, one can divide and conquer recursively by calculating ***CB<sub>i***, ***j***</sub> for i in the middle of a range for which a left and right boundary are given and using the "no crossing paths" property to cut the remaining number of options in half by sectioning the lower indices to the left or equal options and the higher indices to the greater or equal options. The required processing time budget ***C(n,m)*** for processing ***n*** class-breaks using *m* values for previous class breaks is ***c*** × ***m*** + max ***i*** : [***C(n/2,i)+C(n/2,m+1-i)***]. A budget of ***C(n,m)*** := ***c × (m + n ) × (log<sub>2</sub>(n)+1)*** is sufficient, thus the calculation of ***CB<sub>i, j***</sub> given all ***CB<sub>i, j - 1***</sub> requires processing time of ***C(n,n)***, which is ***O(n * logn)***.

# proof of the no crossing paths property

Addition: the no crossing paths property might be equivalent to what elsewhere is called the convex Monge condition, which allows for the found speed-up of solving a dynamic programming problem.

- Given indices ***p1***, ***p2***, ***i1***, ***i2*** such that ***p1*** < ***p***2 = ***i***1 < ***i2,

$$\\begin{align}
m_p     &:= m(\\{p1 .. p2-1\\}) &w_p     &:= w(\\{p1 .. p2-1\\}) \\\\
m_j     &:= m(\\{p2 .. i1  \\}) &w_j     &:= w(\\{p2 .. i1  \\}) \\\\
m_n     &:= m(\\{i1+1 .. i2\\}) &w_n     &:= w(\\{i1+1 .. i2\\}) \\\\
m_{pj}  &:= m(\\{p1 .. i1  \\}) &w_{pj}  &:= w(\\{p1 .. i1\\}) \\\\
m_{jn}  &:= m(\\{p2 .. i2  \\}) &w_{jn}  &:= w(\\{p2 .. i2\\}) \\\\
m_{pjn} &:= m(\\{p1 .. i2  \\}) &w_{pjn} &:= w(\\{p1 .. i2\\}) \\\\
\\end{align}$$

- it follows that

$$v_{p1} \< v_{p2} \\le v_{i1} \< v_{i2} \\\\
m_p \< m_{pj} \< m_j \< m_{jn} \< m_n \\\\
m_{pj} \< m_{pjn} \< m_{jn}$$
$$\\begin{align}
w_{pj} &= w_p + w_j \\\\
w_{jn} &= w_j + w_n \\\\
w_{pjn} &= w_p + w_j + w_n = w_{pj}+w_{jn} - w_j \\\\
\\end{align}$$
$$\\begin{align}
w_{pj}  \\times m_{pj}  &= w_p \\times m_p + w_j \\times m_j \\\\
w_{jn}  \\times m_{jn}  &= w_j \\times m_j + w_n \\times m_n \\\\
w_{pjn} \\times m_{pjn} &= w_j \\times m_j + w_n \\times m_n + w_n \\times m_n \\\\
w_{pj}  \\times m_{pj}+w_{jn} \\times m_{jn} &= w_j \\times m_j + w_{pjn} \\times m_{pjn}
\\end{align}$$
$$\\begin{align}
ssm(\\{p1 .. i1\\}) &= w_{pj}  \\times m_{pj}^2 \\\\
ssm(\\{p2 .. i1\\}) &= w_{j}   \\times m_{j}^2 \\\\
ssm(\\{p1 .. i2\\}) &= w_{pjn} \\times m_{pjn}^2 \\\\
ssm(\\{i1 .. i2\\}) &= w_{jn}  \\times m_{jn}^2
\\end{align}$$

- and it follows that the following quantities are positive:

$$\\begin{align}
w_{pj1} &:= w_j \\times (m_{jn} - m_{j}) / (m_{jn} - m_{pj}) &> 0 \\\\
w_{jn1} &:= w_j \\times (m_{j} - m_{pj}) / (m_{jn} - m_{pj}) &> 0 \\\\
w_{pj3} &:= w_{pj} - w_{pj1} &> 0\\\\
w_{jn3} &:= w_{jn} - w_{jn1} &> 0\\\\
\\end{align}$$

- note that

$$\\begin{align}
w_{pj1}+w_{jn1} &= w_j \\\\
w_{pj3}+w_{jn3} &= w_{pjn} \\\\
w_{pj1}+w_{pj3} &= w_{pj} \\\\
w_{jn1}+w_{jn3} &= w_{jn} \\\\
\\\\
w_{pj1} \\times m_{pj} + w_{jn1} \\times m_{jn} &= w_j \\times m_j \\\\
w_{pj3} \\times m_{pj} + w_{jn3} \\times m_{jn} &= w_{pj} \\times m_{pj}+w_{jn} \\times m_{jn}-w_j \\times aj = w_{pjn} \\times m_{pjn}
\\end{align}$$

- and therefore

$$\\begin{align}
w_{pj1} \\times (m_{pj} - m_j    )^2 &+ w_{jn1} \\times (m_{jn} - m_j    )^2 &= w_{pj1} \\times m_{pj}^2 &+ w_{jn1} \\times m_{jn}^2 - w_{j}   \\times m_{j}^2 &> 0  \\\\
w_{pj3} \\times (m_{pj} - m_{pjn})^2 &+ w_{jn3} \\times (m_{jn} - m_{pjn})^2 &= w_{pj3} \\times m_{pj}^2 &+ w_{jn3} \\times m_{jn}^2 - w_{pjn} \\times m_{pjn}^2 &> 0
\\end{align}$$

- summation of these two inequalities gives:

***w<sub>pj***</sub> × ***m<sub>pj***</sub><sup>2</sup> + ****w<sub>jn***</sub> × ***m<sub>jn***</sub><sup>2</sup> - ***w<sub>j***</sub> × ***m<sub>j</sub><sup>2</sup>*** - ***w<sub>pjn***</sub> × ***m<sub>pjn</sub><sup>2***</sup> > 0

- thus

***w<sub>jn***</sub> × ***m<sub>jn***</sub><sup>2</sup> - ***w<sub>j</sub>*** × ***m<sub>j</sub><sup>2***</sup> > ***w<sub>pjn***</sub> × ***m<sub>pjn***</sub><sup>2</sup> - ***w<sub>pj***</sub> × ***m<sub>pj***</sub><sup>2</sup>

- which can be restated as

***ssm***({***p2***.***i2***}) - ***ssm***({***p2***..***i1***}) > ***ssm***({***p1***..***i2***}) - ***ssm***({***p1***..***i1***})

- given that : *S**S**M*<sub>*i*1, *j*</sub> = *S**S**M*<sub>*p*2 - 1, *j* - 1</sub> + *s**s**m*({*p*2..*i*1}) = *S**S**M*<sub>*p*1 - 1, *j* - 1</sub> + *s**s**m*({*p*1..*i*1}) for all *p*1 \< *p*2 when *p*2 is taken as *C**B*<sub>*i*1, *j*</sub>,
- adding the last two inequalities results in:

***SSM<sub>p2*** - 1, ***j*** - 1</sub> + ***ssm***({***p2..i2***}) > ***SSM***<sub>***p1*** - 1, ***j*** - 1</sub> + ***ssm***({***p1..i2***}) for all ***n*** = ***i***

- given the definition for ***CB***, this implies that ***CB****<sub>***i2, j***</sub> cannot be p1 and therefore ***CB<sub>i2, j***</sub> = ***p2** = ***CB<sub>i1, j***</sub>, QED.

# further improvements

Note that

- for each ***i***, and ***j*** < ***i***: ***SSM***<sub>***i,j***</sub> < ***SSM***<sub>***i,j*** + 1</sub> since splitting up any class would make the total ***SSM*** larger.
- it is not always true that ***SSM***<sub>***i,j***</sub> < = ***SSM***<sub>***i + 1,j***</sub>, for example, take ***v***<sub>1</sub> =  - 10 and ***v***<sub>2</sub> = 0, then ***SSM***<sub>1, 1</sub> = 1 × (-10)<sup>2</sup> = 100 and ***SSM***<sub>2, 1</sub> = 2 × (-5)<sup>2</sup> = 50
- From the previous paragraph, it follows that:

***ssm***({***p2..i2***}) - ***ssm***({***p1..i2***}) > ***ssm***({***p2..i1***}) - ***ssm***({***p1..i1***})

- for each ***i1***, ***i2*** > ***i1*** and ***j*** < ***i1***:
  
***SSM***<sub>***i2***, ***j + 1***</sub> - ***SSM***<sub>***i2***, ***j***</sub> = ***SSM***<sub>***i1***, ***j*** + 1</sub> - ***SSM***<sub>***i1***, ***j***</sub>,
    which can be proven by induction.

following the lines of the latter induction proof it can be shown (elaborate!) that for each ***i*** and ***j*** < ***i***:
***CB***<sub>***i,j***</sub> = ***CB***<sub>***i,j*** + 1</sub>, thus ***CB***<sub>***i,j***</sub> can be used as a lower bound when calculating ***CB***<sub>***i***, ***j + 1***</sub>.

# previously known algorithms

- Iteratively moving values from classes with high variation to classes with low variation is discussed on Wikipedia, which does not always result in an optimal solution. Start for example with Class 1 with (1 4) and Class 2 with (99 100). The described algorithms prescribes that 4 needs to go to Class 2 since Class 1 has the highest variation.

<!-- -->

- Local moves as long as the total sum of squared deviations decreases (as described in the [ESRI FAQ](http://support.esri.com/en/knowledgebase/techarticles/detail/26442)). Note that this has the risk of sticking to a local optimum. For example when starting with Class 1 = {1, 8, 9, 10} and Class 2 = {16}, the SSD = 50. When moving the 10 to class 2, the SSD increases to 56, but the global minimum is reached when Class 1 = (1) and Class 2 = (8, 9, 10, 16) with an SSD of 38.75.

# source code

Source code is part of the [GeoDms](https://github.com/ObjectVision/GeoDMS) and can be found [[here|CalcNaturalBreaks]] or in our
GitHub Repository, look for the function [ClassifyJenksFisher()](https://github.com/ObjectVision/GeoDMS/blob/main/clc/dll/src1/CalcClassBreaks.cpp).

# acknowledgements

This algorithm and proof of its validity and complexity is found by Maarten Hilferink, also being the author of this page. © Object Vision BV. The work for developing this method has partly been made possible by a software development request from the [<http://www.pbl.nl> PBL Netherlands Environmental Assessment Agency](http://www.pbl.nl_PBL_Netherlands_Environmental_Assessment_Agency).

# references

- Fisher, W. D. (1958). On grouping for maximum homogeneity. American Statistical Association Journal, 53, 789-798.
- Jenks, G. F. (1977). Optimal data classification for choropleth maps, Occasional paper No. 2. Lawrence, Kansas: University of Kansas, Department of Geography.
- Evans, I.S. (1977). The selection of class intervals. Transactions of the Institute of British Geographers, 2:98-124.
- <https://arxiv.org/pdf/1701.07204.pdf>
- <https://www.ncbi.nlm.nih.gov/pmc/articles/PMC5148156/>
- <https://link.springer.com/content/pdf/10.1007%2FBF02574380.pdf>
- <https://en.wikipedia.org/wiki/SMAWK_algorithm>

# links

- <http://en.wikipedia.org/wiki/Jenks_natural_breaks_optimization> (WikiPedia)
- <http://danieljlewis.org/2010/06/07/jenks-natural-breaks-algorithm-in-python>
- <http://danieljlewis.org/files/2010/06/Jenks.pdf>
- <http://www.biomedware.com/files/documentation/spacestat/interface/map/classify/About_natural_breaks.htm>
- <http://marc.info/?l=r-sig-geo&m=118536881101239>
- <http://code.google.com/p/geoviz/source/browse/trunk/common/src/main/java/geovista/common/classification/ClassifierJenks.java?spec=svn634&r=634>
- <http://www.tandfonline.com/doi/abs/10.1080/01621459.1958.10501479>
- <http://www.casa.ucl.ac.uk/martin/msc_gis/evans_class_intervals.pdf>
- <http://cran.r-project.org/web/packages/classInt/classInt.pdf>
- <http://www.srnr.arizona.edu/rnr/rnr419/publications/jenks_caspall_1971.pdf>