This page provides a list of GeoDMS [Template](Template "wikilink")
implementations with common geodetic operations, such as conversion
between geodetic coordinate and datum systems (ie: spherical
\[longitude, latitude, height\] to WGS84 ECEF \[X, Y, Z\] frame).

[`Template`](Template "wikilink")` plh2xyzVec`
`{   `
`    `[`unit`](unit "wikilink")<uint32>` rowset;`
`    `[`attribute`](attribute "wikilink")<float64>` lat (rowset);`
`    `[`attribute`](attribute "wikilink")<float64>` lon (rowset);`
`       `
`    `[`parameter`](parameter "wikilink")<float64>` a          := 6378137.0;             // WGS84 equatorial radius`
`    `[`parameter`](parameter "wikilink")<float64>` f          := 1.0 / 298.257223563;   // Earth flattening`
`    `[`parameter`](parameter "wikilink")<float64>` e2         := 2.0 * f - f * f;       // excentricity e (squared)`
`    `[`attribute`](attribute "wikilink")<float64>` N (rowset) := a / `[`sqrt`](sqrt "wikilink")`(1.0-e2 * `[`sqr`](sqr "wikilink")`(`[`sin`](sin "wikilink")`(lat*`[`pi`](pi "wikilink")`()/180.0))); // Kromtestraal Oost-West richting`
`       `
`    `[`attribute`](attribute "wikilink")<float64>` X (rowset) := N * `[`cos`](cos "wikilink")`(lat*`[`pi`](pi "wikilink")`()/180.0) * `[`cos`](cos "wikilink")`(lon*`[`pi`](pi "wikilink")`()/180.0);`
`    `[`attribute`](attribute "wikilink")<float64>` Y (rowset) := N * `[`cos`](cos "wikilink")`(lat*`[`pi`](pi "wikilink")`()/180.0) * `[`sin`](sin "wikilink")`(lon*`[`pi`](pi "wikilink")`()/180.0);`
`    `[`attribute`](attribute "wikilink")<float64>` Z (rowset) := (N-e2*N)*`[`sin`](sin "wikilink")`(lat * `[`pi`](pi "wikilink")`()/180.0);`
`}`

[`Template`](Template "wikilink")` plh2xyzParam`
`{`
`    `[`parameter`](parameter "wikilink")<float64>` lat;`
`    `[`parameter`](parameter "wikilink")<float64>` lon;`
`       `
`    `[`parameter`](parameter "wikilink")<float64>` a  := 6378137.0;             // WGS84 equatorial radius`
`    `[`parameter`](parameter "wikilink")<float64>` f  := 1.0 / 298.257223563;   // Earth flattening`
`    `[`parameter`](parameter "wikilink")<float64>` e2 := 2.0 * f - f * f;       // excentricity e (squared)`
`    `[`parameter`](parameter "wikilink")<float64>` N  := a / `[`sqrt`](sqrt "wikilink")`(1.0-e2 * `[`sqr`](sqr "wikilink")`(`[`sin`](sin "wikilink")`(lat*`[`pi`](pi "wikilink")`()/180.0))); // Kromtestraal Oost-West richting`
`       `
`    `[`parameter`](parameter "wikilink")<float64>` X  := N * `[`cos`](cos "wikilink")`(lat*`[`pi`](pi "wikilink")`()/180.0) * `[`cos`](cos "wikilink")`(lon*`[`pi`](pi "wikilink")`()/180.0);`
`    `[`parameter`](parameter "wikilink")<float64>` Y  := N * `[`cos`](cos "wikilink")`(lat*`[`pi`](pi "wikilink")`()/180.0) * `[`sin`](sin "wikilink")`(lon*`[`pi`](pi "wikilink")`()/180.0);`
`    `[`parameter`](parameter "wikilink")<float64>` Z  := (N-e2*N)*`[`sin`](sin "wikilink")`(lat * `[`pi`](pi "wikilink")`()/180.0);`
`}`

[`Template`](Template "wikilink")` Haversine`
`{`
`       `[`parameter`](parameter "wikilink")<float64>` lat1_degrees: isHidden="True";`
`       `[`parameter`](parameter "wikilink")<float64>` lat2_degrees;`
`       `[`parameter`](parameter "wikilink")<float64>` lon1_degrees;`
`       `[`parameter`](parameter "wikilink")<float64>` lon2_degrees;`
`       `[`parameter`](parameter "wikilink")<float64>` lat1_radian := lat1_degrees * pi() / 180.0;`
`       `[`parameter`](parameter "wikilink")<float64>` lat2_radian := lat2_degrees * pi() / 180.0;`
`       `[`parameter`](parameter "wikilink")<float64>` lon1_radian := lon1_degrees * pi() / 180.0;`
`       `[`parameter`](parameter "wikilink")<float64>` lon2_radian := lon2_degrees * pi() / 180.0;`
`       `[`parameter`](parameter "wikilink")<float64>`     deltaLon_radian := lon1_radian - lon2_radian;`
`       `[`parameter`](parameter "wikilink")<float64>`     deltaLat_radian := lat1_radian - lat2_radian;`
`       `[`parameter`](parameter "wikilink")<float64>`     a               := sqr(sin(deltaLat_radian/2d)) + (((cos(lat1_radian) * cos(lat2_radian))) * sqr(sin(deltaLon_radian/2d)));`
`       `[`parameter`](parameter "wikilink")<km>`          distance        := (2d * 6371d * atan(sqrt(a) / (sqrt(1d - a))))[km];`
`}`