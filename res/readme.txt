Readme bestand voor de Dms Software
Augustus 2008 - geschreven voor Dms versie 5.40

MINIMALE SYSTEEM CONFIGURATIE
- Pentium 3 processor (geadviseerd Pentium Dual-Core)
- 512 Mb intern geheugen (1gb geadviseerd voor 100m discrete allocatie)
- 20 GigaByte vrije schuifruimte 
- Windows 2000, XP of Vista

DIRECTORY STRUCTUUR
- The executable (DMSCLient.exe) en de DMS dll's worden geinstalleerd in de programma directory, 
  die bij de installatie wordt opgegeven (default C:\Program Files\Object Vision\DmsXXX).
- overige componenten worden geplaatst in <ProgramFiles>/ipp20; <ProgramFiles>\Common Files\ESRI en de <SystemRoot> directories.
- de project .zip file moet worden uitgepakt in een zelf te kiezen project directory, bijvoorbeeld C:\projects\nllater,
  waarmee de volgende onderdelen van het project worden uitgepakt naar subdirectories van deze project directory:
  - configuratiebestanden (*.dms) naar de cfg subdirectory 
  - primaire data (*.asc, *.tif, *.mdb, *. shp, *.shx en *. dbf) bestanden naar de data subdirectory
  - documenten (*.doc) naar de doc subdirectory
  - project specifieke dll (met logo, splash screen en about box) in de bin subdirectory  

DmsClient.exe STARTEN
Bij de eerste keer starten geeft de DmsClient aan dat de configuratie niet kan worden gevonden.
Ga vanuit de gepresenteerde FileOpen dialoog naar de cfg subdirectory van de zelf gekozen project directory.
Selecteer hierin het xxx.dms configuratiebestand. De DmsClient zal de lokatie van deze 
configuratie nu bewaren in <ProgramDir>/dms.ini, opdat deze een volgende keer direct gevonden kan worden.

DiscCompressie aan zetten
Het is aan te raden om, in verband met eventuele asciigrid data, de project data
compressed te bewaren op de harddisk; dit is mogelijk met NTFS file systeem (dus niet FAT).
De volgende stappen zijn nodig om de project directory compressed op te slaan:
- Selecteer vanuit de Verkenner de project directory
- Activeer met de rechtermuisknop de Properties/Eigenschappen dialoog
- Klik in het tabblad General/Algemeen op de knop Advanced/Geavanceerd en vink de optie 
  Compress Contents to Save Disk aan.

  