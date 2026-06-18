# Plan: extend `CrdTransformation` to rotation, 3D rotation and projective tilting

Status: design / planning. No code changed yet.

## 0. Scope and vocabulary

The user-requested complexity ladder (ascending), and what each level *adds*:

| Lvl | Name             | Form                                                       | Adds                          |
|-----|------------------|------------------------------------------------------------|-------------------------------|
| a   | Translation      | `p' = p + c`  (factor == (1,1))                            | nothing                       |
| b   | IsoScale         | `p' = s·p + c`, `\|sx\|=\|sy\|`, 4 orientations            | uniform zoom + axis flips     |
| c   | AnisoScale       | `p' = (fx·x, fy·y) + c`, 4 orientations  **(current model)**| per-axis zoom                 |
| d   | Affine2D         | `p' = M₂ₓ₂·p + c`                                          | rotation + shear              |
| e   | Affine3D         | `p' = M₃ₓ₃·(x,y,z) + c`                                    | true 3D rotation (z input)    |
| f   | Projective       | level-e then perspective divide `/(D − z')`                | tilt / vanishing-point        |

Two **independent** transform chains use `CrdTransformation` today, and the ladder applies differently to each:

1. **World→Device (view) chain** — `ViewPort::m_w2vTr`, `GraphVisitor::m_Transformation`.
   This is where MapView interaction introduces rotation/tilt/perspective. Target: up to **f**.
2. **Grid→World (georeference) chain** — `UnitProjection` (`tic/Projection.h`), `ViewPortInfo`/`ViewPortInfoEx` (stg), GDAL world-file georef (`gdal_base.cpp`, `DllMain.cpp`).
   Rasters can be georeferenced with a rotation (GDAL world files carry the 2 off-diagonal terms that we currently *assert to be 0*). Target here is at most **d** (rotated/sheared raster). Levels e/f are meaningless for a 2D georeference.

The transform that actually drives raster blitting is the **composite** `grid2device = grid2world (≤ d) ∘ world2device (≤ f)`, so the raster path must cope with the full **f**.

Design contract from the request:
- *Each transformation keeps its own complexity level* → the object carries a complexity **tag** and stores only what that level needs.
- *A simpler transform can still answer the more-complex requests* (you can always `Apply()` a level-a transform through the projective code path), *but a consumer that needs a simpler guarantee (axis-separability) must refuse a more complex transform* → "simple-only" queries (`Factor`, `Offset`, `V2WFactor`, `Orientation`, separable raster) are valid **only** when `Complexity() ≤ AnisoScale`.

---

## 1. Core type redesign (`rtc/dll/src/geo/Transform.h`)

### 1.1 Representation

Keep the cheap separable fields as the canonical storage for **a–c** (zero overhead, byte-identical behaviour, still a usable map key — see `grid_coord_key = pair<CrdTransformation,IRect>` in `ShvBase.h:159`). Add a general homogeneous payload for **d–f**, selected by a tag:

```cpp
enum class TransformComplexity : UInt8 {
    Translation = 0,  // a
    IsoScale,         // b
    AnisoScale,       // c   <-- everything today is <= this
    Affine2D,         // d
    Affine3D,         // e
    Projective,       // f
};

template <class T>
class Transformation {
    // canonical for a..c (orientation encoded in factor signs, as today):
    Point<T> m_Factor = {1,1};
    Point<T> m_Offset = {0,0};

    // payload for d..f: row-major 3x3 homogeneous H mapping (x,y,1)->(x',y',w'),
    // plus optional 3D/eye terms for e/f. Inline (no heap) so the type stays a cheap value/key.
    // Live only when m_Complexity >= Affine2D.
    std::array<T,9>  m_H;        // d/f  (planar homography; identity-ish otherwise)
    T                m_EyeDist;  // f only: the D in /(D - z')
    // e (true z): a 3x4 block; stored compactly, see 1.4.

    TransformComplexity m_Complexity = TransformComplexity::AnisoScale;
};
```

`m_H` is only populated for `≥ Affine2D`; for `≤ AnisoScale` it is ignored and `m_Factor/m_Offset` remain authoritative, so existing constructors, `==`, `<`, hashing and the GridCoord map key are unchanged at the levels used today.

### 1.2 Member functions to ADD

Classification queries:
```cpp
TransformComplexity Complexity()     const;
bool IsAxisSeparable() const { return m_Complexity <= TransformComplexity::AnisoScale; } // a..c
bool IsAffine()        const { return m_Complexity <= TransformComplexity::Affine3D;  }  // a..e
bool IsProjective()    const { return m_Complexity == TransformComplexity::Projective;}  // f
```

Generalized geometry (dispatch internally on the tag — these become the *only* universally safe entry points):
```cpp
template<typename F> Point<F> Apply  (const Point<F>&) const; // affine for a..e; +divide for f
template<typename F> Point<F> Reverse(const Point<F>&) const; // matrix/homography inverse
// rect handling changes meaning under rotation: a rect's image is NOT a rect.
Range<P> ApplyBounds(const Range<P>&) const;        // axis-aligned bounding box of the image (clip use)
std::array<Point,4> ApplyQuad(const Range<P>&) const; // the 4 transformed corners (geometry use)
```
> Correctness note: every current `Apply(rect)`/`Reverse(rect)` site (two-corner transform) is **wrong** for d–f. They must switch to `ApplyBounds` (when they want a clip extent) or `ApplyQuad` (when they want the actual shape). See §2.

Local-linearization queries (replace the scalar/per-axis "zoom" queries that break under d–f):
```cpp
Matrix2x2<T> JacobianAt(const Point<T>& worldPt) const; // constant for a..d, varies for f
CrdType      LocalScaleAt(const Point<T>& worldPt) const { return sqrt(abs(det(JacobianAt(p)))); }
CrdType      MinMaxScaleAt(const Point&) const;          // singular values, for anisotropy-aware LOD
```

Composition / inversion generalized to matrices:
```cpp
Transformation operator*(const Transformation&) const;   // matrix multiply if either > c
Transformation operator/(const Transformation&) const;
Transformation Inverse() const;                           // full 3x3 homogeneous inverse for f
bool CanReverse() const;                                  // det != 0 (replaces IsSingular for d..f)
```

Builders (introduce the new complexity levels at MapView interaction points):
```cpp
static Transformation Rotation   (CrdType angle, Point center);             // -> Affine2D
static Transformation Affine2x3   (const std::array<CrdType,6>&);           // -> Affine2D
static Transformation FromHomography(const std::array<CrdType,9>&);         // -> Projective
static Transformation Tilt        (CrdType tiltAngle, CrdType eyeDist D);   // -> Projective
```

Normalization (so a rotation that happens to be axis-aligned collapses back to the fast path):
```cpp
void SimplifyIfPossible();   // demote Affine2D->AnisoScale when off-diagonals ~0, etc.
Transformation PromotedTo(TransformComplexity) const;
```

### 1.3 Member functions to RESTRICT (assert simple-only)

These stay (callers below still need them) but gain `assert(IsAxisSeparable())` and are documented as level-≤-c only:
`Factor()`, `Offset()`, `V2WFactor()`, `Orientation()`, `X_dir()`, `Y_dir()`, `IsSingular()` (superseded by `CanReverse()`), `ZoomLevel()` (redefine as `LocalScaleAt(center)` for general transforms, or keep asserting).

### 1.4 3D (level e) — in scope

Level **e** (and a true `D − z'` divide at **f**) is wanted: layers will carry z. The first deliverable is **points and polygons with a separate z *attribute*** (not a new geometry ValueComposition yet — see §6). So:

- Storage for e/f: extend the homogeneous payload to a **3×4 block** `(x,y,z,1) → (x',y',w')` plus the eye distance `D` for the perspective divide. For the common planar raster (z≡0) this still reduces to the 3×3 homography, so the raster path is unaffected.
- `Apply`/`Reverse` gain a z-aware overload: `Apply(Point xy, T z)` and a `Point3<T>` form. 2D callers keep calling the 2-arg form (z defaults to 0). Level a–d ignore z; e/f consume it.
- `JacobianAt`/`LocalScaleAt` take the full `(x,y,z)` so LOD/symbol scaling is correct on tilted 3D geometry.

The order of work stays: a–c (separable) → d (2×3 affine) → f planar (3×3 homography, raster tilt) → **e/f with real z** (3×4 + divide, for z-attributed vector layers). The tag reserves all slots from the start so the representation never has to change again.

---

## 2. Usage-site inventory and required action

Legend — **Path**: *PP* = "point-pusher", only calls `Apply/Reverse/operator*/Inverse` → works at any complexity once §1.2 dispatch lands (only the rect-vs-quad fix needed); *SEP* = relies on axis-separability (`Factor/Offset/V2WFactor/ZoomLevel/Orientation`) → needs a guard + alternative path or a Jacobian-based replacement.

### 2.1 View chain (shv) — must reach level f

| Site | Calls | Path | Action |
|------|-------|------|--------|
| `GraphVisitor.cpp:170` ctor `m_Transformation(0,scaleFactors)` | ctor | SEP(b) | device sub-pixel transform stays ≤ b; keep, it composes on the *left* of the view transform |
| `GraphVisitor.cpp:352,377,591` `Apply(rect)` | Apply | PP | switch to `ApplyBounds` (clip extents) |
| `GraphVisitor.cpp:394,842` `Reverse(rect/pt)` | Reverse | PP | `Reverse(pt)` fine; rect→`ApplyBounds` of inverse |
| `GraphVisitor.h:89` `GetLogicalTransformation` divides by sub-pixel factor pair | operator/ | PP | works once `operator/` generalized |
| `DcHandle.cpp:155`, `GraphVisitor.cpp:924` `AddTransformation = w2v * m_Transformation` | operator* | PP | works once `operator*` generalized (the place where a rotated/tilted w2v enters the draw stack) |
| `ViewPort.h:136-137` `Factor()*GetScaleFactors()` (`…WorldToDeviceFactors`) | Factor | SEP | replace with `LocalScaleAt(center)` (scalar) where used for zoom display |
| `ViewPort.cpp:447,452` `ZoomLevel()`,`Factor()` (`GetCurrLogicalZoom…`) | SEP | SEP | zoom UI: use `LocalScaleAt`; expose rotation/tilt via new getters |
| `ViewPort.cpp:333,1198` `WorldScale(deviceDelta)` (border inflate, **Pan**) | WorldScale | SEP | Pan delta is in device space → convert with `Reverse(p0+delta) − Reverse(p0)` (inverse Jacobian), not per-axis `WorldScale` |
| `ViewPort.cpp:331` `IsSingular()` | SEP | both | → `CanReverse()` |
| `ViewPort.cpp:431,436,652,666,678,1225` `Reverse/Apply` | PP | PP | pt forms fine; rect forms → `ApplyBounds` |
| `Controllers.cpp` (`Reverse` ×~12: zoom/select rect) | Reverse | PP | device→world mouse mapping works at any complexity; **rubber-band rect becomes a quad** — selection geometry should use `ApplyQuad`/world polygon, not an AABB, under rotation |
| `Carets.cpp:293` `Reverse` | PP | PP | fine; caret shapes that assume axis-aligned need the quad |
| `FeatureLayer.cpp:1186,1219,1369,1369,…,468(poly),DrawPoints/Network/Polygons` `Apply(pt)` | Apply | PP | **vector features just transform their vertices** — resolution-independent, work at f for free |
| `FeatureLayer.cpp:68,1352`, `DrawPolygons.h:107,257,321` `ZoomLevel()` for LOD/symbol/visibility | ZoomLevel | SEP | replace with `LocalScaleAt(featureCenter)`; for tilt, scale varies across the view so LOD must be computed per-feature (or per-tile) rather than once |
| `FeatureLayer.cpp:943,1736` `geoRadius/=Factor().X()` (circle select) | Factor | SEP | use `LocalScaleAt(worldPnt)` (or invert Jacobian) so radius is correct under rotation |
| `FeatureLayer.cpp:434` `WorldScale(borderRect)` (extents inflator) | WorldScale | SEP | inflate by `JacobianAt`-mapped border, or inflate in device space then `ApplyBounds`-inverse |
| `GeoTypes.h:591` `TPoint2GPoint`: `p*Factor()+Offset()` | Factor/Offset | SEP | this is the logical→device sub-pixel map (≤ b); keep but `assert(IsAxisSeparable())`. Do **not** route the rotated world transform through it |

### 2.2 Raster path (shv) — the core work, see §3

| Site | Calls | Action |
|------|-------|--------|
| `GridCoord.h/.cpp` (`m_GridRows/Cols`, `V2WFactor`, `Orientation`, `CalcGridNrs`, `AdjustGridNrs`, `GetGridRowPtr/ColPtr`) | SEP | the entire separable resampler; gate behind `IsAxisSeparable()` and add the general path (§3) |
| `GridFill.h` `GridFill<>` (row/col duplicate-run blit) | SEP | keep for ≤ c; bypassed for d–f |
| `GridLayer.cpp:1013-1074` `DrawGrid` (uses `GetClippedRelDeviceRect`, tiles) | SEP | branch on composite complexity: separable today, else delegate to transformed blit |
| `GridDrawer.cpp:513 CopyToDrawContext` → `DrawImage(destRect,…)` | axis-aligned | add transformed variant (§3.2) |
| `WmsLayer.cpp:66,78,83,750,778-781,800,917` (`Offset/Apply/ZoomLevel/Factor`) | mixed | WMS tiles are axis-aligned in their own CRS; under view rotation the tile blit needs the transformed-blit path too |

### 2.3 Georeference chain (tic/stg/clc) — extend to level d only

| Site | Calls | Action |
|------|-------|--------|
| `tic/Projection.h:21` `UnitProjection : CrdTransformation` | — | inherits new representation; a rotated raster georef becomes an `Affine2D` projection |
| `Metric.cpp:362,372,382` ctors | ctor | accept a 2×3 affine ctor for rotated georef |
| `Metric.cpp:424` `ZoomLevel()` (`GetUnitlabeledScalePair`) | SEP | `LocalScaleAt` |
| `Metric.cpp:433` `GetCompositeTransform` `operator*=` | composition | works once `operator*=` does matrix multiply |
| `Metric.cpp:454` `operator<<` (`Factor*Unit+Offset`) | SEP | extend textual form to print a matrix when `> c` |
| `OperUnit.cpp:482,486,487,494` (`gridset` op: ctor, `*=`, `Offset/Factor`, `InplReverse`) | SEP | generalize; `Offset/Factor` extraction only valid if result stays ≤ c (else keep matrix) |
| `OperConv.h:292,331,337,510,522` pre/post rescaler `Apply/Inverse/IsIdentity` | PP | `Apply` fine; only matters if grid georef gains rotation |
| `Poly2GridOper.cpp:561,610,660,689` `Inverse/-=/InplApply` (polygon rasterization) | PP | vertices transform fine at any complexity; **scanline fill happens in grid space after transform**, so rotation of the *view* doesn't affect it; a rotated *georef* is handled by the generalized `Inverse`/`InplApply` |
| `ViewPortInfo.h:59` `GetViewPortInGrid = Apply(rect)` | Apply | `ApplyBounds` (it wants the grid AABB to read) — but see §3.4: a rotated/tilted viewport needs the grid **quad**, then read its bounding tile set |
| `ViewPortInfoEx.h:50` `IsIdentity` | PP | fine |
| `DllMain.cpp:144-176` `GetAffineTransformationFromGridDataItem` (`Offset`, `Transformation(offset,factor)`) | SEP | already builds an affine; extend to read rotation terms |
| `DllMain.cpp:197-202` `WriteGeoRefFile` (writes 6 params, **hardcodes rotation = 0**) | SEP | write the real off-diagonal terms from the matrix when `≥ Affine2D` |
| `DllMain.cpp:585-593` `ViewPortInfoEx` ctor (`GetCompositeTransform`, `IsSingular`, `operator/`, `Orientation`) | SEP | `operator/` generalized; `Orientation()` only valid ≤ c → for a rotated georef use the matrix directly; `IsSingular`→`CanReverse` |
| `gdal_base.cpp:1705-1716` `GetTransformation` (**asserts gdalTr[2]==0 && [4]==0**) | SEP | relax the asserts; map the full 6-param GDAL affine into an `Affine2D` (this is exactly GDAL's rotation support) |
| `gdal_base.cpp:1113-1116` `GetAffineTransformationFromDataItem` (`Offset/Factor` only) | SEP | export the off-diagonal terms when `≥ Affine2D` |

### 2.4 Summary of which sites "can support up to reprojection"

- **Already fine (point-pushers)** once `Apply/Reverse/operator*/Inverse` dispatch on the tag and rect calls move to `ApplyBounds`/`ApplyQuad`: all vector-feature drawing (`FeatureLayer` point/arc/polygon vertex transforms), the draw-stack composition (`AddTransformation`), mouse→world mapping (`Controllers`, `Carets`), `Poly2GridOper`, `OperConv` rescalers.
- **Need a guarded alternative path** (axis-separable assumers): the **raster blit** (`GridCoord`/`GridFill`/`GridDrawer`/`GridLayer`, `WmsLayer`), the **zoom/LOD/symbol scalars** (`ZoomLevel`/`Factor.X` in `FeatureLayer`, `DrawPolygons`, `ViewPort`), **Pan/border** (`WorldScale`), and the **georeference I/O** (`gdal_base`, `DllMain`, `ViewPortInfoEx`).
- **Sub-pixel device map** (`GeoTypes.h:591`, `GraphVisitor` ctor) stays deliberately ≤ b and is asserted as such.

---

## 3. Raster data blitting under rotation / projection

`GridFill` is a **separable nearest-neighbour resampler**: per device row → exactly one grid row (`GetGridRowPtr`), per device column → one grid col, with run-length duplication of repeated rows/cols. This is only valid when the composite `grid2device` is **axis-aligned scale+flip (≤ c)**. It must be kept (it is the fast common case) and *bypassed* for d–f.

### 3.1 Strategy: branch in `GridLayer::DrawGrid` / `GridCoord` on composite complexity

```
composite = grid2world * world2device   // CrdTransformation
if composite.IsAxisSeparable():   // a..c
    -> current GridCoord row/col arrays + GridFill   (unchanged)
else:                             // d..f
    -> render source tile to an intermediate DIB at ~source resolution
       (existing GridFill with an *identity-ish* grid->buffer map, or a direct copy)
    -> issue ONE transformed blit: DrawContext::DrawImageTransformed(tile2device, dib)
```

`GridCoord` gets a complexity branch: for ≤ c it precomputes the row/col arrays as today; for d–f it instead exposes `{source tile DIB, tile2device CrdTransformation}` and leaves resampling to the device.

### 3.2 New DrawContext primitive

Add `DrawImageTransformed(const CrdTransformation& src2device, const void* dib, w, h, bpp, palette, op)`:

- **Qt path (`QtDrawContext`) — primary GUI, handles d–f natively.**
  Qt's `QTransform` is a full 3×3 homography (`m13/m23 ≠ 0` ⇒ perspective). Build a `QTransform` from `src2device` (affine for d/e-planar, projective for f), `m_Painter->setTransform(t, true)`, set `QPainter::SmoothPixmapTransform` for interpolation, then `drawImage(QRectF(0,0,w,h), img)`. This gives rotation **and** perspective/tilt for free. (Current `DrawImage` at `QtDrawContext.cpp:393` already does an axis-aligned `drawImage(destRect,img)`; this generalizes the dest to a transform.)

- **GDI path (`GdiDrawContext`) — affine only, degrade for f.**
  `StretchDIBits` (`GdiDrawContext.cpp:324`) is axis-aligned only. For **d** use `PlgBlt` (parallelogram = 2×3 affine) — note `PlgBlt` source must be a memory DC bitmap, so allocate a DIBSection. For **f** (perspective) GDI has no primitive: either (i) `SetWorldTransform`(XFORM is affine only — insufficient for f), or (ii) fall back to the **CPU inverse-map resampler** (§3.3), or (iii) Direct2D/GDI+ (larger dependency). Recommended: GDI does ≤ d via `PlgBlt`, and **f routes to the CPU resampler**. Document GDI perspective as resampler-backed.

- **Export / headless path** must also work without a live painter → the CPU resampler (§3.3) is the portable backstop and the reference implementation.

### 3.3 Portable CPU inverse-map resampler (fallback + export + GDI-perspective)

For the destination clip rect, inverse-map each device pixel to source space and sample:
- **Affine (d):** incremental DDA — `srcRowStart += invRowStep` per scanline, `src += invColStep` per pixel (2 adds/pixel, nearest or bilinear). Same cost class as today, just not separable.
- **Projective (f):** maintain homogeneous `(u,v,w)` per pixel; perspective-correct sample needs a divide per pixel (or per short span with linear approximation). Use `Reverse()` which is itself a homography for the planar case.

This is the only place that has to understand pixel-level resampling; everything above it just hands down a `CrdTransformation`.

### 3.4 Tile selection and clipping under rotation/tilt

`GridCoord::GetClippedGridRect` and `ViewPortInfo::GetViewPortInGrid` currently intersect axis-aligned rects. Under d–f:
- The device clip rect's pre-image in grid space is a **quad** (d) or a general convex quad (f). Use `ApplyQuad` of the inverse, take its **bounding tile set** for which tiles to load, then let the resampler clip precisely.
- Under tilt (f) the far side of the view covers far more ground → the loaded grid extent can explode. Add a **far-plane / max-source-extent cap** and pick a coarser overview/zoom level for tilted views (mip-style), reusing the existing background-WMS tile-level snapping logic (`ViewPort::ZoomToTargetRoi`).

### 3.5 Anti-aliasing / quality

Nearest-neighbour is fine for classified rasters (palette identity must be preserved — note `DrawGrid` uses `SrcAnd`/Multiply compositing at `GridLayer.cpp:1066` and `QtDrawContext.cpp:392`). For continuous rasters under rotation/tilt, enable bilinear (Qt `SmoothPixmapTransform`); keep nearest for palette layers to avoid inventing class ids.

---

## 4. Phasing

1. **Core type, backward-compatible.** Add the tag (default `AnisoScale`), `Complexity()`/`IsAxisSeparable()`, make `Apply/Reverse/operator*/operator//Inverse` dispatch (matrix path for `> c`), add `ApplyBounds`/`ApplyQuad`/`JacobianAt`/`LocalScaleAt`, restrict-assert the SEP queries. Everything stays at level c → no behaviour change. Land + run regression (vector + raster) to prove parity.
2. **Rect→bounds/quad sweep.** Convert all `Apply(rect)`/`Reverse(rect)` sites (§2) to `ApplyBounds` or `ApplyQuad`. Still level c, still parity, but now correct-by-construction for higher levels.
3. **Scalar-query sweep.** Replace `ZoomLevel/Factor.X/WorldScale` LOD/symbol/Pan sites with `LocalScaleAt`/Jacobian/inverse-delta. Still parity at c.
4. **Georeference level d.** Relax `gdal_base.cpp` asserts, read/write GDAL rotation terms (`DllMain`), generalize `UnitProjection` composite & `ViewPortInfoEx`. Test with a rotated world-file raster — vector + separable raster unaffected, rotated raster now georeferences correctly (blitted via §3 path).
5. **Raster transformed-blit (d).** `DrawImageTransformed` Qt(`setTransform`)+GDI(`PlgBlt`)+CPU resampler; `GridLayer`/`GridCoord` complexity branch. MapView rotation of raster layers works.
6. **View rotation (d) end-to-end.** `ViewPort` orientation→rotation; introduce `Transformation::Rotation` at the interaction/controller layer; rotate-by-drag UI.
7. **Navigation gestures + GUI (d).** Rotate/tilt controllers, key bindings, compass/tilt caret, toolbar changes, `ViewPoint` round-trip (§7, §8). Lands with phase 6 so rotation is reachable the moment it works.
8. **Projective tilt (f).** Qt projective `QTransform` + CPU perspective resampler; tile-extent cap + overview selection (§3.4); tilt gesture wired to the perspective transform.
9. **z-attribute vector layers (e/f).** FeatureLayer reads an optional z attribute; 3×4 transform + `D−z'` divide; z-aware `Apply`/LOD (§6).

## 5. Risks / watch-points

- `grid_coord_key = pair<CrdTransformation,IRect>` is a `std::map` key with `operator<`/`==` (`Transform.h:203,219`). The added payload must participate in ordering/equality only when `> c`, and ≤ c keys must compare exactly as today or the `GridCoord` cache (`ViewPort::GetOrCreateGridCoord`) will thrash.
- `Apply(rect)` silently returning a 2-corner box is a latent correctness bug the moment any caller sees a `> c` transform; the §4.2 sweep must be complete before any `> c` transform can reach a layer.
- Tilt (f) source-extent blow-up (§3.4) — must cap before loading tiles or a tilted view can try to read the whole world at full resolution.
- GDI has no perspective primitive — keep the CPU resampler as the contractual fallback so headless/export and the GDI path stay correct.
- LOD under tilt varies across the screen; per-view single `ZoomLevel` is no longer meaningful — symbol/line-width/visibility must move to per-feature/per-tile `LocalScaleAt`.

---

## 6. z-coordinate (3D) layers — first step

Goal stated by the user: points and polygons that carry a **separate z attribute** (revisit value types / ValueCompositions later). This keeps the geometry types untouched and adds z as an ordinary numeric attribute on the same domain.

- **Data binding.** `FeatureLayer` (and `GraphicPointLayer`/arc/polygon subclasses) gain an optional **z aspect/theme** — a `Float32/Float64` attribute on the feature domain (per point) or on the polygon/arc point-domain (per vertex). When absent, z≡0 and everything behaves as level ≤ d. Reuse the existing `Theme`/aspect plumbing (`m_Themes[AN_...]`) rather than inventing a new channel.
- **Draw path.** The `DrawPoints`/`DrawNetwork`/`DrawPolygons` transformers (`FeatureLayer.cpp:1186,1219,1369`, `DrawPolygons.h:468`) currently `Apply(2D point)`. Add the z lookup and call `Apply(xy, z)`. Vertices are still resolution-independent, so once `Apply` is z-aware these draw at e/f for free.
- **Selection / hit-testing.** Picking stays 2D against the *projected* geometry: `Reverse` a device point onto the z=0 plane (or ray-cast to the tilted plane); circle/rect select use the projected positions. No z inversion needed for picking in the first step.
- **`ValueComposition` revisit (later).** A native 3D point/polygon ValueComposition (so geometry itself is `(x,y,z)`) is a separate, larger change touching `tic`/`clc` value types, storage, and operators — explicitly deferred. The z-attribute approach above is the bridge and exercises the entire level-e/f transform path end-to-end first.

---

## 7. Navigation: mouse gestures and keys

### 7.1 The gesture grammar (decided)

Principle (after review): **every action requires a button press** — no no-button "moves" — so the pointer finger always confirms the action. The **modifier key chooses the gesture family**; all are left-button **drags** (`MOUSEDRAG`, `AbstrController.h:46`) except marquee zoom, which is on both `Alt`+drag and the right button. All rotation/tilt pivots about the **view centre**. (Input routing **implemented**; orbit/tilt/eye-height are stubs pending the core transform — see §7.3.)

| Modifier | left-button **drag** (`MOUSEDRAG`) | left **click** |
|----------|-------------------------------------|----------------|
| **none**  | **Pan** (unchanged, `ViewPort.cpp:725`) | info / query in neutral |
| **Shift** | **Orbit about view centre:** tangential motion ⇒ rotate (yaw); radial inward ⇒ tilt; radial outward ⇒ untilt | — |
| **Alt**   | **Marquee zoom** (rubber-band rectangle) | — |
| **Ctrl**  | **Apply active selection tool** (rect / circle / polygon) | **Apply active selection tool** (point default) |
| **Ctrl+Shift** | — | **Copy coordinate** to clipboard |

Right button & wheel:

| Input | Action |
|-------|--------|
| **Right-drag** (> `DRAG_THRESHOLD_PIXELS`) | **Marquee zoom** (co-equal with `Alt`+drag) |
| **Right-click** (≤ threshold) | **Context menu** (unchanged) |
| **Wheel** | Zoom, cursor-anchored (unchanged, `ViewPort.cpp:646`) |
| **Shift + Wheel** | **Raise / lower viewpoint** (eye height / camera altitude) |

Notes / decisions:
- **Marquee zoom = `Alt`+drag *and* right-drag** (both primary). Right-drag discriminates click vs drag with `DRAG_THRESHOLD_PIXELS` (`ShvUtils.h`); a sub-threshold right-release falls through to the context menu. Caveat for the Linux GUI: some window managers grab `Alt`+left-drag as "move window" (modern GNOME defaults to `Super`, so `Alt` is usually free); the right-drag path is the WM-proof alternative. The viewport `accept()`s the `Alt`+press so it doesn't trigger the menu-bar mnemonic.
- **Ctrl+click/drag = selection** (default point); **Ctrl+Shift+click = copy-coordinate** (replaces the old Ctrl-click copy-coord at `ViewPort.cpp:726`). The selection toolbar buttons choose *which* tool Ctrl applies (§8).
- **Viewpoint height** is on **Shift+Wheel**, so Shift+drag is free for the primary orbit.
- Orbit decomposition (Shift+drag): with screen centre `C`, cursor `P`, drag motion `M`: tangential component `M·t̂` (t̂ ⟂ C→P) → yaw; radial component `M·r̂` (r̂ = (C−P)/|C−P|) → pitch (inward = more tilt, outward = untilt). A small dead-zone near `C` avoids a singular radial direction.

### 7.2 Keyboard (complements the gestures)

Existing: `+`/`-` zoom from centre, arrows pan (`ViewPort.cpp:879-885`), wheel zooms cursor-anchored (`:646`). Add:

| Key | Action |
|-----|--------|
| **Shift + ← / →** | Rotate yaw − / + (fixed step, e.g. 15°) |
| **Shift + ↑ / ↓** | Tilt + / − (fixed step) |
| **`N`** | **Restore north** (`TB_RestoreNorth`): yaw → 0, tilt/zoom/centre untouched |
| **`T`** | **Untilt view** (`TB_RestoreUntilted`): pitch → 0 (+ eye-height reset), yaw untouched |

The two resets are independent: restoring north leaves any tilt in place, and untilting leaves any rotation in place. (When both yaw and tilt are 0 the transform `SimplifyIfPossible()`-collapses back to level c and the fast raster path resumes.)

### 7.3 Implementation

- **`ViewPort::MouseEvent` rework.** Today controllers launch on `LBUTTONDOWN` keyed off the sticky `m_ControllerID`. The new model dispatches on the **modifier at `LBUTTONDOWN`**: plain → `PanController`; `SHIFTKEY` → `OrbitController`; `ALTKEY` → `MarqueeZoomController`; `CTRLKEY` → active selection controller. All are ordinary button-drag gestures, so the existing `DualPointController` launch/stop machinery (`CLOSE_EVENTS` = `LBUTTONUP|…`, `AbstrController.h:70`) applies unchanged — no special no-button capture handling. Shift+Wheel is handled in the `MOUSEWHEEL` branch (`:646`) as an eye-height step.
- **New controllers** (mirror `PanController`/`DualPointController`, `Controllers.h`): `OrbitController` (Shift+drag → yaw from tangential, pitch from radial), `MarqueeZoomController` (Alt+drag → zoom rect; can reuse `ZoomInController`'s rect→ROI math directly). Selection controllers already exist; they are invoked under Ctrl instead of via a sticky mode. (Eye-height needs no controller — it's a wheel step and the optional tilt caret.)
- **Add an `ALTKEY` `EventID` bit** (`AbstrController.h:43` enum currently has `CTRLKEY`/`SHIFTKEY` but no Alt) and plumb the Alt modifier through the Qt→`EventInfo` translation in `DmsViewArea`.
- **Each gesture** builds a `Transformation::Rotation`/`Tilt`/eye-height delta and composes it into `m_w2vTr` (now d/f-capable). A motion that returns the view to axis-aligned and untilted calls `SimplifyIfPossible()` so the transform drops back to level c and the **fast separable raster path resumes** automatically.
- **`ViewPort` state additions:** `m_Rotation`, `m_Tilt`, `m_EyeHeight`/`m_EyeDistance`; `SetRotation/GetRotation/SetTilt/GetTilt/SetEyeHeight`. The two reset tools are just `SetRotation(0)` (restore north) and `SetTilt(0)`+eye-height reset (untilt) — independent, no combined "reset orientation". `CalcWorldToClientTransformation` composes ROI-fit (existing) → yaw → pitch/perspective → eye-height.
- **`ViewPoint` round-trip** (`ViewPort.h:36`, currently `center, zoomLevel, spatialReference`) gains `rotation`, `tilt`, `eyeHeight` so clipboard/bookmark locations restore full orientation.

---

## 8. GUI additions and the zoom/selection mode question

### 8.1 Decision: no sticky mouse modes — gestures + modifier-driven selection

With §7, plain drag pans, Shift orbits/raises, Ctrl zooms/selects — so **there is no longer a need for sticky pan or zoom mouse modes at all**, and selection also becomes modifier-driven rather than a sticky mode:

- **Drop the sticky `TB_ZoomIn2` (marquee) and `TB_ZoomOut2` (click) modes.** Marquee zoom is now **Alt+drag** (or right-drag fallback); zoom in/out is covered by wheel (cursor-anchored), `+`/`-`, and the zoom-extent buttons (`TB_ZoomAllLayers/ActiveLayer/SelectedObj`).
- **The selection toolbar buttons become a radio group that selects the *active selection tool*** that **Ctrl+click/drag** applies (`TB_SelectObject`=point is the default), instead of putting the mouse into a sticky select mode.
- This is exactly the "always navigating unless you ask for something else" model the user proposed, taken one step further: navigation needs no mode at all, and selection is a held-Ctrl action.
- Keep a **settings/registry flag** to restore the classic sticky `TB_ZoomIn2/Out2`/select modes for existing-user muscle memory; keep the buttons visible (now choosing the active selection tool / one-shot zoom) with updated tooltips for discoverability.

### 8.2 New toolbar buttons / `ToolButtonID`s (`ShvUtils.h:175` enum, keep in sync with `fmDmsControl.pas`)

- **`TB_RestoreNorth`** — one-shot Button Command: yaw → 0, leaving tilt, eye-height, zoom and centre untouched (key `N`). Calls `ViewPort::SetRotation(0)`. Icon: compass/north-arrow. `OnCommandEnable` greys it out when already north (yaw == 0).
- **`TB_RestoreUntilted`** — one-shot Button Command: pitch → 0 and reset eye-height to the orthographic default, leaving yaw untouched (key `T`). Calls `ViewPort::SetTilt(0)` (+ eye-height reset). Icon: flatten/2D. Greyed out when already untilted (tilt == 0).
- Both are plain Button Commands (like `TB_ZoomAllLayers`, not checkable mode tools): add `case TB_RestoreNorth:`/`case TB_RestoreUntilted:` to `ViewPort::OnCommand` (`ViewPort.cpp:917`) and bind the keys in `ViewPort::OnKeyDown` (`:874`).
- **`TB_Rotate`, `TB_Tilt`** — *optional* sticky fallback modes for touch / no-modifier-key users (drag to rotate / tilt about centre). Gestures are primary; these are the discoverable affordance.
- Register all of these in `getAvailableMapviewButtonIds()` / `getToolbarButtonData()` (`DmsToolbar.cpp:156,170`): the two restore buttons as single-state entries `{TB_RestoreNorth}` / `{TB_RestoreUntilted}` (like `TB_ZoomAllLayers` at `:183`), the optional mode tools with `{TB_Neutral, TB_X}` checkable pairs.

### 8.3 New overlay carets (reuse `ScaleBarCaret`/`NeedleCaret` infra)

- **Compass rose** — shows north under the current yaw; **drag to rotate**, **click to reset north**. A `Caret` like `ScaleBarCaret` (`ViewPort::m_ScaleBarCaret`), toggled by a `TB_CompassOn/Off` pair. Doubles as the discoverable, mouse-only way to rotate.
- **Tilt / pitch indicator** — small gauge or horizon line; optional drag-to-tilt; hidden at pitch 0.
- Both read `m_Rotation`/`m_Tilt`, never the transform internals.

### 8.4 Scale bar under rotation/tilt

The scale bar assumes one isotropic scale; under tilt the scale varies across the screen. Start by reporting scale at the **view centre** via `LocalScaleAt(center)` with a "≈" marker; a perspective-following graticule is a later option.
