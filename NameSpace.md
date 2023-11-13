[[Tree items|tree item]] often refer to other tree items, in defining [[units|unit]], [[expressions|expression]] or other [[properties|property]]. References to other items are configured by name. The referred name means the direct name of the
configured item and the relevant parent item names. Which [[parent items names|parent item]] need to be configured in a reference, is depending on the location of the tree item referring to the other items.

If an item is configured in the same context (same parent item) as it's referred items, the reference can be configured with only the direct name no parent item name has to be configured. This also
applies to the direct [[subitems|subitem]] of each of the parents of the item. For items in other branches parent items need to be added to your reference.

## search path

Within the GeoDMS items are found in a their search path. This path starts in the context of the configured item containing the reference. If the referred item is not found in this context, the parent level of this item is searched, until the root level is reached.

Items configured in side branches are not part of this search path. The GeoDMS needs to be informed about the side branches in which the requested items occur. This can be done by specifying this branche information in the name of the referred items. The relevant parent item names need to be added to the name of the
referred item, until the level of a parent item is reached which is part of the search path.

Another way of indicating that a branche, not being a part of the search path, need to be searched for items is by configuring the [[Using]] property,

## example

Assume the following configuration:

<pre>
container LandUseScanner
{
   container Units
   {
       unit&lt;float32&gt; m   := BaseUnit('m', float32) , label = "meter";
       unit&lt;float32&gt; km2 := 1000.0 * m             , label = "kilometer";
       unit&lt;float32&gt; s   := BaseUnit('s', float32) , label = "second";  
       container speed
       {
           unit&lt;float32&gt; m_per_s := m / s, label = "meter per second";
       }  
   }
   container Calc
   {
       parameter&lt;Units/m&gt; dist := 10[Units/m];
       container speed
       {
          parameter&lt;Units/m&gt;             dist    := 50[Units/m];
          parameter&lt;Units/s&gt;             period  :=  5[Units/s];
          parameter&lt;Units/speed/m_per_s&gt; speed50 := dist / Period;
          parameter&lt;Units/speed/m_per_s&gt; speed10 := Calc/dist / Period;
       }
    }
}
</pre>

This example contains multiple references. The following examples can be derived from this script to illustrate the NameSpace concept of the GeoDMS:

**1. Same Container**: The *km* (Kilometer) unit in the *Units* container contains a reference (within the expression) to the *m* (Meter) unit. The name used for this reference item is only *m*, as the *m* item is configured in the same container (Units) as the *km* item. This *m* item can be therefore be found and no parent item name is needed.

**2. Within Search Path**: The *m_per_s* (meter per second) unit in the *Units/speed* container contains references (within the expression) to both the *m* and *s* items. These m and s items are confiured in the *Units* container. As in example 1, only the direct tree item names are used in the reference, as the *Units* container is within the search path of the *m_per_s* item. The full search path for the *m_per_s* item
is:
- first the container *Calc* of the *Units* container
- second the container *Units*
- third the container *LandUseScanner*
The *m* item can be be found in the *Units* container and therefore no parent item name is needed.

**3. Parent name needed**: The item *dist* in the Calc container contains two references(in the definition of the values unit and in the expression) to the item *m* configured in the *Units* container. The full search path for this *dist* item is:
- first the container *Calc*
- second the container *LandUseScanner*
Within this search path the item *m* is not found. But in the container *LandUseScanner* the container *Units* can be found. So in this case the name of the direct parent (*Units*) needs to be added to the name of the referred tree item (*m*), resulting in the name: *Units/m.*

**4. Multiple parent names needed**: The items *speed50* and *speed10* in the *Calc/speed* container contains references(in the definition of the values unit) to the item *m_per_s* configured in the *Units/speed* container. The full search path for these items are:
- first the container *speed*of the *Calc* container
- second the container *Calc*
- third the container *LandUseScanner*
Within this search path the item *m_per_s* is not found. Also the item: *speed/m_per_s* cannot be found in this search parth. But in the container LandUseScanner the container *Units* can be found. So in this case the name of both parents (*Units/speed*) needs to be added to the name of the referred tree item (m), resulting in the name: *Units/speed/m_per_s*.

**5. Adding parent names can result in different outcomes**: The items *speed50* and *speed10* result in outcomes of respectively 10 and 2. Configuring the item *Calc/dist* or *dist* does matter in this case. This also relates to the search path for these items (see example 4). In the expression configured for the *speed50* item, the *dist* item is found in the same container (*speed*). This GeoDMS does not look any futher and uses this *dist* item (with as value 50 units/s). In the expression configured for the *speed10* item, the *Calc/dist* item is not an item name that can be found in the *speed* container. Buth the item name can be found in the parent container, *Calc* (with as value 10 units/m). So in the configured expression for these items, references are made to other items based on the parent names added.
