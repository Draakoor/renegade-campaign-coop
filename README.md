# Renegade Campaign Coop

[![GHCR build](https://github.com/Draakoor/renegade-campaign-coop/actions/workflows/ghcr.yml/badge.svg)](https://github.com/Draakoor/renegade-campaign-coop/actions/workflows/ghcr.yml)

A Pterodactyl/Docker development environment for C&C Renegade: **TT 4.8.4, Dragonade 1.11.1, Wine, 8 players**.

**M06, M07 and M09 include custom coop adaptations; complete playtesting is still pending.** The rotation is M01 through M11, starting at M01. Tutorial is excluded because it does not work reliably in coop. New volumes start private with a password; existing volumes retain settings until explicitly migrated. The live public test is not a fully validated campaign release.

## Image and workflow

```text
ghcr.io/draakoor/renegade-campaign-coop:main
```

The workflow tests the Python code and 150 orderings of shared mission objectives, compiles the Win32 plugin from source on a Windows runner, builds the Linux amd64 image, and checks its runtime and behavior when game assets are missing. It publishes to GHCR only after those checks pass. Pushes to `main`, `v*` tags and manual runs publish images; pull requests run checks without publishing. Image tags are `main`, `sha-<full-commit-sha>` and, where applicable, the version tag. There is deliberately no `latest` tag.

GitHub Actions uses `GITHUB_TOKEN` with `packages: write`; no additional registry secrets are required. Actions are pinned to commit SHAs, and the Wine base image is pinned to a digest. The published image digest appears in the workflow summary. Use that digest instead of `:main` to pin a deployment.

The image contains Wine, Perl, BRenBot 1.56 build 1, its plugins, the installer and the compiled game plugin with source. BRenBot runs beside the Windows FDS inside the same Helsinki container. It needs no separate PC installation. Original levels, CoopBeta game assets, passwords and live profiles are excluded; assets are assembled in the private persistent volume.

## Initial setup with Docker Compose

1. Clone the repository and create `volume/input/`. On Linux, the `container` user (UID/GID 1000) needs write access to `volume/`.
2. Copy the unmodified [CoopBeta424.zip](https://multiplayerforums.com/files/file/9-coopbeta/) to `volume/input/CoopBeta424.zip`.
3. Copy the original `server.dat` from your own Renegade FDS installation to `volume/input/server.dat`. Also place `M06.mix`, `M07.mix` and `M09.mix` from `Data/`, and `M06.LVL`, `M07.LVL` and `M09.LVL` from the EA installation's `Modding Support/Single Player Level Source Files/` directory, in `volume/input/`. All eight input files are checked against `sources.lock.json`.
4. Copy `.env.example` to `.env` and set `TT_DOWNLOAD_URL=http://YOUR_PUBLIC_IP:35002/`.
5. Start the server:

```sh
docker compose up -d
docker compose logs -f renegade
```

On first startup, the pinned TT/DA packages are downloaded from their official sources and verified using SHA256. The image assembles the server from those packages and the local input files. Wine then installs the required Microsoft/DirectX runtimes. This first startup takes several minutes.

| Port | Protocol | Purpose |
| --- | --- | --- |
| 35000 | UDP | Gameplay |
| 35001 | UDP | GameSpy queries |
| 35002 | TCP | Automatic TT package downloads |

Ports are fixed in this development build. Changing Docker port mappings alone does not update the advertised game port. There is no publicly accessible administration port. The download server serves only files from the TTFS `files` and `packages` directories.

The generated game password is stored privately in `volume/server/data/svrcfg_cnc.ini` under `bPassword`. For example, to join a private test directly:

```text
game.exe +connect YOUR_IP:35000 +netplayername TestPlayer +pass YOUR_TEST_PASSWORD
```

`NoGameplayPending=true` allows play with one participant. A TT client needs the package download. Actual client joins and complete mission playthroughs are not yet covered by the completed acceptance checks.

## Pterodactyl

Import `egg-renegade-campaign-coop.json` into a nest. The egg uses the GHCR image and starts `/opt/renegade-coop/runtime.py`. Assign allocation 35000 plus 35001/35002, and set `TT_DOWNLOAD_URL` to the public address. Suggested resources: 2048 MiB RAM, 200% CPU and 10240 MiB disk. Upload the eight input files listed above to `input/` through the file manager, then start the server.

Console input reaches the plugin through a file in the volume. `daver` displays the version; `quit` shuts down cleanly. The panel's Stop action sends `quit`. With Docker, `docker compose stop` sends SIGTERM. After a timeout, the supervisor terminates only its own Wine prefix.

An existing `bundle-dev-004` volume can be reused. Stop and back up the server before changing images; preserve `installed.json`, `server/`, `.wine/`, `defaults/` and `validation.json`. Investigate a remaining legacy `runtime.lock` only after confirming that the old process has stopped. The new image uses a stable file lock that is released if the process exits or crashes.

## Public test mode

Operators can explicitly open the incompletely tested campaign for public testing. Stop the server, then run the following against the volume; the tool is located at `/opt/renegade-coop/tools/set_public.py` inside the image:

```sh
python3 /opt/renegade-coop/tools/set_public.py --root /home/container --mode public-test
```

Then start normally. The tool backs up existing settings under `backups/publication-<UTC>/`, disables the game password and synthetic mission diagnostics, and writes `publication.json`. The previous test password is retained for a return to private operation; it is neither advertised nor logged. The tool does not change `validation.json`, the rotation or mission progression. To return to private mode, stop the server and run the tool with `--mode development`.

The supervisor advertises UDP 35001 only after Dragonade responds to queries. Outbound UDP 27900 must be reachable. It uses all seven community masters documented by TT and a 25-second timer independent of paused gameplay time. Heartbeats stop when the server shuts down; master entries then expire. `Public campaign test - mission playtests pending` and `Validation=pending` identify the outstanding acceptance work.

## Persistence and updates

`/home/container` is the server volume. It contains `server/`, `.wine/`, the verified `release/` tree, `validation.json` and backups. Live settings are not overwritten automatically. Changing an image tag **does not automatically update** an already installed game server. To update the plugin or game server, prepare a new verified release tree and install it while the server is stopped:

```sh
python3 /opt/renegade-coop/bootstrap.py --root /home/container
```

The installer verifies every release file, backs up replaced binaries/assets, and adds configuration files only when they are missing. Back up the entire volume before a migration as well. To roll back, stop only this server and restore its backed-up volume. **No scripts in this project restart nginx or other host services.**

### Updating only the plugin

For an existing compatible TT 4.8.4 / DA 1.11.1 volume, package the newly built plugin using the SHA256 of the currently installed DLL:

```sh
python tools/make_plugin_update.py --plugin build/plugin/da_campaign.dll --from-sha256 CURRENT_DLL_SHA256 --output build/coop-update.zip
```

Stop only the Renegade server, upload the archive, and run:

```sh
python3 /opt/renegade-coop/tools/update_plugin.py --root /home/container --payload /home/container/coop-update.zip
```

The updater verifies the old/new binary hashes, updates the live and release manifests, backs up every changed file and adds missing campaign settings. Ordinary updates preserve operator values, admin names, rotation and game assets. The explicit `--brenbot-migration` packaging flag also updates the volume supervisor, sets M01-M11 in live/default rotation settings and removes obsolete Novara scaling/console settings. Select the new BRenBot image for this migration. Every changed file is backed up first. Interrupted writes are rolled back. Starting the regular runtime resumes the last active mission from its beginning. Updates never reload nginx or other host services.

## Development status

- Previous private hosting checks passed for Tutorial startup on Windows and Wine, the panel console, clean shutdown/restart, external GameSpy queries and HTTP package downloads.
- CoopBeta supplies Tutorial, M01–M05, M08, M10 and M11. Complete mission playthroughs have not yet been verified.
- M06: shared War Room computer hacking, scientist rescue, Sydney escort and the Mendoza objective. The computer trigger runs once and survives an interrupted dialogue.
- M07: shared Dead6/Sydney, SAM, SSM and nuclear objectives. Individual player deaths do not cause mission failure; deaths of required escort characters do.
- M09: Mobius escort with the protective suit, surface and rendezvous stages. The suit sequence runs without a single-player camera or teleport. Evacuation requires Mobius and at least one living player in the destination area.
- All three: shared objectives/keys, safe respawns at the mission start or a reached checkpoint, late-join synchronization, GDI team, and no friendly fire, team switching or time limit. Success advances to the next map; failure reloads the same map. A server or map restart discards the checkpoint.
- Damage correction for M06/M07/M09: CoopBeta's `Commando_Stealth` and `Commando_Desert` presets use invulnerable `Blamo` armor. When placing a human campaign player, the plugin replaces it with Havoc's normal skin/armor types. The model, weapons and health are preserved; NPC presets are unchanged. Isolated engine tests confirm damage and death on all three maps ([test report](validation/player-armor-2026-10-20.json)); testing with a real player is still pending.
- Original LVL files supply positions and scripts for missing NPCs. The installer generates private `.wcm` tables from them; the plugin restores placed soldiers omitted by the FDS. Original level files remain unchanged. These three maps use their original game assets plus the existing CoopContent package.
- `!cp` uses a reached shared checkpoint on M06/M07/M09, or a visited original purchase station on other missions. `!goto <player>` moves to a teammate. Both require living characters on foot, three seconds without movement, ten seconds out of combat and a thirty-second cooldown. Ladders, elevators, restricted sequences and unsafe destinations are rejected. Original CoopBeta respawns are preserved.
- `!skins [search] [page]` lists every available soldier model from the installed presets: currently 76 unique models, including GDI, Nod, civilians, heroes and mutants. `!skin <ID|alias|model>` changes appearance only; `!skin default` restores the purchased class model. Weapons, armor and credits are preserved. IDs are generated from the installed catalog; client rendering still needs playtesting.
- `!donate <player name/ID> <credits>` transfers whole credits to a teammate. `!tdonate <credits>` splits the amount equally among other connected teammates; the remainder stays with the sender. Invalid amounts, insufficient funds, self-transfers and empty teams are rejected.
- The additional Novara adaptive damage scaling and `!difficulty` command have been removed. Original mission scripts retain their behavior. Auditing the CoopBeta plugin and loaded map scripts did not establish a general scaling formula based on connected player count; a difficulty-related powerup script in M11 does not prove one.
- Fixed map-placed weapons and keycards replenish after collection. NPC/player drops are excluded. Original repeating spawners keep ownership of replenishment; one-time mission scripts are not copied onto replacements. Mission-script removals without collection are not reversed.
- Shared keycards synchronize to all living players, respawns and late joiners. Explicitly revoked keys remain unavailable until granted again.
- Original CoopBeta maps keep their own purchase stations (M01: 12, M02: 20, M03: 8, M04: 5, M05: 7, M08: 3, M10: 4, M11: 7). No additional red Novara consoles are spawned there. M06/M07/M09 use the same `pct_zone_gdi` preset at the start and reached checkpoints, with terrain and clearance checks. Use the station or `!buy` within 6 meters to buy GDI infantry; the refill button or `!heal` restores health, armor and ammunition. Purchases preserve position and shared keys.
- Dragonade's query currently reports `TC=1` because of an upstream factory check, even though the configuration and plugin block team switching. Verify this with real clients and correct the reported value.
- In public test mode, the supervisor sends heartbeats every 25 seconds according to the [TT protocol](https://tiberiantechnologies.org/Docs/?page=GameSpy%20Protocol%20and%20New%20Broadcaster). Dragonade continues to answer UDP queries; its old heartbeat remains restricted to localhost. A successful master response contains `heartbeatresult=0`. Joining through an actual server list must be verified separately.
- Local checks cover loading all three maps, original spawn points, restoring 45/41/119 NPCs on M06/M07/M09, Dead6/Mobius/Sydney with their scripts, map transitions, reloads and mission restart after Mobius dies. See the [technical test report](validation/native-2026-10-19.json). These diagnostics used **zero real clients**.
- Operators can begin with solo testing. A full solo playthrough of every mission, real multiplayer joins, sequences and an eight-client load test remain outstanding. Public test mode does not count as completed campaign acceptance.
- `ResumeRotation=true` is enabled. The supervisor corrects the FDS rotation index advanced during shutdown using the last loaded map; that mission starts from the beginning after a restart. There are no persistent checkpoint saves.
- `RELEASE_MODE=development` remains set. Public test mode is selected only through the backed-up configuration change and `publication.json`.

## Testing missions locally and updating existing volumes

The new files are `da_campaign.dll`, `data/M06.mix`, `data/M07.mix`, `data/M09.mix` and their corresponding `.wcm` tables. The installer builds them from verified inputs. For an existing volume, stop and back it up first, generate a new release tree with `tools/prepare_bundle.py`, then install it with `bootstrap.py`. `server/tt.cfg` is intentionally preserved: after backing it up, merge the new `gameDefinitions` and desired rotation from `defaults/tt.cfg`. M06/M07/M09 require the original maps on the TT client; no new models or textures are distributed for them.

`campaign_dump` in the panel console writes mission objects/spawners and shared state to local diagnostic output. On an isolated test server only, `CampaignDiagnostics=1` in the `[General]` section of `da.ini` enables `campaign_probe_spawners` and `campaign_test_mobius_death`. The former spawns NPCs without players; the latter tests Mobius's death and the subsequent map restart. These synthetic events do not constitute mission acceptance. Keep the option disabled during normal operation.

With diagnostics enabled, `campaign_test_player_armor` uses a synthetic actor on M06/M07/M09 to check damage before and after the armor correction, including lethal damage. The command refuses to run while living human players are present. For real players, `campaign_dump` also logs skin/armor types, health and whether the character can die.

`campaign_supplies` displays tracked weapons/keycards and purchase terminals. On an empty diagnostic server, `campaign_test_supplies` uses a synthetic collection notification to check replenishment and the exclusion of later drops. This does not replace a real client test of pickups or the purchase menu.

The [supply test report](validation/coop-services-2026-10-20.json) records map-loading checks for all twelve maps and successful respawn probes on M06/M07/M09. Replacement objects stayed exactly at their pickup locations. The purchase menu, actual purchases and shared key ownership with multiple clients have not yet passed gameplay acceptance.

`campaign_test_features` checks all 76 catalog models while preserving the synthetic actor's class, health, armor, weapons, keys and position. `campaign_test_vehicles` creates and checks all 34 driveable presets, including aircraft. `campaign_test_portals` checks a clear site and original checkpoint assets on M06/M07/M09. These are empty-server engine checks, not client rendering, driving, portal crossing or mission-completion tests. Run `python tools/test_native_features.py --root PATH_TO_FDS --host TEST_IP --port QUERY_PORT --output evidence.json --services` on a password-protected, empty server starting on M01. It advances all eleven missions.

## BRenBot and command ownership

The image fetches official BRenBot at the commit and SHA256 in `sources.lock.json`. Linux Perl uses our local adapter to talk to the Windows FDS under Wine. The bot has no IRC connection, RenRem socket or SSGM TCP connection. Structured game events separate player identity from chat text; outgoing replies and sounds are tied to the current player connection. The adapter cannot execute map, kick or administrator console commands.

- BRenBot core owns `!version`.
- `novara_coop` owns `!help`, `!commands`, `!coophelp`, `!extras`, `!rotation` and `!scripts`.
- `funsounds` owns `!sounds [page|on|off]`, `!taunts`, `!taunt <keyword>`, `!sound`, `!disablesounds` and `!enablesounds`. It includes all 49 keyword/WAV mappings from upstream Funsounds 1.02, using sounds already present in the game. Public chat plays to unmuted players; team chat stays within that team. Private chat is not forwarded. Matching is case-insensitive at word boundaries, with one sound per message, a 10-second player cooldown and a 3-second global cooldown.
- The game plugin owns purchases/refills, checkpoints, skins, credit transfers and panel-approved administrator actions. Duplicate Dragonade donations, BRenBot administrative commands, voting, team balancing and the former C++ sound/help handlers are disabled or removed.

Persistent bot configuration is in `brenbot/brenbot.cfg`, `brenbot/funsounds.json` and `brenbot/plugins/funsounds.xml`. SQLite state is in `brenbot/brenbot.dat`. Startup preserves these settings. Managed Perl plugin source and `commands.xml` update from the image with backups. Do not enable nickname-only bot moderators; `!admin` plus panel confirmation remains authoritative. Bot output appears in the panel as `[BRenBot]`. A failed bot retries twice without taking down the game.

M06/M07/M09 checkpoint portals use the original `Marker Flag`, `Simple_Sydney_SandM_Wall` and `TDA_Teleport_Zone`, verified against CoopBeta M01. Completing a checkpoint creates a portal near the previous reached location and retargets earlier portals to the latest checkpoint. Portal sites require level ground, a clear walking path and space for the full laser frame. In low rooms the same wall asset is rotated in its plane to fit below the ceiling. A location without enough space is logged and leaves `!cp` available. No future objectives are unlocked by the portals. Real client crossing and visual placement still need verification.

## Configuration in the panel

Live settings are stored under `server/` in the server volume. The repository's `defaults/` directory is used only to initialize missing configuration files. Back up the relevant file before editing it.

| File | Settings |
| --- | --- |
| `brenbot/funsounds.json` | Chat keyword to existing game WAV mapping |
| `brenbot/plugins/funsounds.xml` | Chat triggers and sound cooldowns |
| `server/da.ini` | Dragonade, coop features, spawn protection, GameSpy queries |
| `server/data/svrcfg_cnc.ini` | `bGameTitle`, `MaxPlayers`, `StartingCredits`, game password, game rules |
| `server/tt.cfg` | Rotation, map definitions and TT downloads |
| `server/server.ini` | Game port, network settings, `ResumeRotation` |
| `server/data/paths.ini` | Internal data directories; also migrate TTFS files when renaming them |

Coop settings in the `[General]` section of `server/da.ini`:

| Key | Default | Meaning |
| --- | ---: | --- |
| `CampaignSharedKeys` | `1` | Shared key ownership |
| `CampaignWeaponRespawnSeconds` | `15` | Fixed weapon respawn delay; `0` disables replenishment |
| `CampaignKeyRespawnSeconds` | `5` | Fixed keycard respawn delay; `0` disables replenishment |
| `CampaignTerminals` | `1` | Enable purchase terminals |
| `CampaignCatchup` | `1` | Catch-up commands to reached checkpoints/stations |
| `CampaignPlayerGoto` | `1` | Teleport to a connected teammate |
| `CampaignCatchupWarmupSeconds` | `3` | Time to stand still before teleporting |
| `CampaignCatchupCombatSeconds` | `10` | Required time since dealing/taking damage |
| `CampaignCatchupCooldownSeconds` | `30` | Delay between successful catch-up commands |
| `CampaignSkins` | `1` | Cosmetic skin commands |
| `CampaignDonations` | `1` | Validated player/team credit transfers; keep `EnableDonate=0` |
| `CampaignAdminName` | empty | Player name allowed to request administrator approval |

Dragonade supports map-specific overrides such as `[M06.mix]`. Placement and supply tracking initialize per map; restart or change maps after editing those settings. Timing values are clamped to 0-300 seconds. `!help` comes from the BRenBot `novara_coop` plugin; `campaign_features` in the panel reports shared checkpoint and cosmetic state.

### Administrator approval

A name alone does not authenticate an administrator on GameSpy/LAN. `CampaignAdminName` therefore permits only an access request: type `!admin` in-game, then confirm the privately displayed `campaign_admin <Code>` command in the authenticated panel console within five minutes. Each request is single-use and expires when the player leaves; administrator access ends when that connection ends. No passwords or client identifiers are published. `!adminhelp` lists `!givecredits <player|me|all> <amount>`, `!vehicles [search] [page]`, `!spawnvehicle <ID|preset>`, `!map <M01..M11>`, `!nextmap`, `!restartmap` and `!kick <player-ID>`. Vehicle spawning requires a living administrator on foot, a clear position, a five-second cooldown and a configurable per-map cap (`CampaignAdminVehicleLimit=20`). Scripted NPC behavior is removed from administrator vehicles. The catalog includes all driveable GDI, Nod, civilian and flying presets; scenery, turrets and vehicles without entry transitions are excluded. There is no chat command for executing arbitrary console commands.

The port uses the spawner interface of the pinned FDS version. The installer rejects other executable hashes. Upgrading TT/Dragonade requires another compatibility review of those interfaces.

## Building locally

Requires Python 3.11+ and a Visual Studio x86 developer environment:

```powershell
python tools/fetch_inputs.py --work build/inputs --only dragonade
python tools/build_plugin.py --dragonade build/inputs/da --output artifacts
python -m unittest discover -s tests -v
docker build -t renegade-campaign-coop:local .
```

Import libraries are generated from the exact release DLL exports. The installer requires no additional Python packages. `tools/prepare_bundle.py` can also create a complete local upload bundle from existing inputs; use `--help` to see its parameters. Do not commit local bundles or game assets to the repository.

## Sources and licensing

Hosting/installer code: GPL-2.0-or-later (`COPYING`). The combined mission plugin, including the adapted EA controllers: GPL-3.0-or-later (`COPYING.plugin`, including EA's additional terms). See `THIRD_PARTY.txt`. These licenses do not grant additional rights to the original game assets. Sources: [TT](https://tiberiantechnologies.org/downloads), [Dragonade](https://github.com/TheUnstoppable/Dragonade), [CoopBeta](https://multiplayerforums.com/files/file/9-coopbeta/), [EA Renegade source code](https://github.com/electronicarts/CnC_Renegade). Versions and checksums are documented in `sources.lock.json`.
