*[[File, Folder and Read functions]] exec(ute)_ec(errorcode)*

## syntax

- exec_ec(*command*)

## definition

exec_ec(*command*) **executes** the *command* [[argument]] and returns it's [ExitCode](https://learn.microsoft.com/en-us/windows/win32/debug/system-error-codes--0-499-),

This enables a modeller to use that result in for example the construction of a storage name of a source that can only be read after completion of that process.

## applies to

- *command* with string [[value type]]

## since version

7.314

## example

This example shows how to use the *exec_ec* function to make a list of files in a folder and store the resulting list in a text file, that can be used later in the process to read all files from the folder.

The CanGenerate parameter can be used in your expression to process all files, making sure the list of files or an error code is generated first.

<pre>
container folderinfo
{
  container impl
  {
     parameter&lt;string&gt; FileNameDirInfo := '%LocalDataProjDir%/dirinfo_' + date +'.str';
     parameter&lt;string&gt; DirCmdOrg       := Expand(., 'Dir '+ XmlDir +'/*.xml > ' + FileNameDirInfo);
     parameter&lt;string&gt; DirCmd          := Replace(DirCmdOrg, '/', '\\') + ' /B';
   }

   parameter&lt;uint32&gt; writeFileList     := 
      <B>exec_ec(</B>Expand(., '%env:ComSpec%'), '/c ' + impl/DirCmd, Expand(., '%LocalDataProjDir%')<B>)</B>;
   parameter&lt;bool&gt;   CanGenerate       := writeObjectenlijst == 0;
}
</pre>

## see also

- [[exec]]
- [[execDll]]