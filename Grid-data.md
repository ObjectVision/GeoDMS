[[images/grid_format.png]]

Grid data are numeric or boolean [[attributes|attribute]] for a [[grid domain]]. 

In GeoDMS applications, grid data is often spatial, meaning it describes a rectangle of the earth surface.

The spatial reference of grid data is derived from the [[projection]] information of the domain unit.

## read/write
Grid data in GeoDMS projects can be read from and or written to the following formats:
- [[GeoTiff]] format (advised format)
- BMP (undocumented)
- [[ASCII Grid]]
- [[Arc Info Binary Grid]]

## calculations
Most functions in the GeoDMS apply to attributes of both one-dimensional as well as grid domains. 

The [[Grid functions]] apply only to attributes of grid domains, as they calculate with the two-dimensional structure of the data.

## see also
- [[vector data]]

