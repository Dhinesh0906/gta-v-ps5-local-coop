# GTA V PS5 — local co-op, split-screen and Story Mode menu

Experimental local two-controller co-op for one PS5. Includes a resident Story Mode menu, a controllable second character and horizontal split-screen. Player 2 appears above Player 1; split-screen can be switched off while keeping Player 2 active.

**Use the prebuilt ELF files in [Releases](https://github.com/Dhinesh0906/gta-v-ps5-local-coop/releases). No save import is required.**

## Compatibility

| Component | Required / tested configuration |
| --- | --- |
| Game | Native PS5 GTA V, **PPSA04264**, update **01.011.001** |
| Firmware 13.42 | Loader **043**: menu, co-op and split-screen confirmed in a live session |
| Firmware 13.60 | Original loader **040** historically confirmed; 043 includes a 13.60 profile, but its newer camera guard has not been retested on a 13.60 console |
| Debugger | Included **PS5Debug-NG v1.3.2-local** |
| Payload startup | Compatible jailbreak and Payload Manager; tested with Payload Manager 0.5.2 |
| Controllers | Two DualSense controllers assigned to different local PS5 users |

**This is not a universal firmware payload.** Loader 043 selects between the 13.42 and 13.60 profiles for split-screen. Other firmware, title IDs, regions and updates are unverified. Having the same game name is insufficient: check both its title ID and update.

The included debugger was modified to start without waiting for an external network address. Runtime communication stays on the PS5 through `127.0.0.1`; a Mac, PC or Ethernet cable is not required during play.

## Download

Get these two files from the release:

1. `ps5debug-NG_v1.3.2-local.elf` — required local debugger.
2. `gta5-coop-autoload-043.elf` — menu, co-op, cameras, split output and resident recovery worker.

The full installation ZIP also contains the original 13.60 loader 040, firmware-specific copies, corresponding source, licences and checksums. For the preserved original 13.60 setup, use **040 instead of 043**, with the same debugger. **Never run both GTA loaders.**

Releases contain no saves, account IDs, game executable, game archives or console save databases. FTP is optional for transferring files; it is not a gameplay dependency and is not included in this release.

## Install

1. Start your usual compatible jailbreak and Payload Manager. Keep your existing game-mounting prerequisites.
2. Upload the two required `.elf` files using Payload Manager, preserving their filenames. Alternatively copy the folders from the ZIP's `AUTO-13.42-13.60/payloads/` into `/data/pldmgr/payloads/`. The intended layout is:

   ```text
   /data/pldmgr/payloads/
     ps5debug-NG/ps5debug-NG_v1.3.2-local.elf
     gta5-coop-autoload/gta5-coop-autoload-043.elf
   ```

3. To start manually, launch the debugger, wait at least two seconds, then launch the GTA loader. For automatic startup, enable Payload Manager autoload and append the following sequence **after your existing jailbreak/game-mounting entries** in `/data/pldmgr/autoload.txt`:

   ```text
   !10000
   ps5debug-NG_v1.3.2-local.elf
   !2000
   gta5-coop-autoload-043.elf
   ```

   Delay values are milliseconds. Preserve existing setup entries and avoid duplicate debugger or GTA worker entries. Replace an older GTA loader entry rather than appending a second GTA worker. For original 13.60 loader 040, substitute its filename in both the folder and autoload entry.

4. Launch **Story Mode free roam**, with the pause/mod menus closed, and allow up to a minute for installation. Complete the Prologue normally before using co-op.
5. Turn on the second controller under a **different local user**. Disable “Use Second Controller for Assistance.”
6. Double-click R3 on Controller 1 to create Player 2. Choose GTA's **Performance** graphics mode for the smoother tested result.

Copying files alone does not execute them. They must be launched manually or through enabled autoload. A full console restart still requires running the usual jailbreak and starting Payload Manager. The current 043 package has **not had a fresh unplugged cold-restart validation**; resident/autoload configuration is provided without claiming that test has passed.

## Controls

| Controller 1 action | Result |
| --- | --- |
| L1 + D-pad Right | Open the Story Mode menu |
| Quickly double-click R3, releasing between clicks | Spawn/despawn Player 2 |
| Main menu → Split-screen → X | Toggle split-screen while keeping co-op active |

Split-screen defaults to ON while Player 2 is active. P2 is on top; P1 is below. The audio listener follows P1. The split image uses 720p detail in a 1080p output carrier; it does **not** change GTA's source render buffers to native 720p or automatically change the PS5's TV resolution.

P2 has custom movement, combat and vehicle-control adapters. It is an extra controlled character, not a second native GTA Online player. Full P1 control/animation parity, identical vehicle handling, independent world streaming at arbitrary distance and stable performance in every scene are **not verified**. This is an experimental Story Mode project, not a finished multiplayer engine.

## Troubleshooting

- **Menu does not appear:** check exact game build, debugger startup, then the single GTA loader entry. Leave free roam unpaused while the worker installs.
- **Menu/co-op work but split-screen does not:** check firmware and split toggle. Other firmware is not automatically supported. Do not use original 040 on 13.42.
- **No second player:** check the second controller's separate user assignment, then double-click R3 on Controller 1. A second controller in assistance mode is not independent input.
- **Low frame rate:** use Performance rather than Fidelity or Performance RT. Performance is scene-dependent; no minimum FPS is promised.
- **Disabling:** remove only the GTA loader's autoload entry and restart the game/console as appropriate. Do not delete unrelated jailbreak or game-mounting payloads. Do not run several copies of the resident worker.

## Sources, verification and building

- `source/loader043/`: current loader source, helper sources, linker scripts and frozen generated asset headers.
- `source/loader040-original/`: preserved original 13.60 source snapshot.
- `source/ps5debug-NG/`: corresponding debugger source with its dependencies and licences.
- `source/ps5debug-loopback.patch`: the debugger change removing the external-address startup wait.
- `COMPATIBILITY.json`: exact compatibility and payload hashes.
- `SHA256SUMS.txt`: checksums for the release assets. The full ZIP has its own per-file verifier.

With a built [PS5 Payload SDK](https://github.com/ps5-payload-dev/sdk), build a loader from its frozen generated headers:

```sh
cd source/loader043
PS5_SDK=/path/to/sdk/install bash build-loader.sh
```

Compilation is unnecessary for normal installation. The packaged snapshots were compiled with `-Wall -Wextra -Werror`; their ELF sections matched the distributed originals except for the build-ID note. This verifies the source snapshot, not untested console/game combinations. Consult `source/README.txt` and the debugger's own build instructions for further details.

## Credits and licence

This repository packages work developed from [GronedWaffel/ps-neighborhood](https://github.com/GronedWaffel/ps-neighborhood). Debugger: [Pharaoh2k/ps5debug-NG](https://github.com/Pharaoh2k/ps5debug-NG), including its credited original authors and contributors. Startup manager: [itsPLK/ps5-payload-manager](https://github.com/itsPLK/ps5-payload-manager). SDK: [ps5-payload-dev/sdk](https://github.com/ps5-payload-dev/sdk).

Main project and debugger: GPL-3.0, with third-party notices retained in their source trees. Retain corresponding source and licence notices when redistributing modified binaries. See [LICENSE](LICENSE) and individual dependency notices.
