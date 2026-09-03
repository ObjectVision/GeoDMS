---
name: geodms-commit
description: Committing in the three GeoDMS repositories, the engine (C:\dev\GeoDMS_2026), the wiki (C:\dev\GeoDMS.wiki) and the regression suite (C:\dev\tst). The commit message convention (subject starts with #<issue>, cross-repo references, the body), the staging discipline needed because several sessions share one working copy and one index, the line-ending facts (git status lies, git diff does not), and the rule that nothing is ever pushed. Use for every commit, amend, or question about what to commit where.
---

# Committing in the GeoDMS repositories

Three repositories, one set of rules:

| Repo | Path | Branch | Holds |
|---|---|---|---|
| ObjectVision/GeoDMS | `C:\dev\GeoDMS_2026` | `main` | engine, GUI, `testcases\`, `doc\`, `nsi\`, `batch\`, these skills |
| ObjectVision/GeoDMS.wiki | `C:\dev\GeoDMS.wiki` | `master` | the user documentation, published to geodms.nl |
| ObjectVision/GeoDMS-Test | `C:\dev\tst` | `main` | the unit and regression suite, `full.py`, the test references |

Never push, in any of them, not even a tag. The user pushes. Commit only when asked; "commit
X to the wiki" means a local commit there. A behaviour change is one commit in the engine, one
in the wiki (see geodms-wiki), and where a regression reference or test moves, one in tst.

## The message

The subject line starts with the issue number: `#1234 What changed, said as a fact`.

```
#1234 Serialize GDALAllRegister, which is not thread-safe

GeoDmsRun died with 0xC0000374 when ten gdalwrite.grid items with an aggregating
IntegrityCheck were pulled by one ExplicitSuppliers trigger. ...

Verified: the reporter's repro 20 of 20 clean (7 of 8 crashed before); testcases 243/0.
```

- `#1234` with the hash, first on the line. GitHub links the commit into the issue timeline
  only in that form, and Visual Studio shows only the first line of a message.
- In `tst`, a bare `#37` is a GeoDMS-Test issue. A GeoDMS issue is written
  `ObjectVision/GeoDMS#1234`, still first on the line. In the wiki, `#1234` is the GeoDMS
  issue; the wiki has no tracker of its own.
- Several issues: the main one first, the others in the body. No issue: a plain subject.
- `Fixes #1234` in the body closes the issue automatically when the commit reaches `main` on
  GitHub. Use it only when the issue really is finished by this commit; otherwise closing is
  a deliberate step with a debrief (geodms-issues).
- The body says what was wrong, why this change is right, what was measured before and
  after, and what was verified (battery counts, unit result) and what was not. Someone
  reading `git log` in a year should not need the issue open beside it. No em dashes or
  en dashes as punctuation; no names of colleagues; no review of anyone's input.
- Keep the co-author trailer your harness prescribes. Do not add "Generated with" footers.
- Write the message to a file and commit with `git commit -F <file>`. PowerShell 5.1 splits a
  message with embedded double quotes into path arguments, and the error reads like a
  pathspec problem.
- A version bump (`rtc\dll\src\RtcVersionNumbers.h`) is its own commit,
  `Bump the release version to 20.19.2`.

Already committed but not pushed: `git commit --amend -F <file>` fixes the message.

## Before staging: whose work is this

Several sessions edit this working copy at once, not in separate worktrees, and they share
the index. Three things follow.

Stage explicitly. `git add <path> <path>`, never `git add -A`, `git add .` or `git commit -a`.
Look at `git diff -- <path>` for every file you stage and confirm every hunk is yours; a file
you edited may have been edited by another session since. For an untracked file read the
whole thing, `git diff` shows nothing for it and the entire content goes in.

Use `git diff --name-only` (plus `--cached`) to ask what is really modified, never
`git status`. Since every blob is stored with LF (`* text=auto`), status reports a file as
modified whenever its on-disk bytes are not the checkout form, even when the content hashes
identically. A plain `git add` of such paths stages nothing and clears the noise. Never
stage with `-c core.autocrlf=false`.

The index is shared, so `git add` followed by `git commit` is not atomic: anything another
session staged in between goes into your commit. When every file you are committing is
yours alone, commit by path, which ignores the rest of the index:

```
git commit -F C:\...\msg.txt -- rtc/dll/src/tic/TreeItem.cpp testcases/fn_test_x.dms
```

When a file mixes your hunks with someone else's, the path form is wrong too: it commits the
working copy of that path and discards your careful staging. Then build the commit on a
temporary index, touching neither the shared index nor the working copy:

```bash
OLD=$(git rev-parse HEAD)                    # once; reuse for read-tree, -p and the guard
git diff $OLD -- <file> > /tmp/all.patch
grep -n "^@@" /tmp/all.patch                 # pick your hunks
sed -n '1,4p;<from>,<to>p' /tmp/all.patch > /tmp/mine.patch
export GIT_INDEX_FILE=/tmp/idx
git read-tree $OLD
git apply --cached /tmp/mine.patch
TREE=$(git write-tree)
unset GIT_INDEX_FILE
NEW=$(git commit-tree $TREE -p $OLD -F /tmp/msg.txt)
git update-ref refs/heads/main $NEW $OLD
git reset -- <file>                          # shared index back on HEAD; working copy untouched
```

Read `HEAD` once: a second read may see another session's commit and hang your tree on a
newer parent, silently dropping what came in between. Do not skip the final `git reset`, or
`git status` shows every file as `MM` with a cached diff that is the exact inverse of your
commit. Afterwards always `git show --stat HEAD`: a file you did not touch means the parent
was stale or the path form took the working copy.

## Wiki repo

`git pull` before editing; others write to `master` from other machines and from the web
editor, and a stale edit becomes a conflict with markers inside a public page. One commit per
subject. See geodms-wiki for what goes on a page.

## tst repo

Several machines commit here. After a pull or merge, check for loss with
`git diff <incoming> HEAD -- <file>` and account for every removed line in the message.
Machine-specific paths are never hardcoded; `full.py` takes an overridable parameter with an
environment variable. After a change under `Operator\cfg\Operator\`, regenerate the
pre-18.1.0 mirror with `python batch\make_operator_pre1810.py`. `C:\dev\tst\CLAUDE.md` has the
rest.

## Case-sensitive paths

A few files under `shv\dll\src` are tracked in lower case (`dataview.cpp`, `dataview.h`,
`movableobject.h`, `palettecontrol.h`). `git diff -- shv/dll/src/DataView.cpp` is empty on
this file system; use the tracked spelling, and check with `git diff --name-only` when a diff
looks too quiet.
