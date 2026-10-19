# Renegade Campaign Coop

[![GHCR build](https://github.com/Draakoor/renegade-campaign-coop/actions/workflows/ghcr.yml/badge.svg)](https://github.com/Draakoor/renegade-campaign-coop/actions/workflows/ghcr.yml)

Pterodactyl-/Docker-Entwicklungsumgebung für C&C Renegade: **TT 4.8.4, Dragonade 1.11.1, Wine, 8 Spieler**.

**M06, M07 und M09 enthalten jetzt eigene Coop-Anpassungen; vollständige Spieltests stehen noch aus.** Neu aufgebaute Volumes enthalten die private Testrotation Tutorial → M01–M11. Bestehende Volumes behalten ihre Einstellungen und benötigen das unten beschriebene Update. Neue Volumes starten passwortgeschützt und ohne öffentliche GameSpy-Heartbeats. Ein Betreiber kann den unten beschriebenen öffentlichen Testbetrieb ausdrücklich aktivieren. Das öffentliche Repository und Image stellen den Entwicklungsstand bereit; sie bedeuten keine Freigabe der Kampagne.

## Image und Workflow

```text
ghcr.io/draakoor/renegade-campaign-coop:main
```

Der Workflow testet den Python-Code und 150 Reihenfolgen gemeinsamer Missionsziele, kompiliert das Win32-Plugin aus den Quellen auf einem Windows-Runner, baut das Linux-amd64-Image und prüft dessen Laufzeit und Verhalten ohne Spieldaten. Erst danach veröffentlicht er es auf GHCR. Pushes auf `main`, Tags `v*` und manuelle Starts veröffentlichen; Pull Requests prüfen ohne Veröffentlichung. Tags sind `main`, `sha-<vollständiger Commit>` und gegebenenfalls der Versions-Tag. Es gibt bewusst keinen `latest`-Tag.

GitHub Actions nutzt `GITHUB_TOKEN` mit `packages: write`; zusätzliche Registry-Secrets sind nicht erforderlich. Actions sind auf Commit-SHAs und das Wine-Basisimage auf einen Digest festgelegt. Der fertige Image-Digest steht in der Workflow-Zusammenfassung. Für einen festgeschriebenen Betrieb diesen Digest statt `:main` verwenden.

Das Image enthält Laufzeit, Installer, Plugin und Pluginquellen. **Es enthält keine Original-Leveldateien, CoopBeta-Spieldaten, Testkennwörter oder laufenden Serverprofile.** Die erforderlichen Assets werden ausschließlich im persistenten Servervolume zusammengestellt.

## Ersteinrichtung mit Docker Compose

1. Repository klonen und `volume/input/` anlegen. Unter Linux benötigt der Benutzer `container` (UID/GID 1000) Schreibzugriff auf `volume/`.
2. Die unveränderte [CoopBeta424.zip](https://multiplayerforums.com/files/file/9-coopbeta/) nach `volume/input/CoopBeta424.zip` kopieren.
3. Die originale `server.dat` aus der eigenen Renegade-FDS-Installation nach `volume/input/server.dat` kopieren. Zusätzlich `M06.mix`, `M07.mix`, `M09.mix` aus `Data/` und `M06.LVL`, `M07.LVL`, `M09.LVL` aus `Modding Support/Single Player Level Source Files/` der EA-Installation in `volume/input/` ablegen. Alle acht Eingaben werden gegen `sources.lock.json` geprüft.
4. `.env.example` nach `.env` kopieren und `TT_DOWNLOAD_URL=http://DEINE_OEFFENTLICHE_IP:35002/` setzen.
5. Starten:

```sh
docker compose up -d
docker compose logs -f renegade
```

Beim ersten Start werden die festgelegten TT-/DA-Pakete von ihren offiziellen Quellen heruntergeladen und mit SHA256 geprüft. Das Image stellt daraus und den lokalen Eingaben den Server zusammen. Danach installiert Wine die benötigten Microsoft-/DirectX-Laufzeiten. Dieser erste Start dauert mehrere Minuten.

| Port | Protokoll | Zweck |
| --- | --- | --- |
| 35000 | UDP | Spiel |
| 35001 | UDP | GameSpy-Abfrage |
| 35002 | TCP | Automatische TT-Paketdownloads |

Die Ports sind in dieser Entwicklungsfassung festgelegt. Ein bloßes Ändern des Docker-Portmappings passt die Spielport-Ankündigung nicht an. Es gibt keinen öffentlich erreichbaren Administrationsport. Der Downloadserver liefert ausschließlich Dateien aus den TTFS-Verzeichnissen `files` und `packages`.

Das generierte Spielkennwort steht privat unter `volume/server/data/svrcfg_cnc.ini`, Schlüssel `bPassword`. Für einen direkten Testbeitritt beispielsweise:

```text
game.exe +connect DEINE_IP:35000 +netplayername Testspieler +pass DEIN_TESTKENNWORT
```

Ein Spieler ist durch `NoGameplayPending=true` zugelassen. Ein Client mit TT benötigt den Paketdownload. Ein tatsächlicher Client-Beitritt und das Durchspielen der Missionen sind noch nicht Teil der bestandenen Abnahme.

## Pterodactyl

`egg-renegade-campaign-coop.json` in ein Nest importieren. Das Egg verwendet das GHCR-Image und startet `/opt/renegade-coop/runtime.py`. Die Allocation 35000 plus 35001/35002 zuweisen; `TT_DOWNLOAD_URL` auf die öffentliche Adresse setzen. Vorgeschlagene Ressourcen: 2048 MiB RAM, 200 % CPU, 10240 MiB Disk. Die acht oben aufgeführten Eingaben über den Dateimanager unter `input/` bereitstellen, dann starten.

Die Konsole wird über eine Datei im Volume an das Plugin übergeben. `daver` zeigt die Version, `quit` beendet geordnet. Der Panel-Stop sendet `quit`. Bei Docker sendet `docker compose stop` SIGTERM. Der Supervisor beendet nach einem Timeout ausschließlich seinen Wine-Prefix.

Ein bereits aufgebautes `bundle-dev-004`-Volume kann weiterverwendet werden. Vor einem Imagewechsel den Server stoppen und sichern; `installed.json`, `server/`, `.wine/`, `defaults/` und `validation.json` erhalten. Ein noch vorhandenes altes `runtime.lock` erst nach bestätigtem Ende des alten Prozesses untersuchen. Das neue Image nutzt eine stabile Dateisperre, die nach einem Prozessabbruch wieder freigegeben wird.

## Öffentlicher Testbetrieb

Der Betreiber kann die unvollständig getestete Kampagne ausdrücklich für öffentliche Tests freigeben. Dazu den Server stoppen und im Volume ausführen (im Image liegt das Werkzeug unter `/opt/renegade-coop/tools/set_public.py`):

```sh
python3 /opt/renegade-coop/tools/set_public.py --root /home/container --mode public-test
```

Anschließend normal starten. Das Werkzeug sichert die bestehenden Einstellungen unter `backups/publication-<UTC>/`, deaktiviert das Spielpasswort und künstliche Missionsdiagnosen und schreibt `publication.json`. Das bisherige Testkennwort bleibt für eine Rückkehr zum privaten Betrieb erhalten; es wird weder angekündigt noch protokolliert. `validation.json`, Rotation und Missionsfortschritt werden nicht verändert. Mit `--mode development` bei gestopptem Server wieder privat schalten.

Der Supervisor kündigt UDP 35001 erst an, wenn Dragonade auf Abfragen antwortet. Ausgehend muss UDP 27900 erreichbar sein. Er nutzt alle sieben von TT dokumentierten Community-Master und einen 25-Sekunden-Timer unabhängig von pausierter Spielzeit. Beim Stoppen enden die Heartbeats; Listeneinträge verfallen beim Master. `Public campaign test - mission playtests pending` und `Validation=pending` kennzeichnen die noch offene Abnahme.

## Persistenz und Updates

`/home/container` ist das Servervolume. Es enthält `server/`, `.wine/`, das geprüfte `release/`, `validation.json` und Sicherungen. Laufende Einstellungen werden nicht automatisch überschrieben. Ein geänderter Image-Tag aktualisiert einen bereits installierten Spielserver **nicht** heimlich; für Änderungen am Plugin bzw. Spielserver einen neuen geprüften Release-Baum bereitstellen und bei gestopptem Server installieren:

```sh
python3 /opt/renegade-coop/bootstrap.py --root /home/container
```

Der Installer prüft alle Release-Dateien, sichert ersetzte Binärdateien/Assets und übernimmt nur fehlende Konfigurationen. Vor einer Migration zusätzlich das ganze Volume sichern. Zur Wiederherstellung nur diesen Server stoppen und dessen gesichertes Volume zurückspielen. **Keine Skripte in diesem Projekt starten nginx oder andere Hostdienste neu.**

## Entwicklungsstand

- Im bisherigen privaten Hostingtest: Tutorialstart unter Windows und Wine, Panel-Konsole, geordnetes Stoppen/erneuter Start, externe GameSpy-Abfrage und HTTP-Paketdownload erfolgreich.
- CoopBeta-Dateien vorhanden: Tutorial, M01–M05, M08, M10, M11. Noch keine vollständigen Missionsdurchläufe nachgewiesen.
- M06: gemeinsames Hacken des War-Room-Rechners, Wissenschaftler-Rettung, Sydney-Eskorte und Mendoza-Ziel. Der Rechner-Auslöser ist einmalig und übersteht einen unterbrochenen Dialog.
- M07: gemeinsame Dead6-/Sydney-, SAM-, SSM- und Nuklearziele. Einzelne Spielertode lösen keinen Missionsfehlschlag aus; der Tod erforderlicher Begleitfiguren schon.
- M09: Mobius-Eskorte mit Schutzanzug, Oberfläche und Rendezvous. Die Schutzanzug-Szene wird ohne Einzelspieler-Kamera/Teleport ausgeführt. Evakuierung verlangt Mobius und mindestens einen lebenden Spieler im Zielgebiet.
- Alle drei: gemeinsamer Ziel-/Schlüsselstand, sichere Respawns am Missionsstart bzw. erreichten Kontrollpunkt, Synchronisierung späterer Beitritte, GDI, kein Friendly Fire/Teamwechsel/Zeitlimit. Erfolg wechselt zur nächsten Karte, Fehlschlag lädt dieselbe Karte neu. Ein Server-/Kartenneustart verwirft den Kontrollpunkt.
- Schadenskorrektur für M06/M07/M09: Die CoopBeta-Presets `Commando_Stealth` und `Commando_Desert` verwenden unverwundbare `Blamo`-Rüstung. Beim Platzieren eines menschlichen Kampagnenspielers ersetzt das Plugin diese durch Havocs normale Körper-/Rüstungstypen. Modell, Waffen und Lebenspunkte bleiben erhalten; NPC-Presets werden nicht verändert. Der isolierte Engine-Test bestätigt Schaden und Tod auf allen drei Karten ([Prüfbericht](validation/player-armor-2026-10-20.json)); der Test mit einem echten Spieler steht noch aus.
- Die Original-LVL-Dateien liefern Positionen und Skripte fehlender NPCs. Der Installer erzeugt daraus private `.wcm`-Tabellen; das Plugin stellt die vom FDS ausgelassenen platzierten Soldaten wieder her. Original-Leveldateien bleiben unverändert. Die drei Karten nutzen ihre Original-Spielassets plus das bestehende CoopContent-Paket.
- Der Legacy-`!goto`-Befehl ist noch nicht portiert.
- Die DA-Abfrage meldet durch einen Upstream-Factory-Test derzeit `TC=1`, obwohl Konfiguration und Plugin Teamwechsel sperren. Mit echten Clients prüfen und die Anzeige korrigieren.
- Der Supervisor sendet im öffentlichen Testbetrieb alle 25 Sekunden Heartbeats gemäß [TT-Protokoll](https://tiberiantechnologies.org/Docs/?page=GameSpy%20Protocol%20and%20New%20Broadcaster). Dragonade beantwortet weiterhin die UDP-Abfragen; sein alter Heartbeat bleibt auf localhost begrenzt. Ein erfolgreicher Master meldet `heartbeatresult=0`. Ein echter Listenbeitritt muss separat geprüft werden.
- Lokal geprüft: Laden aller drei Karten, ursprüngliche Startpunkte, Wiederherstellung von 45/41/119 NPCs (M06/M07/M09), Dead6/Mobius/Sydney inklusive Skripten, Kartenwechsel, Neuladen und Missionsneustart nach Mobius-Tod. [Technischer Prüfbericht](validation/native-2026-10-19.json). Diese Diagnosen hatten **null echte Clients**.
- Der Betreiber kann zunächst allein testen. Ein vollständiger Solo-Durchlauf aller Missionen, echte Mehrspieler-Beitritte, Sequenzen und der Lasttest mit acht Clients sind noch offen. Öffentlicher Testbetrieb gilt nicht als bestandene Kampagnenabnahme.
- `ResumeRotation=true` ist gesetzt. Der Supervisor korrigiert den beim Beenden vorgerückten FDS-Rotationsindex anhand der zuletzt geladenen Karte; die Mission beginnt nach dem Neustart wieder von vorn. Kein persistenter Checkpoint-Spielstand.
- `RELEASE_MODE=development` bleibt gesetzt. Öffentlicher Testbetrieb wird ausschließlich durch die gesicherte Konfigurationsänderung und `publication.json` gewählt.

## Missionen lokal prüfen und bestehende Volumes aktualisieren

Die neuen Dateien sind `da_campaign.dll`, `data/M06.mix`, `data/M07.mix`, `data/M09.mix` und die zugehörigen `.wcm`-Tabellen. Der Installer baut diese aus den geprüften Eingaben. Für ein bestehendes Volume zuerst stoppen und sichern, einen neuen Release-Baum mit `tools/prepare_bundle.py` erzeugen und mit `bootstrap.py` installieren. `server/tt.cfg` wird absichtlich nicht überschrieben: die neuen `gameDefinitions` und die gewünschte Rotation aus `defaults/tt.cfg` nach Sicherung gezielt übernehmen. Für M06/M07/M09 werden die originalen Karten auf dem TT-Client benötigt; neue Modelle/Texturen werden nicht verteilt.

`campaign_dump` in der Panel-Konsole schreibt Missionsobjekte/Spawner und den gemeinsamen Zustand ins lokale Protokoll. Ausschließlich auf einem isolierten Testserver lässt `CampaignDiagnostics=1` in `[General]` der `da.ini` die Befehle `campaign_probe_spawners` und `campaign_test_mobius_death` zu. Der erste erzeugt NPCs ohne Spieler; der zweite prüft Mobius-Tod und Kartenneustart. Diese künstlichen Ereignisse sind keine Missionsabnahme. Im normalen Betrieb bleibt die Option aus.

`campaign_test_player_armor` prüft bei aktivierten Diagnosen auf M06/M07/M09 an einer künstlichen Testfigur Schaden vor/nach der Rüstungskorrektur sowie tödlichen Schaden. Der Befehl verweigert den Test bei anwesenden lebenden Spielern. `campaign_dump` protokolliert für echte Spieler zusätzlich Körper-/Rüstungstyp, Lebenspunkte und Sterblichkeit.

Die Portierung verwendet die Spawner-Schnittstelle der festgelegten FDS-Version. Der Installer lehnt andere Executable-Hashes ab. Ein Upgrade von TT/Dragonade benötigt eine erneute Prüfung der Schnittstellen.

## Lokal bauen

Python 3.11+ und eine Visual-Studio-x86-Entwicklungsumgebung:

```powershell
python tools/fetch_inputs.py --work build/inputs --only dragonade
python tools/build_plugin.py --dragonade build/inputs/da --output artifacts
python -m unittest discover -s tests -v
docker build -t renegade-campaign-coop:local .
```

Die Importbibliotheken entstehen aus den exakten Release-DLL-Exports. Der Installer arbeitet ohne zusätzliche Python-Pakete. `tools/prepare_bundle.py` kann auch ein vollständiges lokales Uploadpaket aus den vorhandenen Eingaben erzeugen; `--help` zeigt die Parameter. Lokale Pakete und Spieldaten nicht ins Repository aufnehmen.

## Quellen und Lizenz

Hosting-/Installer-Code: GPL-2.0-or-later (`COPYING`). Das kombinierte Missionsplugin mit den angepassten EA-Controllern: GPL-3.0-or-later (`COPYING.plugin` einschließlich EA-Zusatzbedingungen). Siehe `THIRD_PARTY.txt`. Die Lizenz erteilt keine zusätzlichen Rechte an den ursprünglichen Spielassets. Quellen: [TT](https://tiberiantechnologies.org/downloads), [Dragonade](https://github.com/TheUnstoppable/Dragonade), [CoopBeta](https://multiplayerforums.com/files/file/9-coopbeta/), [EA-Renegade-Quellcode](https://github.com/electronicarts/CnC_Renegade). Versionen und Prüfsummen sind in `sources.lock.json` dokumentiert.
