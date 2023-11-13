## external components

The GeoDMS can execute external executables (with and without parameters) and scripts in for instance MsAccess. Dependencies in external components can not be managed by the GeoDMS. If relevant, they need be made explicit to the GeoDMS (see [[ExplicitSuppliers]] [[property]]).

## executables

This paragraph presents an example on how to execute an external executable. In this example an executable: _C:\\acsl\\math\\amath.exe_ need to be executed with a set of parameters. Example:

`C:\acsl\math\amath.exe -I C:\data\PcDitch\bel\PCD_WP_v3.m -P C:\data\PcDitch\bel\pd121.prx`

The command consists of the following parts:

-   *C:\\acsl\\math\\amath*: the name of the ACSL executable
-   *-I*: a first parameter
-   *C:\\data\\PcDitch\\bel\\PCD_WP_v3.m* : a second parameter, refering to a external model configuration script.
-   *-P*: a third parameter
-   *C:\\data\\PcDitch\\bel\\pd121.prx:* a fourth parameter, refering to a file with a set of other model parameters.

Configuration to execute the executable:
<pre>
container ConfigurationSettings
{
  container ACSL_exe: Descr = "c:\\acsl\\math\\amath";
  container Data_Dir: Descr = "c:\\data\\PcDitch\\bel\\";
}
container Command
{
    container ModelScript:     Descr = "PCD_WP_v3.m";
    container ModelParameters: Descr = "pd121.prx";
    container Execute: Using = "ConfigurationSettings"
    , := EXEC(
           DESCR(ACSL_exe) +  ' -I ' + `
           DESCR(Data_Dir) + `
           DESCR(ModelScript) + '  -P ' + `
           DESCR(Data_Dir) + `
           DESCR(ModelParameters)`
        );
}
</pre>

The external component is configured to the Execute container. The function EXEC executes the ACSL executable with the set of parameters.
The configuration element DESCR(ACSL_exe) is configured to use the value of the description property of the ACSL_exe item in the [[expression]] It also makes the GeoDMS aware the ModelScript has become a supplier of the Execute item.

## Ms Access functions

Functions written in scripts in Ms Access databases can also be executed. A necessary precondition to execute a Ms Access function is to add an Eval function in the Ms Access database containing the function to be executed. This Eval function need to look as follows:

<pre>
Function DoAction(ByVal VarAction As Variant)
   Eval (VarAction)
End Function
</pre>

From a GeoDMS configuration this DoAction function can now be executed. The function can call any other function in the same database (be aware, only function, no subroutines).

Two GeoDMS functions can be used, both run a MsAccess application with a given database name, function name and optional parameters.

1.  EXEC_ACCESS_ACTION_DB: The Ms Access application does not become visible.
2.  EXEC_VIS_ACCESS_ACTION_DB: The Ms Access application becomes visible.

Example:

container PrepareDatabase
<pre>
{
   parameter&lt;string&gt; codeDb         := fullPathName(System/Database, System/Database/MetaDbFullName);
   parameter&lt;string&gt; dataDir        := fullPathName(System/Database, System/Database/DbDirectory);
   parameter&lt;string&gt; sourceTemplate := dataDir + '/Sources/Elpen_Template.mdb';
   container FSS:
     := EXEC_VIS_ACCESS_ACTION_DB(
         codeDb
        ,'ImportFSSSourceFiles('
            + quote(dataDir + '/Elpen_FSS.mdb')
            + ',' + quote(sourceTemplate)
            + ',' + quote(dataDir + '/Sources')  + '
        )'
     );
}
</pre>

The first parameter of the EXEC_VIS_ACCESS_ACTION_DB operator is the codeDb [[parameter]]. This parameter has an expression configured that results in a full path name of the MsAccess database file. Within this database the function ImportFSSSourceFiles is called, the second parameter in the expression. The function ImportFSSSourceFiles should occur in the database. This function requests three parameters, configured between the brackets in the function call. The Eval function in MsAccess requests parameters to be quoted. Therefore, each function parameter is configured with the quote function.