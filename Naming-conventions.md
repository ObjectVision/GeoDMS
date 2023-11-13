To make configurations clear (also for non-developers), maintainable and transferable, it is strongly recommended to follow the conventions for naming your [[tree items|tree item]]. Clear, meaningful and
unambiguous naming of items in your model is essential for understanding the logic and the maintainability of your model. Therefore we advice you to think well about your tree item names, don't use abstract variables names like: a, b, c or var1, var2, var3.

tree item names are [case insensitive](wikt:case_insensitive#English "wikilink") with regard to the normal A..Z letters from the alphabet. For other characters, like the Greek alphabet (allowed since version 7.315), names are case
sensitive. Use lowercases for keywords like [[container]], [[attribute]] and [[unit]].

The conventions are often not binding, the GeoDMS will not generate syntax errors if the conventions are not complied with. See [[tree item name]] for the allowed syntax for tree item names.

## general

- **Limit the length of tree item names:** 
    Use the label, description or url [[property]] for longer names and/or extra descriptions and prevent redundancy in [[naming|tree item name]] of tree items (see next dot).
- **Prevent redundancy in full tree item names:** An item is defined by it's full name, not only by the direct name in it's parent container. Don't absorb information in a name which is already part of a direct or indirect [[parent item]], see example:

<pre>
container InhabitantsPerRegion
{
    container Year1999
    { 
        attribute&lt;nrInhabitants&gt; InhGroningen1999 (region);
        attribute&lt;nrInhabitants&gt; InhFriesland1999 (region);
        attribute&lt;nrInhabitants&gt; InhDrenthe1999   (region);
    }
}
</pre>

In this example two types of redundancies occur:

1. The characters: Inh (number inhabitants) are part of each attribute name, but number of inhabitants is already the name of an indirect parent.
2. The year 1999 is repeated in each attribute name.

The preferable configuration is:

<pre>
container InhabitantsPerRegion
{
    container Year1999
    { 
        attribute&lt;nrInhabitants&gt; Groningen (region);
        attribute&lt;nrInhabitants&gt; Friesland (region);
        attribute&lt;nrInhabitants&gt; Drenthe   (region);
    }
}
</pre>

The full name of the second attribute is now: InhabitantsPerRegion/Year1999/Friesland. This name contains all relevant and no redundant information.

The exception to this rule is the case where tree item names would only consist of numeric characters. The next example illustrates this:

<pre>
container LandUse
{ 
   attribute&lt;ha&gt; lu1990 (Nl100mGrid);
   attribute&lt;ha&gt; lu2000 (Nl100mGrid);
   attribute&lt;ha&gt; lu2010 (Nl100mGrid);
}
</pre>

The abbreviation lu means land use and is as redundant with the parent container name. Tree item names 1990, 2000, 2010 are however no valid tree item names, this type of redundancy is acceptable.

- **Use the postfix: _rel** in naming [[relations|Index attribute]], e.g. region_rel as attribute name for the relation of a building to a region
- **Name objects singular**, e.g. airport in stead of airports and road in stead of roads
- **Use capitals for acronyms**, e.g. NIP (for Nederland in Plannen)
- **Use for abbreviation lower-case letters**, e.g. lu for land use 
- **Limit tree item names to the most relevant information**, e.g.: heather instead of heathland
- **In conjunctions, name first the generic followed by more specific information**, e.g.: agr_cattle, lu_labour_office. The information after the hyphen _ indicates a further specification or limiting condition. In case no alternatives are available, a liberal approach is allowed, e.g. wet_highpeatsoil or wet_peatsoil_high. As an alternative, the conjunction can also be composed with the first elements in lower-case letters and the next elements starting with a capital, e.g. luModel, luLumos
- **Combinations of concepts can be combined in one name**, e.g. SocCultural.
- **Don't use keywords like: attribute, parameter, container, template, unit for tree items names**.
- **Don't use [[composition type|composition]] names: poly, polygon, arc for domain units**. 
- **Don't use [[operators and functions|Operators and functions]] names for [[templates|template]]**.

## units

In a GeoDMS configuration three types of units are distinguished:

1. Quantity units for quantity [[data items|data item]] expressed in [[values units|values unit]] like meter or euro.
2. Class units for classified data (like soil types, region keys) or for data to be classified.
3. Geographical domains as municipality or NlGrid100m.

These different unit types are configured in different containers, often called Units, Classifications and Geography.

## quantity units

- **Configure quantity units in a units container**, preferably as one of the first configured containers.
- **Use as much as possible si units or monetary units as base units**. Derive other units as square meter from base units with expressions.
- **Use the following naming conventions for si units and factors**:

| *Quantity*       | *Unit*   | *Symbol* |
|------------------|----------|----------|
| Length           | meter    | m        |
| Mass             | kilogram | kg       |
| Time             | seconde  | s        |
| Elektric current | ampere   | A        |
| Temperature      | kelvin   | K        |
| Qauntity matery  | mol      | mol      |
| light intensity  | candela  | cd       |

| *Factor*         | *name* | *Symbol* |
|------------------|--------|----------|
| 10<sup>-24</sup> | yocto  | y        |
| 10<sup>-21</sup> | zepto  | z        |
| 10<sup>-18</sup> | atto   | a        |
| 10<sup>-15</sup> | femto  | f        |
| 10<sup>-12</sup> | pico   | p        |
| 10<sup>-9</sup>  | nano   | n        |
| 10<sup>-6</sup>  | micro  | Âµ (mi)  |
| 10<sup>-3</sup>  | milli  | m        |
| 10<sup>-2</sup>  | centi  | c        |
| 10<sup>-1</sup>  | deci   | d        |
| 10<sup>1</sup>   | deka   | da       |
| 10<sup>2</sup>   | hecto  | h        |
| 10<sup>3</sup>   | kilo   | k        |
| 10<sup>6</sup>   | Mega   | M        |
| 10<sup>9</sup>   | Giga   | G        |
| 10<sup>12</sup>  | Tera   | T        |
| 10<sup>15</sup>  | Peta   | P        |
| 10<sup>18</sup>  | Exa    | E        |
| 10<sup>21</sup>  | Zetta  | Z        |
| 10<sup>24</sup>  | Yotta  | Y        |

- **Use the prefix Nr for numbers**, like NrInhabitants, NrResidences.
- **Use the range property** to specify the range of floating point units, if known and different from the default range of the value type, see [[value type|value types]] for the range of the value types.
- **Use the NrOfRows property** to specify the range of integer units, if known.

## class units

- Configure class units in a **Classifications** container.
- Use the **NrOfRows** property to indicate the number or classes.
- As subitems of a class unit, the following attributes are often configured:

* A **ClassBreaks** item, with the class boundaries translated into class numbers. The DialogType property for this attribute need to be configured with the value: *Classification*. Example:

<pre>
attribute&lt;km&gt; ClassBreaks: DialogType = "Classification", [0,200,400,800]; 
</pre>

The [[values unit]] of this classes item (in the example km) should correspond with the values unit of the data to be classified. The first class gets the value 0, the second class a value 1 etc.

For classified data no classes item need to be configured (think e.g. about region keys).

* A **Label** item, with the labels used for the classes (in the legend of the map view and in the table view). The DialogType property for this attribute should be configure with as value:*LabelText*. Example:

<pre>
attribute&lt;string&gt; Label: DialogType = "LabelText",
    ['0 - 200','200 - 400','400 - 800','> 800']; 
</pre>

The values unit of this Label item is of the string values unit.

*  A set of possible style items for the visualisation of the classes in a map view. These are attributes configuring the [[visualisation styles|Visualisation Style]] for each class. Example:

<pre>
attribute&lt;uint32&gt; SymbolColor: DialogType = "SymbolColor",
   [rgb(128,255,0),rgb(0,128,0),rgb(0,64,128),rgb(255,0,0)]; 
</pre>

This item configures the used symbol colors for each class. See visualisation style for the possible style items.