*[[File, Folder and Read functions]] storage_name*

## syntax

- storage_name(*item_with_storage*)

## definition

storage_name(*item_with_storage*) results in a string [[parameter]] with the **value of the [[StorageName]] [[property]]** configured for the *item_with_storage*.

[[Folders and Placeholders]] are expanded in the result.

## applies to

- [[data item]] *item_with_storage* with a configured storage.

## example

<pre>
1. attribute&lt;uint8&gt;  griddata (GridUnit): StorageName = "%projdir%/data/testgrid.asc";
2. parameter&lt;string&gt; storage_name := <B>storage_name(</B>griddata<B>)</B>;
</pre>

*result: storage_name = 'c:/prj/tst/data/testgrid.asc'*

## see also

- [[PropValue]]