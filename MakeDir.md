*[[File, Folder and Read functions]] MakeDir(ectory)*

## syntax

- MakeDir(*target_foldername*)

## definition

MakeDir(*target_foldername*) **creates a new folder** *target_foldername*, if it does not yet exists.

## description

In the GeoDMS use forward slashes(/) in path names in stead of backward slashes.

## applies to

[[data item|data item]] or [literal](https://en.wikipedia.org/wiki/Literal_(computer_programming)) *target_foldername* with string [[value type]]

## example

<pre>
parameter&lt;string&gt; MakeDir := <B>MakeDir(</B>'c:/tmp'<B>)</B>;
</pre>

*result: updating this parameter creates a new folder: c:\tmp.*