To structure a GeoDMS configuration, we advise to organise [[configuration files|Configuration File]] thematically.

Logical parts of the configuration [[units|unit]], [[classification schemes|Classification]], [[geography]] are configured in separate configuration  files.

With the Include statement a branch of the tree, stored in a separate configuration file is added to the configuration.

The hierarchical structure of the included files need to correspond with the subfolder structure on disk. A  configuration file with units, included in the root level of a project configuration, needs to be located in the subfolder with the name of the root configuration file) (excluding the extension .dms).

Configuration files at lower levels in the configuration need to be located in subsubfolders.

## syntax

Include statements are not configured as [[properties|property]] of [[tree items|tree item]], but as separate statements.

Use the keyword include, preceded by a \# character and the configuration file, name between the less than(\<) and greater than(>) characters for each included file, see example.

## example

<pre>
container LandUseAllocationProject
{
   #include &lt;Units.dms&gt;
   #include &lt;Classifications.dms&gt;
   #include &lt;SourceData.dms&gt;
   #include &lt;ScenarioComponents.dms&gt;
}
</pre>
