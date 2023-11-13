_[[User Guide GeoDMS GUI]]_ - TreeView

## introduction

[[images/GUI/treeview.png]]

The Treeview is the main navigation component of the application. It presents the hierarchical structure of a configuration in a view like the Windows Explorer. By default, the first level items are shown (the root item is not shown, it's name in presented in the title bar). Each item in the tree (called a [[Tree item]]) is presented with a [[name|Tree item name]] and an icon. 

The selected item in the Treeview is the active item in the application. This is an important concept, as many functions of the application work on the active tree item. By clicking the right mouse button a pop-up menu can be activated, with a set of menu options that work on this active tree item. The [[Detail pages]] also present information on this active item.

## icons

The main purpose of the icons is to inform the user on the type of item and, if relevant, show which default viewer will be used for the item. Double-clicking or pressing the Enter key on a selected tree item activates this viewer. The following icons are in use:

|icon|description|
|----|-----|
| [[images/GUI/TV_globe.bmp]] |A [[data item]] that can be viewed in a map. This implies the [[domain unit]] of the data item has a geographic relation (see [[how to model]] for [[How to configure a coordinate system]]). Dependent on the geographic domain, the data is visualized in a [[grid]], [[point]], [[arc]] or [[polygon]] layer|
|[[images/GUI/TV_table.bmp]] | A data item that cannot be visualized on a map (it has no geographic relation), it's default viewer is a table.
|[[images/GUI/TV_palette.bmp]] | A data item that contains a palette (a set of color values, corresponding to a classification).
|[[images/GUI/TV_container.bmp]] | A [[container]], not containing data items as direct [[subitem]]s.
|[[images/GUI/TV_container_table.bmp]] | A container, containing data items as direct subitems, it's default viewer is a table. 
|[[images/GUI/TV_unit_white.bmp]]|  A tree item with no data item and no subitems, e.g. a [[unit]].

## colors
The color used for a tree item name indicate it's status. Three statuses are distinguished:
1. **Not yet calculated**: an item is are not yet calculated.
2. **Valid**: the results are calculated successfully and the integrity checks configured for the item and its suppliers are met.
3. **Failed**: the application failed in updating the results or the integrity checks are not met. 
In the first case, the results are not available; an error is raised indicating what problem occurred while updating the tree item. 
In the second case, the results are available and can be presented in a view; a warning color indicates the results are not valid.

The colors for these statuses can be viewed or edited with the Settings > GUI options dialog, section TreeView, Show state colors. 

The application controls when and how to update tree items. If a user requests a view on a data item, the required tree items are first updated, before the results are presented in the view. If a tree item has become valid, this status is ‘stored’ until changes are made in the calculation rules. This means the second time the same view (or another view requiring the same data items) is requested, the data item is already valid and the results can be visualized immediately.
 

## pop-up/context menu
[[images/GUI/popupmenu.png]]

With a right mouse click, a pop-up menu can be requested for actions on the active tree item. This menu contains the following options, some options are only available for data items of for items with the status: failed

- **[[Export primary data]]** : exports the contents of the active data item (or the subitems of a container) with the export dialog.
- **Step up to FailReason**: if a data item is failed, this option activates the first failed supplier. 
- **Run up to Causa Prima (i.e. repeated Step up)**: if a data item is failed, the GeoDMS tries to find the the item for which the first error occurs.
- **Open in Editor**: Opens the current item in the [[Configuration File Editor]] as configured in the Settings > Local machine options dialog, section Configuration File Editor.  
- **Update TreeItem**: update the tree item, without showing the result in a view. The process is visualized with a process scheme. 
- **Update SubTree**: update the tree item and all it's subitems, without showing the result in a view. The process is visualized with a process scheme. 
- **Code Analysis**: Four options to find out if the item configured is used in calculating results
  - _set source_: set the item as source item to be analysed
  - _set target_: set the item as target item for the calculation process 
  - _add target_: add the item as target item for the calculation process
  - _clear target_: clear the item as target item for the calculation process
- **Default**: calculate the results and use the default views (indicated by the icon) to view the results.
- **Table**: calculate the results and present the results in a table view
- **Map**: calculate the results and present the results in a map view
- **Statistics**: calculate the results and present the results in a statistics window
