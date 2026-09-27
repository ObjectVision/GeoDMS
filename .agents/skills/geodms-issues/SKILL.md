---
name: geodms-issues
description: Writing text that lands on GitHub for ObjectVision/GeoDMS and ObjectVision/GeoDMS-Test, through gh: issue comments, the debrief that closes an issue, new issues, follow-up issues for deferred scope, and open questions. When to post and when to show a draft first, the hash reference, the language of the thread, what a debrief must contain, and what never goes into public issue text. Use for any request to comment on, open, close, or draft text for an issue.
---

# Text for GitHub issues

`github.com/ObjectVision/GeoDMS` is public and every comment mails the participants. Text
there cannot be unpublished. `gh` is authenticated as the user, so anything you post appears
under their name.

## Post, or draft

An imperative from the user ("post", "comment", "close", "open an issue") is the permission;
do it and report the number or the link. Without such a request, show the text as a draft in
the first person and post only after agreement. Agreement with an analysis or a plan is not
agreement to publish it.

Read the thread before writing to it: `gh issue view <n> --comments`. A question already
answered there, or in a closed issue you cite, costs the reader more than it saves you.

Post with a file, never an inline string: `gh issue comment <n> --body-file <file>`,
`gh issue create --title ... --body-file <file>`. PowerShell 5.1 mangles embedded quotes.

## Style

- Write in the language of the thread. Many GeoDMS threads are in Dutch; keep the code
  identifiers, paths and error texts verbatim in either language.
- First person, on behalf of the user. No "we" for Object Vision, no attribution footer
  ("Generated with ..."), no name of an AI in the text. The co-author trailer belongs in
  commits only (geodms-commit).
- Refer to people by role, the reporter, the maintainer of the suite, unless you are quoting
  the thread.
- Reference issues as `#1234`; commits by short sha, `c8d5e6af`; files as `path:line`.
- Bold only for headings, never inside a sentence. No em dashes or en dashes as punctuation.
- Separate observation from interpretation: a measured number is a fact, its cause is a
  hypothesis until shown in the code. Say what you did not test.

## The debrief that closes an issue

Someone opening the issue later must understand from the comment alone why it could close,
without opening the code. In this order:

1. What was wrong, with the cause named at the code location (`geo/dll/src/Connect.cpp:412`)
   and the mechanism in one or two sentences.
2. How it was diagnosed: the repro (a probe config in the issue, or the reporter's), the
   measurement, what was ruled out.
3. What changed: the commits by short sha, the version it ships in (from
   `rtc\dll\src\RtcVersionNumbers.h`, so `20.19.2`), the wiki page updated when behaviour
   changed.
4. What was verified, with numbers: the repro before and after, `testcases` counts, the unit
   suite; and what was not verified, plainly.
5. What remains, if anything, as an open list (below), or a pointer to the follow-up issue.

The issue records what was wrong and how it was found; the wiki records what a modeller must
now do differently. Do not duplicate the wiki text into the issue or the debrief into the
wiki (geodms-wiki).

## Closing

"This can close" is three steps in this order: the debrief comment, the commits with `#<n>`
first in the subject, then `gh issue close <n>`. If in the course of it the issue turns out
not to be finished, do not close; say what stands open and leave the decision to the user.

Scope that was narrowed on the way gets its own issue rather than a silent omission: a
request the thread asked for and the fix did not deliver is filed as a follow-up, linked from
the debrief (as #1236 was split from #1228).

## Open questions

If anything stays open, end with a heading and bullets grouped by whoever acts, no prefix per
bullet, each answerable with yes, no or a number. Before writing a bullet, check that the
answer is not already in the thread or the code, that you cannot measure it yourself, and that
its premise holds. Add no @-mentions; who is addressed is decided at posting. No open points:
no heading.

## A new issue

Title as a fact, not a question. Body: the configuration path or the operator, the exact
command line or GUI steps, the observed number against the expected one, the version and
flavour (`20.19.1.m`), the log lines (`[E]` text, exit code, assertion), what you already
excluded, and a minimal probe config in a fenced block when one exists. In the tst repo the
same, plus the test name (`t641_2`) and the result folder.

## Never

- Probe whether something works on a live issue or release by writing placeholder text; a
  release body was once replaced with "test edit". Write only final content.
- Post measurements gathered while another build was writing to `bin\` (geodms-build).
- Claim a verification you did not run. "Not verified" is an acceptable sentence; a hollow
  OK is not.
