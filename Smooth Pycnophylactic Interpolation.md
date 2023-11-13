Smooth Pycnophylactic Interpolation is a
[disaggregation](disaggregation "wikilink") where *z*<sub>*i*</sub>
have minimimal quadratic slopes subject to the Pycnophylactic Principle.

Mathematically: minimize
*f*(*z*) := (*D*<sub>*x*</sub>*z*)<sup>*T*</sup>*D*<sub>*x*</sub>*z* + (*D*<sub>*y*</sub>*z*)<sup>*T*</sup>*D*<sub>*y*</sub>*z*
subject to: $\\forall r: \\sum\\limits_{i} z_i \* q_i^r = Z_r$, where
*D*<sub>*x*</sub> and *D*<sub>*y*</sub> are the linear operations that
result in the partial discrete difference in the x and y directions
respectively.

# notes

Since
(*D**z*)<sub>*i*</sub> := *z*<sub>*i*</sub> − *z*<sub>*i* − 1</sub>,

one can derive that \\( ((D z)^T D z) = (z^T D^T D z)

# \\sum\\limits_{i

1}^n(z_i - z_{i-1})^2

# \\sum\\limits_{i

1}^n(z_i^2 + z_{i-1}^2 - 2 z_i z_{i-1})

# z_0^2 + \\sum\\limits_{i

1}^n(2 z_i^2 - 2 z_i z_{i-1}) - z_n^2 \\)

and
${{\\partial f(z)} \\over {\\partial z_i}} = (z^T D^T D)^T_i + (D^T D z)_i = (D^T D^{TT} z)_i + (D^T D z)_i = 2(D^T D z)_i$

this convex optimization problem can be reformulated as: \\( {{\\partial
\[ f(z) + \\sum\\limits_{r} \\lambda_{r} (\\sum\\limits_{i} z_i \*
q_i^r - Z_r)}\] \\over {\\partial z_i}}

# 0

\\sum\\limits_{i \\in \\{ x, y \\} } 4 z_i - 2 z_{i-1} - 2 z_{i+1} +
\\sum\\limits_{r} \\lambda_{r} q_i^r ) \\),

subject to $\\forall r: \\sum\\limits_{i} z_i \* q_i^r = Z_r$

from which follows that \\( z_{x,y} = {1 \\over 4} (z_{x-1,y} +
z_{x+1,y} + z_{x,y-1} + z_{x,y+1} ) - {1 \\over 8} \\sum\\limits_{r}
\\lambda_{r} q_i^r \\)

# Links

[Tobblers work,
1979](http://www.geog.ucsb.edu/~kclarke/Geography232/Pycno.pdf)