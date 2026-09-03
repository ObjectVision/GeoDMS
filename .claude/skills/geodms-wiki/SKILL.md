---
name: geodms-wiki
description: Documenting a GeoDMS behaviour change on the GitHub wiki at C:\dev\GeoDMS.wiki, which is published to geodms.nl. When a page is due (any change a modeller can notice), pull before edit, the "Since GeoDMS x.y.z" convention, what breaks on upgrade, topic page not changelog, the page-name and menu mechanics of the site converter, operator spelling, and what belongs in the issue instead. Use when a change alters observable behaviour, when asked to update or write a wiki page, or when a page is found to disagree with the engine.
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

## How the existing pages do it

- Date it: "**Since GeoDMS 20.19.2** ..." at the paragraph that changed, so a reader on an
  older build knows why their build disagrees. When the old behaviour was wrong rather than
  different, say what it was; people debug configurations written years ago against these
  pages.
- Say what breaks. A check that never fired can surface on the first run after upgrading; an
  area that was read wrong changes on upgrade and the old number was the wrong one. Name it,
  it is the whole point of the change and still a surprise.
- Put it on the topic page (`IntegrityCheck.md`, `gdalwrite.vect`, `Command-line-options.md`,
  `Gui-scripting.md`), not in a changelog. Link with `[[Page-Name]]` or
  `[[label|Page-Name]]`, and add the reverse link on the pages that should point back.
- Operator names, value types and keywords in the spelling the engine registers (lower
  case, since #1161); the editor language files are generated from the same registry.
- The version to cite is the one in `rtc\dll\src\RtcVersionNumbers.h` after the bump, the
  version the change will ship in, not the last published release.
- The wiki records what a modeller must now do differently. The diagnosis, the measurements
  and the ruled-out hypotheses stay in the issue (geodms-issues).

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

## When the page and the engine disagree

The engine is what happened. The disagreement is a finding that belongs on the page, fixed
now rather than remembered: change the paragraph around a stale value too, or the old
reasoning stays under a new number. Sibling wikis (GeoDMS_Academy, RSopen, NetworkModel_PBL,
CRISP) are separate clones and separate publications.

## Committing

One commit per subject, message starting with `#<n>` for the GeoDMS issue, then what changed
(geodms-commit). Recent examples: `#1228 connect/connect_info/dist_info: the reversed
argument order, and maxSqrDist on the _eq/_ne variants`.
