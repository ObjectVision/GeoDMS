## introduction

!!! Since the [GeoDMS](GeoDMS "wikilink") 8 series, the CalcCache is no
longer in use. The current computers with more RAM memory and the
improvements in the 8 series make the CalcCache less needed. Still we
are studying alternatives for the CalcCache at the moment, to make
interim model results more persistent and improve the performance.

The documentation on this CalcCache is still available, for GeoDMS 7
series users.

The [GeoDMS](GeoDMS "wikilink") uses a CalcCache to store calculation
results on disk. The CalcCache consists of a set of calculated (interim)
results, stored as files in the *%CalcCache%* folder.

The default value of the *%CalcCache%* folder is
*%[LocalDataDir](LocalDataDir "wikilink")%/%ProjDir%/CalcCachex%env%.v%version%*
(see [Folders and Placeholders](Folders_and_Placeholders "wikilink")).

The CalcCache should be considered as an extension of the internal
memory, therefore configuring a network drive for the CalcCache is
discouraged. It will slow down the application and burdens the network.

The CalcCache folder and it's subfolders are automatically created by
the [GeoDMS](GeoDMS "wikilink") when the results need to be stored.

## topics

-   [Folder and Filenames](CalcCache_Folder_and_Filenames "wikilink")
-   [Purpose](CalcCache_Purpose "wikilink")
-   [Update Mechanism](Update_Mechanism "wikilink")
-   [Cached Results](Cached_Results "wikilink")
-   [Session Specific](Session_Specific "wikilink")
-   [Managing Files](CalcCache_Managing_Files "wikilink")