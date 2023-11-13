Deze manual is specifiek bedoeld voor het kunnen draaien van Vesta ten
behoeve van MONDAINE voor de Havenstad use case.

## benodigdheden

1.  Installeer GeoDMS
2.  Download Vesta configuratie
3.  Download Vesta source data

### *installeer GeoDMS*

-   Download de meest recente versie van GeoDMS
    [hier](GeoDms_Setups "wikilink")
-   Installeer de software
-   In Tools > Options vink alles aan zoals hier:

[<File:Website> InstallatieManual
Settings.png](File:Website_InstallatieManual_Settings.png "wikilink")

-   Wil je niet dat GeoDMS de source data op C:/SourceData of local data
    op C:/LocalData zet/zoekt, open dan GeoDMS en ga naar Tools >
    Options en verander de paden onderaan:

[<File:Website_InstallatieManual_Paths.png>](File:Website_InstallatieManual_Paths.png "wikilink")

### *Vesta configuratie*

1.  Download de Vesta configuratie
    [hier](https://github.com/RuudvandenWijngaart/VestaDV/archive/GeneralisatieConversie.zip)
2.  Pak de zip uit en plaats het in je ProjDir-folder, bijvoorbeeld
    C:/GeoDMS/ProjDir/VestaDV-GeneralisatieConversie

### *Vesta source data*

1.  Download de Vesta source data
    [hier](https://surfdrive.surf.nl/files/index.php/s/m4A8krP02PJTVe6)
2.  Pak de zip uit en plaats het in je SourceData-folder, bijvoorbeeld
    C:/GeoDMS/SourceData/Vesta_SD41

## vesta model run

1.  Open GeoDMS GUI
2.  File > Open configuration file >
    C:\\GeoDMS\\ProjDir\\VestaDV-GeneralisatieConversie\\Runs\\Havenstad.dms
3.  In (dit nog nader te bepalen pad) staan resultaten

<!-- -->

1.  dit item exporteert de resultaten die in python ingelezen kunnen
    worden om ESDL van te maken.