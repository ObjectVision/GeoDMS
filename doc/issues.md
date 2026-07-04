# Open GitHub issues, classified

Snapshot of the 41 open issues at https://github.com/ObjectVision/GeoDMS/issues, updated 2026-07-04
(first taken 2026-07-03 with 45 open issues; see "Recently closed" at the bottom for the delta).
Grouped by implementability. Buckets:

- **A. Low hanging fruit** — small, well-defined fixes; no design decisions needed.
- **B. Implementable after minor design choices** — clear scope; one or two decisions to settle first.
- **C. Refactoring** — the fix lives in internal mechanisms, not a local patch.
- **D. Needs design** — new algorithms, semantics, or architecture.
- **E. Test issues** — extending the test process.
- **F. Documentation issues** — docs/website work, no engine code.
- **G. Other** — roadmap, questions, investigations.

## A. Low hanging fruit

| Issue | Rationale |
|---|---|
| [#1143](https://github.com/ObjectVision/GeoDMS/issues/1143) Read MMD: no storagename in detail page | Detail-page omission for one storage manager; small fix in the property display path. |
| [#1129](https://github.com/ObjectVision/GeoDMS/issues/1129) map-view zoom +/−/scrollwheel broken | Regression in the June 10 build; once bisected, likely a small event-handling fix. |
| [#597](https://github.com/ObjectVision/GeoDMS/issues/597) `min_ifdefined` / `min_alldefined` operators | Direct analogue of existing `min_elem_ifdefined` operators — copy the pattern. |
| [#846](https://github.com/ObjectVision/GeoDMS/issues/846) Event-log message when item finishes | "Fence container without the fence" — stripped-down variant of existing machinery. |
| [#579](https://github.com/ObjectVision/GeoDMS/issues/579) Value info shows export-xml variables first | Ordering/filtering fix in the value-info page. |
| [#828](https://github.com/ObjectVision/GeoDMS/issues/828) Layer control: thicker 3D lines, darker selected grey | Pure UI tweak; the exact values are specified in the issue. |
| [#292](https://github.com/ObjectVision/GeoDMS/issues/292) Metainfo url fails with single backslashes | Path-to-URL escaping fix in the detail pages. |

## B. Implementable after minor design choices

| Issue | Design choice to settle |
|---|---|
| [#1145](https://github.com/ObjectVision/GeoDMS/issues/1145) Export Primary Data exports wrong (Buurt) geometry | Bug, not feature: needs repro/debug on OVSRV08; fix is picking the geometry of the right domain. |
| [#990](https://github.com/ObjectVision/GeoDMS/issues/990) `union_data` with unmatching but equal-sized domains | Hard error or warning? Might existing configs rely on it? |
| [#973](https://github.com/ObjectVision/GeoDMS/issues/973) Missing VAT in dbf/gpkg export | Always emit VAT, or an export-dialog option (with ArcGIS Pro note)? |
| [#1124](https://github.com/ObjectVision/GeoDMS/issues/1124) Floppy should also trigger ExportSettings xml write | Largely superseded: since #411 the floppy opens the Export Primary Data dialog, and `ExportSettings/MetaInfo` sidecars are written for every storage-manager export from that dialog; a name collision with the dataset (e.g. `.xml` exports) is now diverted to `<stem>.meta.<ext>`. Remaining decision: close as superseded, or re-scope to also writing the sidecar in the native-CSV path (`DoExportTableorDatabaseToCSV` bypasses storage managers, so it writes none). |
| [#711](https://github.com/ObjectVision/GeoDMS/issues/711) GDAL: writing column subset creates null columns | Skip other columns or write their data? Always include geometry? |
| [#694](https://github.com/ObjectVision/GeoDMS/issues/694) Show/hide items via model parameter | Property name and GUI-vs-config override semantics; use case (lus_demo) is clear. |
| [#471](https://github.com/ObjectVision/GeoDMS/issues/471) Edit Config Source on template items | Proposed solution (two menu entries) is in the issue. |
| [#421](https://github.com/ObjectVision/GeoDMS/issues/421) `sort_index` producing a new sorted domain | Operator name/signature (multi-criteria); semantics clear from the wiki workaround. |
| [#337](https://github.com/ObjectVision/GeoDMS/issues/337) `select_with_attr` variants (linked unit, parents, namespace) | Which variants, and the naming scheme. |
| [#612](https://github.com/ObjectVision/GeoDMS/issues/612) Value info for null value | Labeled "tiny issue", but first decide what should be shown (reason for null? supplier?). |
| [#319](https://github.com/ObjectVision/GeoDMS/issues/319) Improving icons | Decide the icon set (units vs containers, spatial-ref compass, grid domains); then asset work. |
| [#859](https://github.com/ObjectVision/GeoDMS/issues/859) Qt color dialog with transparency | Pick the Qt dialog/widget replacing the CommonControl. |
| [#917](https://github.com/ObjectVision/GeoDMS/issues/917) `xx_minkowski_sum` with variant as argument | Argument shape and which backends (cgal/bg/geos) to ship first. |
| [#367](https://github.com/ObjectVision/GeoDMS/issues/367) GDAL cannot process UNC paths | Path translation vs GDAL VSI prefixes; then a contained fix. |

## C. Refactoring

| Issue | Rationale |
|---|---|
| [#1128](https://github.com/ObjectVision/GeoDMS/issues/1128) PhaseContainer progress deferred until final consumer joins | Interest/commit scheduling internals; the fix is in how phase results are committed. |
| [#1105](https://github.com/ObjectVision/GeoDMS/issues/1105) geodms.pyd Python ABI mismatch across .m/.c installers | Build/packaging restructuring for a consistent bundled Python and coexistence with user GDAL/QGIS. |
| [#975](https://github.com/ObjectVision/GeoDMS/issues/975) Source description tab broken | Regression in supplier-traversal/source-description generation; likely needs reworking that traversal. |
| [#795](https://github.com/ObjectVision/GeoDMS/issues/795) Paths missing in log messages (esp. with indirection) | Requires threading item context through diagnostics for indirect expressions. |
| [#403](https://github.com/ObjectVision/GeoDMS/issues/403) Don't collect recollected items into subset | Changes how subitems are collected in `select_with_attr` subunits. |
| [#298](https://github.com/ObjectVision/GeoDMS/issues/298) Optimize `mapping(D,V)` for coordinate-separable transformations | Performance refactor of coordinate transformation storage, tied to XY-order/EPSG cleanup. |

## D. Needs design

| Issue | Rationale |
|---|---|
| [#856](https://github.com/ObjectVision/GeoDMS/issues/856) 2-dimensional Dijkstra (time + cost) | Non-trivial pruning semantics (Pareto frontier over two criteria). |
| [#659](https://github.com/ObjectVision/GeoDMS/issues/659) R (or Python) integration for calculations | Whole interop architecture: data marshalling, process model, error handling. |
| [#724](https://github.com/ObjectVision/GeoDMS/issues/724) Circular units (wrap-around grid/time) | New unit semantics rippling through operators and metric checking. |
| [#634](https://github.com/ObjectVision/GeoDMS/issues/634) Editable layer-control data via copy-on-write | Needs a copy-on-write design for calculated visualisation properties. |
| [#734](https://github.com/ObjectVision/GeoDMS/issues/734) Reuse classbreaks across mapviews | Where do classifications live, and how do views share them? |
| [#587](https://github.com/ObjectVision/GeoDMS/issues/587) Storage-read functions in keyExpr | Language-level change to make storage reads expressible in calculation rules. |
| [#302](https://github.com/ObjectVision/GeoDMS/issues/302) Winding-order reversal operator | May be subsumed by `split_polygon`/cleaning strategy — needs that decision first. |
| [#757](https://github.com/ObjectVision/GeoDMS/issues/757) Home-brewed polygon operators (remaining items) | The bg_* items are done; polygon_connectivity and fault-tolerant sweep overlay are algorithm design. |

## E. Test issues

- [#1031](https://github.com/ObjectVision/GeoDMS/issues/1031) — Include the bundled dms-files in the test process.
- [#943](https://github.com/ObjectVision/GeoDMS/issues/943) — Add RS trede mapview to the GUI test.

## F. Documentation issues

- [#1080](https://github.com/ObjectVision/GeoDMS/issues/1080) — Publish the Academy wiki on geodms.nl so it's Google-findable (website/docs, no engine code).

## G. Other

- [#810](https://github.com/ObjectVision/GeoDMS/issues/810) — Component planning 2024/2025: roadmap umbrella, not an implementable issue.
- [#830](https://github.com/ObjectVision/GeoDMS/issues/830) — Question/investigation: why is tif DialogData needed while the projection choice doesn't matter? Outcome determines whether there's a bug at all.
- [#949](https://github.com/ObjectVision/GeoDMS/issues/949) — Performance investigation: ~100s difference between GUI and Run for the same model; needs profiling before further classification.

## Recently closed (delta since 2026-07-03)

All four resolved by the export-flow work on branch `refactor_ownership` (commits `ca674002`,
`7920c9c7`, `534b665a`):

- [#411](https://github.com/ObjectVision/GeoDMS/issues/411) — The table-view floppy now builds a
  `Desktops/Default/ViewData` config table referencing the thematic attributes of all
  DataItemColumns and opens the Export Primary Data dialog on it. Also added the XML (*.xml)
  export format (GML driver, enabled without geometry), classified GML as non-updatable so
  `.gml`/`.xml` datasets are written in one go, and made the dialog hold interest in all storable
  items so every gdalwrite export happens in a single session. Polish left: friendlier folder
  suggestion, ViewData container accumulation per floppy click, group-by column test coverage.
- [#872](https://github.com/ObjectVision/GeoDMS/issues/872) — Export failures (failed items,
  exceptions, no-exportable-data) now pop a modal "Export failed" box with the fail reason and the
  dialog resets for a retry, instead of falsely reporting "Export ready".
- [#823](https://github.com/ObjectVision/GeoDMS/issues/823) — Closed as completed: table → csv is
  now floppy → Export (CSV preselected for non-mappable domains).
- [#630](https://github.com/ObjectVision/GeoDMS/issues/630) — Closed as completed alongside the
  floppy/export-dialog rework.

Related fix without its own issue: an `ExportSettings/MetaInfo` sidecar whose expanded name
collides with the exported dataset (e.g. primary data exported to `<stem>.xml`) is now diverted to
`<stem>.meta.<ext>` with an event-log warning, instead of being silently destroyed by the dataset
write (#1124 discussion).

## Cross-cutting observations

- **Export-flow cluster**: of the original seven (#823, #411, #1124, #630, #872, #973, #711), four
  are closed. #411's pattern — route table exports through the Export Primary Data dialog via a
  constructed `Desktops/Default/ViewData` config table — is the base for what remains: #1124 needs
  only a close-or-rescope decision, #973 (VAT option) and #711 (column-subset behavior) are
  dialog/driver options on top of the same machinery.
- **Least certain classifications**: #1145 and #975 both need a debugging session before it's clear
  whether they're an afternoon or a refactor.
