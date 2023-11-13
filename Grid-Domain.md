[[images/grid_format.png]]

A [[grid]] domain is a two-dimensional [[domain unit]], each cell is defined by a row and a column number.

This implies the [[value type]] of the [[domain|domain unit]] is of type PointGroup.

Most grid domains in the GeoDMS are used to describe a part of the world, called [[geographic grid domains|geographic grid domain]].

Furthermore, grid domains are also in use to define [[kernels|kernel]] for [[potential]] calcualtions. 

## grid functions

Most [[functions|operators and functions]] in the GeoDMS work on [[data items|data item]] of one (table) of two (grid) dimensional domains.

There is group of functions that explicitly use the two-dimensional structure of grid domains, for instance to calculate nearby relations. 
This group is called [[grid functions]].