# Licensing of GeoDMS

## The software

The GeoDMS engine, GUI, GeoDmsRun, the Python module, the shipped `.dms` files and the scripts in
this repository are free software: you can redistribute them and/or modify them under the terms
of the GNU General Public License as published by the Free Software Foundation, either version 3
of the License, or (at your option) any later version (SPDX: `GPL-3.0-or-later`). The licence
text is [license.txt](../license.txt).

Source files start with a header such as

```
// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
```

or, in a Python script, the same two lines after `#`. The header refers to that licence, version 3
or any later version.

## Files from other projects in this repository

- `rtc/dll/src/ipolygon/` is derived from Boost.Polygon (Copyright 2008 Intel Corporation and
  others) and is licensed under the Boost Software License, version 1.0
  ([LICENSE_1_0.txt](../rtc/dll/src/ipolygon/LICENSE_1_0.txt)), as stated at the top of each file.
- `clc/dll/src/Random.cpp` reproduces the distribution algorithms of Boost.Random
  (Copyright Jens Maurer 2000-2001, Steven Watanabe 2011; Boost Software License 1.0), so that
  random sequences stay identical across platforms. The text of that licence is
  [LICENSE_1_0.txt](../rtc/dll/src/ipolygon/LICENSE_1_0.txt), to which the file refers.
- `geo/dll/src/Poly2GridOper.cpp`, the polygon rasterizer of poly2grid and poly2allgrids, is
  adapted from `alg/llrasterize.cpp` of [GDAL](https://gdal.org), which is under the MIT License.
  [Poly2GridOper-GDAL-MIT.txt](../geo/dll/src/Poly2GridOper-GDAL-MIT.txt) beside it holds the
  copyright notice of that file and the licence, which asks that both go with every copy, the
  compiled Geo.dll included.
- `geo/dll/src/CGAL_60/Polygon_repair/repair.h` comes from CGAL (Copyright 2023 GeometryFactory),
  which licenses it under GPL-3.0-or-later or under a commercial licence. GeoDMS uses it under the
  GPL.
- `qtgui/exe/res/fonts/dmstext.ttf`, the text font of the GUI, merges three fonts of the Noto
  project: Noto Sans Medium 2.007 (Copyright 2015-2021 Google LLC), Noto Sans Arabic Medium and
  Noto Sans Math, each licensed under the [SIL Open Font License, version 1.1](https://openfontlicense.org/).
  The font itself names only Noto Sans Medium. [dmstext-OFL.txt](../qtgui/exe/res/fonts/dmstext-OFL.txt),
  beside it, holds the copyright notices of the three fonts and the licence, and says what is
  known of the versions of the Arabic and math fonts. GeoDmsGuiQt compiles the font in, and the
  setups also install it as `misc/fonts/dmstext.ttf`.
- `qtgui/exe/res/fonts/remixicon.ttf`, the icon font of the GUI, is
  [Remix Icon](https://github.com/Remix-Design/RemixIcon) version 3.5, identical to
  `fonts/remixicon.ttf` at tag v3.5.0 of its repository, which licenses it under the
  [Apache License, version 2.0](https://www.apache.org/licenses/LICENSE-2.0).
  [remixicon-License.txt](../qtgui/exe/res/fonts/remixicon-License.txt) beside it holds the file
  `License` of that tag, unchanged, below a few lines that say which font and version it covers.
  GeoDmsGuiQt compiles the font in. Since 2026-01-25 the repository carries a licence of its own,
  the Remix Icon License v1.0: a newer Remix Icon brought into GeoDMS needs that licence text, and
  its entry in `INSIDE_GEODMS` of `tools/setup_licences.py` must then name that licence.
- The submodule `vcpkg` is [vcpkg](https://github.com/microsoft/vcpkg) of Microsoft, under the MIT
  License. `vcpkg-overlay-ports/` holds two ports taken from vcpkg, under the same licence, whose
  text is [LICENSE.txt](../vcpkg-overlay-ports/LICENSE.txt) there; its README says why they are
  there.
- `vcpkg-globio/licenses/` holds the licence texts of libraries in the GLOBIO flavour whose conda
  packages carry none of their own.

The Boost Software License, the MIT License and the Apache License 2.0 are compatible with the GPL
version 3. The SIL Open Font License allows a font to be bundled with software under any licence.

## Images of the GUI

GeoDmsGuiQt compiles in the files that `qtgui/exe/GeoDmsGuiQt.qrc` lists. Apart from the two fonts
above, they are part of GeoDMS and come under its licence, as its source code does: the icons of
the toolbars, the detail pages, the event log and the tree view, and the program icon; the branch
lines of the tree view, which `tools/make_tree_line_images.py` draws; and the splash screen below. `GEODMS_GUI_FILES` in
`tools/setup_licences.py` lists them, and a setup stops on a file in the resource file that is
neither in that list nor an item of `INSIDE_GEODMS` with its licence text.

## Generated images

`qtgui/exe/res/images/WorldHeatMap.jpg`, the splash screen that the GUI compiles in, and the six
images in `splash/` were generated with OpenAI image models from prompts by Object Vision B.V.,
which curated them. OpenAI's terms of use assign to the user whatever rights OpenAI has in such
output. Object Vision B.V. distributes the images with GeoDMS under the same licence as the
software.

## Libraries in the setups

The setups bundle third-party libraries, such as GDAL, PROJ, GEOS, Boost and Qt, each under its
own licence. Every setup installs `license.txt` and, beside it, a folder `licenses` with the
licence text of each bundled library and a table, `licenses/README.txt`, of which binary comes
from which library under which licence (#1371; generated by `tools/setup_licences.py`).
The folder `licenses/geodms` holds the licence texts of the material from other projects that
GeoDMS's own files contain, the two fonts and the code from GDAL above, and a second table in
`licenses/README.txt` says which installed file contains which (#1389).

The Python wheels carry the licence texts of what they bundle in the folder `licenses` of their
`.dist-info`, with a table in `licenses/third-party/README.txt`, written by `tools/make-wheels.py`.
They hold no fonts and no GUI, so of the material above only the text of the code from GDAL goes
with them, as `licenses/third-party/geodms/Poly2GridOper-GDAL-MIT.txt`.

## Documentation

The [wiki](https://github.com/ObjectVision/GeoDMS/wiki), published as [geodms.nl](https://geodms.nl),
and the [GeoDMS Academy](https://github.com/ObjectVision/GeoDMS_Academy/wiki) are © Object Vision
B.V. and licensed under the [Creative Commons Attribution-ShareAlike 4.0 International License](https://creativecommons.org/licenses/by-sa/4.0/)
(CC BY-SA 4.0). This licence covers the documentation only, not the software.
