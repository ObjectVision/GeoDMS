*[[File, Folder and Read functions]] InitFile*

## syntax

- InitFile(*source_filename*, *target_filename*)

## definition

InitFile(*source_filename*, *target_filename*) **copies** the *source_filename* to the *target_filename*.

If the *target_filename* already exists, **it will not be overwritten** (the [[CopyFile]] function does overwrite an existing *target_filename*)

## description

In the GeoDMS use forward slashes(/) in path names in stead of backward slashes.

## applies to

[[data items|data item]] or [literal](https://en.wikipedia.org/wiki/Literal_(computer_programming)) *source_filename* and *target_filename* with string [[value type]]

## conditions

The *source_filename* must exist, if not an error is generated.

The folder of the *target_filename* to which the file is copied will be created, if it did not already exists.

## example
<pre>
parameter&lt;string&gt; InitFile := <B>InitFile(</B>'c:/tmp/test.txt', 'd:/tmp/test.txt'<B>)</B>;
</pre>

*result: updating this item copies the source file c:/tmp/test.txt to the target file d:/tmp/test.txt if this targetfile does not yet exists.*

## see also

- [[CreateFile]]
- [[CopyFile]]