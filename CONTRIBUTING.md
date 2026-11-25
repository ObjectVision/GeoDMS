# Contributing to GeoDMS

GeoDMS is developed by Object Vision B.V. and used by modellers at planning agencies, research
institutes and governments. Bug reports, questions, documentation fixes and code are welcome
from anyone. This page says how to report a problem, how to propose a change, and what a
contribution has to meet before it is merged.

## Reporting a problem or asking a question

Open an issue at [github.com/ObjectVision/GeoDMS/issues](https://github.com/ObjectVision/GeoDMS/issues).
English and Dutch are both fine. A report is most useful when it contains:

- the GeoDMS version and flavour, as in the name of the installation folder, such as `20.30.0.m`;
- the configuration, or better a small configuration that shows the problem on its own, in a fenced code block;
- what you did: the GUI steps, or the exact `GeoDmsRun` command line;
- what you saw against what you expected: the message, the `[E]` lines of the log, the exit code, a wrong number next to the right one;
- what you already ruled out.

Questions about how to model something are welcome as issues too. The [GeoDMS Academy](https://github.com/ObjectVision/GeoDMS_Academy/wiki)
and the [wiki](https://github.com/ObjectVision/GeoDMS/wiki) may already answer them.

Security problems: do not open a public issue; follow [SECURITY.md](SECURITY.md).

## Proposing a change

For anything larger than a small fix, open an issue first and describe what you want to change
and why, so that the approach can be agreed before you spend time on it.

1. Fork the repository and make a branch from `main`.
2. Build with the committed solution or presets only, as described on
   [Compiling the GeoDMS](https://github.com/ObjectVision/GeoDMS/wiki/Compiling-the-GeoDMS):
   `all22.sln` with the Visual Studio 18 msbuild on Windows, or the CMake presets
   `windows-x64-release` and `linux-x64-release`. Do not change the toolset, the vcpkg setup or
   the props files.
3. Run the regression battery against your build: `testcases\run_testcases.bat` on Windows. A new
   operator or a fixed bug comes with a case in `testcases/` that fails without your change and
   passes with it; a negative case states the message it must fail with in a line
   `// EXPECT-FAILURE: <text>`.
4. Write each commit subject as `#<issue> <what the commit does>`, in English.
5. A change that a modeller can notice, such as a new operator, a changed rule or a new message,
   also updates the page about it on the [wiki](https://github.com/ObjectVision/GeoDMS/wiki).
   Mention the page in the pull request; the wiki is edited separately.
6. Open a pull request that names the issue, says what you tested and what you did not.

A maintainer reviews the change and may ask for changes, tests or a smaller scope. Keep a pull
request to one subject: two small pull requests are reviewed sooner than one large one.

## Licence of contributions

GeoDMS is licensed under the GNU General Public License, version 3 or (at your option) any later
version ([license.txt](license.txt)). Object Vision B.V. accepts a contribution under a
contributor licence agreement, which you sign once, before your first contribution is merged,
and which then covers all your later contributions. Under that agreement:

- you keep the copyright of your contribution;
- you grant Object Vision B.V. a perpetual, worldwide, non-exclusive and royalty-free licence to
  use, modify and distribute your contribution as part of GeoDMS, under the GPL and also under
  other licence terms, and a licence to any patent of yours that the contribution needs;
- you confirm that the contribution is your own work, or that you have the right to submit it on
  these terms, and, if you made it as part of your job, that your employer agrees.

A maintainer sends you the agreement when you open your first pull request; you can also ask for
it at [info@objectvision.nl](mailto:info@objectvision.nl) beforehand.

Code taken from another project keeps its own copyright notice and must have a licence that is
compatible with the GPL version 3. Say where it comes from in the pull request.

## Contributions made with AI tools

The maintainers of GeoDMS use coding agents themselves, and contributions made with AI tools are
accepted on the same terms as any other, plus these:

- You are the author. You must understand every line you submit and be able to explain it and
  change it when asked in review. Do not submit code you could not have reviewed yourself.
- Disclose the tool: end each commit message that an AI tool helped write with a trailer that
  names the tool and the model, such as `Co-Authored-By: <tool> (<model>)` or `Assisted-by: <tool>`.
- No unattended submissions. A pull request, issue or comment must be written or checked by the
  person who submits it; accounts that post agent output without a human in the loop are blocked.
- Keep it small and plain. Generated code tends to be long; more code is more code to maintain.
  Pull requests that are mostly boilerplate, restate the issue or do not pass the battery are
  closed without a detailed review.
- An agent that works in this repository follows [AGENTS.md](AGENTS.md).

## Code of conduct

Everyone who takes part in GeoDMS, in issues, pull requests or elsewhere, follows the
[code of conduct](CODE_OF_CONDUCT.md).

## Contact

Object Vision B.V., [info@objectvision.nl](mailto:info@objectvision.nl), for anything that does
not fit in an issue, such as support on a model or a project.
