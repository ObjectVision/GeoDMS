*[[Miscellaneous functions]] GeoDmsBuildConfig*

## syntax

- GeoDmsBuildConfig()

## definition

GeoDmsBuildConfig() results in a string [[parameter]] indicating if the running GeoDMS executable is a **Release** or **Debug** version.

## since version

7.018

## example

<pre>
parameter&lt;string&gt; GeoDmsBuildConfig := <B>GeoDmsBuildConfig()</B>;
</pre>

*result: GeoDmsBuildConfig = Release or GeoDmsBuildConfig = Debug*