# Changes made by this port

QuickLoot IE LE 1.0.0 is a modified version of QuickLoot IE 4.1.3 by Miss Corruption
(https://github.com/MissCorruption/QuickLootIE), modified by Mhacks79 for Skyrim Legendary
Edition on 2026-11-06. It is released under the same terms as the original:
GPL-3.0-or-later WITH the Modding Exception and the GPL-3.0 Linking Exception
(LICENSE, EXCEPTIONS.md). The MIT notice of QuickLootRE (c) 2018 ryan-rsm-mckenzie is kept
in MIT. The unmodified original source is in `original/QuickLootIE-4.1.3` (in a release archive, under `Source/`).

## The plugin (`src/`)

Rewritten for Skyrim LE 1.9.32.0.8 and SKSE 1.7.3 (32-bit), without CommonLibSSE-NG or the
Address Library. Each file says at its top which original file it derives from.

| LE file | Derived from (QuickLoot IE 4.1.3 src/) | What changed |
|---|---|---|
| main.cpp | XSEPlugin.cpp, Observers/*.cpp | SKSE 1.7.3 entry points; event sinks only record and ask the game task |
| Loot.cpp/.h | MenuVisibilityManager.cpp, LootMenuManager.cpp, LootMenu.cpp (actions), Items/ItemStack.cpp (Take, Use) | all engine work on the game thread; LE engine calls; no container lid animation |
| Items.cpp/.h | Items/ItemStack*.cpp, Items/QuickLootItemStack.cpp, Items/Inventory.cpp, Items/ItemDefines.h, LootMenu.Sorting.cpp | plain rows instead of live engine objects; SKSE/I4/Completionist/Artifact data removed |
| LootMenu.cpp/.h | LootMenu.cpp, UniversalMenu.cpp, CLIK/ | LE IMenu; the movie only draws what the game task published |
| Input.cpp/.h | Input/InputManager.cpp, Input/InputObserver.cpp, Input/ButtonArt.cpp, Config/UserSettings.cpp (keys) | LE input events; mapping flags restored when the window hides |
| Hooks.cpp/.h | Behaviors/ActivationPrompt.cpp, Behaviors/LockpickActivation.cpp, Observers/SneakStateObserver.cpp | LE call sites; sneak changes read from a HUD frame probe |
| Settings.cpp/.h | Config/UserSettings.cpp, Config/SystemSettings.cpp, Config/Papyrus.h, QuickLootIEMCM.psc (defaults) | INI instead of MCM + JSON |
| Khaos.cpp/.h | QuickLootIEMCM.psc (pages) | new: KHA-OS settings page |
| Engine.cpp/.h, Guard.cpp/.h, Verify.cpp/.h, Log.cpp/.h, Signatures.h, Version.h | - | new |

## Assets

- `data/Interface/LootMenuIE.swf`: the original movie with its SWF header version changed from
  15 to 10 for the LE Scaleform player; every tag is byte-identical
  (tools/prepare_swf.py, evidence/swf-conversion.json). Its authoring source (LootMenuIE.fla
  and the ActionScript 2 classes) is in `original/QuickLootIE-4.1.3/res/swf/Source`.
- `data/Interface/translations/QuickLootIE_*.txt`: the original files with one line added,
  `$qlie_Hold{}`, so that hold-to-use keys show a translated label.
- Removed: QuickLootIE.esp, the Papyrus scripts and QuickLootIE_splash.swf (they only
  served the MCM).

## Third-party files in the source

- `src/Khaos/KhaosAPI.h`, `KhaosThemeAPI.h`: KHA-OS interface headers (c) Deno46,
  included with permission (see NOTICE.txt next to them).
- `ThirdParty/imgui`: Dear ImGui, MIT (LICENSE.txt).
- SKSE 1.7.3 source by the SKSE team (skse_license.txt): a build dependency, shipped in each release archive under `Source/skse-le-sdk`.
