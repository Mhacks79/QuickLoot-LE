# QuickLoot IE LE

A port of [QuickLoot IE](https://github.com/MissCorruption/QuickLootIE) by Miss Corruption to
**Skyrim Legendary Edition** (TESV 1.9.32.0.8, SKSE 1.7.3).

Look at a chest, a barrel or a body and its contents show up in a small window next to the
crosshair. Take one item, take everything, use it, or open the container the usual way,
without going through the inventory screen.

## Features

- Loot window for containers and corpses (ash piles included), with the original QuickLoot IE
  interface: item icons, stolen / read / enchanted / best-in-class markers, value, weight and
  value-per-weight columns.
- Take, take all, use (equip, eat, read) and open as usual, on keyboard, mouse and gamepad.
  Holding Activate still grabs the object.
- Stealing works as in the vanilla game: owned items show as stolen and taking them can be
  witnessed.
- Configurable sorting, window position and size, opacity, visibility conditions
  (combat, sneaking, empty containers, third person, horseback, after unlocking) and exclusions.
- Settings page in [KHA-OS](#requirements) (optional), or a plain INI file.
- No plugin file, no scripts, nothing written to the save: safe to install or remove mid-game.
- Text in the game's own language, accents and non-Latin alphabets included.

## Requirements

- Skyrim Legendary Edition 1.9.32.0.8
- SKSE 1.7.3
- SkyUI 5.1 (item and key icons)
- KHA-OS (optional): in-game settings page. Without it, edit
  `Data\SKSE\Plugins\QuickLootIELE.ini`.

## Installation

Download the archive from the [Releases](../../releases) page and install it with your mod
manager. The archive contains the game files, the license files and the complete source of
the build.

## Default controls

| Action | Keyboard | Gamepad |
|---|---|---|
| Take | E | A |
| Take all | R | X |
| Use (equip / eat / read) | Shift + E | Y |
| Open the container as usual | Q | Back |
| Scroll | Mouse wheel, arrow keys, numpad 8 / 2 | D-pad |

## Differences from QuickLoot IE (Special Edition)

- The MCM, the Papyrus scripts and the plugin file are replaced by an INI file and a KHA-OS page.
- Container lids are not animated when an item is taken.
- Integrations with Special Edition-only mods (Completionist, NPCs Names Distributor, Artifact
  Tracker, Better Third Person Selection, Dismembering Framework, Inventory Interface
  Information Injector), VR support and the plugin API are not included.

The full list of changes, file by file, is in [CHANGES.md](CHANGES.md).

## Building from source

See [docs/BUILDING.md](docs/BUILDING.md). In short: MSVC v143 (Win32), the SKSE 1.7.3 source,
and this repository placed in `skse_plugins\QuickLootIELE` of that source tree.

## Repository layout

| Path | Contents |
|---|---|
| `src/` | The SKSE plugin (C++). Each file names the original file it derives from. |
| `src/Khaos/` | KHA-OS interface headers, see [NOTICE.md](NOTICE.md). |
| `ThirdParty/imgui/` | Dear ImGui, MIT license. |
| `data/` | Game files: the INI, the loot window movie and its translations. |
| `tests/` | Offline tests of the settings and input code, and a mutation check. |
| `tools/` | SWF header conversion and the startup self-check table generator. |
| `evidence/` | How each engine address, offset and calling convention used by the port was established. |
| `original/QuickLootIE-4.1.3/` | The original QuickLoot IE 4.1.3 source, unmodified, including the Flash source (`.fla`) of the loot window. |

## License

QuickLoot IE LE is free software, released under the same terms as QuickLoot IE:
**GPL-3.0-or-later** with the **Modding Exception** and the **GPL-3.0 Linking Exception
(with Corresponding Source)**.

- [LICENSE](LICENSE): GNU General Public License v3.0
- [EXCEPTIONS.md](EXCEPTIONS.md): the two additional permissions under section 7
- [MIT](MIT): the notice of QuickLootRE (ryan-rsm-mckenzie), which QuickLoot EE (ilian53) and
  then QuickLoot IE were forked from. The MIT license requires this notice to travel with the
  code derived from it; it does not change the license of this port, which is GPL-3.0.

These files are copied unchanged from the original project. The modifications made by this
port, with their date, are described in [CHANGES.md](CHANGES.md), and the third-party
components and their licenses are listed in [NOTICE.md](NOTICE.md).

## Credits

- **Miss Corruption**: QuickLoot IE, the original mod.
- **Mhacks79**: the Legendary Edition port.

QuickLoot IE descends from QuickLoot EE by ilian53 and QuickLootRE by ryan-rsm-mckenzie (see
[NOTICE.md](NOTICE.md)). The contributors to the original project are credited in
[original/QuickLootIE-4.1.3/README.md](original/QuickLootIE-4.1.3/README.md).

---

This port was developed with AI assistance (Claude).
