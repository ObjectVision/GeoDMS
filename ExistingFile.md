*[[File, Folder and Read functions]] ExistingFile*

## syntax

- ExistingFile(*alternative*, *filename*)

## definition

ExistingFile(alternative, filename) results in a string value with the **full path name of the *filename*** [[argument]] if the file exists and in the *alternative* argument if the file does not exists.

If no path information is configured, the default path for both arguments is the %ConfigDir% placeholder.

## applies to

- [[data items|data item]] or [literal](https://en.wikipedia.org/wiki/Literal_(computer_programming)) *alternative* and *filename* with string [[value type]]

## since version

6.026

## example

<pre>
parameter&lt;string&gt; ExistingFile := <B>ExistingFile(</B>'c:/tmp/test.txt', 'd:/tmp/test.txt'<B>)</B>;
</pre>

*result: Updating this item results in the value 'd:/tmp/test.txt' if this file exists and if not in the value 'c:/tmp/test.txt'.*