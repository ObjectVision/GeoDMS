GeoDMS [[configuration files|configuration file]] can be viewed or editted with an [ASCII editor](https://en.wikipedia.org/wiki/Text_editor). We advice to use an editor supporting parameters for opening a file and positioning a cursor. This makes editing easy, the menu option: edit configuration source in the [[GeoDMS GUI]] can immediately show the relevant part of the code (if the setting in the Tools > Options is correctly configured).

We have experience and made language definition files available (highlighting the GeoDMS grammar in different colors) for two editors.

As the development of Crimson Editor is stopped, we advice to use Notepad ++.

## Notepad ++

The Notepad ++ editor can be downloaded from: [<https://notepad-plus-plus.org>.](https://notepad-plus-plus.org/downloads/)

The language definition file can be found in the same folder as the installed GeoDMS software (%programfiles%/ObjectVision/GeoDms*Version*).

In Notepad ++ activate the menu option language > user defined language > define your language and in this dialog the import button to import
this language definition file: GeoDMS_npp_def.xml.

For working with this editor, the [[GeoDMS GUI]] setting for Tools > Options > Advanced > DMS <span><u>E</u></span>ditor should be:

<code>"C:\Program Files\Notepad++\Notepad++.exe" "%F" -n%L</code>

(default setting after installing the GeoDMS).

If you installed Notepad ++ in a different folder as Notepad suggested, adjust the reference to the folder.

If your command line parameters do not work, see the issue [[Notepad++ command line parameters not working]].

## Crimson Editor

For Crimson Editor (if you still want to use it, use [version 3.70.](http://www.crimsoneditor.com)), language definition files can de
downloaded here: [CE_syntax_files](https://geodms.nl/downloads/CE/CE_syntax_files.zip)

Copy the contents of the zip file in the indicated folders in the Crimson Editor program files folder.

In the [[GeoDMS GUI]] the setting for Tools > Options > Advanced > DMS <span><u>E</u></span>ditor should be:

<code>"%ProgramFiles32%\Crimson Editor\cedt.exe" /L:%L "%F"</code>

If you installed Crimson Editor in a different folder as the folder Crimson Editor suggested, adjust the reference to the folder. If your folder contains spaces please use the quoted full path as in the NotePad++ example.