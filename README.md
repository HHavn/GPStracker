![Build Status](https://github.com/HHavn/GPStracker/actions/workflows/build.yml/badge.svg)

GPS Tracker Brugsanvisning.
GPS tracker. Automatisk start og stop af tracking. Output er KML filer, der læses af Google Earth.
Tracker kan styres via SMS. Status og position kan læses via SMS.
Mulighed for deep sleep, så batteri levetiden kan udnyttes maximalt.


Filstruktur.
Sketchen er opdelt i flere .ino filer efter ansvarsområde. Arduino samler automatisk
alle .ino filer i samme mappe til én sketch, så filerne deler globale variabler uden
videre.

GPStracker.ino	Includes, defines, globale variabler, setup() og loop().
Utils.ino	Generelle hjælpefunktioner: Print, Println, MapFloat, split.
Power.ino	Deep sleep, batteri-måling, GPS sleep/wake.
Modem.ino	GSM modem start/stop og status.
Sms.ino		Modtagelse/afsendelse af SMS og kommando-dispatch.
Config.ino	Læsning/skrivning af Config.cfg og SMS getparam/setparam.
SdStorage.ino	SD-kort info, daglig logfil og KML-logger, lavniveau filfunktioner.
Gps.ino		GPS-task, position/tid-parsing, sommertid (DST).
utilities.h	Board pin-definitioner.
Config.cfg	Konfigurationsfil på SD-kortet (se afsnittet "SD kort" nedenfor).


Legale kommando (Der skelnes ikke mellem store og små bogstaver):
Getstat, getstatus, gst
Getpos, getposition, gpo
Getparam, gpa
Setparam, spa
Seton, son		Sætter ON_OFF_PIN (pin 32) høj.
Setoff, soff		Sætter ON_OFF_PIN (pin 32) lav.

Setparam parameter.
radius=N		N er radius i meter. Radius for bevægelse detektion. (5..100)
loginterval=N	N er tid i sekunder mellem hver log. (1..200)
deepsleep=N		N er tid i timer. (0..48)
logenable=true/false	True: logging vil starte når radius overskrides.	
debug=0		0: disabled. 1: der sendes debug SMS løbende. 2: der sendes minimal debug info.

Ved Setparam placeres data delen i en ny linie.
F.eks.:
Setparam
radius=12

Ved Setparam gemmes parameter data i konfig fil på SD kortet (Config.cfg).

Når log er enablet, vil logning starte ved bevægelse ud over radius.
En aktiv log vil stoppe, når der ikke er registreret bevægelse i 5 minutter. (Enheden bliver inden for radius i 5 minutter).
En aktiv logning vil stoppe, hvis GPS signal mistes i mere end 60 sekunder.


DeepSleep.
Når deepsleep parameter er over nul, vil enheden gå i deep sleep efter 5 minutters inaktivitet.
Ved deep sleep, stoppes GSM, GPS sættes i sleep og CPU sættes i sleep. Der bruges minimal strøm.
Når deep sleep tiden udløber, vækkes CPU og systemet booter.


SD kort.
Filen Config.cfg er setparam filen.
Indhold:
# GPS Tracker Config File.  !! No empty lines !!
NetworkAPN=internet
tlf=+4540156239
radius=50
loginterval=10
deepsleep=1
logenable=true
debug=true

Der produceres en daglig aktivitet fil.
Filnavn: 20250822 (YYYYMMDD)
Filen viser system boot og track aktivitet.


KML filer.
Når der trackes, produceres en KML fil. Dobbel klik på filen og Google Earth viser track.
Filnavn: 202506141040.KML (YYYYMMDDHHMM)

