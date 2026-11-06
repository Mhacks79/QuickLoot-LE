# Notices

## This work

QuickLoot IE LE is a modified version of QuickLoot IE 4.1.3 by Miss Corruption
(https://github.com/MissCorruption/QuickLootIE), modified by Mhacks79 for Skyrim Legendary
Edition, first published on 2026-11-06. See [CHANGES.md](CHANGES.md) for what was changed.

It is licensed under GPL-3.0-or-later WITH the Modding Exception AND the GPL-3.0 Linking
Exception (with Corresponding Source), as is the original: [LICENSE](LICENSE),
[EXCEPTIONS.md](EXCEPTIONS.md). These license files are reproduced unchanged.

## Lineage

QuickLoot IE is itself the end of a chain of forks, and this port inherits all of it:

1. **QuickLootRE** by ryan-rsm-mckenzie (published as SniffleMan), MIT license, 2018.
2. **QuickLoot EE** by ilian53 (https://github.com/Eloquence4/QuickLootEE), a fork of
   QuickLootRE, kept under the same MIT license and the same copyright line.
3. **QuickLoot IE** by Miss Corruption and contributors, which relicensed its own work under
   GPL-3.0-or-later with the two exceptions and kept the MIT notice for the code it took over.
4. **QuickLoot IE LE** (this repository), a port of QuickLoot IE 4.1.3.

The MIT license lets its code be reused in a GPL project on one condition: its copyright and
permission notice must stay with every copy. That is the only reason the [MIT](MIT) file is
here, reproduced unchanged. It is not the license of this port: QuickLoot IE LE as a whole is
distributed under GPL-3.0-or-later with the exceptions above.

This program comes with ABSOLUTELY NO WARRANTY, to the extent permitted by law (GPL-3.0,
sections 15 and 16).

## Third-party components

| Component | Location | Author | License |
|---|---|---|---|
| QuickLoot IE 4.1.3 (original source, unmodified) | `original/QuickLootIE-4.1.3/` | Miss Corruption and contributors | GPL-3.0-or-later with exceptions; `include/QuickLootAPI.h` MIT; CLIK ActionScript components as distributed by the original project |
| Dear ImGui | `ThirdParty/imgui/` | Omar Cornut and contributors | MIT (`ThirdParty/imgui/LICENSE.txt`) |
| KHA-OS interface headers | `src/Khaos/KhaosAPI.h`, `src/Khaos/KhaosThemeAPI.h` | Deno46 | Included with the author's permission (`src/Khaos/NOTICE.txt`) |
| SKSE 1.7.3 source (build dependency, not in this repository) | see `docs/BUILDING.md` | Ian Patterson, Stephen Abel, Paul Connelly and the SKSE team | SKSE license (`skse_license.txt` in the SKSE source); shipped in the `Source` folder of each release archive as required by the Linking Exception |

The loot window movie in `data/Interface/LootMenuIE.swf` is the original QuickLoot IE movie
with only its SWF header version changed (see `tools/prepare_swf.py` and
`evidence/swf-conversion.json`). Its source is `original/QuickLootIE-4.1.3/res/swf/Source/`.

## Corresponding source for releases

Every release archive contains a `Source` folder with the complete corresponding source of
the binaries it ships: this repository's code, the SKSE 1.7.3 files it is compiled with, Dear
ImGui and the original QuickLoot IE source.
