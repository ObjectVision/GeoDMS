*[[Recent Developments]]: Clean-up support*

Configurations, especially for continuing projects with multiple contributors, tend to expand to a level that the overview on the whole configuration get lost.

Configuration parts are added, but less attention is given to removing unused parts of the configuration/source data. Partly this is because it is not always clear which items/data are still 'in use'.

We implemented functions to help the user in finding out unused parts of the configuration/source data. Items can be set from the pop-up menu as:

-   source: to find out which other items are using this item.
-   target: to find out which other items are used to calculate this item.

The aim was not be on a full automatic clean-up procedure as:

-   there is no objective manner to find out if configuration parts/data are not useful anymore.
-   cleaning up configuration files automatically would also mean reformatting, which we think can better be done by the modeller.

Furthermore, further support for finding all used data sources with the Source Descr Tab in the detail pages, will be considered and/or clarified.

## related issues

-   [issue 1092](http://mantis.objectvision.nl/view.php?id=1092)