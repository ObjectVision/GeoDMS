GeoDMS language support for Visual Studio Code
==============================================

The folder next to this file is the extension as it ships with THIS GeoDMS
release. The operator names, value types and keywords it highlights and
completes are the ones this engine registers, so a configuration written with
its completions reports no case mix-up warnings.

To install it by hand, copy the whole geodms-language folder to

    %USERPROFILE%\.vscode\extensions\local.geodms-language-0.0.2

and restart Visual Studio Code.

The setup also writes that profile copy for you, but it runs elevated, so the
copy lands in the profile of the account that ran the setup. If that was not
the account you use to edit .dms files, install from this folder instead.

Packaged extensions, and newer ones
-----------------------------------

Signed .vsix packages, for Visual Studio Code and for Visual Studio 2022/2026,
are published at

    https://github.com/ObjectVision/GeoDMS_Languages/releases/latest

Install one from the Extensions panel: the "..." menu > "Install from VSIX...".
For Visual Studio, double-click the .vsix.

Take a package that is not older than this GeoDMS release. An older one still
spells the value types UInt32, Float64, SPoint and the literals TRUE / FALSE,
which the engine has registered in lower case since 20.9; typing those into a
configuration makes it report a case mix-up for every one of them.

Notepad++
---------

The Notepad++ user-defined language file is GeoDMS_npp_def.xml, in the GeoDMS
installation folder itself. Import it with
Language > User defined Language > Define your language > Import.
