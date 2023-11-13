*[CalcCache](CalcCache "wikilink") [Managing
Files](CalcCache_Managing_Files "wikilink")*

## compress files (also advised for SourceDataDir)

If possible, put the [CalcCache](CalcCache "wikilink") in a folder on a
disk with a [NTFS](https://en.wikipedia.org/wiki/NTFS) partition. The
[NTFS](https://en.wikipedia.org/wiki/NTFS) disk management system allows
you to keep files in a compressed on disk.

The following steps are needed to store files compressed:

1\) Browse in the Windows Explorer to your LocalDataDir, see for the
physical location the [GeoDMS GUI](GeoDMS_GUI "wikilink") \> Tools \>
Options \> Advance tab \> Paths> LocalDataDir.

2\) View the properties (right mouse click), a following dialog appears
(Windows 10 layout):

[<File:CCproperties.png>](File:CCproperties.png "wikilink")

3\) Click the Advanced button, a following dialog appears (Windows 10
layout)

[<File:CCadvanced.png>](File:CCadvanced.png "wikilink")

Activate the option *Compress contents to save disk space* (third
checkbox) and deactivate Indexing Service (second checkbox). Indexing on
[CalcCache](CalcCache "wikilink") folders is not useful and makes you
system slow.

An additional dialog can pop up, in which you indicate to compress both
folders and subfolders.

Dependent on the amount of data in the folder and subfolders, the
process to compress files can take some time.

The advantage of storing files compressed is that the amount of disk
space is saved substantially. Furthermore the
[GeoDMS](GeoDMS "wikilink") can read the compressed files faster, as
less disk access is needed. A small disadvantage is that writing files
take some more time, as the data need to be compressed first. But still
it is advisable to keep the files compressed on disk.