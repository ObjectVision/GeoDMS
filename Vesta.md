Vesta Begrippen

Het ruimtelijk energiemodel Vesta berekent het energiegebruik en de
CO2-uitstoot van de gebouwde omgeving (onder andere woningen, kantoren,
winkels en ziekenhuizen) en de glastuinbouw voor de periode van 2010 tot
2050. Daarnaast kan het model de effecten berekenen van
gebouwmaatregelen en gebiedsmaatregelen voor warmtelevering in termen
van vermeden CO2-uitstoot, energiegebruik, investeringskosten en
financiële opbrengsten.

Deze pagina beschrijft begrippen die centraal staan in de model
structuur van Vesta, het conceptuele model

# indices

-   Vraag categorie
    *c* := {Ruimteverwarming, Tapwater, Elektrische Apparatuur}.
-   Zichtjaar *t* := {2010, 2020, 2030, 2040, 2050}
-   Planregio *r*: inidiceert de verschillende planregio's. Dit kunnen
    PC4 gebieden zijn, of een raster van 500m, 1km, of 2km.

# bebouwingscomponenten

Bebouwingscomponenten zijn verzamelingen bebouwingsobjecten die als een
soort tabel verwerkt worden. Beschikbare bebouwingscomponenten zijn:

-   Woningen obv GMP (2004..2006) met 1 record per PC6
-   Woningen obv de BAG (2015) met 1 record per verblijfsobject met
    woonfunctie
-   Woningen worden gesloopt obv Wlo2Energy resultaten.
-   Nieuwbouw woningen obv Wlo2Energy resultaten.
-   Utiliteiten obv LISA.
-   Utiliteiten obv de BAG (2015) met 1 record per verblijfsobject met
    gebruiksfuncties niet zijnde woonfunctie en niet alleen industrie.
-   Utiliteiten worden gesloopt obv Wlo2Energy resultaten.
-   Nieuwbouw utiliteiten obv Wlo2Energy resultaten (nu alleen in uitleg
    lokaties, positieve delta's binnen bestaand bebouwd gebied worden
    niet verwerkt).
-   Glastuinbouw
-   Af/Bij kaarten voor glastuinbouw

Een bebouwingscomponent moet een vast aantal kenmerken hebben om
verwerkt te worden, zie onder.

# ruimtelijke vraag

Naast opgaven van KengetalTypeDomein, BouwjaarDomein, SubtypeDomein
moeten ook values eenheden worden gedefinieerd:

-   JoinedUnit, de quantity waarmee de grootte van eenbebouwingsobject
    wordt gedefinieerd. Dit is bijvoorbeeld voor Lisa Utiliteiten het
    aantal medewerkers.
-   ModelUnit

In de modelobjecten wordt het aantal c gegeven per model-object. Voor
Lisa Utiliteiten is dit het vloergebruik per medewerker, Voor de overige
Bebouwingscomponenten is deze factor 1 aangezien daar Modelunit en
JoinedUnit samenvallen (per m2 of per woning).

# modelobjecten

Per modelobject wordt een energielabel aangenomen en daarbij passende
functionele vraagkenmerken gegeven. Tevens wordt per modeloject
aangegeven wat de mogelijke gebouwverbeteringen (labelsprongen) kosten
en opleveren (gebouwefficiency coefficient tov het aangenomen label).

# ruimtelijke vraag

Ruimtelijke vraag ontstaat door Bebouwingsobjecten te relateren aan
modelobjecten. Voor de functionele vraag wordt eerst gekeken of uit de
energielabel registratie al een label voor het object bekend is en op
basis daarvan wordt er eventueel aangenomen dat er al een labelsprong
heeft plaatsgevonden, waarvan de kosten irrelevant zijn en het reeds
behaalde efficiencyvoordeel de functionele vraag beperkt. Tevens wordt
een correctiefactor toegepast obv het klimatogische koude kaarten.

# gebouwopties

Gebouwopties zijn mogelijke verbeteringen van qua isolatie of opwekking
per bebouwingsobject

# gebiedsopties

-   Restwarmte: kentens van planregio's
-   Geothermie: 1 planregio
-   WijkWKK / BMC: 1 planregio
-   WKO: clusteres van individuele bebouwingsobjecten,

# BuildingComponent

A Building Component is a `unit`<uint32> that has the following
sub-items (attrbutes without specified domain have the BuildingComponent
as domain):

    unit<uint8>  KengetalTypeDomein:= Classifications/WoningtypeGeoHoogte;
    unit<uint8>  BouwjaarDomein := Classifications/
    unit<uint8>  SubTypeDomein := combine(KengetalTypeDomein, BouwjaarDomein); // TODO: rename to ModelObject_rel

    unit<uint32> BebouwingsObject:= .. // selfref. TODO: should be removed
    {
        attribute<string> code;  // used to link building objects of subsequent years; TODO: should be removed as an object should only have one history
        attribute<string> Label; // used in TableViews of these building objects
        attribute<String> PC6code; // OBSOLETE ?

        // the following attributes are only used if the related PlanRegio has been selected.
        attribute<RegioIndelingen/PC4>                PC4_rel;
        attribute<RegioIndelingen/grid500m>           grid500m_rel;
        attribute<RegioIndelingen/grid1km>            grid1km_rel;
        attribute<RegioIndelingen/grid2km>            grid2km_rel;

        attribute<RegioIndelingen/Socrates>           Socrates_nr; // ??? OBSOLETE ? Or available as RapRegion partitioning

        // attributes used by VestaRunData to calculate demand characteristics
        attribute<nrAansl>                            nrAansluitingen;
        attribute<nrWoningen>                         nrJoinedUnits; // could be number of residences or labour positions, a unit for demand
        attribute<Float64>                            AandeelWarmteVraag; // between 0 and 1.
        attribute<Classifications/GeenOnderverdeling> GeenOnderverdeling := const(0,.,Classifications/GeenOnderverdeling);
        attribute<KengetalTypeDomein>                 KengetalType       := Impl/WoningType;
        attribute<UInt8>                              SubType            := value(KengetalType *  UInt8(#(Classifications/BouwJaarBAG)) + Impl/BouwjaarBag_rel, classifications/WoningTypeBouwjaarBAG);
        attribute<BJ>                                 BouwJaar;

        // location, required for some area options
        attribute<rdc_meter> point

        // additional geographic extent characteristics, required by the WKO clustering
        container Gebied
        {
            attribute<Float64> n       := const(1.0, ..);
            attribute<Float64> MEAN_x  := Float64(PointCol(point));
            attribute<Float64> MEAN_y  := Float64(PointRow(point));
            attribute<Float64> SSD_xx  := const(0.0, ..);
            attribute<Float64> SSD_xy  := const(0.0, ..);
            attribute<Float64> SSD_yy  := const(0.0, ..);
        }

        // attributes that relate to observed energy labels that can overrule the label that is otherwise assumed for each model type.
        attribute<Classifications/energielabel> EnergieLabelData_rel;
        {
            attribute<uint32>                       totaal    (BagWoning):= uint32(isDefined(EnergieLabelData_rel));
            attribute<uint32>                       TotWeight (BagWoning):= const(0,BagWoning);//"1 * aantal_ap + 2 * aantal_a + 3 * aantal_b + 4 * aantal_c + 5 * aantal_d + 6 * aantal_e + 7 * aantal_f + 8 * aantal_g";
            attribute<Classifications/energielabel> AvgLabel  (BagWoning):= EnergieLabelData_rel;
        }

        // optional parameters, used only for certain BestaandeWoningRapGroep options
        attribute<Classifications/eigendom_woning  > Eigendom; // wordt gebruikt bij BestaandeWoningRapGroep [ 4 ]
        attribute<Classifications/InkomensKlasse>    Inkomen;  // wordt gebruikt bij BestaandeWoningRapGroep [ 8 ]
    }

# links

-   Planbureau voor de leefomgeving
    -   [Vesta ruimtelijk energiemodel voor de gebouwde omgeving, PBL
        2012](http://www.pbl.nl/publicaties/2012/vesta-ruimtelijk-energiemodel-voor-de-gebouwde-omgeving)
    -   [Naar een duurzamere warmtevoorziening van de gebouwe omgeving
        in 2015, PBL
        2012](http://www.pbl.nl/publicaties/2012/naar-een-duurzamere-warmtevoorziening-van-de-gebouwde-omgeving-in-2050)
        -   [Rapport](http://www.pbl.nl/sites/default/files/cms/publicaties/PBL-2012-Duurzamere%20warmtevoorziening-500264002.pdf)
        -   [Interactieve
            kaart](http://geoservice.pbl.nl/website/flexviewer/index.html?config=cfg/PBL_Energie.xml)
    -   [Functioneel Ontwerp, CE Delft zomer
        2014](http://www.pbl.nl/sites/default/files/cms/publicaties/pbl-2014-ce-delft-functioneel-ontwerp-vesta-2.0.pdf)
-   [Object Vision product
    page](http://www.objectvision.nl/gallery/products/vesta)

<!-- -->

-   [Vesta_download_Mondaine](Vesta_download_Mondaine "wikilink")