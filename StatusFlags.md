The [[GeoDMS GUI]] and [[GeoDMSRun]] maintain a status word with binary flags, which is read from registry key @@@ and saved to that key by the GUI when editing Tools -> Options.

They can also be set from the command line of the executables. with S*x* or C*x* which sets or clears a flag with *'x*' processed as following:

-   **A** : Admin mode

<!-- -->

-   **S**: SuspendForGUI

<!-- -->

-   **C**: ShowStateColor

<!-- -->

-   **V**: TreeViewVisible;

<!-- -->

-   **D**: DetailsVisible;
-   **E**: EventLogVisible;
-   **T**: ToolBarVisible;
-   **I**: CurrentItemBarHidden;
-   **R**: DynamicROI

Multi Tasking:

-   **0**: Suspend View Updates to favour GUI
-   **1**: Multi Tasking flag for Tile/segment tasks
-   **2**: Multi Tasking flag for Multiple operations simultaneously
-   **3**: Pipelined Tile operations

Note that: for GeoDMSRun only the Multi Tasking flags are relevant

StatusFlags need to be placed after the name of the executable and the optional logfile name and before the name of the configuration file. The
SatusFlags selves can be configured in any order.

## example
<pre>
"C:\Program Files\ObjectVision\GeoDms7317\GeoDmsRun.exe" <B>/S1 /S2</B> "C:\prj\cfg\dms.dms" /Results
</pre>

runs the GeoDMSRun with Multi Tasking flags 1 and 2 set