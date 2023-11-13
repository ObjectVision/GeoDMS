## introductie

Kaarten zijn een nuttig instrument om inzicht te krijgen in de
ruimtelijke verdeling van doelgroepen.

Wij hebben een methode ontwikkeld om met behulp van patronen een helder
en gedetailleerd beeld te krijgen van deze verdeling. De methode wordt
o.a. gebruikt in de [WoonZorgwijzer](https://www.woonzorgwijzer.info) en
in veel demografische kaarten die we maken.

Deze pagina beschrijft het hoe en waarom van deze methode.

## fysiek versus sociaal domein

Kaarten worden traditioneel veel gebruikt in het fysieke domein om
objecten weer te geven, zie bijvoorbeeld:

[300px](File:FysiekDomeinBagBRK.png "wikilink") *[BAG](BAG "wikilink")
panden en percelen uit de [BRK](BRK "wikilink")*

Objecten hebben meestal een vaste locatie (denk aan gebouwen, percelen
etc.). Het vastleggen van de exacte ligging is vaak belangrijk,
bijvoorbeeld omdat ze een eigendomssituatie beschrijven.

In het [sociale domein](Social_Domain "wikilink") draait het niet om
objecten, maar om subjecten (personen). Belangrijke verschillen tussen
objecten en subjecten bij het maken van kaarten zijn:

1.  Subjecten hebben geen vaste locatie. Ze hebben vaak wel een
    woonadres, maar verplaatsen zich ook naar bijvoorbeeld school, werk,
    winkel etc. Het concept van een leefomgeving past beter bij een
    subject dan één specifieke locatie.
2.  Subjecten hebben het recht op privacy. Het adres van een persoon is
    een persoonsgegeven dat in Nederland door de
    [AVG](https://autoriteitpersoonsgegevens.nl/nl/over-privacy/wetten/algemene-verordening-gegevensbescherming-avg)
    beschermd wordt en niet zomaar met een ieder gedeeld kan worden.

Subjecten op een specifieke locatie op een kaart zetten (zoals objecten)
is vanwege verschil 1 minder logisch en vanwege verschil 2 ook meestal
ongewenst.

Een extra nadeel is dat er vaak meer personen in een
huis/appartementsgebouw wonen. Het weergeven hiervan op hun locatie,
resulteert in veel overlap en niet in een helder beeld van het voorkomen
van bepaalde groepen.

## administratieve grenzen

Een veel gebruikte oplossing voor de hierboven geschetste issues met
subjecten is om gebruik te maken van (administratieve) indelingen.

Informatie over personen wordt geaggregeerd naar postcodegebieden,
buurten, wijken, gemeentes etc. Op het gebiedsniveau worden kaarten
gemaakt zoals in het volgende voorbeeld:

[\|300px](File:Administratief.png "wikilink") a*antal inwoners per CBS
buurt in de gemeente Leiden*

Het werken met administratieve indelingen heeft een aantal voordelen:

-   Het levert een eenvoudig te begrijpen kaartbeeld op
-   Je kunt data eenvoudig uitwisselen en combineren met andere bronnen

Helaas kennen deze visualisaties ook een aantal nadelen:

-   **gebiedsindeling:** de gebiedsindeling kan het beeld sterk
    beïnvloeden, zeker als de data niet homogeen verspreid is over een
    gebied. Of een verpleeghuis net aan de ene of aan de andere kant van
    een buurtgrens ligt, zal het beeld van de spreiding van 75 plussers
    op buurtniveau sterk kunnen beïnvloeden
-   **kleine gebieden:** als de indeling veel kleine gebieden betreft
    (denk aan PC6 gebieden) dan zal je voor doelgroepen vaak te weinig
    waarnemingen hebben in een gebied om, rekening houdend met privacy
    overwegingen, resultaten voor deze gebieden te tonen. Dat levert
    kaarten op met veel missing data.
-   **grote gebieden:** als de indeling grotere gebieden betreft (denk
    aan wijken/gemeentes), is het risico op missing data kleiner, maar
    is de kans groter op uitmiddelen. Eén wijk kan bijvoorbeeld bestaan
    uit hoogbouw en een groot buitengebied, in een andere wijk heb je
    een veel homogenere spreiding van bebouwing. Het aantal inwoners kan
    dan vergelijkbaar zijn, maar de spreiding binnen de wijk varieert
    wel sterk.
-   **omvang van de gebieden:** om administratieve gebieden voor
    doelgroep analyses vergelijkbaar te maken, wordt er vaak voor
    gekozen de omvang van gebieden in termen van aantal inwoners
    ongeveer gelijk te houden. Dat betekent dat gebieden in de
    binnenstad (waar veel mensen dichtbij elkaar wonen) vaak in
    oppervlakte veel kleiner zijn dan gebieden in het buitengebied. Op
    een kaart overheersen daardoor vaak de buitengebieden, terwijl veel
    sociale problematiek zich juist in de kwa oppervlakte kleine
    gebieden in de binnenstad afspeelt.
-   **geen relatie met 'buurtconcept' van de bewoner:** voor een
    individuele bewoner is het idee over zijn buurt meestal de eigen
    woning met een gebied eromheen, waarin hij zich vaak begeeft.
    Voorzieningen liggen in de buurt als ze voor de bewoner in zijn
    'buurtconcept' liggen, niet als ze in een administratief vastgelegde
    buurt liggen. De meeste bewoners zullen vaak niet eens weten waar
    administratieve buurt- en wijkengrenzen liggen.

Om deze problemen te ondervangen en patronen te laten zien van
doelgroepen, onafhankelijk van administratieve indelingen (die voor
andere doelen bepaald zijn), hebben we een alternatieve visualisatie
techniek ontwikkeld

## patronen

Voor het maken van patroonkaarten gebruiken we brondata op adres of pc6
niveau zijn. Deze data zal in het proces verwerkt worden van een
persoonsgegeven tot een gebiedsgegeven (niet meer herleidbaar naar een
individueel persoon).

Omdat de brondata dus wel persoonsgegevens betreft, is het belangrijk
dat voor de verwerking hiervoor wordt voldaan aan de AVG, zie ons
privacy protocol.

De volgende stappen worden doorlopen om tot patroonkaarten te komen:

#### 1) Het maken van een [Herkomst/bestemmingsmatrix](Origin_Destination_matrix_(trip_table) "wikilink") en het aggregeren naar nabije locaties

[<File:Pc4topc4.png>](File:Pc4topc4.png "wikilink")

Op het schaalniveau waarop data wordt aangeleverd, maken we een
[herkomst/bestemmingsmatrix](Origin_Destination_matrix_(trip_table) "wikilink"),
waarin we de afstanden berekenen van iedere herkomst adres/postcode naar
alle andere bestemming adressen/postcodes tot aan bijvoorbeeld 100 of
300 meter. Dit resulteert voor iedere herkomst locatie in een set van
bestemming locaties die 'in de buurt' liggen. Per herkomst locatie
tellen we nu het aantal personen op de locatie zelf en van de set
bestemmingslocaties die 'in de buurt' liggen (eventueel met een gewicht
dat afneemt als de afstanden groter worden).

Net als bij de administratieve indelingen, aggregeren we dus aantallen
naar een buurtconcept. Het verschil met de administratieve indeling is
dat de buurt in deze methodiek verschilt per herkomst locatie. Een buurt
in deze methodiek bestaat hier uit alle andere locaties op een afstand
van bijvoorbeeld 100 of 300 meter vanaf de herkomst locatie. Geografisch
overlappen deze buurten, je kunt de totalen niet zomaar bij elkaar
optellen.

[300px](File:Woonomgeving.png "wikilink")

Met dit buurt concept, implementeren we het idee van de woonomgeving.
Personen bewegen zich in een omgeving, waarbij de kans dat je je in of
nabij je je huis begeeft, groter is dan verder weg van je huis.

Belangrijk hierbij is ook hoe je het begrip verder weg
operationaliseert. Stel dat je aan de rand van een kanaal woont, dan kan
de afstand hemelsbreed naar de overkant van het kanaal beperkt zijn,
maar als je het kanaal niet kan oversteken zal dit een minder belangrijk
onderdeel uitmaken van je woonomgeving. Daarom operationaliseren het
begrip ver weg door te kijken naar feitelijke loopafstanden en berekenen
we afstanden in de
[herkomst/bestemmingsmatrix](Origin_Destination_matrix_(trip_table) "wikilink")
over de weg.

Het resultaat van deze aggregatie bewerking is een maat voor het
voorkomen van doelgroepen op een locatie en in zijn feitelijke
nabijheid. Dit resultaat is, mits de aantallen locaties in de buurt
groot genoeg zijn, niet meer herleidbaar naar individuele personen.

Voor de [WoonZorgwijzer](https://www.woonzorgwijzer.info) zijn de
resultaten van het model op postcode6 niveau berekend en met een
[herkomst/bestemmingsmatrix](Origin_Destination_matrix_(trip_table) "wikilink")
(maximale afstand van 300 meter) conform deze methodiek geaggregeerd
(ook wel uitgesmeerd genoemd).

Het resultaat is door het CBS op privacy getoetst en akkoord bevonden om
te gebruiken in openbare websites.

#### 2) het vertalen van uitgesmeerde resultaten naar een ruimtelijk patroon

##### 2.1 koppeling aan BAG panden

Het resultaat van stap 1 vertalen we naar een ruimtelijk patroon met
behulp van de [BAG.](BAG "wikilink")

-   Als de brondata op adresniveau beschikbaar is, koppelen we deze
    m.b.v. postcode en huisnummer aan nummeraanduidingen in de
    [BAG](BAG "wikilink"). Via de nummeraanduiding wordt een relatie
    gelegd met een [BAG](BAG "wikilink") verblijfsobject en vervolgens
    geaggregeerd naar het [BAG](BAG "wikilink") pand waar de
    verblijfsobject in gelegen zijn.
-   Als de brondata op pc6 niveau beschikbaar is, koppelen we deze aan
    de nummeraanduidingen in de [BAG](BAG "wikilink") met deze
    postcode. Net als bij het adresniveau aggregeren we deze waarden via
    het verblijfsobject naar het [BAG](BAG "wikilink") pand niveau.

Hiermee wordt het resultaat van stap 1 gekoppeld is aan panden uit de
[BAG](BAG "wikilink"), zie bijvoorbeeld de verdeling van hen het aantal
verwachte personen met een somatische aandoening (bron:
[WoonZorgwijzer](https://www.woonzorgwijzer.info)):

[400px\|border](File:Patroon_bag_pc6.png "wikilink") *verdeling van het
aantal verwachte personen met een somatische aandoening geprojecteerd
m.b.v. de BAG pand geometrie, gerelateerd via postcode.*

Deze weergave op pand niveau geeft, zeker als je verder uitzoomt, geen
helder beeld van de verdeling van de doelgroepen. Daarvoor bewerken we
deze data in de volgende stappen tot schaal afhankelijke patroonkaarten.

##### 2.2 van BAG panden naar gridcellen

De pand geometrieën vertalen we naar kleine gridcellen (voor de
[WoonZorgwijzer](https://www.woonzorgwijzer.info) gebruiken we cellen
van 5 \* 5 meter, in lokale toepassingen inmiddels ook 2 \* 2 meter),
zie de volgende figuur:

[400px\|border](File:patroon_gridcell.png "wikilink") *pand geometrie
vertaald naar 5 \* 5 meter cellen met de verdeling zoals bij 2.1*

##### 2.3 uitsmeren van BAG panden tot een gebiedspatroon

Vanuit deze kleine gridcellen bereken we nu een gebiedspatroon, gebruik
makend van een vorm van ruimtelijke
[interpolatie](https://nl.wikipedia.org/wiki/Interpolatie).

In de [GeoDMS](GeoDMS "wikilink") maken we hiervoor gebruik van de
[potential](potential "wikilink") functie. Deze bewerking is
gerelateerd aan het begrip dat in de GIS wereld
[heatmap](wikipedia:Heat_map "wikilink") wordt genoemd.

Zie de figuur voor een voorbeeld van het resultaat:

[400px\|border](File:patroon_pot.png "wikilink") *patroon voor een
groter gebied gebaseerd op kleine cellen op basis van pand geometrieën*

De resulterende patronen aggregeren we ook naar grids met grotere cellen
(voor de [WoonZorgwijzer](https://www.woonzorgwijzer.info) 10 \* 10 en
20 \* 20 meter), bedoeld voor visualisaties in uitgezoomde kaarten.

##### 2.4 uitfilteren kleine gebieden en infrastructuur

In de laatste stap filteren we

-   kleine outliers uit de kaarten, vaak het resultaat van afrondingen.
-   infrastructuur elementen zoals water en hoofdwegen. Deze filtering
    is schaal afhankelijk. Uitgezoomd worden alleen grotere wateren uit
    de kaarten gefilterd, verder ingezoomd ook kleinere wateren,
    hoofdwegen e.d. Deze filtering voeren we uit omdat het niet logisch
    is om iets te zeggen over de verdeling van doelgroepen in
    bijvoorbeeld een meer of op een snelweg. Bijkomend voordeel is dat
    door deze filtering de referentie van kaarten beter wordt.
    Infrastructuur elementen zijn in het snel zien voor de ruimtelijke
    interpretatie. Door deze uit de gridkaarten te halen, worden ze via
    de gebruikte achtergrondlagen in de visualisatie beter zichtbaar
    (vaak zelfs met labels), zie het volgende figuur:

[400px\|border](File:patroon_result.png "wikilink") *resulterende
schaalafhankelijke patronen, waarin outliers en infrastructuur elementen
zijn uitgefilterd*

Het resultaat van de stappen 1 en 2.1 t/m 2.4 is een set van
schaalafhankelijke gridkaarten. Deze visualisaties nemen veel van de
nadelen van de administratieve indelingen weg:

1\) Omdat er geen gebruik gemaakt wordt van een vaste administratieve
indeling, kan zo'n indeling ook niet het beeld bepalen. Het ruimtelijk
patroon wordt volledig bepaald door het patroon in de data.

2\) Als een doelgroep relatief weinig voorkomt, kan in stap 1 eenvoudig
de afstandsmaat vergoot worden, om toch voldoende locaties te hebben om
het niet herleidbaar zijn naar personen te genereren.

Deze methodiek is geschikt voor een heldere visualisatie van de
ruimtelijke verdeling van doelgroepen (dus waar komen bepaalde groepen
relatief veel voor).

De methode is minder geschikt voor het bepalen of uitwisselen van
aantallen, omdat daarvoor een duidelijke gebiedsafbakening noodzakelijk
is.