_[[User Guide GeoDMS GUI]]_ - classification and palette editor

## edit classes and or colors

[[images/GUI/paletteeditor.png]]<br>

The (number of) classes, the type of classification and the colors used for each class can be edited with the classification and palette editor. 

Edits made with this editor are only is use for the current session. If you want to save the edits, you need to add or edit the relevant [[configuration file]], see the section [[classification]].  

The editor is activated from the [[Map View Legend]], with the pop-up menu option: Edit Palette on a layer with a classification.

## columns

- **BrushColor**: a column showing the colors used as [[BrushColor|Polygon visualisation]]. Individual colors can be edited by double-clicking on a specific color cell. A Windows color dialogue appears. The pop-up menu option for this column contains (additional to the generic applicable [[table|Table View]] column menu options) the following options:
   - **Remove BrushColor**: remove the column from the view.
   - **ramp Colors**: ramp the colors between the start and and color or between the start and an intermediate color and between an intermediate color and the end color. To set an intermediate color, select a cell in the column, choose a color and keep the cell highlighted.
   - **Change Brush/Pen Color**: select a color from a palette editor or set the color to fully transparent for the brush or the pen.
   - **Classify _data item_**: classify the original data item, see the class’s column.
   - **Classes** : split and merge to split the current class or merge two or more existing classes.

- **LabelText**: individual labels can be edited by activating the cell and typing a new label or by pressing the F2 function key and editing the current label. The pop-up menu option for this column contains (additional to the generic applicable [[table|Table View]] column menu options) the following options:
   - **Remove LabelText**: remove the column from the view.
   - **Classify _data item_**: classify the original data item, see the class’s column.
   - **Classes** : split and merge to split the current class or merge two or more existing classes.
   - **ReLabel**: relabel the contents of the label column with derived labels from the class boundaries

- **classBreaks**: individual classes can be edited by activating the cell and typing a new class value or by pressing the F2 function key and editing the current class value. The pop-up menu option for this column contains (additional to the generic applicable [[table|Table View]] column menu options) the following options:
   - **Remove ClassBreaks**: remove the column from the view
   - **Classify _data item_**. reclassify the original data item. First, specify the number of requested classes in the class’s text box. Next, select one of the types of classifications that are available:
      - **Unique values** (available for all [[data items|data item]] with a number of unique occurrences that fits in the range of the [[domain unit]]): a class is added for each value that occurs in the data item. The number of classes is based on the number of unique occurrences. The maximum number of classes is dependent on the domain unit of the class unit. 
      - **Equal counts non-zero** (available for all data items): see Equal count with as difference that the value 0 is treated separately. 
      - **Equal counts** (available for all data items): the occurring data items are split up into classes with, as far as possible, an equal count of occurrences in each class. The class breaks are first set to the number of sorted data elements (excluding the no data values) divided by the number of requested classes. Each class break is checked to determine whether the next data element has the same value as the data element at the break. If this is the case, the class break is increased to the next data element, until a data element is found with a new value. If this data element is not found before 
the next class break, the last class break is removed and the number of classes is decreased by one class.
      - **Equal intervals non-zero** (available for all data items): see Equal intervals with as difference that the value 0 is treated separately. 
      - **Equal intervals** (available for all data items): the range of the data in a data item, or the specified range of a values unit, is split up in intervals of the same size. If no range is specified the default range for the value type of the unit is used. 
      - **JenksFisher non-zero**: see JenksFisher with as difference that the value 0 is treated separately. 
      - **JenksFisher** : applies the [[JenksFisher classification method|Fisher's Natural Breaks Classification complexity proof]] to minimize the variance within each class.
      - **Logarithmic** intervals (available for all items with a minimum value of zero): the range of the data in a data item or the specified range of  values unit is split up in intervals of the same size using a logarithmic distribution. If no range is specified the default range for the value type of the unit is used.
   - **Classes** : split and merge to split the current class or merge two or more existing classes.

- **Count**: a dervied column, showing the number of elements in each class. The values can not be edited. The pop-up menu option for this column contains (additional to the generic applicable [[table|Table View]] column menu options) the following options:
   - **Remove count**: remove the column from the view.
   - **Ramp values**: option is disabled.
   - **Classify _data item_**: classify the original data item, see the class’s column.
   - **Classes** : split and merge to split the current class or merge two or more existing classes.

