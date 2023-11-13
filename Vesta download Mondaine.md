Deze manual is specifiek bedoeld voor het kunnen draaien van Vesta ten
behoeve van MONDAINE voor de Havenstad use case.

## Voorbereiding

### *Installeer GeoDMS*

1.  Download de meest recente versie van GeoDMS
    [hier](GeoDms_Setups "wikilink")
2.  Installeer de software
3.  Vink in Tools > Options alles aan zoals hier:

[<File:Website_InstallatieManual_Settings.png>](File:Website_InstallatieManual_Settings.png "wikilink")

-   Wil je niet dat GeoDMS de source data op C:/SourceData of local data
    op C:/LocalData zet/zoekt, open dan GeoDMS en ga naar Tools >
    Options en verander de paden onderaan:

[<File:Website_InstallatieManual_Paths.png>](File:Website_InstallatieManual_Paths.png "wikilink")

### *Vesta configuratie*

1.  Download de Vesta configuratie
    [hier](https://github.com/MaartenHilferink/VestaDV/archive/refs/heads/MONDAINE2.zip)
    of doe een git-clone van die repository [MONDAINE2
    branch](https://github.com/MaartenHilferink/VestaDV.git)
2.  Pak de zip uit en plaats het in je ProjDir-folder, bijvoorbeeld
    "C:/GeoDMS/ProjDir/VestaDV-MONDAINE"

### *Vesta source data*

1.  Download de Vesta source data
    [hier](https://surfdrive.surf.nl/files/index.php/s/Pi6T1Hw11MgVNCX)
2.  Pak de zip uit en plaats het in je SourceData-folder, bijvoorbeeld
    "C:/GeoDMS/SourceData/Vesta_SD41"

## Model run

1.  in GeoDMS GUI open een strategie, bijvoorbeeld:
    "C:/GeoDMS/ProjDir/VestaDV-MONDAINE/Runs/S0_Referentie.dms"
2.  Wil je de resultaten van een tijdstap zien? Navigeer dan naar:
    /LeidraadResultaten/Exports/ESDL/PerRekenstap/Res2030/PerStudyArea/Regio_selectie
    om de container met alle resultaten te zien
3.  Hier kun je dubbelklikken op een item om het te laten uitrekenen,
    bijvoorbeeld: "H02_Vraag_Ruimteverwarming"
4.  Wil je dit scenario voor alle tijdstappen voor het studiegebied per
    buurt naar csv wegschrijven? Dubbelklik dan op:
    /LeidraadResultaten/Exports/ESDL/Generate_PerPlanRegio_All

## Batch Model run

1.  Wil je de resultaten van alle strategieën in één keer naar csv
    wegschrijven? Open dan "batch_leidraad_ESDL.bat" in de map
    "C:/GeoDMS/ProjDir/VestaDV-MONDAINE/Runs" met een tekst editor.
2.  Hierin staan een parameters die je kunt instellen: studiegebied,
    flipstatus, en scenario. Tenzij je bepaalde buurten een strategie
    wilt opleggen en daar een invoertabel voor hebt opgegeven, zet dit
    op FALSE. Wil je een speciek vooraf bepaald scenario uitrekenen?
    Kies dan die, of selectieer 'LosseStrategieen'.
3.  Vervolgens zet je in de lijst met beschikbare strategieën, de
    strategieën aan die je wilt meenemen. Je zet een strategie aan,
    wanneer er geen 'REM' voorstaat.
4.  Gebeurt er niks? Wellicht heb je GeoDMS ergens anders geïnstalleerd
    of kloppen andere padverwijzingen niet. Pad dan het bestand
    "set.bat" aan in "C:/GeoDMS/ProjDir/VestaDV-MONDAINE/Runs/path". Als
    je de GeoDMS GUI al open hebt, werkt het ook niet. Sluit dan de GUI
    af en run de batch opnieuw.