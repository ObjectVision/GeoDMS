*[[File, Folder and Read functions]] exec(ute)Dll*

## syntax

- execDll(*dll*, *function*)

## definition

execDll(*dll*, *function*) **executes the *function* in the *dll***.

## applies to

- dll and function with string [[value type]]

## example

<pre>
container execDll := <B>execDll(</B>'C:/dev/bin/accessrun.dll','RunProcAndWait'<B>)</B>;
</pre>

## see also

- [[exec]]
- [[exec_ec]]