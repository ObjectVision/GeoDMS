*[[File, Folder and Read functions]] FullPathName*

## syntax

- fullPathName(*item*, *folder_or_filename*)

## definition

fullPathName(*item*, *folder_or_filename*) results in the **full path name** of the *folder_or_filename* [[argument]] in the context of the *[[item|tree item]]* argument.

In the GeoDMS use forward slashes(/) in path names in stead of backward slashes.

## applies to

- argument *item* can be any tree item.
- [[data item]] or [literal](https://en.wikipedia.org/wiki/Literal_(computer_programming)) *folder_or_filename* with string [[value type]]

## example
<pre>
parameter&lt;string&gt; rootfile := <B>fullPathName(</B>., '%projDir%\cfg\test.dms'<B>)</B>
</pre>

*result: rootfile= 'C:/prj/test/cfg/test.dms' (if C:/prj/test is the projdir)*