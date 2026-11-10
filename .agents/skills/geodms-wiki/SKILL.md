---
name: geodms-wiki
description: Documenting a GeoDMS behaviour change on the GitHub wiki at C:\dev\GeoDMS.wiki, which is published to geodms.nl. When a page is due (any change a modeller can notice), pull before edit, reference and not a log (the present tense, no "Since GeoDMS x.y.z" paragraphs, a version only in the since-version section), topic page not changelog, the page-name and menu mechanics of the site converter, operator spelling, and what belongs in the issue and the release notes instead. Use when a change alters observable behaviour, when asked to update or write a wiki page, or when a page is found to disagree with the engine.
---

# The GeoDMS wiki

`C:\dev\GeoDMS.wiki` is the clone of `ObjectVision/GeoDMS.wiki.git`, branch `master`. Every
page is also a page on www.geodms.nl, generated nightly and on each change by the converter
in `ObjectVision/GeoDmsWikiToStaticHtml`. So an edit here is an edit to a public website, and
the only way to make one. The wiki's own `CLAUDE.md` is not loaded in an engine session; read
it once before the first edit.

## Before editing

`git pull` in the wiki clone. Other people write to `master` from other machines and from
the GitHub editor; an edit on a stale checkout becomes a merge conflict with markers inside
a published page. Never push (geodms-commit).

## When a page is due

A change is not finished when it builds and the tests pass. Anything a modeller can notice
needs its page in the same session as the code and the issue debrief: a new or changed
operator or notation, a property that now warns or errors, a check that fires where it did
not, a rule about what a storage records or a reader may declare, a changed command-line
option, a GUI behaviour. Verifying tells you it works; the wiki makes it usable.

## How a page is written

- Reference, not a log. A page says how GeoDMS works now, in the present tense, as if it had
  always worked that way. It does not say when something changed ("Since GeoDMS 20.19.2 ...",
  "before 20.14 ...", "no longer ..."), what the old behaviour was, what was wrong with it or
  how it was found. A change rewrites the paragraph it affects instead of adding a dated
  paragraph beside it. The history is in the issue and the release notes; a page that reads
  as a log is hard to use, and every dated paragraph is one more that goes stale.
- A version appears in one place: the `## since version` section of an operator or property
  page, which names the release in which the operator, or a form of it, first appears
  ("15.6.0; the partitioned form since 20.24.0"). That is what a reader on an older build
  needs to know why their build refuses it. Cite the version in
  `rtc\dll\src\RtcVersionNumbers.h` after the bump, the release the change ships in, not the
  last published one.
- When a change makes a configuration fail that used to run, the page states the rule it now
  breaks, in words that the reader of that configuration recognises ("an IntegrityCheck that
  fails stops the calculation of the item"); the release notes say that the rule is new.
- Put it on the topic page (`IntegrityCheck.md`, `gdalwrite.vect`, `Command-line-options.md`,
  `Gui-scripting.md`), not in a changelog. Link with `[[Page-Name]]` or
  `[[label|Page-Name]]`, and add the reverse link on the pages that should point back.
- Operator names, value types and keywords in the spelling the engine registers (lower
  case, #1161); the editor language files are generated from the same registry.
- The wiki records what a modeller needs to know. The diagnosis, the measurements and the
  ruled-out hypotheses stay in the issue (geodms-issues).
- English at C1 level: full sentences, plain words and the terms as GeoDMS spells them. No
  bold inside running text, also not as a label at the start of a list item, and no em or en
  dashes as punctuation.

## Site mechanics that are not markdown

- The file name is the page name; a dash is a space. A real hyphen in a page name is the
  Unicode hyphen U+2010. Renaming a file changes a public url and breaks incoming links.
- A new page is built and indexed but does not appear in the left menu of geodms.nl on its
  own; that menu is `nav/geodms.md` in the converter repository, not `_Sidebar.md`. Say so
  whenever you add a page.
- Open a page with a sentence, not a table or an image: the first paragraph is the search
  snippet.
- The wiki documents the software. Project, client and publication lists live on
  objectvision.nl; link, do not copy.
- **A `[[label|Page-Name]]` link inside a table cell needs its pipe escaped**, or the table
  parser reads it as an extra column separator and the row breaks: `[[label\|Page-Name]]`.
  Any page you touch that mixes tables with piped links, check with
  `python3 <skill folder>\scripts\check-table-pipes.py <page.md>`, the skill folder being
  `.claude\skills\geodms-wiki` or its copy under `.agents\skills` (nonzero exit
  and a printed `file:line: link` per hit); run it after editing, since it is easy to add a
  new piped link to an existing table and miss the escape. This has bitten
  `Spatial-joins-and-allocation.md` three times over two review rounds.

## When the page and the engine disagree

The engine is what happened. The disagreement is a finding that belongs on the page, fixed
now rather than remembered: change the paragraph around a stale value too, or the old
reasoning stays under a new number. Sibling wikis (GeoDMS_Academy, RSopen, NetworkModel_PBL,
CRISP) are separate clones and separate publications.

## Committing

One commit per subject, message starting with `#<n>` for the GeoDMS issue, then what changed
(geodms-commit). Recent examples: `#1228 connect/connect_info/dist_info: the reversed
argument order, and maxSqrDist on the _eq/_ne variants`.
