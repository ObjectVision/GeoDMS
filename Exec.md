*[[File, Folder and Read functions]] exec(ute)*

## syntax

- exec(*command*)

## definition

exec(*command*) **executes** the *command* [[argument]].

if the results of the exec command (for instance a list of files to be imported) are used further in the process, use the [[exec_ec]] function to make sure the command is executed or an error code is generated.

## applies to

- *command* with string [[value type]]

## example

container calc := <B>exec(</B>'calc.exe'<B>)</B>;

<I>result: runs the Windows calculator</I>

## see also

- [[exec_ec]]
- [[execDll]]