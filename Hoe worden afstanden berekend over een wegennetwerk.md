## maak een verbonden netwerk

Om afstanden te berekenen over een netwerk, moet dit verbonden zijn. Met
verbonden wordt bedoeld dat herkomst (vaak woningen) en
bestemmingslocaties (vaak voorzieningen) via een link aan een
wegennetwerk verbonden zijn.

De eerste stap in het berekenen van afstanden is daarom dat de herkomst
en bestemmingslocaties worden gekoppeld met het bestaande wegennetwerk,
geïllustreerd door het volgende voorbeeld :

[<File:Afs> netwerk1.jpg](File:Afs_netwerk1.jpg "wikilink")

*figuur 1: woonlokaties, voorzieningen, wegen en water*

In figuur 1 geven de zwarte huisjes de locaties van woningen aan. De
voorzieningen worden gepresteerd door rode vlaggetjes. De zwarte lijnen
tonen de wegsegmenten.

Voor iedere woning en voorziening wordt een verbinding gemaakt naar het
dichtstbijzijnde wegsegment, resulterend in het volgend figuur (zie de
[connect](connect "wikilink") functie):

[<File:Afs> netwerk2.jpg](File:Afs_netwerk2.jpg "wikilink")

*figuur 2: verbonden netwerk van woningen, voorzieningen en
wegsegmenten*

## bereken afstanden over een verbonden netwerk

Met dit verbonden netwerk kunnen afstanden worden berekend van ieder
punt naar ieder ander punt.

Afstanden worden berekend door de som van de afstanden van af te leggen
segmenten bij elkaar op te tellen, zie de volgende figuur:

[<File:Afs> netwerk3.jpg](File:Afs_netwerk3.jpg "wikilink")

*figuur 3: af te leggen segmenten voor de kortste route van bestemming A
naar voorziening B.*

De afstand van A naar B wordt berekend door de som van het aantal meters
te nemen van de af te leggen vijf wegsegmenten (zie de
[dijkstra](Dijkstra_functions "wikilink") functies)